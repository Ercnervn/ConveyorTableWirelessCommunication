#include <WiFi.h>
#include <esp_now.h>
#include <driver/ledc.h>    // Include for ESP32 LEDC PWM functions

// --- PWM configuration ----------------------------
#define PWM_PIN        5    // PWM output pin
#define PWM_FREQUENCY  5000 // PWM frequency in Hz
#define PWM_CHANNEL    0    // PWM channel number
#define PWM_RESOLUTION 8    // PWM resolution in bits

// --- Master selection pins and MAC addresses -------
#define NUM_MASTERS 5
// GPIO pins used to select which master is active
const int masterPins[NUM_MASTERS] = {12, 13, 14, 27, 26};
// MAC addresses corresponding to each master pin
uint8_t masterMACs[NUM_MASTERS][6] = {
  {0xEC, 0x94, 0xCB, 0x6F, 0x26, 0x50}, // Master 1
  {0xC8, 0xC9, 0xA3, 0xFA, 0xF6, 0x64}, // Master 2
  {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01}, // Master 3
  {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, // Master 4
  {0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC}  // Master 5
};

// Currently selected master index (-1 means none)
int currentMaster = -1;
// Allowed MAC address for ESP-NOW reception
uint8_t allowedMasterMAC[6] = {0};

// --- Timeout control -------------------------------
unsigned long lastReceiveTime = 0;
const unsigned long timeoutDuration = 100; // ms before PWM resets if no data

// --- Structure for incoming ESP-NOW data ------------
typedef struct {
  int id;
  int Potdeger;
  int iStart;
  int gStart;
} struct_message;
struct_message incomingData;

// --- ESP-NOW data receive callback -------------------
void onDataRecv(const uint8_t *mac, const uint8_t *incomingBytes, int len) {
  // Only process data from the allowed MAC address
  if (memcmp(mac, allowedMasterMAC, 6) == 0) {
    memcpy(&incomingData, incomingBytes, sizeof(incomingData));
    Serial.printf("Received Potdeger: %d | Master ID: %d\n",
                  incomingData.Potdeger, incomingData.id);

    // Convert 12-bit ADC value to 8-bit PWM
    int pwmValue = map(incomingData.Potdeger, 0, 4095, 0, 255);
    ledcWrite(PWM_CHANNEL, pwmValue);
    lastReceiveTime = millis();
  } else {
    Serial.println("Unauthorized MAC address!");
  }
}

void setup() {
  Serial.begin(115200);
  disableCore0WDT(); // Disable watchdog on core 0
  disableCore1WDT(); // Disable watchdog on core 1

  // Configure master selection pins as inputs with pull-down
  for (int i = 0; i < NUM_MASTERS; i++) {
    pinMode(masterPins[i], INPUT_PULLDOWN);
  }

  // Setup PWM channel and attach pin
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);

  // Initialize WiFi in station mode and start ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);

  lastReceiveTime = millis();
}

void loop() {
  // --- Update master selection based on pin states ---
  int selected = -1;
  for (int i = 0; i < NUM_MASTERS; i++) {
    if (digitalRead(masterPins[i]) == HIGH) {
      selected = i;
      break;
    }
  }
  if (selected >= 0) {
    if (currentMaster != selected) {
      // A new master was selected
      memcpy(allowedMasterMAC, masterMACs[selected], 6);
      currentMaster = selected;
      Serial.printf("Master %d selected.\n", selected + 1);
    }
  } else if (currentMaster != -1) {
    // No master pin is HIGH: clear selection
    memset(allowedMasterMAC, 0, 6);
    currentMaster = -1;
    Serial.println("Master selection cleared.");
  }

  // --- Timeout: reset PWM if no data received recently ---
  if (millis() - lastReceiveTime > timeoutDuration) {
    ledcWrite(PWM_CHANNEL, 0);
    Serial.println("Timeout: PWM reset.");
    lastReceiveTime = millis();
  }

  yield(); // Allow background tasks / watchdog reset
}
