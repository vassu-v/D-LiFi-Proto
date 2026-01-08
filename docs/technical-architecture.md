# Technical Architecture

## Overview

This document explains the engineering decisions, protocol design, and system architecture behind the disaster-resilient LiFi mesh communication system. While the main README focuses on *what* the system does and *how* to build it, this document explains *why* we made specific design choices and *how* the components work together.

---

## System Design Philosophy

### Core Principles

1. **Simplicity over sophistication**: Prioritize designs that work reliably with minimal complexity
2. **Graceful degradation**: System continues functioning even when nodes fail
3. **No infrastructure dependency**: Must operate completely off-grid
4. **Minimal compute requirements**: Must run on ESP8266 with 80MHz CPU and 80KB RAM
5. **Human-centered**: Designed for real disaster scenarios, not lab conditions

### Design Constraints

- **No ACKs**: Optical communication makes acknowledgment impractical (half-duplex, collision-prone)
- **Limited memory**: Cannot store routing tables for large networks
- **Variable topology**: Nodes may fail, lamp orientations vary, obstacles block paths
- **Emergency context**: System must work immediately when disaster strikes, no setup time

---

## Protocol Architecture

### Message Type Design

The protocol uses **5 distinct message types** with different header formats optimized for their purpose:

#### Type 0: INIT (Gradient Setup)
```
Header: [src(4)][id(2)][hop(2)][0] = 9 chars
Example: 000h0100 = HQ with INIT ID "01" at hop 0
```

**Purpose**: Build a distance gradient map across the network

**Why this design:**
- **No message content needed**: INIT only establishes topology, doesn't carry data
- **2-char ID**: Allows gradient rebuilding (increment ID when topology changes)
- **Spreads outward**: Each node increments hop count and forwards (HQ=0, adjacent=1, next=2...)
- **Idempotent**: Nodes accept lower hop counts, ignore higher ones (handles loops naturally)

**Alternative considered**: Flooding with sequence numbers
- **Rejected because**: Requires more memory (sequence tracking per source), doesn't provide distance metric

#### Type 1: BROADCAST (HQ → All Lamps)
```
Header: [src(4)][dst(4)][1][hash(4)] = 13 chars
Example: 000hFFFF1A3F2 = Broadcast from HQ with message hash
```

**Purpose**: System-wide announcements that all lamps display to citizens

**Why this design:**
- **Hash for integrity**: 16-bit hash catches corruption without ACKs
- **Flood routing**: No gradient check, all nodes forward (ensures 100% coverage)
- **Destination FFFF**: Reserved broadcast address, recognized by all nodes

**Alternative considered**: Selective forwarding based on hop count
- **Rejected because**: Risk of missing lamps due to inconsistent gradients, broadcasts need guaranteed delivery

#### Type 2: TARGETED (HQ → Specific Lamp)
```
Header: [src(4)][dst(4)][2][hash(4)] = 13 chars
Example: 000h102a2B5C8 = Message to specific lamp "102a"
```

**Purpose**: Geographically-specific instructions (e.g., "Exit via stairwell B" only near that stairwell)

**Why this design:**
- **Same format as Type 1**: Simplifies parsing, floods network the same way
- **Only destination processes**: All nodes forward (mesh routing), but only target lamp displays to phones

**Alternative considered**: Source routing with explicit path
- **Rejected because**: Requires HQ to know topology, breaks if path changes, more complex headers

#### Type 3: SOS (Lamp → HQ, Emergency)
```
Header: [src(4)][dst(4)][3][hop(2)] = 11 chars
Example: 102a000h305 = SOS from lamp "102a" currently at hop distance 5
```

**Purpose**: Emergency alert that routes to HQ using gradient

**Why this design:**
- **Header-only**: All SOS are identical ("help!"), no need for message content or hash
- **Includes current hop**: Nodes decrement toward HQ (5→4→3→2→1→0)
- **Gradient-based forwarding**: Only nodes closer to HQ forward (prevents backflow)
- **No phone broadcast**: Routes silently to HQ, which then decides response

**Key insight**: Making SOS header-only reduces transmission time by ~60% compared to Type 4

**Alternative considered**: SOS as Type 4 message with fixed content
- **Rejected because**: Wastes bandwidth, hash computation unnecessary for identical messages

#### Type 4: MESSAGE (Node → HQ, Normal Data)
```
Header: [src(4)][dst(4)][4][hash(4)][hop(2)] = 15 chars
Example: 102a000h4C3A705 = Status message from node at hop 5, with hash
```

**Purpose**: Non-emergency data (status updates, sensor readings, diagnostics)

**Why this design:**
- **Full message + hash**: Variable content requires integrity verification
- **Gradient routing**: Uses hop count like Type 3 to route toward HQ
- **Lower priority than SOS**: In congested conditions, SOS (shorter) transmits faster

---

## Gradient Routing System

### Why Gradient Routing?

Traditional mesh networks use routing tables (e.g., OLSR, AODV, Batman). We chose gradient routing instead.

**Routing tables require:**
- Storing paths for every destination (memory intensive)
- Periodic updates to maintain freshness (bandwidth intensive)
- Complex algorithms to compute best paths (CPU intensive)

**Gradient routing requires:**
- One integer per node (its hop distance from HQ)
- Single INIT flood to establish gradient
- Simple comparison: "Is my hop ≤ message hop + K?"

**Trade-off**: Gradient routing may not find the absolute shortest path, but guarantees messages move toward HQ with minimal overhead.

### How INIT Propagates

```
Time T0: HQ sends INIT (hop=0)
Time T1: Adjacent lamps receive, set hop=1, forward with hop=1
Time T2: Next lamps receive, set hop=2, forward with hop=2
...continues outward like a wave
```

**Key behavior**: If a node receives INIT with hop < (myHop - 1), it updates. This handles:
- Initial setup (all nodes start at hop=99)
- Topology changes (new shorter paths discovered)
- Loop prevention (don't update from higher hop counts)

### Forwarding Decision

For SOS and MESSAGE (Types 3, 4):
```cpp
if (myHop <= messageHop + K) {
    forward();  // I'm close enough to HQ to help route
}
```

**The tolerance K (default = 1):**
- K=0: Only forward if I'm strictly closer (rigid, may fail if topology changes)
- K=1: Forward if I'm same distance or closer (adds redundancy, handles minor inconsistencies)
- K=2+: More redundancy, but increases unnecessary traffic

**We chose K=1** as the balance between reliability and efficiency.

### Handling Node Failures

**Scenario**: Node fails, breaking a path

**What happens**:
1. Messages route around via alternate paths (mesh property)
2. If gradient becomes inconsistent, HQ sends new INIT with incremented ID
3. Network rebuilds gradient within seconds

**No manual intervention needed** - gradient naturally adapts to topology changes.

---

## Firmware Architecture

### Non-Blocking Design

**Why non-blocking matters**: A blocking operation (e.g., waiting for IR transmission) would freeze the entire node, missing incoming messages.

**Our approach**:
- Main loop runs continuously, each task checks "should I act now?"
- Button: Edge detection (check if state changed since last loop)
- IR reception: Non-blocking character accumulation
- Retransmission: Time-based triggers (check elapsed time)
- LiFi rebroadcast: Periodic timer (check interval)

**Example: Button handling**
```cpp
// BAD: Blocking
if (digitalRead(SOS_PIN) == LOW) {
    delay(5000);  // Debounce - BLOCKS everything!
    sendSOS();
}

// GOOD: Non-blocking with edge detection
bool current = digitalRead(SOS_PIN);
if (current == LOW && last == HIGH) {  // Falling edge
    if (millis() - lastPress > COOLDOWN) {
        sendSOS();
    }
}
last = current;
```

### State Machine: IR Reception

IR messages arrive character-by-character. We need to handle:
1. Header-only packets (INIT, SOS)
2. Header + message packets (BROADCAST, TARGETED, MESSAGE)
3. Timeouts (incomplete packets)

**State machine**:
```
State: IDLE
  ├─ Receive 9 chars + Type 0 → Process INIT, stay IDLE
  ├─ Receive 11 chars + Type 3 → Process SOS, stay IDLE
  └─ Receive 13/15 chars → WAITING_FOR_MESSAGE

State: WAITING_FOR_MESSAGE
  ├─ Receive message → Process complete packet, return IDLE
  ├─ Timeout (3s) → Discard header, return IDLE
  └─ Receive header → Discard old, start waiting for new message
```

This handles packet loss gracefully: incomplete packets are discarded after timeout, doesn't block future messages.

### Deduplication Cache

**Purpose**: Prevent forwarding loops (node receives its own forwarded message and forwards again → infinite loop)

**Implementation**: Circular buffer of size 3
```cpp
struct { String src; uint16_t hash; } cache[3];
```

**Why size 3?**
- In a 5-node chain, a message can be "seen" by up to 3 nodes simultaneously (sender + 2 forwarders)
- Larger cache wastes memory, smaller cache risks false loops
- Circular buffer: oldest entry automatically overwritten

**Hash collision risk**: With 16-bit hashes (65,536 values) and cache size 3, probability of false duplicate is ~0.005% per message. Acceptable for emergency use.

**Alternative considered**: Sequence numbers per source
- **Rejected because**: Requires tracking state for every source node (N states for N nodes), doesn't scale

### Retransmission Queue

**Problem**: IR communication is lossy (collisions, obstacles, interference). We can't use ACKs (half-duplex, too complex).

**Solution**: Redundant transmission - send each message 2-3 times over the first minute.

**Why not just send 3 times immediately?**
- Wastes bandwidth if first transmission succeeded
- Causes congestion (all 3 copies arrive in burst)
- Doesn't help if interference is temporary (all 3 fail together)

**Time-spaced redundancy (what we do)**:
```
T=0s:   Initial send
T=10s:  Retransmit #1
T=20s:  Retransmit #2
T=60s:  Stop (redundancy window closed)
```

**Benefits**:
- If first send succeeds, subsequent ones are deduped (no harm)
- If first send fails, 10s later conditions may have improved
- Spreads traffic over time (reduces collision probability)

**Implementation**: FIFO queue of size 3
```cpp
struct {
    String header, message;
    unsigned long firstSent;
    uint8_t sentCount;
    bool active;
} queue[3];
```

Each loop iteration checks: "Is it time to resend any queued messages?"

---

## Dashboard Architecture

### Data Flow

```
Arduino (C++) ←[Serial]→ Python (Flask) ←[WebSocket]→ Browser (JavaScript)
     ↓                          ↓                            ↓
   Mesh IR                   SQLite DB                  Leaflet Map
```

### Serial Protocol

**Arduino → Python** (incoming messages):
```
Format: <sender_id> <type> <content>
Example: "102a 3 SOS"
Example: "102a 4 Temperature 25C"
```

**Python → Arduino** (commands):
```
INIT|<id>              → Send INIT with specified ID
BROADCAST|<message>    → Type 1 broadcast
TARGET|<node>|<msg>    → Type 2 targeted
MESSAGE|<node>|<msg>   → Type 4 message
```

**Why this format?**
- Simple parsing (split on space/pipe)
- Human-readable for debugging
- No binary encoding complexity

### Database Schema

**Nodes table**:
```sql
id (PK), name, latitude, longitude, status, last_seen
```

**Messages table**:
```sql
id (PK), sender_id (FK), type, content, is_sos, timestamp
```

**Design decision**: Store all messages (not just active SOS)
- **Benefit**: Full audit log for post-disaster analysis
- **Trade-off**: Database grows over time (acceptable for short-term emergency use)

### WebSocket Real-Time Updates

**Why WebSocket over polling?**
- Instant updates (no 1-5s polling delay)
- Lower server load (no repeated HTTP requests)
- Push notifications (SOS modal triggers immediately)

**Events**:
- `new_message`: Add to feed, update stats
- `sos_alert`: Trigger modal with sound
- `status`: Update Arduino connection indicator

---

## Design Trade-offs

### What We Sacrificed

1. **No encryption**: Messages are plaintext
   - **Why**: Emergency use, need immediate deployment, encryption adds complexity
   - **When to add**: Production deployment in non-emergency scenarios

2. **No ACKs**: Fire-and-forget delivery
   - **Why**: Half-duplex IR, collision-prone, state management complexity
   - **Mitigation**: Redundant transmission provides reliability without ACKs

3. **Limited range (20cm demo)**: Could easily extend to 50m+
   - **Why**: Tabletop demo constraint, easier testing
   - **How to scale**: Add IR amplifiers, focusing lenses (documented in hardware guide)

4. **No dynamic topology discovery**: Manual INIT required after major changes
   - **Why**: Reduces complexity, acceptable for static lamp infrastructure
   - **When to add**: If nodes are mobile or frequently added/removed

5. **Basic hash (not cryptographic)**: 16-bit polynomial hash
   - **Why**: Fast computation on ESP8266, sufficient for corruption detection
   - **Limitation**: Not secure against malicious manipulation (not a disaster scenario concern)

### What We Prioritized

1. **Reliability over efficiency**: Redundant transmission, flood routing for broadcasts
2. **Simplicity over optimization**: Gradient routing instead of complex routing protocols
3. **Immediate usability**: No configuration, works out-of-box with INIT
4. **Graceful degradation**: Network continues functioning when nodes fail
5. **Observable behavior**: Extensive debug logging helps understand what's happening

---

## Performance Characteristics

### Latency

- **Single hop**: 1-2 seconds
- **5-hop chain**: 5-10 seconds
- **Limiting factor**: IR character-by-character transmission (~100ms per char)

**Improvement path**: Burst transmission (send all chars in rapid sequence) could reduce to <1s per hop, but increases collision risk.

### Throughput

- **Header-only (SOS)**: ~11 characters = ~1.1 seconds
- **Standard message**: ~13 char header + message = ~2-3 seconds total
- **Bottleneck**: IR modulation speed (38kHz carrier, NEC protocol overhead)

**Not a concern for emergency use**: Message rate is low (SOS alerts, occasional broadcasts), not continuous data streaming.

### Reliability

- **Deduplication**: 100% effective (no false loops observed in testing)
- **Corruption detection**: 99.995% (16-bit hash catches all single-bit errors, most multi-bit errors)
- **Delivery success**: ~95% with 2x redundancy, ~99% with 3x redundancy (based on 5-hop tests)

### Scalability

**Tested**: 5 nodes, stable operation
**Theoretical**: 50+ nodes possible with current design
**Limitation**: Gradient routing assumes tree-like topology (one HQ), doesn't optimize for highly meshed networks

**If scaling beyond 50 nodes**: Consider hierarchical gradients (multiple HQ nodes, regional routing).

---

## Lessons from Implementation

### What Worked Well

1. **Gradient routing simplicity**: Single integer state, trivial forwarding logic
2. **Header-only SOS**: Significant bandwidth savings for most critical message type
3. **Retransmission queue**: Provides reliability without ACK complexity
4. **Non-blocking firmware**: Allows parallel task handling on single-threaded hardware

### What Was Harder Than Expected

1. **IR TX/RX coordination**: Must stop receiver during transmission (hardware limitation)
2. **State machine for multi-segment messages**: Handling timeouts, incomplete packets
3. **Four-directional sequential transmission**: Timing gaps to prevent self-interference
4. **Cache sizing**: Balance between memory usage and loop prevention effectiveness

### What Would Change for Production

1. **Add encryption layer**: AES-128 or ChaCha20 for message confidentiality
2. **Improve IR range**: Amplifiers, focusing lenses, better antenna design
3. **Outdoor testing**: Validate under sunlight, rain, temperature variation
4. **Failure mode analysis**: Systematic testing of node failures, network partitions
5. **Power optimization**: Sleep modes, duty cycling for battery-powered nodes

---

## Conclusion

This system demonstrates that sophisticated mesh networking doesn't require complex protocols. By carefully choosing which problems to solve (gradient routing, redundant transmission) and which to avoid (ACKs, routing tables), we built a functional disaster communication system on severely resource-constrained hardware.

The key insight: **In emergency scenarios, simple and reliable beats optimal and complex.**

---

**Related Documentation**:
- [Hardware Design](hardware-design.md) - Circuit details and component selection
- [README](../README.md) - Quick start and building guide
- [Project Report](project_report.pdf) - Full technical documentation and testing methodology
