rem %1 - the input executable name
rem %2 - the solution directory
rem %3 - the platform

set "build_dir=%2..\..\build-%3\"
set "base_dir=%2..\..\"

xcopy /y "%1" "%build_dir%"
if ERRORLEVEL 1 goto return_error
xcopy /y/s "%base_dir%data" "%build_dir%\data\"
if ERRORLEVEL 1 goto return_error
xcopy /y/s "%base_dir%images" "%build_dir%\images\"
if ERRORLEVEL 1 goto return_error
xcopy /y/s "%base_dir%modules" "%build_dir%\modules\"
if ERRORLEVEL 1 goto return_error
xcopy /y/s "%base_dir%update" "%build_dir%\update\"
if ERRORLEVEL 1 goto return_error
xcopy /y/s "%base_dir%external\bin\%3\*.*" "%build_dir%"
if ERRORLEVEL 1 goto return_error

goto end

:return_error
echo "File Copy error, aborting"
EXIT 1
    
:end
EXIT 0