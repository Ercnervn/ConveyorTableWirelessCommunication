#include <WiFi.h>
#include <esp_now.h>
#include <driver/ledc.h>    // ledcSetup ve ledcAttachPin için

// --- PWM tanımlamaları --------------------------------
#define PWM_PIN        5
#define PWM_FREQUENCY  5000
#define PWM_CHANNEL    0
#define PWM_RESOLUTION 8

// --- Master seçim pini ve MAC adresi -----------------
#define MASTER_SELECT_PIN 12   // D12 pin’ine gelen sinyal ile master seçilecek

// Master cihazın MAC adresi
uint8_t masterMAC[6] = { 0xEC, 0x94, 0xCB, 0x6F, 0x26, 0x50 };

// Şu an izinli MAC (ilk etapta sıfırlı)
uint8_t allowedMasterMAC[6] = {0};

// Zaman kontrolü
unsigned long lastReceiveTime = 0;
const unsigned long timeoutDuration = 100; // ms

// Gelen veri yapısı
typedef struct {
  int id;
  int Potdeger;
  int iStart;
  int gStart;
} struct_message;
struct_message incomingData;

// ESP-NOW’dan veri geldiğinde çağrılan fonksiyon
void onDataRecv(const uint8_t *mac, const uint8_t *incomingBytes, int len) {
  // Sadece izinli MAC’den gelen veri işlenecek
  if (memcmp(mac, allowedMasterMAC, 6) == 0) {
    memcpy(&incomingData, incomingBytes, sizeof(incomingData));
    Serial.printf("Gelen Potdeger: %d | Master ID: %d\n",
                  incomingData.Potdeger, incomingData.id);

    // PWM’e uygula (12-bit ADC → 8-bit PWM)
    int pwmValue = map(incomingData.Potdeger, 0, 4095, 0, 255);
    ledcWrite(PWM_CHANNEL, pwmValue);

    lastReceiveTime = millis();
  }
  else {
    Serial.println("İzinsiz MAC adresi!");
  }
}

void setup() {
  Serial.begin(115200);
  disableCore0WDT();
  disableCore1WDT();

  // --- Master seçim pini giriş olarak ayarla
  pinMode(MASTER_SELECT_PIN, INPUT_PULLDOWN);

  // --- PWM yapılandırması
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);

  // --- ESP-NOW başlat
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init hatası");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);

  lastReceiveTime = millis();
}

void loop() {
  // --- Master seçimini pin’e göre güncelle
  if (digitalRead(MASTER_SELECT_PIN) == HIGH) {
    // Sinyal geldiğinde izinli MAC’i ayarla
    memcpy(allowedMasterMAC, masterMAC, 6);
    Serial.println("Master seçildi.");
  } else {
    // Sinyal yoksa temizle
    memset(allowedMasterMAC, 0, 6);
  }

  // --- Timeout kontrolü: uzun süre veri gelmezse PWM sıfırlansın
  if (millis() - lastReceiveTime > timeoutDuration) {
    ledcWrite(PWM_CHANNEL, 0);
    Serial.println("Timeout: PWM sıfırlandı.");
    lastReceiveTime = millis();
  }

  yield();
}
