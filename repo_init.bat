@echo off
setlocal

pushd "%~dp0" || exit /b 1
git rev-parse --is-inside-work-tree >nul 2>&1 || goto :failed

for /f "delims=" %%F in ('git ls-files "*.vcxproj.user"') do (
	git update-index --skip-worktree -- "%%F"
	if errorlevel 1 goto :failed
)

if /I "%~1"=="--configure-only" goto :done
start "" "blitter.sln"

:done
popd
exit /b 0

:failed
popd
exit /b 1
