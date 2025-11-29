# camada-aplicacao2

Camada de Aplicação 2

Projeto de integração: ESP32 → ESP-NOW → ESP32 → ESP32 → Backend (FastAPI) → InfluxDB → Grafana
Repositório: adicione seu link aqui

1. Como montar o projeto
Pré-requisitos:

Docker e Docker Compose instalados

Python 3.10+ (opcional caso queira rodar FastAPI fora do Docker)

Arduino IDE ou PlatformIO para programar os ESP32

Rede WiFi local para o ESP “coletor final” (ESP-C)

3 ESP32:

ESP-A (sensor primário)

ESP-B (receptor → agregador → retransmissor ESP-NOW)

ESP-C (coletor final → WiFi → HTTP POST → backend)

2. Hardware / ESP32
ESP-A (Sensor → Emissor ESP-NOW)

Este ESP32 coleta dados brutos dos sensores e envia via ESP-NOW para o ESP-B.

Sensores utilizados (exemplo):

DHT11 — temperatura e umidade

MQ-X — concentração de gás

Outros sensores opcionais

Passos de configuração:

Conecte cada sensor aos pinos definidos no código.

Atualize no código o MAC Address do ESP-B:

uint8_t mac_b[] = { ... };


Faça upload via Arduino IDE ou PlatformIO.

ESP-B (Receptor A → Processador → Reemissor para ESP-C)

Este ESP32 recebe os dados do ESP-A, combina com os dados próprios e envia tudo ao ESP-C via ESP-NOW.

Funções:

Receber pacote do ESP-A

Ler sensores próprios (temperatura, umidade, gás, etc.)

Montar um pacote combinado (A + B)

Enviar para o ESP-C via ESP-NOW

LEDs de status opcionais

Passos:

Atualize o MAC do ESP-C no array:

uint8_t mac_c[] = { ... };


Faça upload.

ESP-C (Receptor final → WiFi → HTTP POST)

Este ESP recebe os pacotes combinados de A + B e envia via HTTP POST para o backend.

Funções:

Receber dados ESP-NOW (estrutura struct_message_B_C)

Conectar ao WiFi

Montar JSON

Realizar POST para o FastAPI

Envio periódico de mock (10s)

Reconexão automática ao WiFi

Atenção:
O ESP-C DEVE enviar para o IPv4 da máquina que está rodando o FastAPI.

Exemplo de URL:

http://192.168.0.10:8000/dados


No código (ESP-C):

const char* SERVER_URL = "http://SEU_IPV4_LOCAL:8000/dados";


Para descobrir seu IP:

Windows: ipconfig

Linux/Mac: ifconfig

3. Como rodar o backend

O backend é composto por:

FastAPI (recebe dados via POST)

InfluxDB 2.0 (banco time-series)

Grafana (visualização)

Tudo sobe automaticamente via Docker.

Passo 1 — Iniciar containers

No diretório do projeto:

docker-compose up -d


Isso irá inicializar:

Container influxdb (porta 8086)

Configurações automáticas (docker-compose):

User: admin

Password: 12345678

Org: meu_org

Bucket: meu_bucket

Container fastapi (porta 8000)

Endpoint principal:

POST /dados


O backend recebe:

temperatura_A

umidade_A

concen_gas_A

timestamp_A

temperatura_B

umidade_B

concen_gas_B

timestamp_B

Cada registro é salvo no InfluxDB como measurement: sensores.

Container grafana (porta 3030 → 3000)

A interface do Grafana ficará disponível em:

http://localhost:3030

4. Como acessar o InfluxDB

Abra no navegador:

http://localhost:8086


Login:

Usuário: admin

Senha: 12345678

Acesse Data Explorer:

Selecione:

Bucket: meu_bucket

Measurement: sensores

Campos disponíveis:

temperatura_A

umidade_A

concen_gas_A

temperatura_B

umidade_B

concen_gas_B

timestamp

Visualize em tabela ou gráfico.

5. Como testar o backend manualmente (sem ESP)

Você pode enviar dados fictícios usando curl:

curl -X POST http://192.168.0.10:8000/dados \
-H "Content-Type: application/json" \
-d '{
  "temperatura_A": 22.5,
  "umidade_A": 60,
  "concen_gas_A": 450,
  "timestamp_A": 1700000000000,
  "temperatura_B": 23.8,
  "umidade_B": 58,
  "concen_gas_B": 620,
  "timestamp_B": 1700000000500
}'


Resposta esperada:

{
  "status": "success",
  "dados": {
    "temperatura_A": 22.5,
    "umidade_A": 60,
    "concen_gas_A": 450,
    "timestamp_A": 1700000000000,
    "temperatura_B": 23.8,
    "umidade_B": 58,
    "concen_gas_B": 620,
    "timestamp_B": 1700000000500
  }
}

6. Como consultar os dados no Grafana

Acesse:

http://localhost:3030


Login padrão:

user: admin

pass: admin

Adicione o InfluxDB como data source.

Escolha o bucket meu_bucket.

Crie dashboards usando os campos:

temperatura_A / B

umidade_A / B

concen_gas_A / B

timestamps
