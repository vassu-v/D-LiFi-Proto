# Disaster-Resilient LiFi Communication System

Off-grid emergency mesh network using solar street lamps and infrared communication. Built for CBSE Regional Science Exhibition 2025-26.

## Problem

Power grids and mobile networks fail simultaneously during natural disasters. Communities lose contact with rescue teams when they need it most.

## Solution

Retrofit solar street lamps with IR transceivers to create a disaster-resilient mesh network. Each lamp forwards emergency messages hop-by-hop using light, operates entirely off-grid, and routes SOS alerts to headquarters using gradient-based routing.

**Protocol:** LiFi (IEEE 802.11bgn standard, includes IR). IR chosen for mesh backbone due to lower power, no visible flicker, mature TSOP receivers, and built-in sunlight filtering via 38kHz AC modulation.

## How It Works

### Gradient Routing

The network uses distance gradients instead of routing tables. HQ sends INIT packets that propagate outward, building a hop-count map. Messages then flow "downhill" toward HQ using decreasing hop counts.

**Example:** Lamp4 (hop=5) → Lamp3 (hop=4) → Lamp2 (hop=3) → HQ (hop=0)

For detailed routing logic, topology examples, and protocol specifications, see [project_report.pdf](docs/project_report.pdf).

### Message Types

| Type | Direction | Purpose | Header Format |
|------|-----------|---------|---------------|
| 0 | HQ → All | Build gradient map | `[src][id][hop][0]` (9 chars) |
| 1 | HQ → All | Broadcast alert | `[src][dst][1][hash]` (13 chars) |
| 2 | HQ → One | Targeted alert | `[src][dst][2][hash]` (13 chars) |
| 3 | Lamp → HQ | SOS emergency | `[src][dst][3][hop]` (11 chars) |
| 4 | Node → HQ | Status message | `[src][dst][4][hash][hop]` (15 chars) |

Types 3 & 4 use gradient routing. Types 1 & 2 flood the network.

### Reliability

- **Deduplication cache:** Prevents forwarding loops using (source, hash) pairs
- **Automatic retransmission:** 2-3 redundant sends in first minute, no ACKs needed
- **Four-directional broadcast:** Coverage regardless of lamp orientation

## Hardware

### IR Transmitter Circuit (per direction)

```
NodeMCU GPIO (D2/D3/D0/D7) 
    → 2.2kΩ resistor 
    → NPN transistor base (2N2222)
    → Collector connects to:
        → 2-3x IR LEDs (940nm) in series
        → 100Ω current-limiting resistor
        → 5V supply
    → Emitter to GND
```

**Why 4 separate pins?** Prevents exceeding single GPIO current limit (~12mA). Hardware can use omnidirectional setup with single amplifier.

### IR Receiver Circuit

```
TSOP38238 IR Receiver
    VCC → 3.3V
    GND → GND
    OUT → NodeMCU GPIO (D5)
```

**Why 38kHz?** Low cost, faster data rates, readily available receivers, AC modulation filters DC sunlight interference.

### SOS Button (Lamp nodes only)

```
Push button between GPIO (D6) and GND
Internal pull-up resistor enabled (INPUT_PULLUP)
```

### Complete Node BOM

- NodeMCU ESP8266
- TSOP38238 IR receiver (38kHz)
- 2N2222 NPN transistor (×4 for 4 directions)
- IR LEDs 940nm (×8-12, 2-3 per direction)
- 100Ω resistor (×4 for LED current limiting)
- 2.2kΩ resistor (×4 for transistor base)
- 220Ω resistor (×1 for status LED)
- Push button (×1 for SOS)
- Breadboard + jumper wires
- Power: USB or 3.7V battery (solar in production)

### Performance

- **Demo range:** ~20cm (tabletop exhibition)
- **Production range:** 50m+ with amplifiers and focusing lenses
- **Latency:** 1-2 seconds per hop
- **Tested:** 5-hop chains, stable indoor operation
- **Protocol:** NEC IR via IRremote library

## Building It

### 1. Flash Node Firmware

```bash
# Install Arduino IDE + ESP8266 board support
# Install IRremote library

# Edit config.h for each node
#define NODE_ID "102a"        # Unique 4-char ID
#define HQ_ID "000h"          # Headquarters address

# Upload to ESP8266
arduino --upload main.ino --port /dev/ttyUSB0
```

### 2. Wire the Hardware

Follow circuit diagrams above. Use separate GPIO pins for each IR transmitter direction.

### 3. Start HQ Dashboard

```bash
cd src/hq/lifi\ hq/code
pip install flask flask-socketio pyserial
python app.py
```

Open http://localhost:5000

### 4. Run 3-Node Demo

1. Flash 3 ESP8266s with IDs: `000h` (HQ), `101a`, `102a`
2. Connect HQ to computer via USB
3. Click "Connect Arduino" in dashboard
4. Send `INIT|01` to build gradient map (wait 5-10s)
5. Press SOS button on `102a`
6. Verify "SOS from 102a" in dashboard

**Expected:** Single hop 1-2s, 5-hop chain 5-10s, zero duplicates.

## Current Status

- 3-5 node mesh with stable forwarding
- Gradient routing with automatic path selection
- SOS alerts route to HQ
- Real-time dashboard monitoring
- Four-directional IR broadcast

## Protocol Limitations

- No encryption (add for production deployment)
- Basic hash integrity (not cryptographically secure)
- Manual INIT required to rebuild gradient after topology changes
- Sequential transmission (not concurrent)
- 5-node maximum tested

## Code Structure

```
├── src/
│   ├── hq/                      # Headquarters node
│   │   ├── arduino/main/        # ESP8266 firmware
│   │   └── code/                # Flask dashboard
│   └── lamp/                    # Lamp node firmware
└── structure/v3/upg/            # Latest protocol version
    ├── config.h                 # Pin assignments, timing, node ID
    ├── ir.h                     # IR communication layer
    ├── lifi.h                   # Routing and protocol logic
    └── main.ino                 # Main event loop
```

**config.h** → Change node ID, pins, timing constants, protocol parameters  
**ir.h** → Handles NEC IR protocol (swap this file to use LoRa, RF, or other physical layers)  
**lifi.h** → Gradient routing, message processing, caching  
**main.ino** → Button handling, message forwarding, retransmission

**To use different communication technology:** Replace `ir.h` with your implementation. Keep function signatures: `irInit()`, `irSendString()`, `irReceiveString()`. Protocol layer stays the same.

## Full Documentation

See [project_report.pdf](docs/project_report.pdf) for complete technical documentation, circuit details, testing methodology, and references.

