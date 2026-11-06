@echo off
rem ParallaxStyle: installa o aggiorna il plugin VST3 (eseguire come amministratore).
rem Prima rimuove ogni versione precedente di ParallaxStyle, poi copia quella nuova.
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Esegui questo script come amministratore: tasto destro, Esegui come amministratore.
    pause
    exit /b 1
)
set "DEST=%CommonProgramFiles%\VST3"
if not exist "%DEST%" mkdir "%DEST%"
for /d %%D in ("%DEST%\ParallaxStyle*.vst3") do rmdir /s /q "%%D"
for /d %%D in ("%~dp0ParallaxStyle-*.vst3") do (
    xcopy /E /I /Y "%%D" "%DEST%\%%~nxD" >nul
    echo Installato: %DEST%\%%~nxD
)
echo Riavvia la DAW e fai una nuova scansione dei plugin.
pause
