$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$testOut = Join-Path $taskRoot 'tmp'
New-Item -ItemType Directory -Force -Path $testOut | Out-Null
& g++ -std=c++11 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'firmware/include') (Join-Path $taskRoot 'firmware/test/test_energy.cpp') -o (Join-Path $testOut 'test_energy.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $testOut 'test_energy.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& g++ -std=c++11 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'firmware/test/arduino_stub') (Join-Path $taskRoot 'firmware/test/test_debug_sketch.cpp') -o (Join-Path $testOut 'test_debug_sketch.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $testOut 'test_debug_sketch.exe')
exit $LASTEXITCODE
