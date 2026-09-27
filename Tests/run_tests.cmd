@echo off
rem Builds and runs all tests: the Invoker Core tests and the Practice tests.
rem
rem   Tests\run_tests.cmd [Debug^|Release] [x64^|Win32] [randomKeyCount [seed]]
rem
rem Defaults: Debug x64, 300000 random keys, seed 12345 (the last two go to the Core tests).
rem Exit code 0 = every check of both programs passed.
setlocal

set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Debug
set PLATFORM=%~2
if "%PLATFORM%"=="" set PLATFORM=x64

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%VSWHERE%" goto :novs

set MSBUILD=
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i
if "%MSBUILD%"=="" goto :novs

"%MSBUILD%" "%~dp0InvokerCoreTests.vcxproj" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /nologo /v:minimal
if errorlevel 1 exit /b 1
"%MSBUILD%" "%~dp0PracticeTests.vcxproj" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /nologo /v:minimal
if errorlevel 1 exit /b 1

set RESULT=0

echo.
echo ===== Invoker Core tests =====
"%~dp0bin\%PLATFORM%\%CONFIG%\InvokerCoreTests.exe" %3 %4
if errorlevel 1 set RESULT=1

echo.
echo ===== Practice tests =====
"%~dp0bin\%PLATFORM%\%CONFIG%\PracticeTests.exe"
if errorlevel 1 set RESULT=1

echo.
if "%RESULT%"=="0" (echo ALL TESTS PASSED) else (echo TESTS FAILED)
exit /b %RESULT%

:novs
echo Visual Studio 2022 with the "Desktop development with C++" workload (MSBuild) was not found.
exit /b 2
