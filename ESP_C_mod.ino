#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>

unsigned long lastReceivedTime = 0;   // Quando o último pacote foi recebido
unsigned long lastSendTime = 0;       // Controle do envio periódico
const long SEND_INTERVAL_MS = 10000;  // Enviar mock a cada 10s

const char* SERVER_URL = "http://10.55.46.131:8000/dados";
const char* ssid = "Galaxy J8F22C";
const char* password = "snhm6588";

// --- Estrutura de dados enviada pelo ESP-B ---
typedef struct struct_message_B_C {
  uint8_t mac_A[6];
  float temperatura_A;
  float umidade_A;
  int   concen_gas_A;
  unsigned long timestamp_A;

  uint8_t mac_B[6];
  float temperatura_B;
  float umidade_B;
  int   concen_gas_B;
  unsigned long timestamp_B;

} struct_message_B_C;

volatile bool dataReceivedFlag = false;
struct_message_B_C data; // buffer global

// ----------------------- WIFI -----------------------
void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("🔌 Conectando ao WiFi");
  int tentativas = 0;

  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Falha ao conectar ao WiFi!");
  }
}

// ----------------------- UTILITÁRIOS -----------------------
String macToString(const uint8_t *mac) {
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

void printMAC(const uint8_t *mac_addr) {
  Serial.print(macToString(mac_addr));
}

// ----------------------- CALLBACK ESP-NOW -----------------------
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data_recv, int len) {

  if (len == sizeof(struct_message_B_C)) {
    memcpy(&data, data_recv, sizeof(data));
    dataReceivedFlag = true;
    lastReceivedTime = millis();   // ⚠ IMPORTANTE: impede HTTP imediato

    Serial.println("\n📡 Dados Recebidos via ESP-NOW");

    // LOG (seguro, pois é rápido)
    Serial.println("===== PACOTE COMBINADO RECEBIDO =====");
    Serial.print("Origem A - MAC: "); printMAC(data.mac_A); Serial.println();
    Serial.printf(" Temp(A): %.2f Hum(A): %.2f Gas(A): %d TS(A): %lu\n",
                  data.temperatura_A, data.umidade_A,
                  data.concen_gas_A, data.timestamp_A);

    Serial.print("Origem B - MAC: "); printMAC(data.mac_B); Serial.println();
    Serial.printf(" Temp(B): %.2f Hum(B): %.2f Gas(B): %d TS(B): %lu\n",
                  data.temperatura_B, data.umidade_B,
                  data.concen_gas_B, data.timestamp_B);
    Serial.println("======================================");

  } else {
    Serial.println("❌ Pacote inválido!");
  }
}

// ----------------------- ENVIO HTTP -----------------------
void enviarParaServidor(struct_message_B_C d) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi desconectado. Reconnecting...");
    conectarWiFi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("❌ Falha ao reconectar. Abortando envio.");
      return;
    }
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"temperatura_A\":" + String(d.temperatura_A, 2) + ",";
  json += "\"umidade_A\":" + String(d.umidade_A, 2) + ",";
  json += "\"concen_gas_A\":" + String(d.concen_gas_A) + ",";
  json += "\"timestamp_A\":" + String(d.timestamp_A) + ",";
  json += "\"temperatura_B\":" + String(d.temperatura_B, 2) + ",";
  json += "\"umidade_B\":" + String(d.umidade_B, 2) + ",";
  json += "\"concen_gas_B\":" + String(d.concen_gas_B) + ",";
  json += "\"timestamp_B\":" + String(d.timestamp_B);
  json += "}";

  Serial.print("\n➡️ Enviando JSON: ");
  Serial.println(json);

  int code = http.POST(json);

  Serial.print(" Código HTTP: ");
  Serial.println(code);

  if (code > 0) {
    Serial.print(" Resposta: ");
    Serial.println(http.getString());
  }

  http.end();
}

// ----------------------- MOCK DATA (para envio periódico) -----------------------
struct_message_B_C criarMockData() {
  struct_message_B_C m;

  uint8_t A[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
  memcpy(m.mac_A, A, 6);

  m.temperatura_A = 25.45;
  m.umidade_A = 62.10;
  m.concen_gas_A = 450;
  m.timestamp_A = 1678886400000UL;

  uint8_t B[] = {0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5};
  memcpy(m.mac_B, B, 6);

  m.temperatura_B = 27.88;
  m.umidade_B = 58.99;
  m.concen_gas_B = 620;
  m.timestamp_B = 1678886405123UL;

  return m;
}

// ----------------------- SETUP -----------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  conectarWiFi();

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  lastReceivedTime = millis();
  lastSendTime = millis();
}

// ----------------------- LOOP -----------------------
void loop() {
  unsigned long now = millis();

  // ✔ 1. PROCESSAR PACOTE DO ESP-NOW APÓS DELAY DE SEGURANÇA
  if (dataReceivedFlag && (now - lastReceivedTime > 30)) {   // ⏳ Delay evita erro -1
    Serial.println("🚀 Enviando via HTTP (evento ESP-NOW)");
    enviarParaServidor(data);
    dataReceivedFlag = false;
    lastSendTime = now;
  }

  // ✔ 2. ENVIO PERIÓDICO A CADA 10s
  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    Serial.println("🕒 Envio periódico (mock)");
    enviarParaServidor(criarMockData());
    lastSendTime = now;
  }
}
