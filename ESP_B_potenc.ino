#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>

#define PIN_LED_ENVIO 27
#define PIN_LED_ERRO 26
//#define PIN_LED_ALERTA 34
#define PIN_DHTB 32
#define PIN_MQ2B 33 // Pino para o Potenciômetro (entrada analógica)

#define DHT_TYPE DHT11

// Define a resolução máxima de leitura analógica do ESP32 por padrão
#define MAX_ANALOG_READ 4095 

DHT dht(PIN_DHTB, DHT_TYPE);

const long intervalo_leitura = 2000;
unsigned long timestamp_leitura_anterior = 0;

uint8_t mac_A[] = {0xFC, 0xE8, 0xC0, 0xE0, 0xFE, 0x80}; // Marinho normal
uint8_t mac_B[] = {0x80, 0xF3, 0xDA, 0x63, 0x7E, 0x48}; //Marinho d
uint8_t mac_C[] = {0x28, 0x56, 0x2F, 0x77, 0xAE, 0x00}; //Guilherme

// --- Definição das Estruturas ---
// Estrutura de recebimento (de A)
typedef struct struct_message_A {
uint8_t mac_A[6];
float temperatura_A;
float umidade_A;
int concen_gas_A;
unsigned long timestamp_A;
} struct_message_A;

// Estrutura de envio (para C) - MODIFICADA
typedef struct struct_message_B_C {
uint8_t mac_A[6];
float temperatura_A;
float umidade_A;
int concen_gas_A;
unsigned long timestamp_A;
uint8_t mac_B[6];
float temperatura_B;
float umidade_B;
int concen_gas_B; // Armazenará a porcentagem (0-100)
unsigned long timestamp_B;
} struct_message_B_C;


// --- Variáveis Globais ---
struct_message_A dadosRecebidosDeA;
struct_message_B_C dadosParaEnviarParaC; // Pacote completo para C

esp_now_peer_info_t peerInfo_C;

// --- Nova Função de Conversão ---
int converterParaPorcentagem(int valorAnalogico) {
    // Mapeia o valor de entrada (0 a MAX_ANALOG_READ) para o valor de saída (0 a 100)
    return map(valorAnalogico, 0, MAX_ANALOG_READ, 0, 100);
}
// ---------------------------------

// --- Callbacks ---

// Callback: Chamado quando B recebe dados (de A)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
 memcpy(&dadosRecebidosDeA, data, sizeof(dadosRecebidosDeA));
 Serial.print("Recebido de A -> Temp: ");
 Serial.println(dadosRecebidosDeA.temperatura_A);
 Serial.print("Recebido de A -> Umidade: ");
 Serial.println(dadosRecebidosDeA.umidade_A);
 Serial.print("Recebido de A -> Concentração de gás: ");
 Serial.println(dadosRecebidosDeA.concen_gas_A);
 memcpy(&dadosParaEnviarParaC, &dadosRecebidosDeA, sizeof(struct_message_A));
}

// Callback: Chamado quando B envia dados (para C)
void OnDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
 Serial.print("\r\nStatus Envio para C: ");
 Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sucesso" : "Falha");
}

void setup() {
 Serial.begin(115200);
 WiFi.mode(WIFI_STA);
 
  pinMode(PIN_LED_ENVIO, OUTPUT);
  pinMode(PIN_LED_ERRO, OUTPUT);
  // O pino PIN_MQ2B (18) será usado como entrada analógica. A declaração pinMode é opcional.
  

 WiFi.macAddress(mac_B);

 memcpy(dadosParaEnviarParaC.mac_B, mac_B, 6);

 dht.begin();
  
 if (esp_now_init() != ESP_OK) {
 Serial.println("Erro ao inicializar ESP-NOW");
 return;
 }

 esp_now_register_recv_cb(OnDataRecv);
 esp_now_register_send_cb(OnDataSent);

 memcpy(peerInfo_C.peer_addr, mac_C, 6);
 peerInfo_C.channel = 0; 
 peerInfo_C.encrypt = false;
 
 if (esp_now_add_peer(&peerInfo_C) != ESP_OK){
 Serial.println("Falha ao adicionar peer C");
 return;
 }
}

void lerSensores() {
int leitura_analogica_mq2B = analogRead(PIN_MQ2B); // Lê o valor bruto (0-4095)

dadosParaEnviarParaC.temperatura_B = dht.readTemperature();
dadosParaEnviarParaC.umidade_B = dht.readHumidity();
// Converte a leitura do potenciômetro para porcentagem
dadosParaEnviarParaC.concen_gas_B = converterParaPorcentagem(leitura_analogica_mq2B);
dadosParaEnviarParaC.timestamp_B = millis();
}

void atualizarStatusLocal() {
if (isnan(dadosParaEnviarParaC.umidade_B) || isnan(dadosParaEnviarParaC.temperatura_B)) {
 Serial.println("Falha ao ler do sensor DHT (B)!");
 }
Serial.println("       ");
Serial.print("Dados locais ");
Serial.print("Temperatura B: ");
Serial.print(dadosParaEnviarParaC.temperatura_B, 1);
Serial.print(" ºC | Umidade B: ");
Serial.print(dadosParaEnviarParaC.umidade_B, 1);
Serial.println("%");
Serial.print("Gás B (Simulado): ");
Serial.print(dadosParaEnviarParaC.concen_gas_B);
Serial.println("%"); // Adiciona o símbolo de porcentagem
}

void loop() {
 if (millis() - timestamp_leitura_anterior >= intervalo_leitura) {
 timestamp_leitura_anterior = millis();
 lerSensores();
 atualizarStatusLocal();
 
 esp_err_t result = esp_now_send(mac_C, (uint8_t *) &dadosParaEnviarParaC, sizeof(dadosParaEnviarParaC));

 if (result == ESP_OK) {
    Serial.print("Pacote combinado enviado para C.");
 } else {
 Serial.println("Erro ao enviar para C.");
 }
 
}
}
