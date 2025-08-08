@echo OFF
setlocal enabledelayedexpansion
cd %cd%
set XCOMPRESSION_PATH="%cd%"


COLOR 8E
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore Cyan Welcome I am your XCOMPRESSION dependency updater bot, let me get to work...
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
echo.


powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XCOMPRESSION - FINDING VISUAL STUDIO / MSBuild
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
cd /d %XCOMPRESSION_PATH%

for /f "usebackq tokens=*" %%i in (`.\..\bin\vswhere -version 16.0 -sort -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    SET MSBUILD=%%i
)

for /f "usebackq tokens=1* delims=: " %%i in (`.\..\bin\vswhere -version 16.0 -sort -requires Microsoft.VisualStudio.Workload.NativeDesktop`) do (
    if /i "%%i"=="installationPath" set VSPATH=%%j
)

IF EXIST "%MSBUILD%" ( 
    echo VISUAL STUDIO VERSION: "%MSBUILD%"
    echo INSTALLATION PATH: "%VSPATH%"
    GOTO :DOWNLOAD_DEPENDENCIES
    )
powershell write-host -fore Red Failed to find VS2019 MSBuild!!! 
GOTO :ERROR

:DOWNLOAD_DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XCOMPRESSION - DOWNLOADING DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------

echo.
rmdir "../dependencies/zstd" /S /Q
git clone https://github.com/facebook/zstd.git "../dependencies/zstd"
if %ERRORLEVEL% GEQ 1 goto :ERROR

rmdir "../dependencies/xerr" /S /Q
git clone https://github.com/LIONant-depot/xerr.git "../dependencies/xerr"
if %ERRORLEVEL% GEQ 1 goto :ERROR


:COMPILATION
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XCOMPRESSION - COMPILING DEPENDENCIES
powershell write-host -fore White ------------------------------------------------------------------------------------------------------

powershell write-host -fore Cyan zstad: Updating...
"%VSPATH%\Common7\IDE\devenv.exe" "%CD%\..\dependencies\zstd\build\VS2010\libzstd\libzstd.vcxproj" /upgrade
if %ERRORLEVEL% GEQ 1 goto :ERROR

rem Forcely change the warning as errors as false
powershell -Command "(gc "%CD%\..\dependencies\zstd\build\VS2010\libzstd\libzstd.vcxproj") -replace '<TreatWarningAsError>true</TreatWarningAsError>', '<TreatWarningAsError>false</TreatWarningAsError>' | Out-File -encoding ASCII "%CD%\..\dependencies\zstd\build\VS2010\libzstd\libzstd.vcxproj""

echo.
powershell write-host -fore Cyan zstad Release: Compiling...
"%MSBUILD%" "%CD%\..\dependencies\zstd\build\VS2010\libzstd\libzstd.vcxproj" /p:configuration=Release /p:Platform="x64" /verbosity:minimal 
if %ERRORLEVEL% GEQ 1 goto :ERROR

echo.
powershell write-host -fore Cyan zstad Debug: Compiling...
"%MSBUILD%" "%CD%\..\dependencies\zstd\build\VS2010\libzstd\libzstd.vcxproj" /p:configuration=Debug /p:Platform="x64" /verbosity:minimal 
if %ERRORLEVEL% GEQ 1 goto :ERROR

:DONE
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
powershell write-host -fore White XCOMPRESSION - SUCCESSFULLY DONE!!
powershell write-host -fore White ------------------------------------------------------------------------------------------------------
goto :PAUSE

:ERROR
powershell write-host -fore Red ------------------------------------------------------------------------------------------------------
powershell write-host -fore Red XCOMPRESSION - FAILED!!
powershell write-host -fore Red ------------------------------------------------------------------------------------------------------

:PAUSE
rem if no one give us any parameters then we will pause it at the end, else we are assuming that another batch file called us
if %1.==. pause