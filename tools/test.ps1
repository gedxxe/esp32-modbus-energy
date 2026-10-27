$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$testOut = Join-Path $taskRoot 'tmp'
New-Item -ItemType Directory -Force -Path $testOut | Out-Null
& g++ -std=c++11 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'firmware/include') (Join-Path $taskRoot 'firmware/test/test_energy.cpp') -o (Join-Path $testOut 'test_energy.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $testOut 'test_energy.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& g++ -std=c++11 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'firmware/test/arduino_stub') -I (Join-Path $taskRoot 'firmware/lib/FortMeter/src') (Join-Path $taskRoot 'firmware/test/test_debug_sketch.cpp') (Join-Path $taskRoot 'firmware/lib/FortMeter/src/ModbusMaster.cpp') -o (Join-Path $testOut 'test_debug_sketch.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $testOut 'test_debug_sketch.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& g++ -std=c++11 -Wall -Wextra -Werror -I (Join-Path $taskRoot 'firmware/test/arduino_stub') -I (Join-Path $taskRoot 'firmware/lib/FortMeter/src') -I (Join-Path $taskRoot 'firmware/include') (Join-Path $taskRoot 'firmware/test/test_diagnostics.cpp') (Join-Path $taskRoot 'firmware/lib/FortMeter/src/ModbusMaster.cpp') -o (Join-Path $testOut 'test_diagnostics.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $testOut 'test_diagnostics.exe')
exit $LASTEXITCODE
