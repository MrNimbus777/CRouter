@echo off
set /p msg="Enter commit message: "
git add .
pause
git commit -m "%msg%"
git remote get-url origin >nul 2>&1
IF ERRORLEVEL 1 (
    git remote add origin https://github.com/MrNimbus777/CRouter.git
) ELSE (
    git remote set-url origin https://github.com/MrNimbus777/CRouter.git
)
git pull --rebase origin main
git push --force origin main
pause