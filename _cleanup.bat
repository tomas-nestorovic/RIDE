REM --- General clean-up ---
del _.d80 _.dsk ride.sdf
del /a:h ride.suo
del C:\WINDOWS\ride.ini
rd /s /q debug ipch release "Release MFC 4.2"

REM --- Main project clean-up ---
cd Main
del res\resource.aps
rd /s /q debug ipch release "Release MFC 4.2"
cd..

REM --- PropGrid project clean-up ---
cd PropGrid
rd /s /q debug ipch release "Release MFC 4.2"
cd..

REM --- TDI project clean-up ---
cd Tdi
rd /s /q debug ipch release "Release MFC 4.2"
cd..
