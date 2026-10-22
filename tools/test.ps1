$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$testOut = Join-Path $taskRoot 'tmp'
New-Item -ItemType Directory -Force -Path $testOut | Out-Null
& g++ -std=c++11 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'firmware/include') (Join-Path $taskRoot 'firmware/test/test_energy.cpp') -o (Join-Path $testOut 'test_energy.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $testOut 'test_energy.exe')
exit $LASTEXITCODE
