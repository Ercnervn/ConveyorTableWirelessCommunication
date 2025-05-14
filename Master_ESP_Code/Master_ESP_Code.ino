// Haberleşme Kütüphaneleri
#include <esp_now.h>
#include <WiFi.h>

// ADC Modül Kütüphanesi
#include "Protocentral_ADS1220.h"
#include <SPI.h>

// ADC Modül için tanımlamalar
#define PGA          1           // Programlanabilir Kazanç = 1
#define VREF         2.048      // Dahili Referans Voltajı  2.048V
#define VFSR         VREF/PGA
#define FULL_SCALE   (((long int)1<<23)-1)

#define ADS1220_CS_PIN   5  // Chip Select Pini
#define ADS1220_DRDY_PIN  4 // Veri hazır Pini

// İLERİ GERİ START için Pin tanımlamaları
const int ILERI_START = 32;
const int GERI_START = 33;  

Protocentral_ADS1220 pc_ads1220;
int32_t adc_data;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Gönderilen Verinin Yapısı (Alıcı Veri yapısı ile aynı olmalıdır!)
typedef struct struct_message {
   int id; // Sunucu Kart ID'si her sunucu için farklı olmalıdır!
   int Potdeger; // Okunan Analog değer için tanımlanan değişken 
   int iStart;
   int gStart;
} struct_message;

// myData adında bir struct_message oluşturuyoruz
struct_message myData;

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nSon Paket Durumu:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Gönderim Başarılı" : "Gönderim Başarılı olmadı");
}

void setup() {
    Serial.begin(115200); // Seri Port'u aktif eder
    WiFi.mode(WIFI_STA); // Karta bulunan WiFi modülün rolünü belirler

    pinMode(ILERI_START, INPUT);
    pinMode(GERI_START, INPUT);
    
    // Haberleşme Protokolünü başlatır
    if (esp_now_init() != ESP_OK) {  
        // Serial.println("ESP-NOW Baslatilamadi");
        return;
    }
    pc_ads1220.begin(ADS1220_CS_PIN, ADS1220_DRDY_PIN);  // ADC Kartın kütüphanesini başlatır
    pc_ads1220.set_data_rate(DR_600SPS);   // Örnekleme hızını belirler
    pc_ads1220.set_pga_gain(PGA_GAIN_1);   // Kazancı belirler
    pc_ads1220.set_conv_mode_single_shot(); // Tek Kanal Analog değer okumak için yazılır

    esp_now_register_send_cb(OnDataSent);

    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
  
    // Eşleşme Ekler     
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        // Serial.println("Eşleşme Başarısız");
        return;
    }
}

void loop() {
    adc_data = pc_ads1220.Read_SingleShot_SingleEnded_WaitForData(MUX_SE_CH0);

    int x = convertToMilliV(adc_data) * 2; // 2V'a kadar okunan değeri 12 bit değerine eşitlemek için okunan data float'tan int'a dönüştürülüp 2 ile çarpılmıştır. Bu değer X adında bir değişkene atanmıştır.
    Serial.println(x);
    myData.id = 2;   // Master Kimlik bilgisi
    myData.Potdeger = x;  // Gönderilecek değer

    int ILERI = digitalRead(ILERI_START);  
    if (ILERI == HIGH) {
        myData.iStart = 255;
        // Serial.println("İleri Start Geldi");
    } else {
        myData.iStart = 0;
        // Serial.println("İleri Start Gelmiyor");
    }

    int GERI = digitalRead(GERI_START);
    if (GERI == HIGH) {
        myData.gStart = 255;
        // Serial.println("Geri Start Geldi");
    } else {
        myData.gStart = 0;
        // Serial.println("Geri Start Gelmiyor");  
    }

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
}

// Modülden okunan float değeri bir int olarak dönüştürür
int convertToMilliV(int32_t i32data) {
    return (int)((i32data * VREF * 1000) / FULL_SCALE);
}
