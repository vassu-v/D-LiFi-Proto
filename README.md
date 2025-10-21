# Disaster-Resilient LiFi Mesh Communication System

A LiFi-based street-lamp mesh network designed to maintain communication during disasters — completely off-grid, powered by solar lamps, and capable of forwarding emergency messages lamp-to-lamp using light.

---

## 🌍 Overview

When mobile networks fail during floods, cyclones, or earthquakes, communities lose contact with rescue teams and vital information.  
This project proposes **transforming solar street lamps into LiFi communication nodes** — creating a self-contained, optical mesh network that stays operational even without internet or electricity.

---

## 🚨 Problem Statement

During natural disasters, power grids and mobile networks often collapse, leaving communities disconnected and uninformed.  
A **low-cost, infrastructure-independent** communication channel is essential for maintaining situational awareness and coordination between citizens and responders.

---

## 💡 Proposed Solution

Each street lamp becomes a **LiFi node** capable of sending and receiving encoded signals through infrared and visible light.

- **LiFi mesh:** Lamps forward messages from one to another via light, forming a resilient optical network.  
- **Broadcast capability:** Important alerts can be transmitted directly to people’s phones using a simple optical receiver or dongle.  
- **Solar-powered:** Operates entirely off-grid, running on its own battery and solar cell.

---

## ⚙️ How the System Works (High-Level)

**Nodes:**
- **Lamp Node:** Forwards messages, deduplicates using a per-source hash cache, and can generate SOS alerts.  
- **Router Node (optional):** Extends reach with caching and temporary storage.  
- **HQ Node:** Acts as a control center — logs messages, displays them, and injects broadcasts into the mesh.

**Message Flow:**
1. An SOS button press on a lamp node generates a message packet (`header + data`).  
2. Lamps forward it hop-by-hop through LiFi (optical IR).  
3. Each node caches recently seen messages to avoid loops or duplication.  
4. The HQ node logs received messages and can broadcast system alerts across the mesh.

**Modes of Operation:**
- **Off-grid:** No dependency on the internet or RF links for the mesh core.  
- **Store-and-forward:** Messages are cached and retried if nodes are temporarily offline or unreachable.

---

## 🧱 Design Decisions and Caveats

**Core Design Choices**
- Pure optical LiFi for the demo — no Wi-Fi or RF in the main communication layer.  
- Uniform node architecture across Lamp Nodes; Router adds memory and caching.  
- SOS button includes cooldown to prevent spam and simulate real operational constraints.  
- HQ interacts with the mesh purely via optical LiFi.

**Demo Limitations**
- Tabletop-scale prototype (5 nodes).  
- No strong ACK or retransmission mechanisms — minor data loss possible.  
- Minimal security (intentionally omitted for clarity and speed).  
- IR timing and framing are placeholder-level; real LiFi modulation planned for next phase.

**Security Stance**
- Security deprioritized for the demo phase.  
- Real deployments will add integrity checks and authentication to prevent spoofing or false messages.

---

## 🧩 Design Challenges & Mitigations

| # | Problem / Challenge | Solution / Mitigation | Notes / Derived |
|---|----------------------|-----------------------|-----------------|
| 1 | Message duplication & loops in IR mesh | Implemented cache with circular buffer (`CACHE_SIZE=3`) and sender-hash deduplication | Ensures messages are forwarded only once |
| 2 | SOS button spam / accidental multiple presses | Edge detection + cooldown (`SOS_COOLDOWN=3min`) | Prevents repeated SOS from same node |
| 3 | Message integrity / corruption | Added 16-bit polynomial hash for each message | Messages with hash mismatch are discarded |
| 4 | Collisions / simultaneous forwarding | Optional random backoff before forwarding | Helps reduce IR collisions |
| 5 | Hash recomputation overhead for SOS | Precomputed SOS hash (`SOS_HASH`) | Avoids unnecessary CPU cycles |
| 6 | Lamp-to-phone LiFi: message missed if no people | Periodic repeat of latest broadcast (every 1 min) using `millis()` | Ensures eventual reception; non-blocking |
| 7 | Header format & parsing consistency | Fixed 13-character header `[src(4)][dst(4)][type(1)][hash(4)]` | Standardizes message structure |
| 8 | Lamp light / LiFi placeholder | `LAMP_LIGHT_PIN` used for visual transmission | Upgradable to actual LiFi driver later |
| 9 | Potential IR message loss due to delays | Recommended: short bursts + framing markers | Improves reliability in scaled deployments |
| 10 | Cache size for small-scale demo | Set `CACHE_SIZE=3` (enough for 5-node mesh) | Balances reliability and memory footprint |
| 11 | HQ not directly in mesh | Defined HQ as separate node; receives via hops | Matches final system design and documentation |

---

## 🧭 Next Steps / To-Do

| Task | Description | Priority |
|------|--------------|-----------|
| 🧠 Real LiFi Integration (IR Layer) | Replace IR placeholders with actual LiFi/IR hardware drivers using proper modulation and framing. | High |
| 💡 LiFi-to-Phone Communication | Implement visible-light modulation for direct alerts to phone receivers or dongles. | High |
| 🖥️ HQ Software / Dashboard | Build HQ node logging and visualization software (serial + GUI) for message and alert tracking. | Medium |
| 🔧 Modular Refactor | Split current monolithic code into per-module files (`ir_comm.cpp`, `cache.cpp`, etc.) under `/structure`. | Medium |
| 🧩 Node Role Implementation | Adapt current node skeleton into three role variants: Lamp Node, Router Node, and HQ Node. | Medium |
| ⚙️ Simulation Mode | Add serial or Wokwi simulation to test message propagation without hardware. | Low |
| 🔐 Lightweight Security Layer | Add checksum or lightweight encryption for real-world deployments. | Low |
| 📖 Extended Documentation | Add diagrams, wiring schematics, and setup notes under `/hardware` and `/docs`. | Medium |

---

## 🪴 Repository Structure (Planned)

