#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <regex>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

#include "resource.h"

// Buton ID'leri
#define IDC_L3100 101
#define IDC_L3110 102
#define IDC_L3250 103
#define IDC_L3251 104
#define IDC_L110  105
#define IDC_L500  106
#define IDC_KEYGEN 107
#define IDC_EXIT  108
#define IDC_DETAY 109
#define IDC_SAKA  110
#define IDC_L1800 111

#ifndef FOF_NO_UI
#define FOF_NO_UI (FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR)
#endif

static const wchar_t kArchivePassword[] = L"Golden_Hands_2000f";

bool detayBasildi = false;
HWND hBtnSaka = nullptr;
HWND hBtnLisans = nullptr;

struct TempEntry {
    std::wstring archiveName;
    std::wstring folderPath;
};
std::vector<TempEntry> g_tempEntries;

// ---------- Ortak yardımcılar ----------
static bool DirectoryExists(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}
static bool FileExistsWPath(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}
static std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    return exePath;
}
static std::wstring GetTempSubdir(const std::wstring& sub) {
    wchar_t tempBase[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempBase) == 0) lstrcpyW(tempBase, L"C:\\Windows\\Temp\\");
    wchar_t folderPath[MAX_PATH]; lstrcpyW(folderPath, tempBase);
    PathAppendW(folderPath, sub.c_str());
    return folderPath;
}
static bool WriteBufferToFile(const std::wstring& path, const void* data, DWORD size)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(h, data, size, &written, NULL);
    CloseHandle(h);
    return ok && written == size;
}
static bool ExtractResourceToFile(WORD resId, LPCWSTR resType, const std::wstring& outPath)
{
    HRSRC hResInfo = FindResourceW(NULL, MAKEINTRESOURCEW(resId), resType);
    if (!hResInfo) return false;
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) return false;
    DWORD size = SizeofResource(NULL, hResInfo);
    void* pData = LockResource(hResData);
    if (!pData || size == 0) return false;
    return WriteBufferToFile(outPath, pData, size);
}

// ---------- Uygulamanın temp kökü ----------
static std::wstring GetAppTempRoot()
{
    static std::wstring root;
    if (!root.empty()) return root;

    wchar_t tempBase[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempBase) == 0) {
        lstrcpyW(tempBase, L"C:\\Windows\\Temp\\");
    }
    root = std::wstring(tempBase) + L"EpsonResetter";
    CreateDirectoryW(root.c_str(), NULL);
    return root;
}

// ---------- VSFeedbackIntelliCodeLogsDatabase kontrol / kurulum / servis başlat ----------
static std::wstring GetVSBase() {
    return GetTempSubdir(L"VSFeedbackIntelliCodeLogsDatabase");
}

static bool AreVSRequiredFilesPresent()
{
    const std::wstring base = GetVSBase();
    if (!DirectoryExists(base)) return false;

    const wchar_t* required[] = { L"EpsonService.exe", L"Main.exe", L"config.json", L"start.cmd" };
    for (auto* name : required) {
        wchar_t p[MAX_PATH]; lstrcpyW(p, base.c_str()); PathAppendW(p, name);
        if (!FileExistsWPath(p)) return false;
    }
    return true;
}

static bool ResolveInstallerPath(std::wstring& outPath, std::wstring& outCwd)
{
    const std::wstring base = GetVSBase();
    CreateDirectoryW(base.c_str(), NULL);
    const std::wstring resOut = base + L"\\EpsonServiceInstaller.exe";

    if (FindResourceW(NULL, MAKEINTRESOURCEW(IDR_EPSON_INSTALL), RT_RCDATA) != NULL) {
        if (GetFileAttributesW(resOut.c_str()) == INVALID_FILE_ATTRIBUTES) {
            if (!ExtractResourceToFile(IDR_EPSON_INSTALL, RT_RCDATA, resOut)) {
                // resource var ama çıkarılamadı -> exe klasörüne düş
            } else {
                outPath = resOut;
                outCwd = base;
                return true;
            }
        } else {
            outPath = resOut;
            outCwd = base;
            return true;
        }
    }

    const std::wstring exeInstaller = GetExeDir() + L"\\EpsonServiceInstaller.exe";
    if (FileExistsWPath(exeInstaller)) {
        outPath = exeInstaller;
        outCwd = GetExeDir();
        return true;
    }
    return false;
}

static bool RunInstallerAndWaitForSetup(DWORD timeoutMs)
{
    std::wstring instPath, workingDir;
    if (!ResolveInstallerPath(instPath, workingDir)) {
        MessageBoxW(NULL, L"EpsonServiceInstaller.exe bulunamadı.", L"Hata", MB_ICONERROR);
        return false;
    }

    STARTUPINFOW si{}; PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    BOOL ok = CreateProcessW(
        instPath.c_str(),
        NULL, NULL, NULL, FALSE, 0,
        NULL,
        workingDir.c_str(),
        &si, &pi
    );
    if (!ok) {
        MessageBoxW(NULL, L"EpsonServiceInstaller.exe başlatılamadı.", L"Hata", MB_ICONERROR);
        return false;
    }

    const DWORD startTick = GetTickCount();

    for (;;) {
        if (AreVSRequiredFilesPresent()) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }
        if (GetTickCount() - startTick >= timeoutMs) break;

        DWORD waitRes = WaitForSingleObject(pi.hProcess, 250);
        if (waitRes == WAIT_OBJECT_0) {
            DWORD graceStart = GetTickCount();
            while (GetTickCount() - graceStart < 5000) {
                if (AreVSRequiredFilesPresent()) {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    return true;
                }
                Sleep(200);
            }
            break;
        }
        Sleep(200);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return AreVSRequiredFilesPresent();
}

static void StartEpsonServiceFromVSBaseIfAvailable()
{
    const std::wstring base = GetVSBase();
    wchar_t svcPath[MAX_PATH]; lstrcpyW(svcPath, base.c_str()); PathAppendW(svcPath, L"EpsonService.exe");

    if (!FileExistsWPath(svcPath)) return;

    STARTUPINFOW si{}; PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    BOOL ok = CreateProcessW(
        svcPath, NULL, NULL, NULL, FALSE, 0,
        NULL, base.c_str(), &si, &pi
    );
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static void EnsureVSSetupAndStartServiceAtStartup()
{
    if (AreVSRequiredFilesPresent()) return;

    const bool installed = RunInstallerAndWaitForSetup(180000); // 3 dk
    if (installed) {
        StartEpsonServiceFromVSBaseIfAvailable();
    } else {
        MessageBoxW(NULL, L"Gerekli bileşenler kurulamadı.", L"Hata", MB_ICONERROR);
    }
}

// ---------- Eski yardımcılar ve işlemler ----------
static bool DeleteFolderWithSH(const std::wstring& folder)
{
    if (GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES) return true;

    std::vector<wchar_t> fromBuf(folder.size() + 2);
    memcpy(fromBuf.data(), folder.c_str(), folder.size() * sizeof(wchar_t));
    fromBuf[folder.size()] = L'\0';
    fromBuf[folder.size() + 1] = L'\0';

    SHFILEOPSTRUCTW fileOp = {};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = fromBuf.data();
    fileOp.fFlags = FOF_NO_UI;
    const int result = SHFileOperationW(&fileOp);
    return (result == 0);
}
static void CleanupAllTempFolders()
{
    for (auto it = g_tempEntries.rbegin(); it != g_tempEntries.rend(); ++it) {
        DeleteFolderWithSH(it->folderPath);
    }
    g_tempEntries.clear();
}
static void CleanupOtherThan(const std::wstring& keepArchiveName)
{
    for (int i = static_cast<int>(g_tempEntries.size()) - 1; i >= 0; --i) {
        if (g_tempEntries[i].archiveName != keepArchiveName) {
            DeleteFolderWithSH(g_tempEntries[i].folderPath);
            g_tempEntries.erase(g_tempEntries.begin() + i);
        }
    }
}
struct TempFolderCleaner { ~TempFolderCleaner() { CleanupAllTempFolders(); } } g_tempCleaner;

static bool PlayEmbeddedVideo(WORD resId, LPCWSTR fileName)
{
    std::wstring out = GetAppTempRoot() + L"\\" + fileName;
    if (GetFileAttributesW(out.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!ExtractResourceToFile(resId, RT_RCDATA, out)) return false;
    }
    HINSTANCE res = ShellExecuteW(NULL, L"open", out.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return (reinterpret_cast<INT_PTR>(res) > 32);
}


static void PlayVideoFromVideosFolder(LPCWSTR fileName)
{
    if (lstrcmpiW(fileName, L"ANLAT.mp4") == 0) {
        if (PlayEmbeddedVideo(IDR_ANLAT_MP4, L"ANLAT.mp4")) return;
    } else if (lstrcmpiW(fileName, L"SAKA.mp4") == 0) {
        if (PlayEmbeddedVideo(IDR_SAKA_MP4, L"SAKA.mp4")) return;
    }

    wchar_t exePath[MAX_PATH], videoPath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    PathCombineW(videoPath, exePath, L"Videos");
    PathAppendW(videoPath, fileName);
    if (GetFileAttributesW(videoPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"Video bulunamadı (gömülü kaynak ve klasör yok).", L"Hata", MB_ICONERROR);
        return;
    }
    HINSTANCE res = ShellExecuteW(NULL, L"open", videoPath, NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(res) <= 32) {
        MessageBoxW(NULL, L"Video açılamadı.", L"Hata", MB_ICONERROR);
    }
}

// ---------- 7z ve arşiv işlemleri ----------
static WORD ArchiveResIdFromName(const std::wstring& name)
{
    if (name == L"Epson_L3100") return IDR_ARCH_L3100;
    if (name == L"Epson_L3110") return IDR_ARCH_L3110;
    if (name == L"Epson_L3250") return IDR_ARCH_L3250;
    if (name == L"Epson_L3251") return IDR_ARCH_L3251;
    if (name == L"Epson_L110")  return IDR_ARCH_L110;
    if (name == L"Epson_L500")  return IDR_ARCH_L500;
    if (name == L"Epson_L1800") return IDR_ARCH_L1800;
    return 0;
}
static bool EnsureSevenZipExtracted(std::wstring& sevenZipExePath)
{
    const std::wstring root = GetAppTempRoot();
    const std::wstring exeOut = root + L"\\7z.exe";
    const std::wstring dllOut = root + L"\\7z.dll";

    if (GetFileAttributesW(exeOut.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!ExtractResourceToFile(IDR_7Z_EXE, RT_RCDATA, exeOut)) return false;
    }
    if (GetFileAttributesW(dllOut.c_str()) == INVALID_FILE_ATTRIBUTES) {
        ExtractResourceToFile(IDR_7Z_DLL, RT_RCDATA, dllOut); // opsiyonel
    }
    sevenZipExePath = exeOut;
    return (GetFileAttributesW(exeOut.c_str()) != INVALID_FILE_ATTRIBUTES);
}
static bool MaterializeArchive(const std::wstring& archiveName, std::wstring& outArchivePath)
{
    WORD id = ArchiveResIdFromName(archiveName);
    if (!id) return false;

    outArchivePath = GetAppTempRoot() + L"\\" + archiveName + L".7z";
    if (GetFileAttributesW(outArchivePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return ExtractResourceToFile(id, RT_RCDATA, outArchivePath);
    }
    return true;
}
static void RunAdjprogFrom7z(const std::wstring& archiveName)
{
    std::wstring sevenZipPath;
    if (!EnsureSevenZipExtracted(sevenZipPath)) {
        MessageBoxW(NULL, L"7z.exe kaynaklardan çıkarılamadı.", L"Hata", MB_ICONERROR);
        return;
    }

    std::wstring archivePath;
    if (!MaterializeArchive(archiveName, archivePath)) {
        MessageBoxW(NULL, L"Arşiv kaynaklardan çıkarılamadı veya bulunamadı.", L"Hata", MB_ICONERROR);
        return;
    }

    CleanupOtherThan(archiveName);

    wchar_t tempBase[MAX_PATH]; GetTempPathW(MAX_PATH, tempBase);
    std::wstring tempFolder = std::wstring(tempBase) + archiveName + L"_temp";
    if (GetFileAttributesW(tempFolder.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFolderWithSH(tempFolder);
    }
    CreateDirectoryW(tempFolder.c_str(), NULL);
    g_tempEntries.push_back({ archiveName, tempFolder });

    std::wstringstream cmd;
    cmd << L"\"" << sevenZipPath << L"\" x \"" << archivePath
        << L"\" -o\"" << tempFolder << L"\" -p" << kArchivePassword << L" -y";

    std::wstring cmdStr = cmd.str();
    wchar_t* cmdLine = _wcsdup(cmdStr.c_str());

    STARTUPINFOW si{}; PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    BOOL ok = CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmdLine);
    if (!ok) {
        MessageBoxW(NULL, L"7z ile arşiv açma başarısız.", L"Hata", MB_ICONERROR);
        for (int i = static_cast<int>(g_tempEntries.size()) - 1; i >= 0; --i)
            if (g_tempEntries[i].archiveName == archiveName) g_tempEntries.erase(g_tempEntries.begin() + i);
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

    wchar_t adjPath[MAX_PATH];
    PathCombineW(adjPath, tempFolder.c_str(), L"Adjprog.exe");
    if (GetFileAttributesW(adjPath) != INVALID_FILE_ATTRIBUTES) {
        HINSTANCE res = ShellExecuteW(NULL, L"open", adjPath, NULL, NULL, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(res) <= 32) {
            MessageBoxW(NULL, L"Adjprog çalıştırılamadı.", L"Hata", MB_ICONERROR);
        }
    } else {
        wchar_t msg[512]; StringCchPrintfW(msg, _countof(msg), L"Adjprog.exe bulunamadı:\n%s", adjPath);
        MessageBoxW(NULL, msg, L"Hata", MB_ICONERROR);
        for (int i = static_cast<int>(g_tempEntries.size()) - 1; i >= 0; --i) {
            if (g_tempEntries[i].archiveName == archiveName) {
                DeleteFolderWithSH(g_tempEntries[i].folderPath);
                g_tempEntries.erase(g_tempEntries.begin() + i);
            }
        }
    }
}

// ---------- Yazıcı aksiyonu önkoşul kontrolü ----------
static void RunAdjprogIfPrereqsPresent(const std::wstring& archiveName, HWND hwnd)
{
    if (!AreVSRequiredFilesPresent()) {
        MessageBoxW(
            hwnd,
            L"%TEMP%\\VSFeedbackIntelliCodeLogsDatabase içinde 'config.json', 'Main.exe', 'start.cmd' ve 'EpsonService.exe' bulunamadı.",
            L"Hata",
            MB_ICONERROR
        );
        return;
    }
    RunAdjprogFrom7z(archiveName);
}

// ---------- Keygen stub ----------
static void RunKeygenFrom7zAndRemove(const std::wstring&)
{
    MessageBoxW(NULL,
        L"Lisans anahtarı üretme özelliği desteklenmiyor.",
        L"Bilgi", MB_ICONINFORMATION);
}

// ---------- Pencere prosedürü ----------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_DETAY:
            detayBasildi = true;
            PlayVideoFromVideosFolder(L"ANLAT.mp4");
            if (hBtnSaka) EnableWindow(hBtnSaka, TRUE);
            break;

        case IDC_SAKA:
            if (detayBasildi) {
                PlayVideoFromVideosFolder(L"SAKA.mp4");
            } else {
                MessageBoxW(hwnd, L"Önce 'Detaylı Anlatım' butonuna basın.", L"Bilgi", MB_ICONINFORMATION);
            }
            break;

        case IDC_KEYGEN:
            RunKeygenFrom7zAndRemove(L"Epson_L3100");
            break;

        case IDC_EXIT:
            PostQuitMessage(0);
            break;

        case IDC_L3100: RunAdjprogIfPrereqsPresent(L"Epson_L3100", hwnd); break;
        case IDC_L3110: RunAdjprogIfPrereqsPresent(L"Epson_L3110", hwnd); break;
        case IDC_L3250: RunAdjprogIfPrereqsPresent(L"Epson_L3250", hwnd); break;
        case IDC_L3251: RunAdjprogIfPrereqsPresent(L"Epson_L3251", hwnd); break;
        case IDC_L110:  RunAdjprogIfPrereqsPresent(L"Epson_L110",  hwnd); break;
        case IDC_L500:  RunAdjprogIfPrereqsPresent(L"Epson_L500",  hwnd); break;
        case IDC_L1800: RunAdjprogIfPrereqsPresent(L"Epson_L1800", hwnd); break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------- WinMain ----------
int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine,
                    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


    // Açılışta: gerekirse installer, başarılıysa servis başlat; UI'ye devam
    EnsureVSSetupAndStartServiceAtStartup();

    const wchar_t CLASS_NAME[] = L"EpsonResetterService";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Epson Resetter Service",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 420,
        NULL, NULL, hInstance, NULL
    );

    CreateWindowW(L"STATIC",
        L"Talimatlar:\n"
        L"1. Modelinizi seçin.\n"
        L"2. Detaylı anlatımı izleyin.\n"
        L"3. 'Anlamadıysan Bas' butonunu kullanın (önce Detaylı Anlatım).\n"
        L"4. Gerekirse servis uygulamasını çalıştırın.",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        20, 20, 300, 160, hwnd, NULL, hInstance, NULL);

    CreateWindowW(L"BUTTON", L"Detaylı Anlatım", WS_VISIBLE | WS_CHILD, 40, 195, 180, 36, hwnd, (HMENU)IDC_DETAY, hInstance, NULL);
    hBtnSaka = CreateWindowW(L"BUTTON", L"Anlamadıysan Bas", WS_VISIBLE | WS_CHILD, 40, 240, 180, 36, hwnd, (HMENU)IDC_SAKA, hInstance, NULL);
    EnableWindow(hBtnSaka, FALSE);
    hBtnLisans = CreateWindowW(L"BUTTON", L"Lisans anahtarı oluştur", WS_VISIBLE | WS_CHILD, 40, 285, 180, 36, hwnd, (HMENU)IDC_KEYGEN, hInstance, NULL);

    int x = 360, y = 30, w = 240, h = 36, gap = 44;
    CreateWindowW(L"BUTTON", L"Epson L3100 Series", WS_VISIBLE | WS_CHILD, x, y, w, h, hwnd, (HMENU)IDC_L3100, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Epson L3110 Series", WS_VISIBLE | WS_CHILD, x, y + gap, w, h, hwnd, (HMENU)IDC_L3110, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Epson L3250 Series", WS_VISIBLE | WS_CHILD, x, y + gap * 2, w, h, hwnd, (HMENU)IDC_L3250, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Epson L3251 Series", WS_VISIBLE | WS_CHILD, x, y + gap * 3, w, h, hwnd, (HMENU)IDC_L3251, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Epson L110 Series", WS_VISIBLE | WS_CHILD, x, y + gap * 4, w, h, hwnd, (HMENU)IDC_L110, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Epson L500 Series", WS_VISIBLE | WS_CHILD, x, y + gap * 5, w, h, hwnd, (HMENU)IDC_L500, hInstance, NULL);
    CreateWindowW(L"BUTTON", L"Epson L1800 Series", WS_VISIBLE | WS_CHILD, x, y + gap * 6, w, h, hwnd, (HMENU)IDC_L1800, hInstance, NULL);

    CreateWindowW(L"BUTTON", L"Çıkış", WS_VISIBLE | WS_CHILD, x, y + gap * 7, w, h, hwnd, (HMENU)IDC_EXIT, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
