#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <math.h>

MAX30105 particleSensor;

const byte RATE_SIZE = 4; // Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; // Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; // Time at which the last beat occurred
long lastPrintTime = 0;
long lastSendTime = 0; // Time at which the last payload was sent

float beatsPerMinute;
int beatAvg;
bool joined = false; // Flag to check if the device has joined the network

// TTN Configuration
static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

static const u1_t PROGMEM DEVEUI[8] = { 0xD8, 0xB3, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

static const u1_t PROGMEM APPKEY[16] = { 0xA2, 0x47, 0xC4, 0x7E, 0xD0, 0x56, 0x73, 0x39, 0xFC, 0xBA, 0x9A, 0x1F, 0x73, 0x42, 0xE9, 0x98};
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

// payload to send to TTN gateway
static uint8_t payload[16];
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 7;

// Pin mapping for Adafruit Feather M0 LoRa
const lmic_pinmap lmic_pins = {
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 4,
    .dio = {3, 6, LMIC_UNUSED_PIN},
    .rxtx_rx_active = 0,
    .rssi_cal = 8,              // LBT cal for the Adafruit Feather M0 LoRa, in dB
    .spi_freq = 8000000,
};

void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Initialize sensor
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 was not found. Please check wiring/power.");
        while (1);
    }
    Serial.println("Place your index finger on the sensor with steady pressure.");

    particleSensor.setup(); // Configure sensor with default settings
    particleSensor.setPulseAmplitudeRed(0x0A); // Turn Red LED to low to indicate sensor is running
    particleSensor.setPulseAmplitudeGreen(0); // Turn off Green LED

    // LMIC init.
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    // Disable link-check mode and ADR, because ADR tends to complicate testing.
    LMIC_setLinkCheckMode(0);
    // Set the data rate to Spreading Factor 7. This is the fastest supported rate for 125 kHz channels, and it
    // minimizes air time and battery power. Set the transmission power to 14 dBi (25 mW).
    LMIC_setDrTxpow(DR_SF7, 14);

    // Start joining
    LMIC_startJoining();
}

void loop() {
    // Heart rate data reading
    long irValue = particleSensor.getIR();
    if (checkForBeat(irValue) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);
        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
            rates[rateSpot++] = (byte)beatsPerMinute; // Store this reading in the array
            rateSpot %= RATE_SIZE; // Wrap variable
            // Take average of readings
            beatAvg = 0;
            for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
            beatAvg /= RATE_SIZE;
        }
    }

    // Print data only every 12 seconds
    if (millis() - lastPrintTime >= 12000) {
        lastPrintTime = millis();
        if (irValue < 50000) {
            Serial.println("No finger?");
        } else {
            Serial.print("BPM=");
            Serial.print(beatsPerMinute);
            Serial.print(", Avg BPM=");
            Serial.println(beatAvg);
        }
    }

    // Build and send payload every 7 seconds if joined
    if (joined && millis() - lastSendTime >= TX_INTERVAL * 1000) {
        lastSendTime = millis();
        buildPayload();
        do_send(&sendjob);
    }

    // Call LMIC's runloop processor
    os_runloop_once();
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
            joined = true; // Set the joined flag to true
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
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
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
        case EV_TXSTART:
            Serial.println(F("Starting new transmission"));
            break;
        default:
            Serial.print(F("ERROR: Unknown event "));
            Serial.println(ev);
            break;
    }
}

void do_send(osjob_t* j) {
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, payload, sizeof(payload), 0);
        Serial.println(F("sending"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void buildPayload() {
    // Fill the payload with some example data
    payload[0] = 0;
    payload[1] = 1;
    payload[2] = 2;
    payload[3] = 3;
    payload[4] = 4;
    payload[5] = 5;
    payload[6] = 6;
    payload[7] = 7;
    payload[8] = (uint8_t)beatAvg; // BPM value
    payload[9] = (uint8_t)(beatAvg >> 8); // BPM value (high byte)
    payload[10] = 10;
    payload[11] = 11;
    payload[12] = 12;
    payload[13] = 13;
    payload[14] = 14;
    payload[15] = 15;

    Serial.print("Payload construit: ");
    for (int i = 0; i < 16; i++) {
        Serial.print(payload[i]);
        Serial.print(" ");
    }
    Serial.println();
}