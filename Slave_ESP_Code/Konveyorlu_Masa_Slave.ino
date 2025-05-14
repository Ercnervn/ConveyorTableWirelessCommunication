#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <MFRC522.h>

// PWM çıkışı için kullanılacak pin
#define PWM_PIN 5
#define PWM_FREQUENCY 5000
#define PWM_CHANNEL 0
#define PWM_RESOLUTION 8

// RFID pin tanımlamaları
#define SS_PIN 21
#define RST_PIN 22
MFRC522 rfid(SS_PIN, RST_PIN); // MFRC522 nesnesi oluştur

// Gelen veri yapısı
typedef struct struct_message {
    int id;         // Master Kart ID'si (her master için farklı olmalıdır!)
    int Potdeger;   // Okunan analog değer
    int iStart;
    int gStart;
} struct_message;

struct_message incomingData;

// İzin verilen master MAC adresi
uint8_t allowedMasterMAC[6] = {0}; // Başlangıçta tanımsız

// RFID okuma kontrolü için flag
bool rfidReadEnabled = true;
unsigned long lastRFIDReadTime = 0;
const unsigned long rfidReadCooldown = 1000; // 1000 ms = 1 saniye

// Zaman kontrolü için değişkenler
unsigned long lastReceiveTime = 0; // Son veri alım zamanı
const unsigned long timeoutDuration = 100; // 2000 ms = 2 saniye (2 saniye boyunca veri gelmezse çıkış sıfırlanacak)

// Callback fonksiyonu: Veri alımı gerçekleştiğinde çağrılır
void onDataRecv(const uint8_t *mac, const uint8_t *incomingDataBytes, int len) {
    // Sadece izin verilen MAC adresinden gelen veriyi işle
    if (memcmp(mac, allowedMasterMAC, 6) == 0) {
        memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));
        
        Serial.print("Gelen veri: ");
        Serial.print(incomingData.Potdeger);
        Serial.print(" | Gelen Master ID: ");
        Serial.println(incomingData.id);
        
        // Gelen veriyi PWM çıkışına uygula
        int pwmValue = map(incomingData.Potdeger, 0, 4095, 0, 255); // 12-bit ADC değerini 8-bit PWM değerine dönüştür
        ledcWrite(PWM_CHANNEL, pwmValue);

        // Son veri alım zamanını güncelle
        lastReceiveTime = millis();
    } else {
        Serial.println("İzinsiz MAC adresinden gelen veri, işlenmedi.");
    }
}

void setup() {
    Serial.begin(115200);

    // Watchdog Timer'ı devre dışı bırak
    disableCore0WDT();
    disableCore1WDT();

    // RFID başlat
    SPI.begin();
    rfid.PCD_Init();
    Serial.println("RFID okuyucu hazır.");

    // WiFi'yi başlat
    WiFi.mode(WIFI_STA);
  
    // ESP-NOW başlat
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Veri alım callback fonksiyonu ekle
    esp_now_register_recv_cb(onDataRecv);

    // PWM çıkışını yapılandır
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(PWM_PIN, PWM_CHANNEL);

    // İlk zamanlayıcı başlangıcı
    lastReceiveTime = millis();
}

void loop() {
    // RFID kartını kontrol et
    if (rfidReadEnabled) {
        readRFID();
    }

    // Veri gelmeyen süreyi kontrol et
    if (millis() - lastReceiveTime > timeoutDuration) {
        // Zaman aşıldı, PWM çıkışını sıfırla
        ledcWrite(PWM_CHANNEL, 0);
        Serial.println("Bağlantı koptu. PWM çıkışı sıfırlandı.");
    }

    // RFID okuma için yeniden etkinleştirme kontrolü
    if (!rfidReadEnabled && (millis() - lastRFIDReadTime > rfidReadCooldown)) {
        rfidReadEnabled = true; // RFID okumasını yeniden etkinleştir
    }

    // Task Watchdog Timer'ı sıfırlamak için yield kullanımı
    yield();
}

// RFID kartını okuyan fonksiyon
void readRFID() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return; // Yeni kart yoksa çık
    }

    String cardID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        cardID += String(rfid.uid.uidByte[i], HEX);
    }
    cardID.toUpperCase(); // Kart ID'sini büyük harfe çevir

    // Kart ID'sini kontrol et ve izin verilen master MAC adresini ayarla
    if (cardID == "234256F7") {
        uint8_t master1MAC[] = {0xEC, 0x94, 0xCB, 0x6F, 0x26, 0x50}; // Master 1'in MAC adresi
        memcpy(allowedMasterMAC, master1MAC, 6);
        Serial.println("Master 1'e izin verildi.");
    } else if (cardID == "53EA68F5") {
        uint8_t master2MAC[] = {0xC8, 0xC9, 0xA3, 0xFA, 0xF6, 0x64}; // Master 2'nin MAC adresi
        memcpy(allowedMasterMAC, master2MAC, 6);
        Serial.println("Master 2'ye izin verildi.");
    } else {
        memset(allowedMasterMAC, 0, 6); // Tanınmayan kart; izinli MAC adresini sıfırla
        Serial.println("Kart tanınmadı, izin yok.");
    }

    // Kartı tekrar okumayı önlemek için kısa bir süreliğine durdur
    rfidReadEnabled = false;
    lastRFIDReadTime = millis();

    // Kartı tekrar okumayı önlemek için durdur
    rfid.PICC_HaltA();
}
