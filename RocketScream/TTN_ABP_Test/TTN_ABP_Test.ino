/*
  Rocketscream Mini Ultra Pro V1
  Board: Arduino Zero (Native USB Port)

  Application: TTN ABP test

  RFM95W connections:
  DIO0 - D2 (fixed)
  DIO1 - D3 (added by eb)
  NSS  - D5 (fixed)
  RST  - D6 (addeb by eb)

  Serial Flash:
  _CS_ - D4

  Note: Mini Ultra Pro V1 does not come with a globally unique Device EUI.
        The EUI is located in a chip and needs to be read out via the I2C
        bus. For V1 devices use an EUI which can be assigned by TTN
        durign the device registration process.
*/

#include <lmic.h>
#include <hal/hal.h>

#include <SPI.h>
#include <Wire.h>     // needed to device EUI from chip on V2 or above
#include <RTCZero.h>
#include <SerialFlash.h>

#define Serial SerialUSB    // Serial console is direct USB

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
// Note: Mini Ultra Pro V2 or greater provide Device EUI on board
// the older V1 does not
//u1_t DEVEUI[EUI64_MAC_LENGTH];
static const u1_t PROGMEM DEVEUI[EUI64_MAC_LENGTH] = { 0x11, 0xAA, 0xF2, 0xF4, 0x31, 0x87, 0x0D, 0x00 };
void os_getDevEui (u1_t* buf) {
  memcpy(buf, DEVEUI, EUI64_MAC_LENGTH);
}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.
static const u1_t PROGMEM APPKEY[16] = { 0x72, 0x07, 0x01, 0x31, 0x4F, 0x47, 0xE7, 0x88, 0x45, 0xB9, 0x14, 0x91, 0x96, 0xEE, 0x9A, 0x0D };
void os_getDevKey (u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}

static uint8_t mydata[] = "VK3ERW";
static osjob_t sendjob;
static lmic_time_reference_t lmic_time_reference;

static bool txActive = false;     // transmission is active
static bool joined = false;       // indicate if we have successully joined
static bool timeSyncDone = false; // network time sync

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

// the setup function runs once when you press reset or power the board
void setup() {
  int count;

  while (!Serial && millis() < 10000);
  Serial.begin(115200);       // fixed baudrate for USB, console can be set to anything
  Serial.println(F("Starting - 1s delay"));
  delay(1000);
  // Initialize serial flash
  SerialFlash.begin(4);
  // Put serial flash in sleep
  SerialFlash.sleep();

  pinMode(LED_BUILTIN, OUTPUT);
  
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
  
  // Enable for Rocketscream Mini Ultra Pro V2 or greater
  //setDevEui(&DEVEUI[EUI64_MAC_LENGTH - 1]);

  // Display device EUI
  Serial.print("Device EUI: ");
  for (count = EUI64_MAC_LENGTH; count > 0; count--) {
    //Serial.print("0x");
    if (DEVEUI[count - 1] <= 0x0F) Serial.print("0");
    Serial.print(DEVEUI[count - 1], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();
  delay(500);
  LMIC_setClockError(MAX_CLOCK_ERROR * 2 / 100);

  // Join request
  LMIC_startJoining();  

}

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

// This function will only work for Mini Ultra Pro V2 or greater
void setDevEui(unsigned char* buf)
{
  Wire.begin();
  Wire.beginTransmission(EUI64_CHIP_ADDRESS);
  Wire.write(EUI64_MAC_ADDRESS);
  Wire.endTransmission();
  Wire.requestFrom(EUI64_CHIP_ADDRESS, EUI64_MAC_LENGTH);

  // Format needs to be little endian (LSB...MSB)
  while (Wire.available())
  {
    *buf-- = Wire.read();
  }
}

void user_request_network_time_callback(void *pVoidUserUTCTime, int flagSuccess) {
    // Explicit conversion from void* to uint32_t* to avoid compiler errors
    uint32_t *pUserUTCTime = (uint32_t *) pVoidUserUTCTime;

    // A struct that will be populated by LMIC_getNetworkTimeReference.
    // It contains the following fields:
    //  - tLocal: the value returned by os_GetTime() when the time
    //            request was sent to the gateway, and
    //  - tNetwork: the seconds between the GPS epoch and the time
    //              the gateway received the time request
    lmic_time_reference_t lmicTimeReference;

    if (flagSuccess == 0) {
        Serial.println(F("USER CALLBACK: Not a success"));
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


void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Schedule a network time request at the next possible time
        //if (!timeSyncDone) {
          LMIC_requestNetworkTime(user_request_network_time_callback, &userUTCTime);
        //}
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        Serial.println(F("Packet queued"));
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
            break;
        case EV_JOINED:
            joined=true;
            txActive=false;
            Serial.println(F("EV_JOINED"));
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
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            txActive = false;
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.print(F("Received "));
              Serial.print(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
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
            break;

        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

void loop() {
  int result;
  os_runloop_once();
  /*
  // Start job (sending automatically starts OTAA too)
  do_send(&sendjob);
  */
  // LED On while transmitting
  if (txActive) {
    digitalWrite(PIN_LED_13, HIGH);
  } else {
    digitalWrite(PIN_LED_13, LOW);
  }
  
  if (joined && !timeSyncDone) {
    /*
    result = LMIC_getNetworkTimeReference(&lmic_time_reference);
    if (result == 0) {
      Serial.println("Invalid time");
    } else {
      Serial.print(F("Time: "));
      Serial.print(lmic_time_reference.tLocal);
      Serial.print(" ");
      Serial.println(lmic_time_reference.tNetwork);
    }
    */
    do_send(&sendjob);
    timeSyncDone = true;
  }
/*
  digitalWrite(PIN_LED_13, HIGH);   // turn the LED on (HIGH is the voltage level)
  //delay(1000);                       // wait for a second
  digitalWrite(PIN_LED_13, LOW);    // turn the LED off by making the voltage LOW
  //delay(1000);                       // wait for a second
*/

}
