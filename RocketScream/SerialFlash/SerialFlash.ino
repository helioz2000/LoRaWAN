/*
 * Program / retrieve device EUI from Serail Flash chip
 * Board: Arduino Zero (Native USB Port)
 * Hardware: Rocketscream Mini Ultra Pro V1
 * 
 * V1 of the Rocketscream does not come with a pre-programmed EUI
 * This code will set a custom EUI in teh serial flash storage chip
 * 
 * SerialFlash library: https://github.com/PaulStoffregen/SerialFlash
 * 
 * Enter new device EUI below and press w to write it to SerialFlash
 */

#define EUI64_MAC_LENGTH 8

// ==========================================================================================================
// reaplace with new device EUI
static const uint8_t PROGMEM DEVEUI[EUI64_MAC_LENGTH] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }; // <<---------
// ==========================================================================================================

#include <SerialFlash.h>
#include <SPI.h>

#define Serial SerialUSB    // Rocketscream serial console is direct USB

#define RADIO_CS_PIN 5      // RFM95W chip select pin
#define FLASH_CS_PIN 4      // digital pin for serial flash chip select pin

typedef struct {
  uint8_t octet[EUI64_MAC_LENGTH];  // device EUI storage
} EUIstruct;

// Create a variable to hold the EUI
EUIstruct devEUI;

char consoleInputChar;
bool EUIfileFound = false;
const char devEUIfileName[] = { "dev_eui.bin" };

void setup() {
  while (!Serial && millis() < 10000);
  Serial.begin(115200);       // fixed baudrate for USB, console can be set to anything
  Serial.println();
  Serial.println();
  Serial.println(F("Serial Flash - Starting"));
  
  pinMode(LED_BUILTIN, OUTPUT); 
  pinMode(RADIO_CS_PIN, OUTPUT);
  pinMode(FLASH_CS_PIN, OUTPUT);
  // disable the radio chip
  digitalWrite(RADIO_CS_PIN, HIGH);
  // enable serial flash chip
  digitalWrite(FLASH_CS_PIN, LOW);
  
  if (!SerialFlash.begin(FLASH_CS_PIN)) {
    error("Unable to access SPI Flash chip");  
  }
  SerialFlash.wakeup();   // coudl be asleep after main processor reset
  if (SerialFlash.exists(devEUIfileName)) {
    EUIfileFound = true;
  }

  if (!EUIfileFound)
    Serial.println(F("No EUI available"));
  else
    Serial.println(F("EUI file found"));
  if (readEUIfile()) {
    printAll();    
  } else {
    Serial.print(F("Proposed EUI: "));
    printEUI(true);
    Serial.println();
  }
  printUsage();
}

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

bool readEUIfile() {
  char buffer[EUI64_MAC_LENGTH];
  int i;
  SerialFlashFile file;
  file = SerialFlash.open(devEUIfileName);
  if (!file) {  // true if the file exists
    Serial.println(F("No EUI file found"));
    return false;
  }
  file.read(buffer, EUI64_MAC_LENGTH);
  file.close();
  for (i=0; i<EUI64_MAC_LENGTH; i++) {
    devEUI.octet[i] = buffer[i];
  }
  return true;
}

bool createEUIfile() {
  if (SerialFlash.createErasable(devEUIfileName, EUI64_MAC_LENGTH)) {
    Serial.println(F("EUI file succesfully created"));
    return true;
  } else {
    Serial.println(F("EUI file creation failed"));
    return false;
  }
}

bool deleteEUIfile() {
  if (SerialFlash.remove(devEUIfileName)) {
    Serial.println(F("EUI file succesfully deleted"));
    return true;
  } else {
    Serial.println(F("EUI file delete failed"));
    return false;
  }
}

void printEUI(bool proposed) {
  int i;
  for (i=0; i<EUI64_MAC_LENGTH-1; i++) {
    if (proposed) {
      printHex2(DEVEUI[i]);
    } else {
      printHex2(devEUI.octet[i]);
    }
    Serial.print(F("-"));
  }
  printHex2(devEUI.octet[EUI64_MAC_LENGTH-1]);
}

void printAll() {
  //devEUI = devEUIstore.read();
  Serial.print(F("Current (stored) EUI: "));
  printEUI(false);
  Serial.println();
  Serial.print(F("Proposed (new) EUI:   "));
  printEUI(true);
  Serial.println();
}

void printUsage() {
  Serial.println(F("press c to create empty EUI file on Serial Flash"));
  Serial.println(F("press d to delete EUI file from Serial Flash"));
  Serial.println(F("press f to format Serial Flash"));
  Serial.println(F("press i to print Serial Flash Information"));
  Serial.println(F("press l to list files on Serial Flash"));
  Serial.println(F("press r to read EUI"));
  Serial.println(F("press w to write proposed EUI to Serial Flash."));
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

const char * id2chip(const unsigned char *id)
{
  if (id[0] == 0xEF) {
    // Winbond
    if (id[1] == 0x40) {
      if (id[2] == 0x14) return "W25Q80BV";
      if (id[2] == 0x15) return "W25Q16DV";
      if (id[2] == 0x17) return "W25Q64FV";
      if (id[2] == 0x18) return "W25Q128FV";
      if (id[2] == 0x19) return "W25Q256FV";
    }
  }
  if (id[0] == 0x01) {
    // Spansion
    if (id[1] == 0x02) {
      if (id[2] == 0x16) return "S25FL064A";
      if (id[2] == 0x19) return "S25FL256S";
      if (id[2] == 0x20) return "S25FL512S";
    }
    if (id[1] == 0x20) {
      if (id[2] == 0x18) return "S25FL127S";
    }
  }
  if (id[0] == 0xC2) {
    // Macronix
    if (id[1] == 0x20) {
      if (id[2] == 0x18) return "MX25L12805D";
    }
  }
  if (id[0] == 0x20) {
    // Micron
    if (id[1] == 0xBA) {
      if (id[2] == 0x20) return "N25Q512A";
      if (id[2] == 0x21) return "N25Q00AA";
    }
    if (id[1] == 0xBB) {
      if (id[2] == 0x22) return "MT25QL02GC";
    }
  }
  if (id[0] == 0xBF) {
    // SST
    if (id[1] == 0x25) {
      if (id[2] == 0x02) return "SST25WF010";
      if (id[2] == 0x03) return "SST25WF020";
      if (id[2] == 0x04) return "SST25WF040";
      if (id[2] == 0x41) return "SST25VF016B";
      if (id[2] == 0x4A) return "SST25VF032";
    }
    if (id[1] == 0x25) {
      if (id[2] == 0x01) return "SST26VF016";
      if (id[2] == 0x02) return "SST26VF032";
      if (id[2] == 0x43) return "SST26VF064";
    }
  }
    if (id[0] == 0x1F) {
        // Adesto
      if (id[1] == 0x89) {
            if (id[2] == 0x01) return "AT25SF128A";
        }  
    }   
  return "(unknown chip)";
}

bool serialFlashInfo () {
  unsigned char buf[256];
  unsigned long chipsize, blocksize;

    // Read the chip identification
  Serial.println();
  Serial.println(F("Read Chip Identification:"));
  SerialFlash.readID(buf);
  Serial.print(F("  JEDEC ID:     "));
  Serial.print(buf[0], HEX);
  Serial.print(' ');
  Serial.print(buf[1], HEX);
  Serial.print(' ');
  Serial.println(buf[2], HEX);
  Serial.print(F("  Part Number: "));
  Serial.println(id2chip(buf));
  Serial.print(F("  Memory Size:  "));
  chipsize = SerialFlash.capacity(buf);
  Serial.print(chipsize);
  Serial.println(F(" bytes"));
  if (chipsize == 0) return false;
  Serial.print(F("  Block Size:   "));
  blocksize = SerialFlash.blockSize();
  Serial.print(blocksize);
  Serial.println(F(" bytes"));
  Serial.println();
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


bool writeSerialFlash() {
  int i;
  bool writeRequired = false;
  char buffer[EUI64_MAC_LENGTH];
  SerialFlashFile file;
  
  // check if the stored EUI is the same as the proposed
  for (i=0; i<EUI64_MAC_LENGTH; i++) {
    if (devEUI.octet[i] != DEVEUI[i]) {
      writeRequired = true; }
  }
  if (writeRequired) {
    // create file if required
    if (!SerialFlash.exists(devEUIfileName)) {
      if (!SerialFlash.createErasable(devEUIfileName, EUI64_MAC_LENGTH)) {
        Serial.println(F("Create EUI file failed"));
        return false;
      }
    }
    file = SerialFlash.open(devEUIfileName);
    if (!file) {  // true if the file exists
      Serial.println(F("EUI file open failed"));
      return false;
    }
    // fill buffer with new EUI
    for (i=0; i<EUI64_MAC_LENGTH; i++) {
      buffer[i] = DEVEUI[i];
    }
    
    Serial.print(F("writing "));
    printEUI(true);
    Serial.println(F(" to Serial Flash"));

    file.write(buffer, EUI64_MAC_LENGTH);
    file.close();

    Serial.println(F("write completed"));
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
    case 'd':
    case 'D':
      deleteEUIfile();
      break;
    case 'f':
    case 'F':
      formatSerialFlash();
      break;
    case 'i':
    case 'I':
      serialFlashInfo();
      break;
    case 'l':
    case 'L':
      listSerialFlashFiles();
      break;
    case 'r':
    case 'R':
      if (readEUIfile()) {
        printAll();
      } else {
        Serial.print(F("Proposed EUI: "));
        printEUI(true);
        Serial.println();
      }
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
