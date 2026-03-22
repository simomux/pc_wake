# pc-remote

Wake your Windows PC remotely via Arduino + Raspberry Pi + Tailscale, then connect automatically via RDP.

```
MacBook (Tailscale)
  → Pi Zero 2W (Tailscale subnet router)
    → Arduino R4 WiFi (LAN HTTP server)
      → transistor relay → motherboard power button
  → RDP (opened automatically once PC is up)
```

## Project structure

```
pc-remote/
├── .env.example                 # copy to .env and fill in your values
├── arduino/main/
│   ├── config.h.example         # copy to config.h and fill in your values
│   └── main.ino                 # HTTP server + power button relay
├── pi/
│   ├── app.py                   # Flask gateway: forwards power pulse, pings PC until up
│   ├── requirements.txt
│   └── install.sh               # automated setup: Tailscale + Flask + systemd
└── scripts/
    └── wake.sh                  # Mac script: wake PC + open RDP automatically
```

## How it works

1. `./scripts/wake.sh` calls `POST /wake` on the Pi over Tailscale
2. Pi tells the Arduino to pulse the power button
3. Pi pings the Windows PC every 3s until it responds (up to 90s)
4. Once the PC is up, `wake.sh` opens RDP automatically

## Setup

### 1. Configure secrets

```bash
cp .env.example .env
# fill in ARDUINO_IP, WIN_PC_IP, and PI_TAILSCALE_IP
```

### 2. Arduino

```bash
cp arduino/main/config.h.example arduino/main/config.h
# fill in WIFI_SSID, WIFI_PASSWORD, and the static IP octets
```

Open `arduino/main/main.ino` in Arduino IDE and upload. Verify:

```bash
curl http://<arduino-ip>/status   # → {"status":"ok"}
```

### 3. Raspberry Pi Zero 2W

Copy the repo to the Pi and run the setup script:

```bash
scp -r . pi@<pi-local-ip>:~/pc-remote
ssh pi@<pi-local-ip>
cd ~/pc-remote && sudo bash pi/install.sh
```

Then bring up Tailscale as a subnet router:

```bash
sudo tailscale up --advertise-routes=192.168.1.0/24 --accept-dns=false
```

Approve the subnet route at [Tailscale Admin → Machines](https://login.tailscale.com/admin/machines).

Add the Pi's Tailscale IP to `.env` → `PI_TAILSCALE_IP`.

### 4. Wake the PC (from anywhere)

```bash
chmod +x scripts/wake.sh
./scripts/wake.sh           # power pulse + wait for PC + open RDP
./scripts/wake.sh power     # power button pulse only
./scripts/wake.sh status    # check Pi and Arduino connectivity
```

## Transistor wiring (PN2222A)

```
Arduino D2 → 1kΩ resistor → Base   (center pin, flat side facing you)
Arduino GND              → Emitter (left pin)
                Collector (right pin) → PWR_SW header pin 1
                                        PWR_SW header pin 2 → Arduino GND
```

The physical power button stays wired in parallel and keeps working normally.

## API reference

### Arduino (port 80)

| Method | Endpoint  | Action                          |
|--------|-----------|---------------------------------|
| GET    | `/status` | Returns `{"status":"ok"}`       |
| POST   | `/power`  | 200ms pulse on D2 (power button)|

### Pi Flask gateway (port 5000)

| Method | Endpoint  | Action                                                        |
|--------|-----------|---------------------------------------------------------------|
| GET    | `/status` | Check Pi and Arduino reachability                             |
| POST   | `/power`  | Forward power pulse to Arduino                                |
| POST   | `/wake`   | Power pulse + ping PC until up (blocks up to 90s) → `{"status":"ready"}` |
