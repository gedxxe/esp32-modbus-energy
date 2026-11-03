"""Verify the public v1 gateway interoperability fixture (no real keys)."""
from pathlib import Path
import hashlib,hmac,json,struct
root=Path(__file__).resolve().parent.parent
v=json.loads((root/'docs/lora-v1-vector.json').read_text())
key=bytes.fromhex(v['key_hex']);body=bytes.fromhex(v['body_hex']);tag=bytes.fromhex(v['tag_hex'])
assert len(key)==32 and len(body)==48 and body[:4]==b'ES\x01\x01'
assert struct.unpack_from('<HHHBBIQ',body,4)==(1,2,1,24,0,7,0x0807060504030201)
assert hmac.compare_digest(hmac.new(key,body,hashlib.sha256).digest()[:16],tag)
assert (body+tag).hex()==v['packet_hex']
print('PASS: public gateway v1 HELLO byte order and independent SHA256-HMAC vector')
