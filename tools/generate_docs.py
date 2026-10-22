"""Generate GitHub-safe local SVGs from the firmware configuration (stdlib only)."""
from pathlib import Path
from html import escape
import re
import csv
import io
import sys

ROOT=Path(__file__).resolve().parent.parent
ASSETS=ROOT/'docs/assets'
BG='#0e1825'; PANEL='#172638'; LINE='#31465c'; TEXT='#edf5ff'; MUTED='#a9bfd2'
RED='#ff7d85'; GRAY='#afbdcf'; BLUE='#57c7ff'; GREEN='#85e3bb'; PURPLE='#bfabff'; AMBER='#ffd184'
version=re.search(r'FIRMWARE_VERSION\[\] = "([^"]+)"',(ROOT/'firmware/include/version.h').read_text()).group(1)
ini=(ROOT/'firmware/platformio.ini').read_text()
libs=dict(re.findall(r'^\s+([^\n]+)@([\d.]+)\s*$',ini,re.M))
def write_generated(path,contents):
    if '--check' in sys.argv:
        if not path.exists() or path.read_text(encoding='utf-8')!=contents:
            raise SystemExit(f'Stale generated asset: {path.relative_to(ROOT)}; run tools/generate_docs.py')
    else:
        path.parent.mkdir(parents=True,exist_ok=True);path.write_text(contents,encoding='utf-8')

class SVG:
    def __init__(self,w,h,title,desc='Diagram lokal Midea Fort. Warna dan label menandai koneksi; data tampilan adalah simulasi.'):
        self.w=w;self.h=h;self.parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img" aria-labelledby="title desc"><title id="title">{escape(title)}</title><desc id="desc">{escape(desc)}</desc><rect width="100%" height="100%" fill="{BG}"/>']
    def rect(self,x,y,w,h,fill=PANEL,stroke=LINE,r=16):
        self.parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}"/>')
    def text(self,x,y,s,size=18,color=TEXT,weight=400,anchor='start',mono=False):
        self.parts.append(f'<text x="{x}" y="{y}" fill="{color}" font-family="{("Consolas, monospace" if mono else "Arial, sans-serif")}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}">{escape(str(s))}</text>')
    def wire(self,points,color=BLUE,width=3,dash=False):
        self.parts.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round" stroke-linecap="round"'+(' stroke-dasharray="7 6"' if dash else '')+'/>')
    def dot(self,x,y,color=TEXT,r=4):
        self.parts.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')
    def port(self,x,y,label,color=TEXT,side='right'):
        self.dot(x,y,color,5);self.text(x+14 if side=='left' else x-14,y+6,label,17,color,anchor='start' if side=='left' else 'end',mono=True)
    def heading(self,title,sub):
        self.text(45,60,title,32,weight=700);self.text(45,96,sub,18,MUTED)
    def save(self,name):
        write_generated(ASSETS/name,'\n'.join(self.parts+['</svg>'])+'\n')

def badge(name,label,value,color,icon):
    left=42+len(label)*7;right=max(50,20+len(value)*7);w=left+right
    s=SVG(w,30,label+' '+value,label+': '+value+'. Metadata project.');s.rect(0,0,w,30,PANEL,LINE,6);s.rect(left,0,right,30,color,color,5)
    if icon=='chip':
        s.rect(10,8,14,14,'none',TEXT,2)
        for z in (12,17,22):s.wire([(z,5),(z,8)],TEXT,1);s.wire([(z,22),(z,25)],TEXT,1)
    elif icon=='display':s.rect(8,7,19,15,'none',TEXT,2);s.wire([(12,25),(24,25)],TEXT,1)
    elif icon=='clock':
        s.parts.append(f'<circle cx="18" cy="15" r="9" fill="none" stroke="{TEXT}" stroke-width="1.5"/>');s.wire([(18,9),(18,15),(23,18)],TEXT,1.5)
    elif icon=='code':s.text(7,21,'{ }',16,TEXT,mono=True)
    elif icon=='check':s.wire([(9,15),(15,21),(26,9)],TEXT,2)
    else:s.wire([(9,19),(15,11),(21,19),(27,11)],TEXT,2)
    s.text(35,20,label,12,TEXT,600);s.text(left+right/2,20,value,12,BG,700,anchor='middle');s.save('badges/'+name+'.svg')

badge('firmware','Firmware',version,BLUE,'chip')
badge('license','License','GPL-3.0-only',GREEN,'check')
badge('platformio','Espressif32','6.10.0',AMBER,'wave')
badge('arduino','Arduino ESP32','2.0.17',GREEN,'chip')
badge('board','ESP32-S3','N16R8',PURPLE,'chip')
badge('freertos','FreeRTOS','2 cores',GREEN,'wave')
for name,key,label,icon,c in [
    ('ssd1306','adafruit/Adafruit SSD1306','SSD1306','display',BLUE),
    ('gfx','adafruit/Adafruit GFX Library','Adafruit GFX','display',GREEN),
    ('busio','adafruit/Adafruit BusIO','Adafruit BusIO','wave',AMBER),
    ('rtclib','adafruit/RTClib','RTClib','clock',PURPLE),
    ('arduinojson','bblanchon/ArduinoJson','ArduinoJson','code',BLUE)]:badge(name,label,libs[key],c,icon)

s=SVG(1400,360,'Midea Engineering Fort Energy Station')
s.text(55,60,'MIDEA  /  ENGINEERING',18,BLUE,700);s.text(55,133,'Fort Energy Station',55,weight=700)
s.text(55,181,'Pembacaan kWh lokal. Interval yang dapat ditelusuri.',24,MUTED)
s.text(55,229,'ESP32-S3 · Modbus RTU · RTC · OLED · Dashboard Wi-Fi',19,MUTED)
for i,(a,b) in enumerate([('09 INTERVAL','06.00–09.00 / 16.30–18.00'),('LOCAL FIRST','192.168.4.1'),('DUAL CORE','Akuisisi + antarmuka')]):
    x=55+i*425;s.rect(x,265,400,65);s.text(x+18,291,a,12,BLUE,700);s.text(x+18,315,b,17)
s.save('hero.svg')

# Full schematic. Net labels are electrical connections, not physical header positions.
s=SVG(1800,1320,'Wiring lengkap ESP32-S3, OLED, RTC, RS485, Fort, tombol dan buzzer')
s.heading('Wiring lengkap · Midea Fort Energy Station','Pin sesuai board.h · RS485 3,3 V terkonfirmasi · ESP32 satu-satunya master · gambar sambungan logis')
s.rect(650,180,350,780,'#18344a',BLUE);s.text(825,225,'ESP32-S3',30,weight=700,anchor='middle');s.text(825,258,'N16R8 · DevKitC style',18,MUTED,anchor='middle')
s.rect(715,120,220,45);s.text(825,149,'USB-C / suplai board',16,anchor='middle');s.wire([(825,165),(825,180)],RED)
s.text(825,590,'UART1 + I2C',20,BLUE,600,anchor='middle');s.text(825,625,'Core 0: web / UI',17,MUTED,anchor='middle');s.text(825,653,'Core 1: akuisisi',17,MUTED,anchor='middle');s.text(825,705,'3V3 dari regulator board',16,RED,anchor='middle')
for y,label,c in [(300,'3V3',RED),(340,'GND',GRAY),(380,'GPIO8 / SDA',BLUE),(420,'GPIO9 / SCL',GREEN),(800,'GPIO4 / PREV',PURPLE),(840,'GPIO5 / NEXT',PURPLE),(880,'GPIO6 / OK',PURPLE)]:s.port(650,y,label,c,'left')
for y,label,c in [(300,'3V3',RED),(340,'GND',GRAY),(390,'GPIO18 / RX',BLUE),(430,'GPIO17 / TX',GREEN),(490,'GPIO16 / DIR',PURPLE),(800,'GPIO7 / BUZZ',AMBER)]:s.port(1000,y,label,c)
for top,title,sub,ys in [(240,'OLED SSD1306','I2C · 128 × 64 · 0x3C',[300,340,380,420]),(530,'RTC DS3231','I2C · 0x68 · baterai backup',[590,630,670,710])]:
    s.rect(60,top,330,235);s.text(85,top+37,title,24,weight=700);s.text(85,top+65,sub,15,MUTED)
    for y,label,c in zip(ys,['VCC','GND','SDA','SCL'],[RED,GRAY,BLUE,GREEN]):s.port(390,y,label,c)
# The OLED's first ports share the title region: cover text was positioned above.
for y,c in [(300,RED),(340,GRAY),(380,BLUE),(420,GREEN)]:s.wire([(390,y),(650,y)],c)
for y,target,x,c in [(590,300,450,RED),(630,340,480,GRAY),(670,380,520,BLUE),(710,420,560,GREEN)]:
    s.wire([(390,y),(x,y),(x,target),(650,target)],c);s.dot(x,target,c)
s.text(60,805,'SDA dan SCL dipakai bersama oleh OLED + RTC.',16,MUTED)
s.rect(1190,240,260,330);s.text(1210,278,'RS485 · 3V3',23,weight=700)
for y,label,c in [(300,'VCC',RED),(340,'GND',GRAY),(390,'RO → RX',BLUE),(430,'DI ← TX',GREEN),(480,'DE',PURPLE),(520,'/RE',PURPLE)]:s.port(1190,y,label,c,'left')
for y,label,c in [(340,'A',AMBER),(390,'B',GREEN)]:s.port(1450,y,label,c)
for y,c in [(300,RED),(340,GRAY),(390,BLUE),(430,GREEN)]:s.wire([(1000,y),(1190,y)],c)
s.wire([(1000,490),(1100,490),(1100,480),(1190,480)],PURPLE);s.wire([(1100,490),(1100,520),(1190,520)],PURPLE);s.dot(1100,490,PURPLE)
s.text(1025,555,'DE + /RE disatukan',15,PURPLE)
s.rect(1540,240,210,330);s.text(1560,278,'FORT',26,weight=700);s.text(1560,310,'Power meter',16,MUTED)
for y,label,c in [(340,'58 / A',AMBER),(390,'59 / B',GREEN)]:s.port(1540,y,label,c,'left');s.wire([(1450,y),(1540,y)],c)
s.text(1560,455,'Port existing',16,MUTED);s.text(1560,482,'belum terhubung',16,MUTED);s.text(1560,509,'ke perangkat lain',16,MUTED)
s.text(1190,612,'Manual: terminal 58 = A, terminal 59 = B.',18,AMBER)
s.text(1190,640,'70 / COM adalah switch input, bukan ground RS485.',16,MUTED)
# Three physical switches, all common to local ground.
s.rect(60,850,330,300);s.text(85,882,'3 × TACTILE SWITCH',20,weight=700)
for y,label in [(920,'PREV'),(985,'NEXT'),(1050,'OK')]:
    s.text(85,y-12,label,16,PURPLE,700);s.wire([(180,y),(240,y)],GRAY);s.dot(240,y,GRAY);s.dot(275,y,PURPLE);s.wire([(240,y),(267,y-15)],GRAY,3);s.wire([(275,y),(390,y)],PURPLE)
s.wire([(180,920),(180,1110)],GRAY);s.text(196,1117,'GND (net bersama)',15,GRAY)
for by,sy,x in [(800,920,590),(840,985,610),(880,1050,630)]:s.wire([(650,by),(x,by),(x,sy),(390,sy)],PURPLE)
s.text(415,1110,'INPUT_PULLUP',16,PURPLE);s.text(415,1138,'Tekan = LOW',16,MUTED)
# NPN low-side driver for an active 2-wire buzzer.
s.rect(1130,700,620,450);s.text(1160,741,'BUZZER AKTIF · driver 2 kaki',23,weight=700)
s.wire([(1000,800),(1080,800),(1080,900),(1190,900)],AMBER)
s.rect(1190,890,85,20,'none',AMBER,2);s.text(1232,876,'1 kΩ',16,AMBER,anchor='middle');s.wire([(1275,900),(1440,900)],AMBER)
s.parts.append(f'<circle cx="1460" cy="900" r="36" fill="none" stroke="{TEXT}" stroke-width="2"/>')
s.wire([(1440,879),(1440,921)],TEXT,3);s.wire([(1440,889),(1480,868),(1560,868),(1560,828)],TEXT);s.wire([(1440,911),(1480,935),(1480,1060)],GRAY)
s.wire([(1471,928),(1479,935),(1469,934)],GRAY,2);s.text(1525,966,'Q1 NPN',16,MUTED);s.text(1525,990,'B / C / E',14,MUTED)
s.text(1419,883,'B',14,AMBER);s.text(1491,855,'C',14,MUTED);s.text(1494,942,'E',14,GRAY)
s.rect(1518,780,84,48,'#203b4f',AMBER,12);s.text(1560,812,'BZ',20,AMBER,700,anchor='middle');s.text(1615,799,'+',18,RED);s.text(1615,834,'−',18,GRAY)
s.wire([(1560,780),(1560,763)],RED);s.text(1560,760,'3V3',16,RED,700,anchor='middle')
s.wire([(1340,900),(1340,963)],GRAY);s.dot(1340,900,AMBER);s.rect(1330,963,20,65,'none',GRAY,2);s.text(1305,1003,'10 kΩ',15,GRAY,anchor='end');s.wire([(1340,1028),(1340,1060),(1480,1060)],GRAY);s.dot(1480,1060,GRAY);s.text(1494,1066,'GND',16,GRAY)
s.text(1160,1100,'Q1 + 1 kΩ + 10 kΩ adalah komponen tambahan.',15,MUTED)
s.text(1160,1127,'Pin fisik B/C/E mengikuti datasheet transistor yang dipakai.',14,MUTED)
s.rect(45,1185,1710,102);s.text(70,1215,'LEGENDA',13,BLUE,700)
for x,label,c in [(70,'3V3',RED),(215,'GND',GRAY),(360,'SDA / RX',BLUE),(560,'SCL / TX / B',GREEN),(810,'GPIO / DIR',PURPLE),(1040,'RS485 A / buzzer',AMBER)]:s.wire([(x,1245),(x+35,1245)],c,4);s.text(x+45,1251,label,16,c)
s.text(70,1276,'Label GND/3V3 yang sama = net yang sama. Titik = sambungan; perpotongan tanpa titik tidak tersambung. Posisi port bukan posisi header fisik.',15,MUTED)
s.save('wiring-overview.svg')

# Separate tactile contact illustration.
s=SVG(1100,480,'Cara memilih kaki tactile switch dan menghubungkannya ke ESP32')
s.heading('Tombol 4 kaki · pilih dua kontak yang berbeda','Ilustrasi pasangan listrik; tentukan pasangan aktual dengan continuity meter.')
s.rect(355,155,330,230);s.text(520,194,'TACTILE SWITCH',20,weight=700,anchor='middle')
for x,label,c in [(355,'A',PURPLE),(685,'B',GRAY)]:
    s.wire([(x,240),(x,335)],c,5)
    for y in [240,335]:s.dot(x,y,c,7);s.wire([(x-40 if x==355 else x+40,y),(x,y)],c)
    s.text(x-65 if x==355 else x+65,291,label,24,c,700,anchor='middle')
s.dot(460,285,PURPLE);s.dot(580,285,GRAY);s.wire([(355,285),(460,285),(555,250)],PURPLE);s.wire([(580,285),(685,285)],GRAY)
s.wire([(90,240),(315,240)],PURPLE);s.text(80,219,'GPIO4 / GPIO5 / GPIO6',19,PURPLE);s.wire([(725,335),(990,335)],GRAY);s.text(855,371,'GND',22,GRAY,700)
s.text(55,434,'A–A selalu terhubung. B–B selalu terhubung. A–B baru terhubung saat ditekan. Gunakan satu A dan satu B.',18,MUTED)
s.save('button-contacts.svg')

s=SVG(1400,650,'Arsitektur dua core dan jalur data')
s.heading('Dua core · satu sumber data konsisten','RTOS queue untuk perintah/snapshot; mutex untuk I2C dan filesystem.')
for x,title,lines in [(55,'CORE 0 / ANTARMUKA',['HTTP + dashboard HP','Tombol / debounce 35 ms','OLED sekitar 3 Hz + buzzer','Wi-Fi tetap dikelola ESP-IDF']),(835,'CORE 1 / AKUISISI',['UART1 Modbus + CRC / retry','RTC + CT/PT + counter kWh','9 interval dengan kualitas data','NVS checkpoint + CSV LittleFS'])]:
    s.rect(x,160,510,300);s.text(x+25,204,title,22,BLUE,700)
    for i,line in enumerate(lines):s.text(x+25,260+i*43,line,20)
s.wire([(565,250),(835,250)],PURPLE,4);s.text(700,228,'Perintah →',18,PURPLE,anchor='middle');s.text(700,277,'queue 6 item',15,MUTED,anchor='middle')
s.wire([(835,380),(565,380)],GREEN,4);s.text(700,355,'← Snapshot',18,GREEN,anchor='middle');s.text(700,407,'queue 1 item',15,MUTED,anchor='middle')
s.rect(55,500,1290,100);s.text(85,539,'Polling target 2 detik  ·  Refresh web 2,5 detik  ·  Watchdog 8 detik',21,weight=600)
s.text(85,575,'Selisih counter membentuk energi; beban CPU dibiarkan rendah agar respons, Wi-Fi dan idle task tetap lancar.',18,MUTED)
s.save('architecture.svg')

s=SVG(1400,650,'Ilustrasi halaman OLED dan navigasi')
s.heading('OLED 128 × 64 · lima halaman + boot','Representasi isi layar dari firmware, bukan foto hardware. Semua angka adalah data simulasi.')
screens=[('BOOT',['MIDEA ENGINEERING','FORT','[============      ]']),('UTAMA',['MIDEA | ENERGY','128432.73','kWh import','08:42:16 WIB       OK','IP 192.168.4.1']),('PAGI',['INTERVAL PAGI','06:00 - 06:30','21.320','lengkap-estimasi']),('SORE',['INTERVAL SORE','16:30 - 17:00','--.---','menunggu']),('HOTSPOT',['HOTSPOT / WIFI','Midea Engineering','AP','192.168.4.1','OK: password']),('DIAGNOSTIK',['DIAGNOSTICS','CRC:0 T/O:2','RTC:OK FS:OK','ID:1 9600N','ALARM ENABLED'])]
for i,(title,lines) in enumerate(screens):
    x=45+i%3*450;y=145+i//3*230;s.rect(x,y,410,205);s.text(x+20,y+30,title,14,BLUE,700);s.rect(x+20,y+45,370,140,'#030b13',LINE,7)
    for j,line in enumerate(lines):s.text(x+35,y+70+j*23,line,16,mono=True)
s.text(45,634,'PREV / NEXT: halaman  ·  OK: interval atau password  ·  Tahan OK 900 ms: mute / unmute alarm',18,MUTED)
s.save('oled-pages.svg')

pins={k:int(v) for k,v in re.findall(r'(PIN_[A-Z0-9_]+)\s*=\s*(\d+)',(ROOT/'firmware/include/board.h').read_text())}
rows=[('P01','3V3','ESP32.3V3','rail.3V3'),('P02','GND','ESP32.GND','rail.GND')]
for part in ['OLED','DS3231','RS485']:
    rows.extend([(part+'-P','3V3','rail.3V3',part+'.VCC'),(part+'-G','GND','rail.GND',part+'.GND')])
for part in ['OLED','DS3231']:
    for sig in ['SDA','SCL']:rows.append((part+'-'+sig,sig,'ESP32.GPIO'+str(pins['PIN_'+sig]),part+'.'+sig))
rows.extend([
    ('R01','UART_RX','RS485.RO','ESP32.GPIO'+str(pins['PIN_RX'])),
    ('R02','UART_TX','ESP32.GPIO'+str(pins['PIN_TX']),'RS485.DI'),
    ('R03','RS485_DIR','ESP32.GPIO'+str(pins['PIN_RS485_DE']),'RS485.DE'),
    ('R04','RS485_DIR','ESP32.GPIO'+str(pins['PIN_RS485_DE']),'RS485./RE'),
    ('R05','RS485_A','RS485.A','FORT.58/A'),('R06','RS485_B','RS485.B','FORT.59/B')])
for key in ['PREV','NEXT','OK']:
    rows.extend([(key+'-IN','BUTTON_'+key,'ESP32.GPIO'+str(pins['PIN_'+key]),key+'.contact_A'),(key+'-GND','GND',key+'.contact_B','rail.GND')])
rows.extend([('BZ01','BUZZER_CONTROL','ESP32.GPIO'+str(pins['PIN_BUZZER']),'R1.1k.input'),('BZ02','BUZZER_BASE','R1.1k.output','Q1.B'),('BZ03','BUZZER_BASE','Q1.B','R2.10k.input'),('BZ04','GND','R2.10k.output','rail.GND'),('BZ05','GND','Q1.E','rail.GND'),('BZ06','BUZZER_LOW','Q1.C','BUZZER.minus'),('BZ07','3V3','rail.3V3','BUZZER.plus')])
stream=io.StringIO(newline='');writer=csv.writer(stream,lineterminator='\n');writer.writerow(['wire','net','from','to']);writer.writerows(rows)
write_generated(ROOT/'docs/wiring-netlist.csv',stream.getvalue())
print(f'Generated {len(list(ASSETS.rglob("*.svg")))} SVG assets for firmware {version}')
