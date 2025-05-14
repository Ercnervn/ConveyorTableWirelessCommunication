# ConveyorTableWirelessCommunication
ESPNOW Wireless Comminication


The analog value read from a closed loop system is transmitted wirelessly with the ESP-NOW protocol. 

ESP32s can send and return data to each other by defining the mac addresses of this protocol within themselves. Using this protocol, the analog data read over an ESP is sent to the other side as a digital number value with the ESP-NOW protocol. In the same way, the ESP receiving the packet converts the digital value into an analog value and generates an analog voltage from the corresponding pin connected on it. Thus, the wireless analog voltage value is transmitted. This process takes place very quickly without data loss and fluctuation.
