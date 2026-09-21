@echo off
setlocal
if "%~1"=="" exit /b 2
if "%~2"=="" exit /b 2
call "%~1" x86 >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /EHsc /std:c++17 /W4 /WX /I "%~dp0..\mss32\include" "%~dp0upgradeportraitlayout_test.cpp" /Fo"%~2\upgradeportraitlayout_test.obj" /Fe"%~2\upgradeportraitlayout_test.exe"
if errorlevel 1 exit /b %errorlevel%
"%~2\upgradeportraitlayout_test.exe"
exit /b %errorlevel%
