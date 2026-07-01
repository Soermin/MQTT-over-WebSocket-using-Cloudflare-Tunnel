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
wss://domain.anda.com:443
→ Cloudflare Tunnel
→ http://localhost:8000
→ Mosquitto WebSocket listener
```

### Publish Test

<img width="1270" height="791" alt="image" src="https://github.com/user-attachments/assets/fb7e78ae-65e1-4197-8e18-a335a66019fa" />

Dari HiveMQ WebSocket Client, pesan dikirim dengan konfigurasi:
```text
Topic   : coba_mqtt_over_websocket
Message : Percobaan Berhasil
QoS     : 0
```

Pada gateway, subscriber lokal dijalankan dengan:
```text
mosquitto_sub -t "coba_mqtt_over_websocket" -v
```

### Pesan berhasil diterima oleh Mosquitto lokal:

<img width="638" height="120" alt="image" src="https://github.com/user-attachments/assets/907472c4-7cd2-43a4-84f5-8e21a9e4a634" />


Pengujian ini membuktikan bahwa client browser dapat melakukan publish MQTT melalui WebSocket dan Cloudflare Tunnel ke Mosquitto Broker lokal.
