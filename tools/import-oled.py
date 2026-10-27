"""Import the project's original frame-header format into the canonical OLED pack."""
from pathlib import Path
import argparse,hashlib,json,re
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('reference',type=Path,help='Folder containing frames/frame_00000.h etc.')
parser.add_argument('--period-ms',type=int,default=42)
parser.add_argument('--check',action='store_true',help='Compare against the current pack without writing')
args=parser.parse_args();root=Path(__file__).resolve().parent.parent
if not 1<=args.period_ms<=1000:raise SystemExit('Frame period must be 1..1000 ms')
files=sorted((args.reference/'frames').glob('frame_*.h'))
if not files or len(files)>65535:raise SystemExit('No supported frame sequence found')
frames=[]
for index,path in enumerate(files):
 if path.name!=f'frame_{index:05d}.h':raise SystemExit('Frames must be contiguous from frame_00000.h')
 text=path.read_text(encoding='utf-8');match=re.search(r'frame_\d+\[\]\s*=\s*\{(.*?)\}',text,re.S)
 if not match:raise SystemExit('Missing bitmap array: '+path.name)
 frame=bytes(int(value,16) for value in re.findall(r'0x([0-9a-fA-F]{2})',match.group(1)))
 if len(frame)!=1024:raise SystemExit('Only 128x64 one-bit frames are supported: '+path.name)
 frames.append(frame)
def pack(data):
 out=bytearray();at=0
 while at<len(data):
  run=1
  while at+run<len(data) and data[at+run]==data[at] and run<128:run+=1
  if run>=3:out.extend([127+run,data[at]]);at+=run
  else:
   start=at;at+=run
   while at<len(data) and at-start<128:
    run=1
    while at+run<len(data) and data[at+run]==data[at] and run<128:run+=1
    if run>=3:break
    at+=min(run,128-(at-start))
   out.append(at-start-1);out.extend(data[start:at])
 return bytes(out)
offsets=[0];payload=bytearray()
for frame in frames:payload.extend(pack(frame));offsets.append(len(payload))
meta={'width':128,'height':64,'frames':len(frames),'period_ms':args.period_ms,'frame_bytes':1024,'format':'Row-major 1-bit bitmap, MSB first','encoding':'Literal token 0..127: token+1 bytes. Run token 128..255: token-127 copies of the following byte.','raw_sha256':hashlib.sha256(b''.join(frames)).hexdigest(),'packed_sha256':hashlib.sha256(payload).hexdigest(),'offsets':offsets,'provenance':'Original animation supplied and owned by the project author'}
folder=root/'firmware/assets/oled'
if args.check:
 assert (folder/'idle.rle').read_bytes()==payload
 assert json.loads((folder/'idle.json').read_text(encoding='utf-8'))==meta
else:
 folder.mkdir(parents=True,exist_ok=True);(folder/'idle.rle').write_bytes(payload);(folder/'idle.json').write_text(json.dumps(meta,indent=2)+'\n',encoding='utf-8',newline='\n')
print(f'PASS: imported {len(frames)} frames, {len(payload):,} packed bytes. Run tools/build-oled.py after writing.')
