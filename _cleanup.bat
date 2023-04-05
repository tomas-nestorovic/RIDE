REM --- General clean-up ---
del _.d80 _.d40 _.dsk _.ima _.img _.trd _.scl _.tap _.mgt _.mbd _.scp ride.sdf
del /a:h ride.suo
del C:\WINDOWS\ride.ini
rd /s /q debug ipch release "Release MFC 4.2" "Debug in RAMdisk"
rd /s /q r:\ipch r:\ride
attrib -h -s /d "%~dp0\.vs"
rd /s /q "%~dp0\.vs"

REM --- Main project clean-up ---
cd Main
del res\resource.aps
rd /s /q debug ipch release "Release MFC 4.2" "Debug in RAMdisk"
cd..

REM --- PropGrid project clean-up ---
cd PropGrid
rd /s /q debug ipch release "Release MFC 4.2" "Debug in RAMdisk"
cd..

REM --- TDI project clean-up ---
cd Tdi
rd /s /q debug ipch release "Release MFC 4.2" "Debug in RAMdisk"
cd..

REM --- Yahel module clean-up ---
cd Yahel
_cleanup.bat
cd..
