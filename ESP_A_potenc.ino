#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>

// #define PIN_LED_VERDE 27
// #define PIN_LED_VERMELHO 26
#define PIN_DHT 18
#define PIN_MQ2 35 // Pino para o Potenciômetro (entrada analógica)
// #define PIN_ENVIO 4
// #define PIN_ERRO 5

#define DHT_TYPE DHT11

// Define a resolução máxima de leitura analógica do ESP32 por padrão
#define MAX_ANALOG_READ 4095 

unsigned long lastBlinkTime = 0; 
long blinkInterval = 200;

uint8_t mac_A[] = {0xFC, 0xE8, 0xC0, 0xE0, 0xFE, 0x80}; //Marinho normal
uint8_t mac_B[] = {0x80, 0xF3, 0xDA, 0x63, 0x7E, 0x48}; //Marinho D

const long intervalo_leitura = 2000;
unsigned long timestamp_leitura_anterior = 0;

DHT dht(PIN_DHT, DHT_TYPE);

typedef struct struct_message_A {
uint8_t mac_A[6];
float temperatura_A;
float umidade_A;
int concen_gas_A; // Este campo armazenará a porcentagem convertida (0-100)
unsigned long timestamp_A;
} struct_message_A;

struct_message_A dadosRecebidosDeA;
esp_now_peer_info_t peerInfo_B;

// --- Nova Função de Conversão ---
int converterParaPorcentagem(int valorAnalogico) {
    return map(valorAnalogico, 0, MAX_ANALOG_READ, 0, 100);
}
// ---------------------------------

void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
if (status != ESP_NOW_SEND_SUCCESS) {
 Serial.println("Falha no envio ESPNOW");
 Serial.println("      "); 
//  blinkNonBlocking(PIN_ERRO, blinkInterval);
}
else{
//  blinkNonBlocking(PIN_ENVIO, blinkInterval);
}
}

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32 Chão de Fábrica...");

  // pinMode(PIN_LED_VERDE, OUTPUT);
  // pinMode(PIN_LED_VERMELHO, OUTPUT);
  // pinMode(PIN_ENVIO, OUTPUT);
  // pinMode(PIN_ERRO, OUTPUT);

  dht.begin();
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo_B.peer_addr, mac_B, 6);
  peerInfo_B.channel = 0;
  peerInfo_B.encrypt = false;

  if (esp_now_add_peer(&peerInfo_B) != ESP_OK) {
    Serial.println("Falha ao adicionar peer");
    return;
  }

  Serial.println("Setup concluído. Iniciando leituras...");
}

void loop() {
  if (millis() - timestamp_leitura_anterior >= intervalo_leitura) {
  timestamp_leitura_anterior = millis();
  lerSensores();
  atualizarStatusLocal();
  enviarDadosEspNow();
}
}

void lerSensores() {
  int leitura_analogica_mq2 = analogRead(PIN_MQ2); // Lê o valor bruto (0-4095)
  dadosRecebidosDeA.temperatura_A = dht.readTemperature();
  dadosRecebidosDeA.umidade_A = dht.readHumidity();
// Chama a nova função para converter a leitura bruta em porcentagem (0-100)
  dadosRecebidosDeA.concen_gas_A = converterParaPorcentagem(leitura_analogica_mq2);
  dadosRecebidosDeA.timestamp_A = millis();
}

void atualizarStatusLocal() {
  if (isnan(dadosRecebidosDeA.temperatura_A) || isnan(dadosRecebidosDeA.umidade_A)) {
    Serial.println("Falha ao ler o sensor DHT!");
  }

  Serial.print("Temperatura: ");
  Serial.print(dadosRecebidosDeA.temperatura_A, 1);
  Serial.print(" ºC | Umidade: ");
  Serial.print(dadosRecebidosDeA.umidade_A, 1);
  Serial.println("%");
  Serial.print("Gás (Simulado): ");
  Serial.print(dadosRecebidosDeA.concen_gas_A);
  Serial.println("%"); 
}

// void blinkNonBlocking(int ledPin, long interval) {
//   unsigned long currentMillis = millis();
//   if (currentMillis - lastBlinkTime >= interval) {
//     lastBlinkTime = currentMillis;
//     int ledState = digitalRead(ledPin);

//   if (ledState == LOW) {
//    digitalWrite(ledPin, HIGH); 
//  } else {
//        digitalWrite(ledPin, LOW); 
//     }
//   }
// }


void enviarDadosEspNow() {
  esp_err_t result = esp_now_send(mac_B, (uint8_t *) &dadosRecebidosDeA, sizeof(dadosRecebidosDeA));
  if (result == ESP_OK) {
    Serial.print("Pacote enviado com sucesso: {");
    Serial.print("%, temp="); Serial.print(dadosRecebidosDeA.temperatura_A, 1);
    Serial.print("C, umidade="); Serial.print(dadosRecebidosDeA.umidade_A, 1);
    Serial.print("%, gas="); Serial.print(dadosRecebidosDeA.concen_gas_A);
    Serial.println("%}"); 
  }
  else{
 }
}