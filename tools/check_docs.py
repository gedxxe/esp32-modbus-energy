"""Check GitHub links, generated SVGs, netlist and version consistency (stdlib)."""
from pathlib import Path
from urllib.parse import unquote,urlsplit
import csv
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent.parent
errors=[]
def need(ok,message):
    if not ok:errors.append(message)
def exact_case(path):
    current=ROOT
    for part in path.relative_to(ROOT).parts:
        if not current.is_dir() or part not in {p.name for p in current.iterdir()}:return False
        current=current/part
    return True
def slugs(text):
    found=set();counts={}
    for heading in re.findall(r'^#{1,6}\s+(.+?)\s*#*$',text,re.M):
        slug=re.sub(r'[^\w\- ]','',heading.lower()).replace(' ','-')
        i=counts.get(slug,0);counts[slug]=i+1;found.add(slug+('-'+str(i) if i else ''))
    found.update(re.findall(r'id="([^" ]+)"',text))
    return found

markdown=[*ROOT.glob('*.md'),*sorted((ROOT/'docs').rglob('*.md')),*sorted((ROOT/'web/vendor').rglob('*.md'))]
link_count=0
for file in markdown:
    text=file.read_text(encoding='utf-8')
    # Fenced examples may contain syntax resembling relative links.
    prose=re.sub(r'^```.*?^```\s*$', '', text,flags=re.M|re.S)
    targets=re.findall(r'\[[^\]\n]*\]\(([^)]+)\)',prose)
    targets+=re.findall(r'(?:src|href)="([^"]+)"',prose)
    for target in targets:
        target=target.strip().strip('<>');parsed=urlsplit(target)
        if parsed.scheme or parsed.netloc:continue
        link_count+=1
        path=(file.parent/unquote(parsed.path)).resolve() if parsed.path else file
        if not path.is_relative_to(ROOT):errors.append(f'{file.relative_to(ROOT)}: link escapes repository: {target}');continue
        need(path.exists(),f'{file.relative_to(ROOT)}: missing link {target}')
        if path.exists():need(exact_case(path),f'{file.relative_to(ROOT)}: wrong filename case: {target}')
        if parsed.fragment and path.suffix=='.md' and path.exists():
            need(unquote(parsed.fragment) in slugs(path.read_text(encoding='utf-8')),f'{file.relative_to(ROOT)}: missing heading in {target}')

svgs=list((ROOT/'docs/assets').rglob('*.svg'))
for file in svgs:
    try:
        tree=ET.parse(file);root=tree.getroot()
        need(root.tag.endswith('svg') and bool(root.get('viewBox')),f'{file.relative_to(ROOT)}: no SVG/viewBox')
        for el in root.iter():
            tag=el.tag.rsplit('}',1)[-1]
            need(tag not in ['script','foreignObject'],f'{file.relative_to(ROOT)}: unsupported active SVG element {tag}')
            for key,value in el.attrib.items():
                need(not key.lower().startswith('on'),f'{file.relative_to(ROOT)}: SVG event handler')
                if key.endswith('href'):need(value.startswith('#'),f'{file.relative_to(ROOT)}: external SVG resource')
    except ET.ParseError as e:errors.append(f'{file.relative_to(ROOT)}: {e}')

version=re.search(r'FIRMWARE_VERSION\[\] = "([^"]+)"',(ROOT/'firmware/include/version.h').read_text()).group(1)
package=json.loads((ROOT/'package.json').read_text())
need(package['version']==version,'package.json and firmware versions differ')
need(package.get('license')=='GPL-3.0-only','package.json license must be GPL-3.0-only')
need((ROOT/'.node-version').read_text().strip()==package['engines']['node'],'Node build runtime differs from package.json')
need('README.id.md' in (ROOT/'README.md').read_text(encoding='utf-8'),'English README must link to Indonesian edition')
license_text=(ROOT/'LICENSE').read_text(encoding='utf-8')
need('Version 3, 29 June 2007' in license_text and 'END OF TERMS AND CONDITIONS' in license_text and len(license_text)>30000,'LICENSE is not the full GPLv3 text')
need((ROOT/'NOTICE').exists(),'Project license scope NOTICE missing')
if (ROOT/'package-lock.json').exists():
    lock=json.loads((ROOT/'package-lock.json').read_text());need(lock['version']==version,'package-lock version differs')
else:errors.append('package-lock.json missing')
need(version in (ROOT/'docs/assets/badges/firmware.svg').read_text(),'Firmware badge version differs')
readme=(ROOT/'README.md').read_text()
need('Firmware '+version in readme,'README firmware version differs')
for library,ver in re.findall(r'^\s+([^\n]+)@([\d.]+)\s*$',(ROOT/'firmware/platformio.ini').read_text(),re.M):
    need(ver in readme,f'README missing pinned version {library}@{ver}')
pins={k:int(v) for k,v in re.findall(r'(PIN_[A-Z0-9_]+)\s*=\s*(\d+)',(ROOT/'firmware/include/board.h').read_text())}
expected={'PIN_SDA':8,'PIN_SCL':9,'PIN_RX':18,'PIN_TX':17,'PIN_RS485_DE':16,'PIN_PREV':4,'PIN_NEXT':5,'PIN_OK':6,'PIN_BUZZER':7}
need(pins==expected,'GPIO assignment changed: update schematic routes and expected pin map together')
with (ROOT/'docs/wiring-netlist.csv').open(newline='',encoding='utf-8') as f:rows=list(csv.DictReader(f))
all_endpoints={r[k] for r in rows for k in ['from','to']}
for pin in pins.values():need('ESP32.GPIO'+str(pin) in all_endpoints,f'GPIO{pin} absent from netlist')
need('FORT.58/A' in all_endpoints and 'FORT.59/B' in all_endpoints,'Fort terminal mapping missing')
need(not any('FORT.70' in x for x in all_endpoints),'Switch-input COM must not be wired as RS485 ground')
need((ROOT/'AGENTS.md').exists(),'Root AGENTS.md missing')
web_result=subprocess.run([sys.executable,str(ROOT/'tools/check_web.py')],capture_output=True,text=True)
need(web_result.returncode==0,web_result.stderr.strip() or web_result.stdout.strip())
result=subprocess.run([sys.executable,str(ROOT/'tools/generate_docs.py'),'--check'],capture_output=True,text=True)
need(result.returncode==0,result.stderr.strip() or result.stdout.strip())
if errors:
    print('\n'.join('FAIL: '+e for e in errors));raise SystemExit(1)
print(f'PASS: {len(markdown)} Markdown files, {link_count} local links/anchors, {len(svgs)} SVGs, {len(rows)} wires, pin map and version {version}')
