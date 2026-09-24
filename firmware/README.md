# Firmware — sentinel node

**Owner:** TBD
**Depends on:** `docs/PROTOCOL.md`, `hardware/` AFE characterisation
**Deliverable for M3:** three dev boards gossiping over real LoRa, quorum firing correctly.

Do not wait for the PCB. Start on dev boards today: an STM32L4 Nucleo or ESP32-S3 devkit plus an SX1262 breakout gets you 80% of the firmware done before the board exists.

## 1. Target

| | Choice | Notes |
|---|---|---|
| MCU | STM32L432KC, or ESP32-S3 | STM32 for the power budget; ESP32 if the team already knows it |
| Radio | SX1262, 865–867 MHz | India ISM band. Not 915 MHz — that is unlicensed elsewhere, not here. |
| Toolchain | PlatformIO | one command builds for both targets |
| RTOS | none, or FreeRTOS if the mesh needs it | a superloop with a timer ISR is enough and easier to reason about |

## 2. Structure

```
firmware/
├── platformio.ini
├── src/
│   ├── main.c
│   ├── adc.c               DMA sampling, 10-cycle RMS envelope
│   ├── baseline.c          EWMA, τ ≈ 10 min
│   ├── detector.c          SUSPECT / RECOVERED state machine
│   ├── arbiter.c           C port of backend decide() — must match vectors
│   ├── mesh.c              LoRa gossip, slotting, quorum collection
│   ├── radio.c             SX1262 driver wrapper
│   ├── power.c             sleep scheduling, battery ADC
│   └── proto.c             message pack/unpack per PROTOCOL.md
└── test/                   runs arbiter.c against docs/vectors/ on host
```

## 3. Signal chain

```
probe → AFE → ADC (DMA, 1 kHz) → 10-cycle window (200 ms)
        → RMS → EWMA baseline → deviation_pct → detector state machine
```

- Sample at 1 kHz — 20 samples per 50 Hz cycle, comfortably above Nyquist with margin for harmonics.
- RMS over exactly 10 cycles (200 ms) so a whole number of cycles is always integrated; partial cycles produce a beat artefact that looks like a fault.
- Baseline must not adapt fast enough to follow a break. τ ≈ 10 min tracks temperature and load drift while a genuine collapse stays far below baseline indefinitely. If τ is too short, the node "forgets" the fault and silently returns to NORMAL with a broken wire on the ground. This is the single most dangerous parameter in the firmware.
- Freeze baseline adaptation entirely while in SUSPECT or CONFIRMED.

## 4. Detector state machine

```
NORMAL ──deviation < −60% for 5 consecutive windows──▶ SUSPECT
SUSPECT ──deviation > −20%──▶ RECOVERED ──2 s──▶ NORMAL
SUSPECT ──quorum met──▶ CONFIRMED
CONFIRMED ──manual reset or RESTORE command──▶ NORMAL
```

Thresholds live in ArbiterConfig (PROTOCOL.md §5) and must be runtime-configurable, not compile-time constants — the utility will want to tune per feeder.

## 5. LoRa mesh

- Slotted TDMA. Each node gets a transmit slot derived from `node_id % N`. Pure ALOHA collides badly the moment several nodes go SUSPECT at once — which is exactly when you need the messages through.
- Time sync from the gateway beacon, once per minute. Cheap GPS PPS is an option if drift proves problematic.
- Priority path: a SUSPECT assertion pre-empts routine telemetry and transmits immediately, out of slot. Latency budget for the whole gossip round is ~800 ms.
- Packet security: 4-byte node ID, rolling `seq`, CRC, and an AES-128-CCM MAC with a per-feeder key. An unauthenticated radio that can trigger a feeder isolation is a vulnerability a judge will find.
- Range: SF9, BW 125 kHz, CR 4/5 as the starting point. Measure actual span range in the field and adjust; higher SF buys range but costs airtime, and airtime is your latency budget.

## 6. Power

Target average current < 5 mA.

- Sleep between sampling windows. Sampling duty cycle around 20%.
- Radio RX windows scheduled, not continuous listen.
- Wake on the priority path immediately when local detector asserts.
- Budget: 6 V 1 W panel + 1 × 18650 LiFePO4, sized for 5 days of monsoon overcast. Show this calculation in the PPT.

## 7. Task list

### Phase 1 — dev board (start now)

- [ ] PlatformIO project, blink, UART logging
- [ ] SX1262 driver, two boards exchanging a packet
- [ ] ADC + DMA sampling at 1 kHz, RMS over 10 cycles, print to UART
- [ ] Feed the ADC from a signal generator and verify RMS accuracy

### Phase 2 — detection

- [ ] EWMA baseline, verified against the simulator's implementation on the same input
- [ ] Detector state machine, all four transitions
- [x] Port `decide()` to `arbiter.c`; host tests pass every vector in `docs/vectors/` (`firmware/test/run.sh`, gcc `-Werror` clean)
- [x] CI check that the C and Python arbiters agree on all vectors (`firmware/test/run.sh` parity gate: pytest + C vectors + gateway HIL). `gateway/src/arbiter.c` is a symlink to this file — one implementation, not a copy.

### Phase 3 — mesh

- [ ] Slot assignment and time sync from gateway beacon
- [ ] Gossip: SUSPECT broadcast, neighbour reply, vote collection with window expiry
- [ ] Priority pre-emption path
- [ ] AES-CCM packet auth
- [ ] Three-board quorum test → M3

### Phase 4 — reliability

- [ ] Independent watchdog, brownout detector
- [ ] Store-and-forward buffer for telemetry during gateway outage
- [ ] `seq` handling across reboot
- [ ] Power profiling with a real meter, confirm < 5 mA average
- [ ] OTA over LoRa — genuinely optional, cut it if time is short

## 8. Acceptance criteria for M3

1. Three boards, real radio, physical simulation of collapse (remove the probe from the field source) → quorum in under 1.5 s.
2. Remove one board's power → the others report it OFFLINE and do not treat it as a vote.
3. Collapse all three simultaneously → no quorum assertion (global collapse veto).
4. C arbiter matches Python on every vector.

## 9. Gotchas

- Baseline τ is a safety parameter, not a tuning knob. See §3.
- The probe is high impedance. Long unshielded leads between plate and buffer will pick up everything in the enclosure. Coordinate with `hardware/` on lead dressing; this will bite during bring-up.
- 50 Hz is not exactly 50 Hz. Indian grid frequency drifts roughly 49.5–50.5 Hz. Do not hard-code the cycle length in samples; track zero crossings or use a wide enough window that the error is negligible.
- `/*` inside a C comment (e.g. writing `docs/vectors/*.json`) trips `-Werror=comment`. Write such paths as `docs-vectors` in comments.

- Do not implement a second, divergent version of the decision logic "just for the node." One `decide()`, ported, tested against shared vectors.
