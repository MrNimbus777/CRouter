@echo off
echo [*] Configuring project...
cmake -G "MinGW Makefiles"

echo [*] Building project...
mingw32-make

echo.
echo Done. Press any key to exit.
pause > nul