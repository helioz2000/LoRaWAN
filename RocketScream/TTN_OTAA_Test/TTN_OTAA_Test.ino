/*
  Rocketscream Mini Ultra Pro V1
  Board: Arduino Zero (Native USB Port)

  Application: TTN OTAA test

  RFM95W connections:
  DIO0 - D2 (fixed)
  DIO1 - D3 (added by eb)
  NSS  - D5 (fixed)
  RST  - D6 (addeb by eb)

  Serial Flash:
  _CS_ - D4

  Note: Mini Ultra Pro V1 does not come with a globally unique Device EUI.
        The EUI is located in a Microchip chip and needs to be read out via the I2C
        bus. 
        
  Sourcing a device EUI: 
        1) use an EUI which can be assigned by TTN durign the device registration process.
        2) purchase and EUI 64 chip and use the EUI from the chip; destroy ship to avoide dupicate EUI's

  Storing the EUI:
        This code provides for 2 options to store the device EUI:
        1) stored in a file on the SerialFlash chip (part of rocketscream board)
        2) hardcoded into the source code (this file)
        See code below for more details
*/

#include <lmic.h>
#include <hal/hal.h>

#include <SPI.h>
#include <RTCZero.h>
#include <SerialFlash.h>    // provide access to serial flash chip

// required for V2 or great to read device EUI from chip
#include <Wire.h> 

#define Serial SerialUSB    // Serial console is direct USB

#define RADIO_CS_PIN 5      // RFM95W chip select pin
#define FLASH_CS_PIN 4      // digital pin for serial flash chip select pin

char consoleInputChar;

// Only relevant for Mini Ultra Pro V2 or greater
#define EUI64_CHIP_ADDRESS 0x50
#define EUI64_MAC_ADDRESS 0xF8
#define EUI64_MAC_LENGTH 0x08

RTCZero rtc;

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
//70B3D57ED0012AC2
static const u1_t PROGMEM APPEUI[8] = { 0xC2, 0x2A, 0x01, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getArtEui (u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}

// This should also be in little endian format, see above.
u1_t DEVEUI[EUI64_MAC_LENGTH];
void os_getDevEui (u1_t* buf) {
  memcpy(buf, DEVEUI, EUI64_MAC_LENGTH);
}

/* Device EUI for Rocketscream V1 board (no EUI chip)
 * Two options to provide the device EUI:
 * 1) stored in a file on the SerialFlash chip
 * 2) hardcoded below
 * If option 1 is present option 2 will be ignored
 */
 
// Option 1) device EUI stored in a file on the SerialFlash chip
const char devEUIfileName[] = { "dev_eui.bin" };
// Option 2) device EUI hardcoded here (copy from TTN application definition)
static const u1_t PROGMEM deviceEUI[EUI64_MAC_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.
static const u1_t PROGMEM APPKEY[16] = { 0x72, 0x07, 0x01, 0x31, 0x4F, 0x47, 0xE7, 0x88, 0x45, 0xB9, 0x14, 0x91, 0x96, 0xEE, 0x9A, 0x0D };
void os_getDevKey (u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}

static uint8_t mydata[] = "VK3ERW";
static lmic_time_reference_t lmic_time_reference;

static bool txActive = false;     // transmission is active
static bool joined = false;       // indicate if we have successully joined
static bool nwTimeRequest = false; // network time request scheduled

osjob_t sendjob;
osjob_t initjob;

uint32_t userUTCTime; // Seconds since the UTC epoch

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

// Pin mapping for lmic library
const lmic_pinmap lmic_pins = {
.nss = 5,
.rxtx = LMIC_UNUSED_PIN,
.rst = 6,
.dio = {2, 3, LMIC_UNUSED_PIN},
};

void setup() {
  while (!Serial && millis() < 10000);
  Serial.begin(115200);       // fixed baudrate for USB, console can be set to anything
  Serial.println();
  Serial.println();
  Serial.println(F("Starting"));
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RADIO_CS_PIN, OUTPUT);
  pinMode(FLASH_CS_PIN, OUTPUT);
  // disable the radio chip
  digitalWrite(RADIO_CS_PIN, HIGH);
  // enable serial flash chip
  digitalWrite(FLASH_CS_PIN, LOW);
  delay(200); // time for CS to take place
  // Initialize serial flash
  if (!SerialFlash.begin(FLASH_CS_PIN)) {
    Serial.println(F("Unable to access SPI Flash chip"));  
  }
  SerialFlash.wakeup();     // may still be asleep after reset of main chip
  setDevEui(&DEVEUI[EUI64_MAC_LENGTH - 1]);  
  // Put SerialFlash in sleep - no longer needed
  SerialFlash.sleep();

  // Display device EUI
  Serial.print("Device EUI: ");
  printDevEUI();
  Serial.println();

  Serial.println(F("10s delay"));
  delay(10000);
  
  // ***** Put unused pins into known state *****
  pinMode(0, INPUT_PULLUP);   // Serial 1
  pinMode(1, INPUT_PULLUP);   // Serial 1
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  pinMode(11, INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
//  pinMode(13, INPUT_PULLUP);  // LED
  pinMode(14, INPUT_PULLUP);  // A0
  pinMode(15, INPUT_PULLUP);  // A1
  pinMode(16, INPUT_PULLUP);  // A2
  pinMode(17, INPUT_PULLUP);  // A3
  pinMode(18, INPUT_PULLUP);  // A4
  pinMode(19, INPUT_PULLUP);  // A5
  pinMode(20, INPUT_PULLUP);  // SCL
  pinMode(21, INPUT_PULLUP);  // SDA
  pinMode(22, INPUT_PULLUP);  // MISO (RFM95W chip)
  pinMode(23, INPUT_PULLUP);  // MOSI (RFM95W chip)
  pinMode(24, OUTPUT);        // SCK (RFM95W chip)
  pinMode(25, INPUT_PULLUP);  // RX LED - not fitted
  pinMode(26, INPUT_PULLUP);  // TX LED - not fitted
  pinMode(30, INPUT_PULLUP);  // Serial
  pinMode(31, INPUT_PULLUP);  // Serial
  pinMode(38, INPUT_PULLUP);  // ATN

  // Initialize RTC
  rtc.begin();
  // Use RTC as a second timer instead of calendar
  rtc.setEpoch(0);
  

  // LMIC init
  os_init();
  // setup initial job
  os_setCallback(&initjob, initfunc);
}

// initial job
static void initfunc (osjob_t* j) {
    // reset MAC state
    LMIC_reset();
    LMIC_setClockError(MAX_CLOCK_ERROR * 2 / 100);
    // start joining
    LMIC_startJoining();
    LMIC_registerRxMessageCb(rxmessage_callback, NULL);
    // init done - onEvent() callback will be invoked...
}

void printDevEUI(void) {
  int count;
  for (count = EUI64_MAC_LENGTH-1; count >= 0; count--) {
    printHex2(DEVEUI[count]);
    Serial.print(" ");
  }
}

/*
 * read device EUI from SerialFlash
 * returns true if EUI was successfuly retrieved
 */
bool readSerialFlash(unsigned char* buf) {
  char buffer[EUI64_MAC_LENGTH];
  int i;
  SerialFlashFile file;
  // wait for SerialFlash to be ready
  while (SerialFlash.ready() == false);
  file = SerialFlash.open(devEUIfileName);
  if (!file) {  // true if the file exists
    Serial.println(F("No EUI file found on SerialFlash"));
    return false;
  }
  Serial.println(F("EUI file found on SerialFlash"));
  file.read(buffer, EUI64_MAC_LENGTH);
  file.close();
  // Format needs to be little endian (LSB...MSB)
  for (i=0; i<EUI64_MAC_LENGTH; i++) {
    *buf-- = buffer[i];
  }
  return true;
}

/*
 * read device EUI and reverse byte order for little endian format
 * first try to read from SerialFlash, if not found use hardcoded EUI
 */
void setDevEui(unsigned char* buf) {
  int i;

  if (!readSerialFlash(buf)) {
  // read EUI from hardcoded definition
  // Format needs to be little endian (LSB...MSB)
    for (i=0; i<EUI64_MAC_LENGTH; i++) {
      *buf-- = deviceEUI[i];
    }
    Serial.println(F("use hard coded device EUI"));
  }
  
  /* for V2 or greater
  Wire.begin();
  Wire.beginTransmission(EUI64_CHIP_ADDRESS);
  Wire.write(EUI64_MAC_ADDRESS);
  Wire.endTransmission();
  Wire.requestFrom(EUI64_CHIP_ADDRESS, EUI64_MAC_LENGTH);

  // Format needs to be little endian (LSB...MSB)
  while (Wire.available()) {
    *buf-- = Wire.read();
  }
  */
}

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

void printUsage() {
  Serial.println(F("Commands"));
  Serial.println(F("s - send packet"));
  Serial.println(F("t - schedule network time request time"));
}

void showRxtxFlags(void) {
  Serial.print(F("LMIC.rxtxFlags: "));
  Serial.print(LMIC.txrxFlags);
  if (LMIC.txrxFlags & TXRX_ACK)
    Serial.print(F(" TXRX_ACK"));
  if (LMIC.txrxFlags & TXRX_NACK)
    Serial.print(F(" TXRX_NACK"));
  if (LMIC.txrxFlags & TXRX_PORT)
    Serial.print(F(" TXRX_PORT"));
  if (LMIC.txrxFlags & TXRX_NOPORT)
    Serial.print(F(" TXRX_NOPORT"));
  if (LMIC.txrxFlags & TXRX_DNW1)
    Serial.print(F(" TXRX_DNW1"));
  if (LMIC.txrxFlags & TXRX_DNW2)
    Serial.print(F(" TXRX_DNW2"));
  if (LMIC.txrxFlags & TXRX_PING)
    Serial.print(F(" TXRX_PING"));
  if (LMIC.txrxFlags & TXRX_LENERR)
    Serial.print(F(" TXRX_LENERR"));
  Serial.println();
}

void printCounters(void) {
  Serial.print(F("Up Ctr: "));
  Serial.print(LMIC.seqnoUp);
  Serial.print(F("  Dn Ctr: "));
  Serial.print(LMIC.seqnoDn);
}

static void user_request_network_time_callback(void *pVoidUserUTCTime, int flagSuccess) {  
  // Explicit conversion from void* to uint32_t* to avoid compiler errors
  uint32_t *pUserUTCTime = (uint32_t *) pVoidUserUTCTime;

  // A struct that will be populated by LMIC_getNetworkTimeReference.
  // It contains the following fields:
  //  - tLocal: the value returned by os_GetTime() when the time
  //            request was sent to the gateway, and
  //  - tNetwork: the seconds between the GPS epoch and the time
  //              the gateway received the time request
  lmic_time_reference_t lmicTimeReference;

  Serial.println("USER CALLBACK network time");

  if (flagSuccess != 1) {
    Serial.print("USER CALLBACK: Not a success: ");
    Serial.print(flagSuccess);
    Serial.println();
    return;
  }

  // Populate "lmic_time_reference"
  flagSuccess = LMIC_getNetworkTimeReference(&lmicTimeReference);
  if (flagSuccess != 1) {
    Serial.println(F("USER CALLBACK: LMIC_getNetworkTimeReference didn't succeed"));
    return;
  }

  Serial.println(F("USER CALLBACK: LMIC_getNetworkTimeReference success!"));
    
  // Update userUTCTime, considering the difference between the GPS and UTC
  // epoch, and the leap seconds
  *pUserUTCTime = lmicTimeReference.tNetwork + 315964800;

  // Add the delay between the instant the time was transmitted and
  // the current time

  // Current time, in ticks
  ostime_t ticksNow = os_getTime();
  // Time when the request was sent, in ticks
  ostime_t ticksRequestSent = lmicTimeReference.tLocal;
  uint32_t requestDelaySec = osticks2ms(ticksNow - ticksRequestSent) / 1000;
  *pUserUTCTime += requestDelaySec;

  // Update the system time with the time read from the network
  /*
  setTime(*pUserUTCTime);

    Serial.print(F("The current UTC time is: "));
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(' ');
    Serial.print(day());
    Serial.print('/');
    Serial.print(month());
    Serial.print('/');
    Serial.print(year());
    Serial.println();
    */
}

static void rxmessage_callback(void *pUserData, u1_t port, const u1_t *pMessage, size_t nMessage) {
  Serial.print(F("rx message callback "));
  Serial.print(nMessage);
  Serial.print(F(" bytes, port:"));
  Serial.println(port);
}

void do_send(osjob_t* j){
    lmic_tx_error_t lmic_tx_error;
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Schedule a network time request at the next possible time
        if (nwTimeRequest) {
          LMIC_requestNetworkTime(user_request_network_time_callback, &userUTCTime);
          nwTimeRequest = false;
        }
        // Prepare upstream data transmission at the next possible time.
        lmic_tx_error = LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        Serial.println(F("Packet queued"));
        if (lmic_tx_error != LMIC_ERROR_SUCCESS) {
          Serial.print(F("LMIC TX error: "));
          Serial.println(lmic_tx_error);
        }
    }
}

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            if (LMIC.txrxFlags) showRxtxFlags();
            break;
        case EV_JOINED:
            joined=true;
            txActive=false;
            Serial.println(F("EV_JOINED"));
            if (LMIC.txrxFlags) showRxtxFlags();
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial.print("netid: ");
              Serial.println(netid, DEC);
              Serial.print("devaddr: ");
              Serial.println(devaddr, HEX);
              Serial.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial.print("-");
                printHex2(artKey[i]);
              }
              Serial.println("");
              Serial.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial.println();
              printCounters();
              Serial.println();
              printUsage();
            }
            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
            // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_RFU1:
        ||     Serial.println(F("EV_RFU1"));
        ||     break;
        */
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            if (LMIC.txrxFlags) showRxtxFlags();
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            if (LMIC.txrxFlags) showRxtxFlags();
            break;
        case EV_TXCOMPLETE:
            txActive = false;
            Serial.print(F("EV_TXCOMPLETE (includes waiting for RX windows) "));
            printCounters();
            Serial.println();
            showRxtxFlags();
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            //os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            if (LMIC.txrxFlags) showRxtxFlags();
            printCounters();
            Serial.println();
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
        case EV_TXSTART:
            txActive = true;           
            Serial.println(F("EV_TXSTART"));
            if (LMIC.txrxFlags) showRxtxFlags();
            printCounters();
            Serial.println();
            break;
        case EV_TXCANCELED:
            txActive = false;
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            txActive = false;
            Serial.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            printCounters();
            Serial.println();
            break;

        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

bool recvOneChar() {
  if (Serial.available() > 0) {
    consoleInputChar = Serial.read();
    return true;
  }
  return false;
}

void handleCommand(char cmd) {
  if (!joined) {
    Serial.print(F("Wait until JOIN is completed"));
    return;
  }
  switch (cmd) {
    case 's':
    case 'S':
      do_send(&sendjob);
      break;
    case 't':
    case 'T':
      // Schedule a network time request at the next possible time
      nwTimeRequest = true;
      Serial.print(F("Network time request will be made with next packet transmission"));
      break;
    default:
      Serial.print(F("Invalid command"));
      break;
  }
}

void loop() {
  int result;
  os_runloop_once();

  if (recvOneChar()) {
    if (consoleInputChar > ' ')
      handleCommand(consoleInputChar);
  }
  // LED On while transmitting
  if (txActive) {
    digitalWrite(PIN_LED_13, HIGH);
  } else {
    digitalWrite(PIN_LED_13, LOW);
  }
  
}
