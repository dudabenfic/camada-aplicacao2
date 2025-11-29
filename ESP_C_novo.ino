#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ====================== CONFIG WIFI / API ======================
#define WIFI_SSID      "CIMATEC-VISITANTE"
#define WIFI_PASSWORD  ""

// API FastAPI (POST /dados)
#define API_HOST   "192.168.0.50"   // <- IP/hostname onde roda sua API (NÃO use "influxdb")
#define API_PORT   8000             // 8000 se exposto como no compose abaixo
#define API_PATH   "/dados"
// ===============================================================

// --- Estrutura exatamente como você forneceu ---
typedef struct struct_message_B_C {
  uint8_t mac_A[6];
  float temperatura_A;
  float umidade_A;
  int   concen_gas_A;
  unsigned long timestamp_A;  // esperado em ms unix
  uint8_t mac_B[6];
  float temperatura_B;
  float umidade_B;
  int   concen_gas_B;
  unsigned long timestamp_B;  // esperado em ms unix
} struct_message_B_C;

volatile bool temPacoteNovo = false;
struct_message_B_C bufferPacote;   // buffer usado no loop para enviar
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Utilitário: MAC para string
String macToString(const uint8_t *mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

// Apenas imprime MAC no Serial (debug)
void printMAC(const uint8_t *mac_addr) {
  Serial.print(macToString(mac_addr));
}

// Callback: chamado quando C recebe dados (de B)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len < (int)sizeof(struct_message_B_C)) {
    Serial.printf("Pacote menor que esperado (%d < %d)\n", len, (int)sizeof(struct_message_B_C));
    return;
  }
  portENTER_CRITICAL(&mux);
  memcpy((void*)&bufferPacote, data, sizeof(bufferPacote));
  temPacoteNovo = true;
  portEXIT_CRITICAL(&mux);

  // Log básico (não faz HTTP aqui!)
  Serial.println("===== PACOTE COMBINADO RECEBIDO DE B =====");
  Serial.print("Origem A - MAC: "); printMAC(bufferPacote.mac_A); Serial.println();
  Serial.printf("  Temp(A): %.2f  Humid(A): %.2f  Gas(A): %d  TS(A): %lu\n",
                bufferPacote.temperatura_A, bufferPacote.umidade_A,
                bufferPacote.concen_gas_A, bufferPacote.timestamp_A);
  Serial.println("------------------------------------------");
  Serial.print("Origem B - MAC: "); printMAC(bufferPacote.mac_B); Serial.println();
  Serial.printf("  Temp(B): %.2f  Humid(B): %.2f  Gas(B): %d  TS(B): %lu\n",
                bufferPacote.temperatura_B, bufferPacote.umidade_B,
                bufferPacote.concen_gas_B, bufferPacote.timestamp_B);
  Serial.println("==========================================");
}

// Faz POST JSON para a API
bool postLeitura(const char* origem, const String& macStr, float temperatura, float umidade, float gas, unsigned long ts_ms) {
  HTTPClient http;
  String url = String("http://") + API_HOST + ":" + String(API_PORT) + String(API_PATH);
  if (!http.begin(url)) {
    Serial.println("Falha em http.begin()");
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  // Monta JSON (simples, sem ArduinoJson)
  // timestamp é opcional; se vier 0, servidor usará UTC atual
  String body = "{";
  body += "\"temperatura\":" + String(temperatura, 6) + ",";
  body += "\"umidade\":"     + String(umidade, 6)     + ",";
  body += "\"gas\":"         + String(gas, 6)         + ",";
  if (ts_ms > 0) {
    body += "\"timestamp\":" + String(ts_ms) + ",";
  }
  body += "\"origem\":\"" + String(origem) + "\",";
  body += "\"mac\":\"" + macStr + "\"";
  body += "}";

  int code = http.POST(body);
  String resp = http.getString();
  http.end();

  Serial.printf("[POST %s] code=%d resp=%s\n", origem, code, resp.c_str());
  return (code >= 200 && code < 300);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // WiFi STA para ESP-NOW + HTTP
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true); // limpa estado
  // Conecta WiFi para conseguir fazer HTTP (ainda que ESP-NOW funcione sem AP)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi NAO conectou (seguirei tentando enviar quando conectar).");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Se houver pacote novo, envia dois POSTs (A e B)
  if (temPacoteNovo) {
    struct_message_B_C localCopy;

    // copia sob mutex e limpa flag
    portENTER_CRITICAL(&mux);
    memcpy(&localCopy, &bufferPacote, sizeof(localCopy));
    temPacoteNovo = false;
    portEXIT_CRITICAL(&mux);

    // Se WiFi caiu, tente reconectar (para o HTTP)
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      unsigned long t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
        delay(250);
      }
    }

    // Envia A
    String macA = macToString(localCopy.mac_A);
    postLeitura("A", macA, localCopy.temperatura_A, localCopy.umidade_A,
                (float)localCopy.concen_gas_A, localCopy.timestamp_A);

    // Envia B
    String macB = macToString(localCopy.mac_B);
    postLeitura("B", macB, localCopy.temperatura_B, localCopy.umidade_B,
                (float)localCopy.concen_gas_B, localCopy.timestamp_B);
  }

  delay(50);
}
