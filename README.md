# MQTT over WebSocket dengan ESP32, Mosquitto, dan Cloudflare Tunnel

Contoh ini menunjukkan cara menghubungkan ESP32 ke broker MQTT melalui **WebSocket** yang dipublikasikan memakai **Cloudflare Tunnel**.


## Arsitektur

```text
ESP32 + DHT22
     |
     | MQTT over WebSocket (ws://hostname:80)
     v
Cloudflare Edge / Public Hostname
     |
     | Cloudflare Tunnel
     v
cloudflared pada gateway
     |
     | HTTP + WebSocket upgrade ke localhost:8000
     v
Mosquitto listener :8000 (protocol websockets)
```

WebSocket diawali sebagai permintaan HTTP dengan mekanisme *upgrade*. Karena itu `cloudflared` dapat menggunakan service `http://localhost:8000` untuk meneruskan koneksi ke listener Mosquitto berbasis WebSocket.

## Struktur berkas

```text
.
├── README.md
├── esp32/
│   └── ESP32_MQTT_WebSocket.ino
├── mosquitto/
│   └── mosquitto-websocket.conf.example
├── cloudflared/
│   └── config.yml.example
└── tools/
    └── test-local.sh
```

## Prasyarat

### Gateway / server broker

- Eclipse Mosquitto dengan dukungan WebSocket.
- `cloudflared` yang telah diautentikasi ke akun Cloudflare.
- Domain atau subdomain yang dikelola melalui Cloudflare.

### ESP32

- Board ESP32 pada Arduino IDE.
- Library **DHT sensor library** oleh Adafruit.
- Library **Adafruit Unified Sensor**.

## 1. Konfigurasi Mosquitto

Salin isi `mosquitto/mosquitto-websocket.conf.example` sebagai berkas konfigurasi tambahan, misalnya:

```bash
sudo nano /etc/mosquitto/conf.d/websocket.conf
```

Contoh konfigurasi:

```conf
# Listener MQTT biasa. Pertahankan bila aplikasi lokal memakai MQTT TCP.
listener 1883 127.0.0.1
protocol mqtt

# Listener khusus MQTT over WebSocket.
# Dibatasi ke localhost karena akses publik diberikan hanya melalui Cloudflare Tunnel.
listener 8000 127.0.0.1
protocol websockets

# Jangan membiarkan broker publik tanpa autentikasi.
listener_allow_anonymous false
password_file /etc/mosquitto/passwd

persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
```

Buat akun MQTT:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32-node
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
sudo ss -ltnp | grep ':8000'
```

Jika Mosquitto gagal saat dijalankan, periksa log berikut:

```bash
sudo journalctl -u mosquitto -n 100 --no-pager
```

> Bila broker Anda sudah mempunyai listener `1883`, **jangan menduplikasinya**. Tambahkan hanya listener WebSocket `8000` dan sesuaikan aturan autentikasi dengan konfigurasi broker yang sudah ada.

## 2. Konfigurasi Cloudflare Tunnel

Buat tunnel dan DNS route. Ganti nama tunnel dan hostname sesuai milik Anda.

```bash
cloudflared tunnel create mqtt-websocket
cloudflared tunnel route dns mqtt-websocket mqtt.example.com
```

Simpan `cloudflared/config.yml.example` sebagai `/etc/cloudflared/config.yml` lalu isi UUID tunnel dan lokasi file kredensial yang dibuat Cloudflare.

```yaml
tunnel: <TUNNEL_UUID>
credentials-file: /etc/cloudflared/<TUNNEL_UUID>.json

ingress:
  - hostname: mqtt.example.com
    service: http://localhost:8000
  - service: http_status:404
```

Jalankan tunnel untuk pengujian:

```bash
sudo cloudflared --config /etc/cloudflared/config.yml tunnel run mqtt-websocket
```

Setelah berhasil, jalankan sebagai service sesuai metode instalasi `cloudflared` pada server Anda. Pantau log saat pengujian:

```bash
sudo journalctl -u cloudflared -f
```

## 3. Konfigurasi sketch ESP32

Buka `esp32/ESP32_MQTT_WebSocket.ino`, kemudian ubah bagian berikut:

```cpp
const char* WIFI_SSID     = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";

const char* MQTT_URI      = "ws://mqtt.example.com:80";
const char* MQTT_USERNAME = "esp32-node";
const char* MQTT_PASSWORD = "PASSWORD_MQTT";

const char* NODE_ID       = "node-01";
```

Upload sketch ke ESP32 dan buka Serial Monitor pada **115200 baud**. Saat koneksi berhasil, serial monitor akan menampilkan:

```text
WiFi connected. IP: ...
MQTT connected
Subscribed: sensors/node-01/command
Published: {"node":"node-01", ...}
```

Data dikirim setiap 10 detik ke:

```text
sensors/node-01/data
```

ESP32 juga berlangganan ke:

```text
sensors/node-01/command
```

## 4. Uji publish dan subscribe

Pada gateway, gunakan listener MQTT TCP lokal untuk melihat pesan yang masuk dari ESP32 WebSocket:

```bash
mosquitto_sub -h 127.0.0.1 -p 1883 \
  -u esp32-node -P 'PASSWORD_MQTT' \
  -t 'sensors/#' -v
```

Kirim perintah balik ke ESP32:

```bash
mosquitto_pub -h 127.0.0.1 -p 1883 \
  -u esp32-node -P 'PASSWORD_MQTT' \
  -t 'sensors/node-01/command' \
  -m '{"action":"ping"}'
```

Perintah akan muncul di Serial Monitor ESP32.

## Catatan keamanan

Contoh utama memakai `ws://...:80` karena mengikuti konfigurasi yang terbukti berjalan pada implementasi awal. Untuk penggunaan jaringan publik atau produksi, gunakan **`wss://...:443`** agar lalu lintas ESP32 ke Cloudflare terenkripsi. Implementasi `wss` membutuhkan validasi sertifikat CA pada ESP32, misalnya dengan certificate bundle atau CA certificate yang sesuai.

Jangan membuka listener port `8000` langsung ke internet. Ikat ke `127.0.0.1` dan biarkan Cloudflare Tunnel menjadi satu-satunya jalur publik. Gunakan autentikasi Mosquitto dan kata sandi yang berbeda untuk setiap lingkungan. Internet sudah cukup gaduh tanpa broker anonim yang mengundang bot iseng masuk.

## Troubleshooting

| Gejala | Penyebab yang umum | Pemeriksaan |
|---|---|---|
| `MQTT disconnected` terus menerus | hostname, DNS, atau Cloudflare Tunnel belum aktif | cek `cloudflared` log dan DNS hostname |
| Error saat WebSocket handshake | listener `8000` bukan `protocol websockets` atau Mosquitto tidak mendukung WebSocket | cek `/etc/mosquitto/conf.d/websocket.conf` dan log Mosquitto |
| Cloudflare `502 Bad Gateway` | `cloudflared` tidak dapat mencapai `localhost:8000` | jalankan `ss -ltnp | grep ':8000'` |
| Koneksi berhasil, tetapi publish ditolak | username/password salah atau ACL membatasi topic | cek `password_file`, kredensial sketch, dan ACL |
| Tidak ada data di subscriber | topic yang dipantau tidak sesuai | gunakan `-t 'sensors/#'` saat pengujian |
| ESP32 gagal terhubung setelah pindah ke `wss://` | sertifikat CA belum tersedia pada firmware | pasang certificate bundle atau CA Cloudflare yang relevan |

## Lisensi

MIT License. Sesuaikan nama penulis sebelum dipublikasikan.
