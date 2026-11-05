@echo off
rem The SDK invokes python3, while Windows usually installs py/python.
if defined DS5_PYTHON (
    "%DS5_PYTHON%" %*
    exit /b
)
where py >nul 2>nul
if not errorlevel 1 (
    py -3 %*
    exit /b
)
python %*
