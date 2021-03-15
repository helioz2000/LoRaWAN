/*
 * Program / retrieve device EUI from Flash storage
 * Board: Arduino Zero (Native USB Port)
 * Hardware: Rocketscream Mini Ultra Pro V1
 * 
 * V1 of the Rocketscream does not come with a pre-programmed EUI
 * This code will set a custom EUI in flash storage
 * 
 */

#define EUI64_MAC_LENGTH 8

// ==========================================================================================================
// put your new EUI in the line below
static const uint8_t PROGMEM DEVEUI[EUI64_MAC_LENGTH] = { 0x00, 0x0D, 0x87, 0x31, 0xF4, 0xF2, 0xAA, 0x11 }; // <<---------
// ==========================================================================================================

#include <FlashStorage.h>

#define Serial SerialUSB    // Rocketscream serial console is direct USB

typedef struct {
  uint8_t octet[EUI64_MAC_LENGTH];  // device EUI storage
} EUIstruct;

// Reserve a portion of FLash Memory
FlashStorage(devEUIstore, EUIstruct);

// Create a variable to hold the EUI
EUIstruct devEUI;

char consoleInputChar;

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

void printEUI() {
  int i;
  for (i=0; i<EUI64_MAC_LENGTH-1; i++) {
    printHex2(devEUI.octet[i]);
    Serial.print(F("-"));
  }
  printHex2(devEUI.octet[EUI64_MAC_LENGTH-1]);
}

void loadNewEUI() {
  int i;
  for (i=0; i<EUI64_MAC_LENGTH; i++) {
    devEUI.octet[i] = DEVEUI[i];
  }
}

void printAll() {
  devEUI = devEUIstore.read();
  Serial.print(F("Current (stored) EUI: "));
  printEUI();
  Serial.println();
  loadNewEUI();
  Serial.print(F("Proposed (new) EUI:   "));
  printEUI();
  Serial.println();
}

void printUsage() {
  Serial.println(F("press r to read EUI"));
  Serial.println(F("press w to write proposed EUI. No need to waste Flash write cycles"));
}

void setup() {
  while (!Serial && millis() < 10000);
  Serial.begin(115200);       // fixed baudrate for USB, console can be set to anything
  Serial.println(F("Starting"));
  
  pinMode(LED_BUILTIN, OUTPUT);
  printAll();
  printUsage();
}

bool writeToFlash() {
  int i;
  bool writeRequired = false;
  
  // check if the stored EUI is the same as the proposed
  devEUI = devEUIstore.read();
  for (i=0; i<EUI64_MAC_LENGTH; i++) {
    if (devEUI.octet[i] != DEVEUI[i]) {
      writeRequired = true; }
  }
  if (writeRequired) {
    loadNewEUI();
    Serial.print(F("writing "));
    printEUI();
    Serial.println(F(" to Flash Storage"));
    devEUIstore.write(devEUI);
    Serial.println(F("write completed"));
    printAll();
  } else {
    Serial.println(F("write bypassed - stored EUI is the same as proposed EUI"));
  }
  return true;
}

bool recvOneChar() {
  if (Serial.available() > 0) {
    consoleInputChar = Serial.read();
    return true;
  }
  return false;
}

void handleCommand(char cmd) {
  switch (cmd) {
    case 'r':
    case 'R':
      //Serial.println(F("read command"));
      printAll();
      break;
    case 'w':
    case 'W':
      //Serial.println(F("write command"));
      writeToFlash();
      break;
    default:
      Serial.println(F("invalid command"));
      break;
  }
}

void loop() {
  if (recvOneChar()) {
    if (consoleInputChar > ' ')
      handleCommand(consoleInputChar);
  }
}
