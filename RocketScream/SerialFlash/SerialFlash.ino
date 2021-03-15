/*
 * Program / retrieve device EUI from Serail Flash chip
 * Board: Arduino Zero (Native USB Port)
 * Hardware: Rocketscream Mini Ultra Pro V1
 * 
 * V1 of the Rocketscream does not come with a pre-programmed EUI
 * This code will set a custom EUI in teh serial flash storage chip
 * 
 * SerialFlash library: https://github.com/PaulStoffregen/SerialFlash
 */

#define EUI64_MAC_LENGTH 8

// ==========================================================================================================
// put your new EUI in the line below
static const uint8_t PROGMEM DEVEUI[EUI64_MAC_LENGTH] = { 0x00, 0x0D, 0x87, 0x31, 0xF4, 0xF2, 0xAA, 0x11 }; // <<---------
// ==========================================================================================================

#include <SerialFlash.h>
#include <SPI.h>

#define Serial SerialUSB    // Rocketscream serial console is direct USB

const int FlashCSpin = 4; // digital pin for flash chip CS pin

typedef struct {
  uint8_t octet[EUI64_MAC_LENGTH];  // device EUI storage
} EUIstruct;

// Create a variable to hold the EUI
EUIstruct devEUI;

char consoleInputChar;
bool EUIfileFound = false;
const char EUIfileName[] = { "dev_eui.bin" };

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

bool formatSerialFlash() {
  uint8_t id[5];
  Serial.print(F("Formatting Serial Flash "));
  
  SerialFlash.readID(id);
  SerialFlash.eraseAll();
  
  //Flash LED at 1Hz while formatting
  while (!SerialFlash.ready()) {
    Serial.print(F("."));
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
  }

  Serial.println();
  Serial.println(F("Formatting Complete"));

  //Quickly flash LED a few times when completed, then leave the light on solid
  for(uint8_t i = 0; i < 10; i++){
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

bool createEUIfile() {
  if (SerialFlash.createErasable("dev_eui.bin", 256)) {
    Serial.println(F("EUI file succesfully created"));
    return true;
  } else {
    Serial.println(F("EUI file creation failed"));
    return false;
  }
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
  //devEUI = devEUIstore.read();
  Serial.print(F("Current (stored) EUI: "));
  printEUI();
  Serial.println();
  loadNewEUI();
  Serial.print(F("Proposed (new) EUI:   "));
  printEUI();
  Serial.println();
}

void printUsage() {
  Serial.println(F("press c to create empty EUI file on Serial Flash"));
  Serial.println(F("press f to format Serial Flash"));
  Serial.println(F("press l to list files on Serial Flash"));
  Serial.println(F("press r to read EUI"));
  Serial.println(F("press w to write proposed EUI."));
}

void spaces(int num) {
  for (int i=0; i < num; i++) {
    Serial.print(' ');
  }
}

void error(const char *message) {
  while (1) {
    Serial.println(message);
    delay(2500);
  }
}

// list all files on serial flash
void listSerialFlashFiles() {
  int numFiles = 0;
  SerialFlash.opendir();
  while (1) {
    char filename[64];
    uint32_t filesize;

    if (SerialFlash.readdir(filename, sizeof(filename), filesize)) {
      Serial.print(F("  "));
      Serial.print(filename);
      spaces(20 - strlen(filename));
      Serial.print(F("  "));
      Serial.print(filesize);
      Serial.print(F(" bytes"));
      Serial.println();
      numFiles++;
    } else {
      break; // no more files
    }
  }
  if (numFiles < 1)
    Serial.println(F("No files found"));
}

void setup() {
  while (!Serial && millis() < 10000);
  Serial.begin(115200);       // fixed baudrate for USB, console can be set to anything
  Serial.println(F("Serial Flash - Starting"));
  
  pinMode(LED_BUILTIN, OUTPUT);

  if (!SerialFlash.begin(FlashCSpin)) {
    error("Unable to access SPI Flash chip");
  } else {
    listSerialFlashFiles();
  }

  //printAll();
  if (!EUIfileFound)
    Serial.println(F("No EUI available"));
  printUsage();
}

bool writeSerialFlash() {
  int i;
  bool writeRequired = false;
  
  // check if the stored EUI is the same as the proposed
  //devEUI = devEUIstore.read();
  for (i=0; i<EUI64_MAC_LENGTH; i++) {
    if (devEUI.octet[i] != DEVEUI[i]) {
      writeRequired = true; }
  }
  if (writeRequired) {
    loadNewEUI();
    Serial.print(F("writing "));
    printEUI();
    Serial.println(F(" to Flash Storage"));
    //devEUIstore.write(devEUI);
    Serial.println(F("write completed"));
    printAll();
  } else {
    Serial.println(F("write bypassed - stored EUI is the same as proposed EUI. No need to waste Serial Flash write cycles."));
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
    case 'c':
    case 'C':
      createEUIfile();
      break;
    case 'f':
    case 'F':
      formatSerialFlash();
      break;
    case 'l':
    case 'L':
      listSerialFlashFiles();
      break;
    case 'r':
    case 'R':
      //Serial.println(F("read command"));
      printAll();
      break;
    case 'w':
    case 'W':
      //Serial.println(F("write command"));
      writeSerialFlash();
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
