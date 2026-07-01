# MQTT over WebSocket dengan ESP32, Mosquitto, dan Cloudflare Tunnel

Contoh ini menunjukkan cara menghubungkan ESP32 ke broker MQTT melalui **WebSocket** yang dipublikasikan memakai **Cloudflare Tunnel**.


## Arsitektur

<img width="1448" height="888" alt="Arsitektur Sistem" src="https://github.com/user-attachments/assets/6df7791e-58ec-4970-9ad1-250a33ce6722" />


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

## 2. Konfigurasi Cloudflare Tunnel melalui Web UI

Konfigurasi Cloudflare Tunnel dilakukan melalui Cloudflare Dashboard agar pembuatan tunnel, hostname publik, dan DNS record dapat dikelola dari antarmuka web.

1. Masuk ke **Cloudflare Dashboard**.

2. Buka menu:

   ```text
   Networking → Tunnels
   ```

   Pada beberapa akun atau tampilan Zero Trust, menu ini dapat berada pada:

   ```text
   Zero Trust → Networks → Connectors → Cloudflare Tunnels
   ```

3. Klik **Create Tunnel**.

4. Pilih tipe connector **Cloudflared**, lalu masukkan nama tunnel, misalnya:

   ```text
   mqtt-websocket
   ```

5. Pilih sistem operasi dan arsitektur server yang menjalankan Mosquitto. Cloudflare akan menampilkan perintah instalasi dan token connector.

6. Salin perintah yang diberikan Cloudflare lalu jalankan pada server. Contoh bentuk perintahnya:

   ```bash
   sudo cloudflared service install <TOKEN_DARI_CLOUDFLARE>
   ```

   Jangan membagikan token tersebut ke repository publik karena token digunakan untuk menghubungkan server ke tunnel Cloudflare.

7. Setelah connector aktif, kembali ke halaman tunnel lalu buka tab **Routes**.

8. Klik **Add route** kemudian pilih **Published application**.

9. Isi konfigurasi berikut:

   | Parameter    | Nilai                   |
   | ------------ | ----------------------- |
   | Hostname     | `mqtt.example.com`      |
   | Service type | `HTTP`                  |
   | URL          | `http://localhost:8000` |

   Ganti `mqtt.example.com` dengan subdomain milik Anda.

10. Klik **Add route** atau **Save hostname**.

Cloudflare akan membuat DNS record untuk hostname tersebut dan meneruskan koneksi HTTP maupun WebSocket ke service lokal Mosquitto pada port `8000`.

Arsitektur koneksinya menjadi:

```text
ESP32
  → ws://mqtt.example.com:80
  → Cloudflare Tunnel
  → http://localhost:8000
  → Mosquitto WebSocket Listener
```

Untuk penggunaan produksi, gunakan koneksi terenkripsi:

```text
wss://mqtt.example.com:443
```

Cloudflare Tunnel tetap diarahkan ke:

```text
http://localhost:8000
```

karena WebSocket dimulai sebagai koneksi HTTP lalu melakukan proses upgrade ke WebSocket pada listener Mosquitto.

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
