rem %1 - the output path
rem %2 - the solution directory
rem %3 - the platform

set "build_dir=%1"
set "base_dir=%2..\"

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
xcopy /y  /d "%base_dir%master-config.cfg" "%build_dir%\"
if ERRORLEVEL 1 goto return_error

goto end

:return_error
echo "File Copy error, aborting"
EXIT 1

:end
EXIT 0