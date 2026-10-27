"""Create the Arduino IDE library ZIP without requiring Node or a firmware build."""
from pathlib import Path
import re,subprocess,sys,zipfile
root=Path(__file__).resolve().parent.parent
subprocess.run([sys.executable,str(root/'tools/check_meter.py')],check=True)
library=root/'firmware/lib/FortMeter'
version=re.search(r'^version=(.+)$',(library/'library.properties').read_text(),re.M).group(1)
output=root/f'output/FortMeter-library-v{version}.zip';output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as archive:
 for p in sorted(library.rglob('*')):
  if p.is_file():archive.write(p,'FortMeter/'+p.relative_to(library).as_posix())
with zipfile.ZipFile(output) as archive:assert archive.testzip() is None
print(f'Arduino library ZIP verified: {output.name}, {output.stat().st_size:,} bytes')
