#include <TinyGPS++.h>
#include <wiring_private.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <math.h>

// GPS setup
TinyGPSPlus gps;
unsigned long previousMillis = 0;
const long interval = 1000; // 1 second interval for GPS update
Uart Serial2(&sercom1, 12, 10, SERCOM_RX_PAD_3, UART_TX_PAD_2);

void SERCOM1_Handler() {
  Serial2.IrqHandler();
}

// Heart rate setup
MAX30105 particleSensor;
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

// Payload setup
unsigned long acc_timer = millis();
const int taskDelay = 100;
static uint8_t payload[16];
static osjob_t sendjob;
const unsigned TX_INTERVAL = 16;

static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

static const u1_t PROGMEM DEVEUI[8] = { 0xD8, 0xB3, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

static const u1_t PROGMEM APPKEY[16] = { 0xA2, 0x47, 0xC4, 0x7E, 0xD0, 0x56, 0x73, 0x39, 0xFC, 0xBA, 0x9A, 0x1F, 0x73, 0x42, 0xE9, 0x98};
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

const lmic_pinmap lmic_pins = {
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3, 6, LMIC_UNUSED_PIN},
    .rxtx_rx_active = 0,
    .rssi_cal = 8,
    .spi_freq = 8000000,
};

unsigned long lastPrintTime = 0;
unsigned long lastNoSatelliteTime = 0;
unsigned long lastSendTime = 0;
const unsigned long noSatelliteInterval = 500;
const unsigned long sendInterval = 1000; // 1 second interval for sending
const unsigned long printInterval = 1000; // 1 second interval for printing

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // GPS setup
  Serial2.begin(9600);
  pinPeripheral(12, PIO_SERCOM);
  pinPeripheral(10, PIO_SERCOM);
  Serial.println("Démarrage du GPS...");

  // Heart rate setup
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power.");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  // Payload setup
  os_init();
  LMIC_reset();
  LMIC_setLinkCheckMode(0);
  LMIC_setDrTxpow(DR_SF7, 14);
  do_send(&sendjob);
}

void loop() {
  // GPS data reading
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    while (Serial2.available() > 0) {
      char c = Serial2.read();
      gps.encode(c);
    }
  }

  // Heart rate data reading
  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    beatsPerMinute = 60 / (delta / 1000.0);
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  // Print values every 5 seconds
  if (millis() - lastPrintTime >= 5000) {
    lastPrintTime = millis();
    Serial.print("BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.println(beatAvg);

    if (gps.location.isValid()) {
      Serial.println("Satellite trouvé");
      Serial.print("Latitude: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("Longitude: ");
      Serial.println(gps.location.lng(), 6);
    } else {
      Serial.println("Pas de satellite trouvé");
    }
  }

  // Build and send payload every second
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    if (gps.satellites.value() > 0) {
      buildPayload();
    } else {
      if (millis() - lastNoSatelliteTime >= noSatelliteInterval) {
        lastNoSatelliteTime = millis();
        Serial.println("Pas de satellite trouvé, envoi annulé");
      }
    }
  }

  os_runloop_once();
}

void buildPayload() {
  payload[0] = gps.location.isValid() ? 1 : 0;

  int32_t lat = gps.location.lat() * 1000000;
  payload[1] = (uint8_t)(lat);
  payload[2] = (uint8_t)(lat >> 8);
  payload[3] = (uint8_t)(lat >> 16);
  payload[4] = (uint8_t)(lat >> 24);

  int32_t lng = gps.location.lng() * 1000000;
  payload[5] = (uint8_t)(lng);
  payload[6] = (uint8_t)(lng >> 8);
  payload[7] = (uint8_t)(lng >> 16);
  payload[8] = (uint8_t)(lng >> 24);

  payload[9] = (uint8_t)beatAvg;
  payload[10] = (uint8_t)(beatAvg >> 8);

  Serial.println("Payload construit:");
  for (int i = 0; i < 11; i++) {
    Serial.print(payload[i]);
    Serial.print(" ");
  }
  Serial.println();

  do_send(&sendjob);
}

void do_send(osjob_t* j) {
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    LMIC_setTxData2(1, payload, sizeof(payload), 0);
    Serial.println(F("sending"));
  }
  os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
}

void onEvent(ev_t ev) {
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
      Serial.println(F("EV_JOINED"));
      LMIC_setLinkCheckMode(0);
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("Payload sent successfully"));
      Serial.println(F("PAYLOAD ENVOYÉ"));
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    case EV_TXSTART:
      Serial.println(F("Starting new transmission"));
      break;
    default:
      Serial.print(F("ERROR: Unknown event "));
      Serial.println(ev);
      break;
  }
}