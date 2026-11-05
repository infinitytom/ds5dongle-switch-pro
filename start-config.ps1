$ErrorActionPreference = 'Stop'
$taskLauncher = Get-Command py -ErrorAction SilentlyContinue
$taskPython = Get-Command python -ErrorAction SilentlyContinue
if ($taskLauncher) {
    & $taskLauncher.Source -3 (Join-Path $PSScriptRoot 'web\serve.py')
} elseif ($taskPython -and $taskPython.Source -notlike '*WindowsApps*') {
    & $taskPython.Source (Join-Path $PSScriptRoot 'web\serve.py')
} else {
    throw 'Python 3 is required. Install Python 3 and run this launcher again.'
}
exit $LASTEXITCODE
