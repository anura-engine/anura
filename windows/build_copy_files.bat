rem %1 - the output path
rem %2 - the solution directory
rem %3 - the platform

:: To avoid having to copy all of Frogatto, first run:
:: New-Item -ItemType Junction -Path "$HOME\Documents\anura\windows\x64-Release\modules\frogatto4" -Target "$HOME\Documents\anura\modules\frogatto4"

set "build_dir=%1"
set "base_dir=%2..\"

:: Copy over module stuff.
xcopy /y/s/d "%base_dir%data" "%build_dir%\data\"
if ERRORLEVEL 1 goto return_error
xcopy /y/s/d "%base_dir%images" "%build_dir%\images\"
if ERRORLEVEL 1 goto return_error
xcopy /y/s/d "%base_dir%modules" "%build_dir%\modules\" /EXCLUDE:%2excluded_from_build_files.txt
if ERRORLEVEL 1 goto return_error
xcopy /y/s/d "%base_dir%update" "%build_dir%\update\"
if ERRORLEVEL 1 goto return_error
xcopy /y/s/d "%base_dir%music" "%build_dir%\music\"
if ERRORLEVEL 1 goto return_error

:: Copy two missing .dlls FrostC reported on his machine. I think we should distribute the proper redistributiable.exe for these?
xcopy /y      "C:\Windows\System32\MSVCP140.dll" "%build_dir%\"
if ERRORLEVEL 1 goto return_error
xcopy /y      "C:\Windows\System32\vcruntime140.dll" "%build_dir%\"
if ERRORLEVEL 1 goto return_error

goto end

:return_error
echo "File Copy error, aborting"
EXIT 1

:end
EXIT 0