# HiveMQ WebSocket Client Test

Testing was conducted using [HiveMQ WebSocket Client](https://www.hivemq.com/demos/websocket-client/).

## Connection

| Parameter | Value |
|---|---|
| Host | `mqtt.example.com` |
| Port | `443` |
| SSL | Aktif |
| Client ID | Bebas dan unik |

The browser connects via:

```text
wss://mqtt.example.com:443
→ Cloudflare Tunnel
→ http://localhost:8000
→ Mosquitto WebSocket listener
```

### Publish Test

<img width="1270" height="791" alt="image" src="https://github.com/user-attachments/assets/fb7e78ae-65e1-4197-8e18-a335a66019fa" />

From the HiveMQ WebSocket Client, the message is sent with the following configuration:
```text
Topic   : coba_mqtt_over_websocket
Message : Percobaan Berhasil
QoS     : 0
```

On the gateway, the local subscriber is run with:
```text
mosquitto_sub -t "coba_mqtt_over_websocket" -v
```

### Message successfully received by the local Mosquitto broker :

<img width="638" height="120" alt="image" src="https://github.com/user-attachments/assets/907472c4-7cd2-43a4-84f5-8e21a9e4a634" />


This test proves that a browser client can publish MQTT messages over WebSocket and Cloudflare Tunnel to the local Mosquitto Broker.
