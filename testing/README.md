# HiveMQ WebSocket Client Test

Pengujian dilakukan menggunakan [HiveMQ WebSocket Client](https://www.hivemq.com/demos/websocket-client/).

## Koneksi

| Parameter | Nilai |
|---|---|
| Host | `gateway.tobafarm.my.id` |
| Port | `443` |
| SSL | Aktif |
| Client ID | Bebas dan unik |

Browser terhubung melalui:

```text
wss://gateway.tobafarm.my.id:443
→ Cloudflare Tunnel
→ http://localhost:8000
→ Mosquitto WebSocket listener
```

Publish Test

Dari HiveMQ WebSocket Client, pesan dikirim dengan konfigurasi:

Topic   : coba_mqtt_over_websocket
Message : Percobaan Berhasil
QoS     : 0

Pada gateway, subscriber lokal dijalankan dengan:

mosquitto_sub -t "coba_mqtt_over_websocket" -v

Pesan berhasil diterima oleh Mosquitto lokal:

Pengujian ini membuktikan bahwa client browser dapat melakukan publish MQTT melalui WebSocket dan Cloudflare Tunnel ke Mosquitto Broker lokal.
