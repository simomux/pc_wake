# pc-remote

Wake your Windows PC remotely via Arduino + Raspberry Pi + Tailscale.

```
MacBook (Tailscale)
  → Pi Zero 2W (Tailscale subnet router)
    → Arduino R4 WiFi (LAN HTTP server)
      → transistor relay → motherboard power button
      → USB HID keyboard → types login password
  → RDP → Windows 11 PC
```

## Project structure

```
pc-remote/
├── .env.example                 # copy to .env and fill in your values
├── arduino/main/
│   ├── config.h.example         # copy to config.h and fill in your values
│   └── main.ino                 # HTTP server + relay + HID keyboard
├── pi/
│   ├── app.py                   # Flask gateway on the Pi
│   ├── requirements.txt
│   └── install.sh               # automated setup: Tailscale + Flask + systemd
└── scripts/
    └── wake.sh                  # Mac-side script: wake the PC over Tailscale
```

## Setup

### 1. Configure secrets

```bash
cp .env.example .env
# fill in .env with your WiFi, Arduino IP, Windows password, and Pi Tailscale IP
```

### 2. Arduino

```bash
cp arduino/main/config.h.example arduino/main/config.h
# fill in config.h with WiFi credentials, Windows password, and desired static IP
```

Open `arduino/main/main.ino` in Arduino IDE and upload. Verify connectivity:

```bash
curl -X GET http://<arduino-ip>/status
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
./scripts/wake.sh            # full sequence: power + wait for boot + password
./scripts/wake.sh status     # check Pi and Arduino connectivity
./scripts/wake.sh power      # pulse power button only
./scripts/wake.sh type-password  # type password only (PC already on)
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

| Method | Endpoint         | Action                                        |
|--------|------------------|-----------------------------------------------|
| GET    | `/status`        | Returns `{"status":"ok"}`                     |
| POST   | `/power`         | 200ms pulse on D2 (power button press)        |
| POST   | `/type-password` | Types the Windows password via HID + Enter    |
| POST   | `/wake`          | Full sequence: power → wait for boot → password |

### Pi Flask gateway (port 5000)

Same endpoints — the Pi proxies every request to the Arduino.
