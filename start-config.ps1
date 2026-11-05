$ErrorActionPreference = 'Stop'
$taskLauncher = Get-Command py -ErrorAction SilentlyContinue
$taskPython = Get-Command python -ErrorAction SilentlyContinue
if ($taskLauncher) {
    & $taskLauncher.Source -3 (Join-Path $PSScriptRoot 'web\serve.py')
} elseif ($taskPython -and $taskPython.Source -notlike '*WindowsApps*') {
    & $taskPython.Source (Join-Path $PSScriptRoot 'web\serve.py')
} else {
    throw '需要 Python 3：安装后重新运行此脚本。'
}
