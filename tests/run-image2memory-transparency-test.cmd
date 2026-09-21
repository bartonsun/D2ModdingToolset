@echo off
setlocal
if "%~1"=="" exit /b 2
if "%~2"=="" exit /b 2
call "%~1" x86 >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /EHsc /std:c++17 /W4 /WX /wd4201 /I "%~dp0..\mss32\include" /I "%~dp0..\D2RSG\ScenarioGenerator\src" "%~dp0image2memory_transparency_test.cpp" /Fo"%~2\image2memory_transparency_test.obj" /Fe"%~2\image2memory_transparency_test.exe" /link user32.lib
if errorlevel 1 exit /b %errorlevel%
"%~2\image2memory_transparency_test.exe" native
if errorlevel 1 exit /b %errorlevel%
"%~2\image2memory_transparency_test.exe" cnc
if errorlevel 1 exit /b %errorlevel%
"%~2\image2memory_transparency_test.exe" legacy
exit /b %errorlevel%
