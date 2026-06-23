@echo off
echo ===================================================
echo   OOPXtreme Smart City Simulator - Compile and Run
echo ===================================================

:: Add local w64devkit compiler to path from university_database directory
set "PATH=C:\Users\NEW\.gemini\antigravity\scratch\university_database\w64devkit\bin;%PATH%"

:: Verify compiler
where g++ >nul 2>nul
if errorlevel 1 goto nocompiler

echo [1/2] Compiling C++ Backend...
g++ -std=c++17 backend/main.cpp -lws2_32 -lcrypt32 -o server.exe
if errorlevel 1 goto compfail

echo [2/2] Compilation successful! Starting localhost server...
echo.
echo Server is running! Open your browser and navigate to:
echo -------------------------------------
echo   http://localhost:8080
echo -------------------------------------
echo.
echo Press Ctrl+C in this terminal window to stop the server.
echo.

server.exe
goto end

:nocompiler
echo [ERROR] C++ Compiler g++ not found in path.
echo Tried adding C:\Users\NEW\.gemini\antigravity\scratch\university_database\w64devkit\bin
pause
exit /b 1

:compfail
echo.
echo [ERROR] Compilation failed.
pause
exit /b 1

:end
