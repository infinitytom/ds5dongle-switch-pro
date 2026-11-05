$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskTempBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$taskFolderName = 'ds5 launcher ' + [char]0x6D4B + [char]0x8BD5 + ' ' + [guid]::NewGuid().ToString('N')
$taskTemp = Join-Path $taskTempBase $taskFolderName
try {
    New-Item -ItemType Directory -Path (Join-Path $taskTemp 'web') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskRoot 'start-config.ps1') -Destination $taskTemp
    # Use the actual legacy powershell.exe parser and Python discovery, with a
    # tiny server fixture so the test never opens a browser or binds a port.
    [System.IO.File]::WriteAllText((Join-Path $taskTemp 'web\serve.py'), "print('DS5_LAUNCHER_OK')", [System.Text.Encoding]::ASCII)
    $taskBytes = [System.IO.File]::ReadAllBytes((Join-Path $taskTemp 'start-config.ps1'))
    if (@($taskBytes | Where-Object { $_ -gt 127 }).Count -ne 0) {
        throw 'The Windows launcher must remain ASCII to support legacy PowerShell encodings.'
    }
    $taskOutput = & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $taskTemp 'start-config.ps1') 2>&1
    if ($LASTEXITCODE -ne 0 -or "$taskOutput" -notmatch 'DS5_LAUNCHER_OK') {
        throw "Legacy PowerShell launch failed: $taskOutput"
    }
    [System.IO.File]::WriteAllText((Join-Path $taskTemp 'web\serve.py'), 'raise SystemExit(7)', [System.Text.Encoding]::ASCII)
    & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $taskTemp 'start-config.ps1')
    if ($LASTEXITCODE -ne 7) { throw 'Python failure exit code was lost.' }
    Write-Output 'Windows PowerShell launcher: parsing, Unicode/spaced paths and failure exit code passed.'
} finally {
    $taskResolved = [System.IO.Path]::GetFullPath($taskTemp)
    if (-not $taskResolved.StartsWith($taskTempBase, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not ([System.IO.Path]::GetFileName($taskResolved)).StartsWith('ds5 launcher ')) {
        throw 'Unexpected launcher test directory; cleanup refused.'
    }
    if (Test-Path -LiteralPath $taskResolved) { Remove-Item -LiteralPath $taskResolved -Recurse -Force }
}
