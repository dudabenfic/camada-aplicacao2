from fastapi import FastAPI, Request
from influxdb_client import InfluxDBClient, Point, WritePrecision
import os
from datetime import datetime

app = FastAPI()

# ⚙️ Configuração do InfluxDB
url = os.getenv("INFLUXDB_URL", "http://influxdb:8086")
token = os.getenv("INFLUXDB_TOKEN", "dT_Ot6Wm9zgDGRMIV7PTlcz6k5RXTrQAcdhhdqRpmml4bbRGmLpc1LZqcvpQwRFEZ1wRyY6F3wdV5IbTNJkCSw==")
org = os.getenv("INFLUXDB_ORG", "meu_org")
bucket = os.getenv("INFLUXDB_BUCKET", "meu_bucket")

client = InfluxDBClient(url=url, token=token, org=org)
write_api = client.write_api()

@app.post("/dados")
async def receber_dados(data: dict):
    try:
        # Converte timestamp do JSON (ms) para datetime
        ts = datetime.fromtimestamp(data["timestamp"] / 1000)

        point = (
            Point("sensores")
            .tag("origem", "esp32")
            .field("temperatura_A", float(data["temperatura_A"]))
            .field("umidade_A", float(data["umidade_A"]))
            .field("concen_gas_A", int(data["concen_gas_A"]))
            .field("temperatura_B", float(data["temperatura_B"]))
            .field("umidade_B", float(data["umidade_B"]))
            .field("concen_gas_B", int(data["concen_gas_B"]))
            .time(datetime.utcnow(), write_precision=WritePrecision.MS) 
        )

        write_api.write(bucket=bucket, org=org, record=point)
        return {"status": "success", "dados": data}
    except Exception as e:
        return {"status": "error", "detalhe": str(e)}
