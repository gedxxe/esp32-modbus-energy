"""Generate GitHub-safe local SVGs from the firmware configuration (stdlib only)."""
from pathlib import Path
from html import escape
import re
import csv
import io
import sys

ROOT=Path(__file__).resolve().parent.parent
ASSETS=ROOT/'docs/assets'
BG='#f5f8f8'; PANEL='#ffffff'; LINE='#cbdada'; TEXT='#183438'; MUTED='#587176'
RED='#b5534a'; GRAY='#5d6f76'; BLUE='#087e82'; GREEN='#28755e'; PURPLE='#6d5897'; AMBER='#906121'
version=re.search(r'FIRMWARE_VERSION\[\] = "([^"]+)"',(ROOT/'firmware/include/version.h').read_text(encoding='utf-8')).group(1)
ini=(ROOT/'firmware/platformio.ini').read_text(encoding='utf-8')
libs=dict(re.findall(r'^\s+([^\n]+)@([\d.]+)\s*$',ini,re.M))
def write_generated(path,contents):
    if '--check' in sys.argv:
        if not path.exists() or path.read_text(encoding='utf-8')!=contents:
            raise SystemExit(f'Stale generated asset: {path.relative_to(ROOT)}; run tools/generate_docs.py')
    else:
        path.parent.mkdir(parents=True,exist_ok=True);path.write_text(contents,encoding='utf-8')

class SVG:
    def __init__(self,w,h,title,desc='Midea Fort schematic. Labels and colors identify connections. Screen values are simulated.'):
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

# Full schematic. Net labels are electrical connections, not physical header positions.
s=SVG(1800,1320,'ESP32-S3, OLED, RTC, RS485, Fort, buttons and optional buzzer wiring')
s.heading('Full wiring · manual-direction alternative','RO/DI/DE-/RE module only · Supplied automatic module uses rs485-auto.svg · Shared I2C/buttons/buzzer unchanged')
s.rect(650,180,350,780,'#e5f1ef',BLUE);s.text(825,225,'ESP32-S3',30,weight=700,anchor='middle');s.text(825,258,'N16R8 · DevKitC style',18,MUTED,anchor='middle')
s.rect(715,120,220,45);s.text(825,149,'USB-C / board supply',16,anchor='middle');s.wire([(825,165),(825,180)],RED)
s.text(825,590,'UART1 + I2C',20,BLUE,600,anchor='middle');s.text(825,625,'Core 0: web / UI',17,MUTED,anchor='middle');s.text(825,653,'Core 1: acquisition',17,MUTED,anchor='middle');s.text(825,705,'3V3 from board regulator',16,RED,anchor='middle')
for y,label,c in [(300,'3V3',RED),(340,'GND',GRAY),(380,'GPIO8 / SDA',BLUE),(420,'GPIO9 / SCL',GREEN),(800,'GPIO4 / PREV',PURPLE),(840,'GPIO5 / NEXT',PURPLE),(880,'GPIO6 / OK',PURPLE)]:s.port(650,y,label,c,'left')
for y,label,c in [(300,'3V3',RED),(340,'GND',GRAY),(390,'GPIO18 / RX',BLUE),(430,'GPIO17 / TX',GREEN),(490,'GPIO16 / DIR',PURPLE),(800,'GPIO7 / BUZZ',AMBER)]:s.port(1000,y,label,c)
for top,title,sub,ys in [(240,'OLED SSD1306','I2C · 128 × 64 · 0x3C',[300,340,380,420]),(530,'RTC DS3231','I2C · 0x68 · battery backup',[590,630,670,710])]:
    s.rect(60,top,330,235);s.text(85,top+37,title,24,weight=700);s.text(85,top+65,sub,15,MUTED)
    for y,label,c in zip(ys,['VCC','GND','SDA','SCL'],[RED,GRAY,BLUE,GREEN]):s.port(390,y,label,c)
# The OLED's first ports share the title region: cover text was positioned above.
for y,c in [(300,RED),(340,GRAY),(380,BLUE),(420,GREEN)]:s.wire([(390,y),(650,y)],c)
for y,target,x,c in [(590,300,450,RED),(630,340,480,GRAY),(670,380,520,BLUE),(710,420,560,GREEN)]:
    s.wire([(390,y),(x,y),(x,target),(650,target)],c);s.dot(x,target,c)
s.text(60,805,'OLED and RTC share the SDA and SCL nets.',16,MUTED)
s.rect(1190,240,260,330);s.text(1210,278,'RS485 · 3V3',23,weight=700)
for y,label,c in [(300,'VCC',RED),(340,'GND',GRAY),(390,'RO → RX',BLUE),(430,'DI ← TX',GREEN),(480,'DE',PURPLE),(520,'/RE',PURPLE)]:s.port(1190,y,label,c,'left')
for y,label,c in [(340,'A',AMBER),(390,'B',GREEN)]:s.port(1450,y,label,c)
for y,c in [(300,RED),(340,GRAY),(390,BLUE),(430,GREEN)]:s.wire([(1000,y),(1190,y)],c)
s.wire([(1000,490),(1100,490),(1100,480),(1190,480)],PURPLE);s.wire([(1100,490),(1100,520),(1190,520)],PURPLE);s.dot(1100,490,PURPLE)
s.text(1025,555,'Tie DE and /RE together',15,PURPLE)
s.rect(1540,240,210,330);s.text(1560,278,'FORT',26,weight=700);s.text(1560,310,'Power meter',16,MUTED)
for y,label,c in [(340,'58 / A',AMBER),(390,'59 / B',GREEN)]:s.port(1540,y,label,c,'left');s.wire([(1450,y),(1540,y)],c)
s.text(1560,455,'Dedicated meter port',16,MUTED);s.text(1560,482,'ESP32 is the',16,MUTED);s.text(1560,509,'only master',16,MUTED)
s.text(1190,612,'Manual: terminal 58 = A, terminal 59 = B.',18,AMBER)
s.text(1190,640,'70 / COM is a switch input common, not RS485 ground.',16,MUTED)
# Three physical switches, all common to local ground.
s.rect(60,850,330,300);s.text(85,882,'3 × TACTILE SWITCH',20,weight=700)
for y,label in [(920,'PREV'),(985,'NEXT'),(1050,'OK')]:
    s.text(85,y-12,label,16,PURPLE,700);s.wire([(180,y),(240,y)],GRAY);s.dot(240,y,GRAY);s.dot(275,y,PURPLE);s.wire([(240,y),(267,y-15)],GRAY,3);s.wire([(275,y),(390,y)],PURPLE)
s.wire([(180,920),(180,1110)],GRAY);s.text(196,1117,'GND (shared net)',15,GRAY)
for by,sy,x in [(800,920,590),(840,985,610),(880,1050,630)]:s.wire([(650,by),(x,by),(x,sy),(390,sy)],PURPLE)
s.text(415,1110,'INPUT_PULLUP',16,PURPLE);s.text(415,1138,'Pressed = LOW',16,MUTED)
# NPN low-side driver for an active 2-wire buzzer.
s.rect(1130,700,620,450);s.text(1160,741,'OPTIONAL BUZZER · NPN driver',23,weight=700)
s.wire([(1000,800),(1080,800),(1080,900),(1190,900)],AMBER)
s.rect(1190,890,85,20,'none',AMBER,2);s.text(1232,876,'1 kΩ',16,AMBER,anchor='middle');s.wire([(1275,900),(1440,900)],AMBER)
s.parts.append(f'<circle cx="1460" cy="900" r="36" fill="none" stroke="{TEXT}" stroke-width="2"/>')
s.wire([(1440,879),(1440,921)],TEXT,3);s.wire([(1440,889),(1480,868),(1560,868),(1560,828)],TEXT);s.wire([(1440,911),(1480,935),(1480,1060)],GRAY)
s.wire([(1471,928),(1479,935),(1469,934)],GRAY,2);s.text(1525,966,'Q1 NPN',16,MUTED);s.text(1525,990,'B / C / E',14,MUTED)
s.text(1419,883,'B',14,AMBER);s.text(1491,855,'C',14,MUTED);s.text(1494,942,'E',14,GRAY)
s.rect(1518,780,84,48,'#fff7e7',AMBER,12);s.text(1560,812,'BZ',20,AMBER,700,anchor='middle');s.text(1615,799,'+',18,RED);s.text(1615,834,'−',18,GRAY)
s.wire([(1560,780),(1560,763)],RED);s.text(1560,760,'3V3',16,RED,700,anchor='middle')
s.wire([(1340,900),(1340,963)],GRAY);s.dot(1340,900,AMBER);s.rect(1330,963,20,65,'none',GRAY,2);s.text(1305,1003,'10 kΩ',15,GRAY,anchor='end');s.wire([(1340,1028),(1340,1060),(1480,1060)],GRAY);s.dot(1480,1060,GRAY);s.text(1494,1066,'GND',16,GRAY)
s.text(1160,1100,'Q1 + 1 kΩ + 10 kΩ are additional parts.',15,MUTED)
s.text(1160,1127,'Find physical B/C/E pin order in the transistor datasheet.',14,MUTED)
s.rect(45,1185,1710,102);s.text(70,1215,'NET LABELS',13,BLUE,700)
for x,label,c in [(70,'3V3',RED),(215,'GND',GRAY),(360,'SDA / RX',BLUE),(560,'SCL / TX / B',GREEN),(810,'GPIO / DIR',PURPLE),(1040,'RS485 A / buzzer',AMBER)]:s.wire([(x,1245),(x+35,1245)],c,4);s.text(x+45,1251,label,16,c)
s.text(70,1276,'Repeated GND/3V3 labels identify one net. Dots mark joins; crossings without dots do not connect. Port positions are not physical headers.',15,MUTED)
s.save('wiring-overview.svg')

# Confirmed four-pin installation, kept separate from manual DE-/RE alternative.
s=SVG(1400,620,'Confirmed automatic-direction RS485 installation','Logical nets, not physical header positions. User-confirmed module TXD to ESP32 GPIO18, RXD from GPIO17. GPIO16 unconnected.')
s.heading('Installed RS485 module · automatic direction','Confirmed crossed TXD/RXD wiring · 3.3 V TTL · No external DE-/RE connection')
s.rect(45,160,340,320);s.text(70,200,'ESP32-S3 N16R8',25,weight=700)
s.rect(610,160,330,320);s.text(635,200,'4-pin TTL module',25,weight=700)
s.rect(1110,160,245,320);s.text(1135,200,'FORT meter',25,weight=700)
for y,left,right,color in [(250,'3V3','VCC',RED),(300,'GND','GND',GRAY),(350,'GPIO18 / RX','TXD → ESP RX',BLUE),(400,'GPIO17 / TX','RXD ← ESP TX',GREEN)]:
    s.port(385,y,left,color);s.port(610,y,right,color,'left');s.wire([(385,y),(610,y)],color)
for y,lab,target,color in [(290,'A+','58 / A',AMBER),(365,'B−','59 / B',GREEN)]:
    s.port(940,y,lab,color);s.port(1110,y,target,color,'left');s.wire([(940,y),(1110,y)],color)
s.text(70,450,'GPIO16: unconnected',18,PURPLE,600)
s.text(45,530,'Arrows show signal direction. Positions are logical ports; follow the printed GPIO and module labels.',18,MUTED)
s.text(45,565,'Fort 70 / COM belongs to switch inputs. Do not connect it as RS485 ground.',18,MUTED)
s.save('rs485-auto.svg')

# Separate tactile contact illustration.
s=SVG(1100,480,'Tactile contact pairs and ESP32 connection')
s.heading('4-leg switch · use two different contact pairs','Electrical contact illustration; identify the actual pairs with a continuity meter.')
s.rect(355,155,330,230);s.text(520,194,'TACTILE SWITCH',20,weight=700,anchor='middle')
for x,label,c in [(355,'A',PURPLE),(685,'B',GRAY)]:
    s.wire([(x,240),(x,335)],c,5)
    for y in [240,335]:s.dot(x,y,c,7);s.wire([(x-40 if x==355 else x+40,y),(x,y)],c)
    s.text(x-65 if x==355 else x+65,291,label,24,c,700,anchor='middle')
s.dot(460,285,PURPLE);s.dot(580,285,GRAY);s.wire([(355,285),(460,285),(555,250)],PURPLE);s.wire([(580,285),(685,285)],GRAY)
s.wire([(90,240),(315,240)],PURPLE);s.text(80,219,'GPIO4 / GPIO5 / GPIO6',19,PURPLE);s.wire([(725,335),(990,335)],GRAY);s.text(855,371,'GND',22,GRAY,700)
s.text(55,434,'A–A and B–B are always joined. Pressing joins A to B. Connect one A to GPIO and one B to ground.',18,MUTED)
s.save('button-contacts.svg')

s=SVG(1400,650,'Tasks, queues and peripheral ownership')
s.heading('Task ownership and data flow','Commands and snapshots are copied through queues; I2C and filesystem operations use mutexes.')
for x,title,lines in [(55,'CORE 0 / UI · PRIORITY 1',['HTTP + browser API','Buttons / 35 ms debounce','OLED ≥300 ms / RGB RMT ≥20 ms','Wi-Fi tasks managed by ESP-IDF']),(835,'CORE 1 / ACQUISITION · PRIORITY 2',['UART1 / CRC / retry / scan','RTC + CT/PT + kWh counter','9 scheduled intervals + quality','NVS checkpoint + CSV / LittleFS'])]:
    s.rect(x,160,510,300);s.text(x+25,204,title,22,BLUE,700)
    for i,line in enumerate(lines):s.text(x+25,260+i*43,line,20)
s.wire([(565,250),(835,250)],PURPLE,4);s.text(700,228,'Command →',18,PURPLE,anchor='middle');s.text(700,277,'queue: 6 values',15,MUTED,anchor='middle')
s.wire([(835,380),(565,380)],GREEN,4);s.text(700,355,'← Snapshot',18,GREEN,anchor='middle');s.text(700,407,'queue: 1 value',15,MUTED,anchor='middle')
s.rect(55,500,1290,100);s.text(85,539,'2 s acquisition target  ·  2.5 s browser refresh  ·  8 s watchdog',21,weight=600)
s.text(85,575,'Yield while waiting: idle and system tasks must run. Target periods are not hard real-time deadlines.',18,MUTED)
s.save('architecture.svg')

s=SVG(1400,650,'OLED screen and button reference')
s.heading('OLED 128 × 64 · five pages + boot','Representative firmware screens, not hardware photographs. All sample values are simulated.')
screens=[('BOOT',['MIDEA ENGINEERING','FORT','[============      ]']),('MAIN',['MIDEA | ENERGY','128432.73','kWh import','08:42:16 WIB       OK','IP 192.168.4.1']),('MORNING',['MORNING INTERVAL','06:00 - 06:30','21.320','complete_estimated']),('EVENING',['EVENING INTERVAL','16:30 - 17:00','--.---','waiting']),('HOTSPOT',['HOTSPOT / WIFI','Midea Engineering','AP','192.168.4.1','OK: password']),('DIAGNOSTICS',['DIAGNOSTICS','CRC:0 T/O:2','RTC:OK FS:OK','ID:1 9600N','ALARM ENABLED'])]
for i,(title,lines) in enumerate(screens):
    x=45+i%3*450;y=145+i//3*230;s.rect(x,y,410,205);s.text(x+20,y+30,title,14,BLUE,700);s.rect(x+20,y+45,370,140,'#030b13',LINE,7)
    for j,line in enumerate(lines):s.text(x+35,y+70+j*23,line,16,color='#e2f4f0',mono=True)
s.text(45,634,'PREV / NEXT: page  ·  OK: interval or password  ·  Hold OK 900 ms: toggle alarm mute',18,MUTED)
s.save('oled-pages.svg')

pins={k:int(v) for k,v in re.findall(r'(PIN_[A-Z0-9_]+)\s*=\s*(\d+)',(ROOT/'firmware/include/board.h').read_text(encoding='utf-8'))}
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



s=SVG(1400,740,'UART1 RTU frame and transceiver direction','Timing is schematic, not to scale. Hardware UART generates serial bits; GPIO16 controls direction.')
s.heading('RTU transaction · UART1 + GPIO16','Function 03 only · up to 3 attempts normally · 1 attempt per register while scanning')
labels=[('ID','01'),('FUNCTION','03'),('ADDR HI','00'),('ADDR LO','82'),('COUNT HI','00'),('COUNT LO','02'),('CRC LO','CRC[7:0]'),('CRC HI','CRC[15:8]')]
for i,(a,b) in enumerate(labels):
 x=45+i*164;s.rect(x,135,153,86);s.text(x+14,163,a,12,MUTED,600);s.text(x+14,199,b,22,BLUE,600,mono=True)
s.text(45,254,'Energy read request: 0x0082, two words. CRC covers the first six bytes; low CRC byte is sent first.',18,MUTED)
s.text(45,327,'GPIO16',18,PURPLE,600);s.text(45,353,'DE + /RE',15,MUTED)
s.wire([(190,355),(440,355),(440,305),(690,305),(690,355),(1345,355)],PURPLE,3)
s.text(275,389,'RX / idle',15,MUTED);s.text(508,289,'TX enabled',15,PURPLE);s.text(900,389,'RX enabled',15,MUTED)
s.text(45,447,'UART1',18,BLUE,600)
s.rect(440,416,250,54,'#e5f1ef',BLUE,5);s.text(565,449,'8 request bytes',18,BLUE,600,anchor='middle')
s.rect(825,416,445,54,'#eef6f2',GREEN,5);s.text(1047,449,'Response: ID / 03 / count / data / CRC',16,GREEN,anchor='middle')
for x,top,bottom in [(440,'Wait t3.5','then HIGH + 30 µs'),(690,'UART.flush()','then GPIO16 LOW')]:
 s.wire([(x,490),(x,515)],GRAY,1);s.text(x,542,top,16,anchor='middle');s.text(x,568,bottom,14,MUTED,anchor='middle')
s.rect(45,614,1300,88);s.text(70,645,'RX deadline: 800 ms after TX flush. Software polls the RX buffer and yields one tick while waiting.',18)
s.text(70,678,'UART and I2C use hardware peripherals. GPIO direction switching and software CRC are not serial bit banging.',17,MUTED)
s.save('rtu-timing.svg')

s=SVG(1400,650,'Firmware startup sequence','After the ESP32-S3 ROM and bootloader transfer control to Arduino setup().')
s.heading('Startup sequence','After ROM / bootloader → setup() → core-0 UI loop + core-1 acquisition task')
steps=[('01','GPIO + RTOS objects',['DE LOW; button pull-ups','I2C / FS mutexes','Command + snapshot queues']),('02','I2C + boot animation',['GPIO8 SDA / GPIO9 SCL','OLED: 180° before first frame','Animation + buzzer ~1.5 s']),('03','LittleFS mount',['begin(false): no auto-format','Hold OK 5 s if mount failed','Manual format erases CSV']),('04','AP + HTTP + watchdog',['192.168.4.1 / 24','Per-boot POST token','Start 8 s watchdog']),('05','Core 1 acquisition',['Load checked NVS records','Start UART1','RTC / ratio / energy / scan']),('06','Core 0 UI loop',['HTTP + queued commands','Buttons / OLED / buzzer / RGB','Publish/read copied state'])]
for i,(num,title,lines) in enumerate(steps):
 x=45+(i%3)*450;y=145+(i//3)*235;s.rect(x,y,410,205);s.text(x+22,y+34,num,15,BLUE,700);s.text(x+64,y+34,title,19,weight=600)
 for j,line in enumerate(lines):s.text(x+22,y+83+j*33,line,17,MUTED)
s.text(45,633,'No wait for a USB Serial host. Missing meter/RTC/OLED/storage is reported; startup does not wait indefinitely.',18,MUTED)
s.save('boot-flow.svg')

print(f'Generated {len(list(ASSETS.rglob("*.svg")))} SVG assets for firmware {version}')
