# Hardware Design

## Overview

This document covers the physical layer implementation: circuit design, component selection rationale, power considerations, and scaling from tabletop demo to production deployment. While the main README provides basic circuit diagrams, this document explains *why* each component was chosen and *how* to optimize for different scenarios.

---

## System Architecture

### Node Components

Each mesh node consists of:
1. **Microcontroller**: NodeMCU ESP8266
2. **IR Transmitters**: 4 directional channels (2-3 LEDs each)
3. **IR Receiver**: TSOP38238 (38kHz demodulator)
4. **User Interface**: SOS button, status LED
5. **Power System**: USB or 3.7V battery (solar in production)

**Design Philosophy**: Use commodity components, minimize custom parts, prioritize availability over performance.

---

## Microcontroller Selection

### Why ESP8266 (NodeMCU)?

**Considered alternatives**:
- Arduino Uno/Nano: Limited I/O, no wireless (future expansion)
- ESP32: More expensive, overkill for this application
- ATtiny85: Too limited (not enough pins, memory)
- Raspberry Pi Pico: More expensive, unnecessary features

**ESP8266 advantages**:
- **12+ GPIO pins**: Enough for 4 TX directions + RX + button + LEDs
- **80 MHz CPU**: Fast enough for hash computation, state machines
- **80 KB RAM**: Sufficient for message buffers, cache, queues
- **3.3V logic**: Matches TSOP receiver requirements
- **Cost**: ~Rs.200-300 per board
- **Built-in USB**: Easy programming and serial debugging
- **WiFi capability**: Future expansion (mesh backbone + internet gateway)

**Pin allocation**:
```
D0 (GPIO16) → IR TX Back
D1 (GPIO5)  → Status LED
D2 (GPIO4)  → IR TX Front
D3 (GPIO0)  → IR TX Right
D5 (GPIO14) → IR RX (TSOP)
D6 (GPIO12) → SOS Button
D7 (GPIO13) → IR TX Left
D8 (GPIO15) → Lamp Light (future LiFi)
```

**Why these specific pins?**
- D0-D3, D5-D8: General purpose, 3.3V tolerant
- Avoided D4 (built-in LED, often inverted logic)
- D6 with INPUT_PULLUP: No external pull-up resistor needed

---

## IR Transmitter Circuit

### Circuit Design (Per Direction)

```
NodeMCU GPIO (D0/D2/D3/D7)
    │
    ├─── 2.2kΩ resistor
    │
    └─── NPN Transistor Base (2N2222)
            │
            ├─── Collector
            │    │
            │    ├─── 2-3× IR LEDs (940nm) in series
            │    │
            │    └─── 100Ω current-limiting resistor
            │         │
            │         └─── 5V supply
            │
            └─── Emitter → GND
```

### Component Selection

#### IR LEDs: 940nm wavelength

**Why 940nm?**
- Peak sensitivity of TSOP receivers (850-950nm range)
- Invisible to human eye (no visible flicker)
- Widely available, commodity pricing
- Good atmospheric penetration

**Alternatives considered**:
- 850nm: Less common, slightly worse receiver match
- Visible red (660nm): Distracting flicker, poor receiver sensitivity
- 1550nm: Telecom wavelength, requires expensive InGaAs detectors

**LED specifications**:
- Forward voltage: ~1.2-1.4V
- Forward current: 20-100mA (we use ~60mA)
- Beam angle: 20-30° (narrower = longer range, wider = better coverage)

**Series configuration (2-3 LEDs)**:
```
3 LEDs: 3 × 1.3V = 3.9V drop
With 100Ω resistor: (5V - 3.9V) / 100Ω = 11mA per LED (low power mode)

2 LEDs: 2 × 1.3V = 2.6V drop  
With 100Ω resistor: (5V - 2.6V) / 100Ω = 24mA per LED (balanced)

Power calculation: 5V × 24mA × 4 directions = 480mW total TX power
```

**Trade-off**: More LEDs = longer range but higher power consumption.

#### Transistor: 2N2222 NPN

**Why NPN transistor?**
- GPIO can only source ~12mA (ESP8266 limitation)
- 2-3 LEDs at 60mA each = 120-180mA total
- Transistor acts as current amplifier/switch

**Why 2N2222 specifically?**
- Collector current: 800mA max (way above our 180mA need)
- Gain (hFE): ~100-300 (base current of 1-2mA switches 180mA collector)
- Switching speed: Fast enough for NEC protocol (~38kHz)
- Cost: <$0.10 each, universally available
- Through-hole package: Easy breadboard prototyping

**Alternative**: BC547 (similar specs, also common)

**Base resistor: 2.2kΩ**
```
GPIO output: 3.3V
Base-emitter drop: 0.7V
Base current: (3.3V - 0.7V) / 2.2kΩ = 1.18mA
Collector current: 1.18mA × 100 (hFE) = 118mA (sufficient for 2-3 LEDs)
```

**Current-limiting resistor: 100Ω**
- Limits LED current to safe operating range
- Prevents thermal runaway
- Can reduce to 47Ω or 68Ω for longer range (check LED datasheet max current)

### Four-Directional Design

**Why 4 separate TX circuits?**

**Problem**: Omnidirectional transmission (all LEDs parallel on one GPIO)
```
4 directions × 60mA = 240mA total
Single GPIO limit: ~12mA
Single transistor: OK, but...
```

**Issues with shared circuit**:
1. **Alignment sensitivity**: All LEDs must fire simultaneously, misalignment reduces range
2. **Power supply strain**: 240mA inrush can cause voltage sag
3. **No directional control**: Can't prioritize specific directions

**Solution: Sequential transmission per direction**
```
TX Front  → Delay 100ms → TX Right → Delay 100ms → TX Back → Delay 100ms → TX Left
```

**Benefits**:
- Only 60mA draw at any instant (no supply sag)
- Each direction gets fresh transmission (better reliability)
- Independent control allows future optimizations (adaptive directionality)

**Trade-off**: 4× longer transmission time (~400ms vs ~100ms), but acceptable for emergency messaging.

### Range Optimization

**Current demo: ~20cm range**

**Limiting factors**:
1. Low LED current (24mA vs 100mA capable)
2. Wide beam angle (30° spreads power)
3. Ambient light noise (indoor testing)

**To achieve 50m+ range**:

**Option 1: Higher power**
```
Reduce resistor: 100Ω → 47Ω
LED current: 24mA → 50mA
Power increase: 2.1× → Theoretical range: 42cm
```

**Option 2: Focusing optics**
```
Add collimating lens (20° → 5° beam)
Power concentration: 4× → Theoretical range: 80cm
```

**Option 3: Amplification**
```
Replace 2N2222 with MOSFET driver (e.g., IRLZ44N)
Drive 6-10 LEDs in parallel at 100mA each
Power increase: 10× → Theoretical range: 2m (still indoor)
```

**Option 4: Outdoor-grade hardware**
```
High-power IR LED (e.g., Osram SFH 4550)
Output: 1W vs 60mW (17× power)
With focusing lens → 20-50m range
```

**Production recommendation**: Option 4 with weatherproof enclosures.

---

## IR Receiver Circuit

### Circuit Design

```
TSOP38238 IR Receiver
    │
    ├─── VCC → 3.3V (NodeMCU)
    ├─── GND → GND
    └─── OUT → NodeMCU GPIO (D5)
```

**No external components needed** - TSOP is self-contained.

### Component Selection: TSOP38238

**What it does**:
- Receives 38kHz modulated IR signal
- Demodulates (removes 38kHz carrier)
- Outputs clean digital signal (active LOW when IR detected)

**Why 38kHz carrier frequency?**

**Alternatives**:
- DC (no carrier): Severely affected by sunlight, ambient IR
- 30kHz: Less standard, fewer receiver options
- 56kHz: Another common frequency, similar performance

**38kHz advantages**:
1. **Sunlight filtering**: DC component of sunlight is filtered out by AC coupling
2. **Component availability**: Most IR receivers are 38kHz (TV remotes, etc.)
3. **Lower cost**: Commodity part due to consumer electronics volume
4. **Faster data rate**: Higher carrier allows faster modulation

**Sunlight interference mitigation**:
```
Sunlight spectrum: Mostly DC + low-frequency flicker (100-120Hz from AC lighting)
TSOP bandpass filter: 35-41kHz (center: 38kHz)
Result: Sunlight DC component rejected, only 38kHz signals pass through
```

**TSOP38238 specifications**:
- Supply voltage: 2.5-5.5V (compatible with 3.3V NodeMCU)
- Output: Open-drain (needs pull-up, but NodeMCU has internal pull-up)
- Range: 35-45m (with proper transmitter)
- Viewing angle: ±45° (90° total cone)

**Why TSOP38238 specifically vs other TSOP models?**
- 38238: Standard sensitivity, good balance
- 38222: Lower sensitivity (shorter range, but less noise)
- 38240: Higher sensitivity (longer range, but more false triggers)

**38238 chosen for**: Balanced performance in indoor + outdoor conditions.

### Receiver Placement

**Considerations**:
1. **Unobstructed view**: Must "see" all 4 transmitter directions
2. **Away from sunlight**: Direct sunlight can saturate receiver (even with 38kHz filter)
3. **Stable mounting**: Vibration changes alignment

**Production mounting**: Center of lamp housing, pointing outward with 360° view (no directionality needed for RX).

---

## User Interface Components

### SOS Button

```
Push Button
    │
    ├─── One terminal → NodeMCU GPIO (D6, INPUT_PULLUP)
    └─── Other terminal → GND
```

**Why INPUT_PULLUP?**
- No external resistor needed (uses internal 20-50kΩ pull-up)
- Button press shorts GPIO to GND → reads LOW
- Button release → pull-up brings GPIO HIGH
- Simpler circuit, fewer components

**Button specifications**:
- Normally open (N.O.) momentary switch
- Rated for 10,000+ actuations (emergency use)
- Large, tactile (easy to press in dark/panic)

**Debouncing**: Software edge detection (see firmware architecture doc), no hardware debounce capacitor needed.

### Status LED

```
NodeMCU GPIO (D1)
    │
    ├─── 220Ω resistor
    │
    └─── LED (red or green)
         │
         └─── GND
```

**Current limiting**:
```
LED forward voltage: ~2.0V (red) or ~3.0V (green/blue)
GPIO output: 3.3V
Current: (3.3V - 2.0V) / 220Ω = 5.9mA (safe for GPIO and LED)
```

**LED selection**:
- Red: Low power, high visibility, universally understood as status
- Green: Indicates "ready/OK" state
- RGB: Future expansion (different colors for different states)

**Why 220Ω vs 330Ω or 470Ω?**
- 220Ω: Brighter LED (5.9mA), better visibility
- 330Ω: Dimmer (3.9mA), saves power
- 470Ω: Very dim (2.8mA), hard to see in daylight

**Trade-off**: Chose 220Ω for visibility (power consumption is negligible: 3.3V × 6mA = 20mW).

### Lamp Light (Future LiFi)

```
NodeMCU GPIO (D8)
    │
    ├─── MOSFET driver or relay
    │
    └─── Street lamp LED (5-20W)
         │
         └─── High voltage supply (12V, 24V, or mains)
```

**Current placeholder**: Simple LED on breadboard for testing.

**Production implementation**: High-power LED driver with PWM modulation at kHz frequencies for visible light communication (phone camera or photodiode receiver).

---

## Power System

### Demo Configuration: USB Power

```
NodeMCU USB port (5V)
    │
    ├─── On-board 3.3V regulator → NodeMCU logic
    └─── 5V rail → IR LED transistor circuits
```

**Power budget**:
```
NodeMCU idle: ~80mA
NodeMCU TX: ~120mA (WiFi off, just CPU + GPIO)
IR LEDs (one direction): ~60mA
Total peak: 120mA + 60mA = 180mA (well within USB 500mA limit)
```

### Production Configuration: Solar + Battery

**System design**:
```
Solar panel (10W, 12V)
    │
    ├─── Charge controller (MPPT preferred)
    │
    └─── LiFePO4 battery (12V, 5Ah)
         │
         ├─── DC-DC buck converter (12V → 5V, 2A)
         │    │
         │    └─── NodeMCU + IR circuits
         │
         └─── Street lamp LED (12V, 5W)
```

**Battery capacity calculation**:
```
Average power: ~150mA @ 5V = 0.75W
Continuous operation: 20 hours
Energy needed: 0.75W × 20h = 15Wh
Battery: 12V × 5Ah = 60Wh (4× margin for cloudy days)
```

**Solar panel sizing**:
```
Daily energy: 15Wh
Sun hours: ~5h (average)
Panel needed: 15Wh / 5h = 3W (minimum)
Chosen: 10W (safety margin for cloudy days, panel degradation)
```

**Why LiFePO4 vs Li-ion?**
- LiFePO4: More stable, wider temperature range (-20°C to 60°C), longer cycle life
- Li-ion: Higher energy density, but less safe in outdoor conditions
- Lead-acid: Too heavy, shorter lifespan

**Charge controller**: MPPT (Maximum Power Point Tracking) extracts 20-30% more energy from solar panel vs PWM controller. Worth the extra cost for 24/7 operation.

### Power Optimization

**Sleep modes** (not currently implemented):
```
Deep sleep: 20µA (wake on button press)
Light sleep: 15mA (wake on timer or interrupt)
Active: 150mA
```

**Potential savings**:
- Sleep between IR receptions: 90% power reduction
- Wake every 100ms to check for signals
- Average power: ~20mA vs 150mA

**Not implemented because**: Emergency system must be always-on, no acceptable latency for wake-up.

---

## Testing & Validation

### Range Testing

**Procedure**:
1. Place TX node on tripod at 0cm
2. Place RX node on second tripod
3. Increase distance in 10cm increments
4. Send 100 messages at each distance
5. Record success rate

**Results (demo configuration)**:
- 0-20cm: 100% success
- 20-40cm: 85% success
- 40-60cm: 60% success (borderline)
- 60cm+: <50% success (unreliable)

**With focusing lens**:
- 0-50cm: 100% success
- 50-100cm: 90% success
- 100cm+: Testing ongoing

### Interference Testing

**Scenarios tested**:
- Indoor LED lighting: No interference (DC filtered)
- Fluorescent lighting: Minimal (60Hz flicker filtered)
- Sunlight (indirect): No issues
- Sunlight (direct on receiver): Saturates receiver (needs shading)
- Other IR remotes: Brief interference, recovers immediately

**Conclusion**: 38kHz filtering is effective for most conditions.

### Power Consumption Measurement

**Measured values** (USB power meter):
- Idle (no TX): 95mA @ 5V = 475mW
- Transmitting (one direction): 155mA @ 5V = 775mW
- Transmitting (4 directions sequential): Average 180mA @ 5V = 900mW

**Battery life estimate**:
```
5Ah @ 12V = 60Wh
Average: 0.75W (mostly idle, occasional TX)
Runtime: 60Wh / 0.75W = 80 hours (3+ days without sun)
```
---


## Scaling Considerations

### From Demo to Production

**Critical changes needed**:

1. **Range improvement**: Add amplifiers + lenses → 50m target
2. **Weatherproofing**: PCB conformal coating + IP65 enclosure
3. **Power system**: Integrate solar panel + battery + charge controller
4. **Mounting**: Design brackets for existing street lamps
5. **Visible light transmitter**: Add high-power white LED with PWM driver for phone communication

**Estimated timeline**:
- PCB design: 2 weeks
- Prototype assembly: 1 week
- Outdoor testing: 4 weeks
- Iteration/fixes: 2 weeks
- **Total**: 9 weeks to production-ready

**Estimated cost per node**: Rs.350-600 (volume pricing at 100+ units)

---

## Conclusion

This hardware design prioritizes simplicity, availability, and cost-effectiveness. The 20cm demo range is intentionally conservative - with proper optics and power, 50m+ is achievable using the same protocol and firmware.

The key insight: **Optical communication doesn't require exotic components.** Commodity IR LEDs, standard receivers, and basic transistors are sufficient for a working mesh network. The complexity is in the protocol (covered in technical architecture doc), not the hardware.

---

**Related Documentation**:
- [Technical Architecture](technical-architecture.md) - Protocol design and firmware implementation
- [README](../README.md) - Quick start and building guide
- [Project Report](project_report.pdf) - Full testing methodology and results
