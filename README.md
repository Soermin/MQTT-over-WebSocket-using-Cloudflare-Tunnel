# MQTT over WebSocket with ESP32, Mosquitto, and Cloudflare Tunnel

This example shows how to connect an ESP32 to an MQTT broker via **WebSocket**, published using **Cloudflare Tunnel**.


## Architecture

<img width="1448" height="888" alt="System Architecture" src="https://github.com/user-attachments/assets/6df7791e-58ec-4970-9ad1-250a33ce6722" />


WebSocket begins as an HTTP request with an *upgrade* mechanism. Because of that, `cloudflared` can use the `http://localhost:8000` service to forward the connection to the WebSocket-based Mosquitto listener.

## File structure

```text
.
├── README.md
├── esp32/
│   └── ESP32_MQTT_WebSocket.ino
├── mosquitto/
│   └── mosquitto-websocket.conf.example
└── tools/
    └── test-local.sh
```

## Prerequisites

### Gateway / broker server

- Eclipse Mosquitto with WebSocket support.
- `cloudflared` authenticated to a Cloudflare account.
- A domain or subdomain managed through Cloudflare.

### ESP32

- ESP32 board on Arduino IDE.
- **DHT sensor library** by Adafruit.
- **Adafruit Unified Sensor** library.

## 1. Mosquitto Configuration

Copy the contents of `mosquitto/mosquitto-websocket.conf.example` as an additional configuration file, for example:

```bash
sudo nano /etc/mosquitto/conf.d/websocket.conf
```

Example configuration:

```conf
# Regular MQTT listener. Keep this if local applications use MQTT TCP.
listener 1883 127.0.0.1
protocol mqtt

# Dedicated MQTT over WebSocket listener.
# Restricted to localhost because public access is only provided via Cloudflare Tunnel.
listener 8000 127.0.0.1
protocol websockets

# Don't leave the broker public without authentication.
listener_allow_anonymous false
password_file /etc/mosquitto/passwd

persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
```

Create an MQTT account:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32-node
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
sudo ss -ltnp | grep ':8000'
```

If Mosquitto fails to start, check the following log:

```bash
sudo journalctl -u mosquitto -n 100 --no-pager
```

> If your broker already has a `1883` listener, **do not duplicate it**. Only add the `8000` WebSocket listener and adjust the authentication rules to match your existing broker configuration.

## 2. Cloudflare Tunnel Configuration via Web UI

Cloudflare Tunnel configuration is done through the Cloudflare Dashboard so that tunnel creation, public hostname, and DNS records can be managed from the web interface.

1. Log in to the **Cloudflare Dashboard**.

2. Open the menu:

   ```text
   Networking → Tunnels
   ```

   On some accounts or Zero Trust views, this menu may be located at:

   ```text
   Zero Trust → Networks → Connectors → Cloudflare Tunnels
   ```

3. Click **Create Tunnel**.

4. Choose the **Cloudflared** connector type, then enter a tunnel name, for example:

   ```text
   mqtt-websocket
   ```

5. Select the operating system and architecture of the server running Mosquitto. Cloudflare will display the installation command and connector token.

6. Copy the command provided by Cloudflare and run it on the server. Example command format:

   ```bash
   sudo cloudflared service install <TOKEN_FROM_CLOUDFLARE>
   ```

   Do not share this token in a public repository, as the token is used to connect the server to the Cloudflare tunnel.

7. Once the connector is active, return to the tunnel page and open the **Routes** tab.

8. Click **Add route** then select **Published application**.

9. Fill in the following configuration:

   | Parameter    | Value                    |
   | ------------ | ----------------------- |
   | Hostname     | `mqtt.example.com`      |
   | Service type | `HTTP`                  |
   | URL          | `http://localhost:8000` |

   Replace `mqtt.example.com` with your own subdomain.

10. Click **Add route** or **Save hostname**.

Cloudflare will create a DNS record for that hostname and forward both HTTP and WebSocket connections to the local Mosquitto service on port `8000`.

The resulting connection architecture becomes:

```text
ESP32
  → ws://mqtt.example.com:80
  → Cloudflare Tunnel
  → http://localhost:8000
  → Mosquitto WebSocket Listener
```

For production use, use an encrypted connection:

```text
wss://mqtt.example.com:443
```

Cloudflare Tunnel is still directed to:

```text
http://localhost:8000
```

because WebSocket starts as an HTTP connection and then performs an upgrade process to WebSocket on the Mosquitto listener.

## 3. ESP32 Sketch Configuration

Open `esp32/ESP32_MQTT_WebSocket.ino`, then modify the following section:

```cpp
const char* WIFI_SSID     = "WIFI_NAME";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";

const char* MQTT_URI      = "ws://mqtt.example.com:80";
const char* MQTT_USERNAME = "esp32-node";
const char* MQTT_PASSWORD = "MQTT_PASSWORD";

const char* NODE_ID       = "node-01";
```

Upload the sketch to the ESP32 and open the Serial Monitor at **115200 baud**. When the connection succeeds, the serial monitor will display:

```text
WiFi connected. IP: ...
MQTT connected
Subscribed: sensors/node-01/command
Published: {"node":"node-01", ...}
```

Data is sent every 10 seconds to:

```text
sensors/node-01/data
```

The ESP32 also subscribes to:

```text
sensors/node-01/command
```

## 4. Publish and Subscribe Test

On the gateway, use the local MQTT TCP listener to view incoming messages from the ESP32 WebSocket:

```bash
mosquitto_sub -h 127.0.0.1 -p 1883 \
  -u esp32-node -P 'MQTT_PASSWORD' \
  -t 'sensors/#' -v
```

Send a command back to the ESP32:

```bash
mosquitto_pub -h 127.0.0.1 -p 1883 \
  -u esp32-node -P 'MQTT_PASSWORD' \
  -t 'sensors/node-01/command' \
  -m '{"action":"ping"}'
```

The command will appear on the ESP32 Serial Monitor.

## Additional Testing

MQTT over WebSocket browser-based testing is available at [testing](testing/README.md).

## Security Notes

The main example uses `ws://...:80` because it follows a configuration proven to work in the initial implementation. For public network or production use, use **`wss://...:443`** to encrypt traffic between the ESP32 and Cloudflare. Implementing `wss` requires CA certificate validation on the ESP32, for example with a certificate bundle or an appropriate CA certificate.

Do not open the `8000` port listener directly to the internet. Bind it to `127.0.0.1` and let Cloudflare Tunnel be the only public pathway. Use Mosquitto authentication and different passwords for each environment. The internet is already noisy enough without an anonymous broker inviting random bots in.

## Troubleshooting

| Symptom | Common cause | Check |
|---|---|---|
| `MQTT disconnected` repeatedly | hostname, DNS, or Cloudflare Tunnel not active | check `cloudflared` log and DNS hostname |
| Error during WebSocket handshake | listener `8000` is not `protocol websockets` or Mosquitto doesn't support WebSocket | check `/etc/mosquitto/conf.d/websocket.conf` and Mosquitto log |
| Cloudflare `502 Bad Gateway` | `cloudflared` cannot reach `localhost:8000` | run `ss -ltnp | grep ':8000'` |
| Connection succeeds, but publish is rejected | wrong username/password or ACL restricting the topic | check `password_file`, sketch credentials, and ACL |
| No data at the subscriber | monitored topic doesn't match | use `-t 'sensors/#'` during testing |
| ESP32 fails to connect after switching to `wss://` | CA certificate not available in firmware | install the relevant Cloudflare CA or certificate bundle |
