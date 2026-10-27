"""Verify the source-shared Fort library and pinned ModbusMaster changes."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parent.parent
library=root/'firmware/lib/FortMeter'
manifest=json.loads((library/'third_party/ModbusMaster/provenance.json').read_text(encoding='utf-8'))
for name,entry in manifest['files'].items():
 data=(library/name).read_text(encoding='utf-8').encode('utf-8')
 if hashlib.sha256(data).hexdigest()!=entry['sha256']:raise SystemExit('ModbusMaster provenance mismatch: '+name)
assert '#include <FortDiagnostics.h>' in (root/'examples/FortSerialDebug/FortSerialDebug.ino').read_text(encoding='utf-8')
assert '#include <FortDiagnostics.h>' in (root/'firmware/src/main.cpp').read_text(encoding='utf-8')
assert 'ModbusMaster master' in (library/'src/FortTransport.h').read_text(encoding='utf-8')
assert (library/'third_party/ModbusMaster/LICENSE').exists()
print('PASS: shared Fort library, ModbusMaster 2.0.1 provenance and service hooks')
