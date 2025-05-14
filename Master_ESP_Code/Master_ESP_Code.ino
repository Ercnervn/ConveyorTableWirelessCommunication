// Communication libraries
#include <esp_now.h>
#include <WiFi.h>

// ADC module library
#include "Protocentral_ADS1220.h"
#include <SPI.h>

// Definitions for ADC module
#define PGA            1           // Programmable Gain Amplifier = 1
#define VREF           2.048       // Internal reference voltage = 2.048V
#define VFSR           VREF/PGA    // Full-scale voltage reference
#define FULL_SCALE     (((long int)1<<23)-1)  // ADC full-scale count (2^23 - 1)

#define ADS1220_CS_PIN    5  // Chip Select pin for ADC
#define ADS1220_DRDY_PIN  4  // Data Ready pin for ADC

// Pins for forward/backward start signals
const int ILERI_START = 32;
const int GERI_START  = 33;

Protocentral_ADS1220 pc_ads1220;
int32_t adc_data;

// Broadcast address for ESP-NOW
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Structure of data to send (must match receiver's structure)
typedef struct struct_message {
   int id;         // Server card ID; unique for each server
   int Potdeger;   // Measured analog value
   int iStart;     // Forward start flag
   int gStart;     // Backward start flag
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Callback after data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // You can print send status here if needed
}

void setup() {
    Serial.begin(115200);               // Initialize serial port
    WiFi.mode(WIFI_STA);                // Set WiFi to station mode

    pinMode(ILERI_START, INPUT);
    pinMode(GERI_START, INPUT);
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        return;
    }

    // Initialize ADC module
    pc_ads1220.begin(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
    pc_ads1220.set_data_rate(DR_600SPS);          // Set sampling rate
    pc_ads1220.set_pga_gain(PGA_GAIN_1);          // Set PGA gain
    pc_ads1220.set_conv_mode_single_shot();       // Single-shot conversion mode

    esp_now_register_send_cb(OnDataSent);

    // Configure peer for broadcasting
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
  
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        return;
    }
}

void loop() {
    // Read single-ended ADC channel 0
    adc_data = pc_ads1220.Read_SingleShot_SingleEnded_WaitForData(MUX_SE_CH0);

    // Convert raw ADC data to millivolts and scale for 12-bit range
    int x = convertToMilliV(adc_data) * 2;
    Serial.println(x);

    myData.id       = 2;    // Master ID
    myData.Potdeger = x;    // Value to send

    // Read forward start signal
    if (digitalRead(ILERI_START) == HIGH) {
        myData.iStart = 255;    // Active
    } else {
        myData.iStart = 0;      // Inactive
    }

    // Read backward start signal
    if (digitalRead(GERI_START) == HIGH) {
        myData.gStart = 255;    // Active
    } else {
        myData.gStart = 0;      // Inactive
    }

    // Send data via ESP-NOW
    esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
}

// Convert ADC reading to millivolts (integer)
int convertToMilliV(int32_t raw) {
    return (int)((raw * VREF * 1000) / FULL_SCALE);
}
