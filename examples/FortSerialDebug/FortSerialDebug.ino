// SPDX-License-Identifier: GPL-3.0-only
// Install the supplied FortMeter 3.1.0 library ZIP in Arduino IDE before compiling.
#include <Arduino.h>
#include <FortDiagnostics.h>
HardwareSerial fortUART(1);
char fortLog[8192]={};
fort::FortDiagnostics debug(fortUART,fortLog,sizeof(fortLog));
void setup(){
  Serial.begin(115200); // Start without waiting for USB enumeration.
  debug.serialInputEnabled=true;debug.serialOutput=true;
  debug.options.manualDirection=true;debug.options.rx=18;debug.options.tx=17;
  debug.options.settleUs=100;debug.options.gapMs=200;debug.options.responseMs=2000;
  debug.onResume=[](){
    debug.emergencyStop();debug.options=fort::FortDiagnostics::DebugOptions{};debug.current=fort::FortDiagnostics::Link{};
    debug.activeBaud=0;debug.openLink(debug.current);debug.logf("[RESUMED] Standalone startup settings restored. Use status to inspect them.\n");
  };
  debug.openLink(debug.current);debug.help();debug.showStatus();
}
void loop(){debug.tick();debug.drainSerial();delay(5);}
