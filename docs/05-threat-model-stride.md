# 05 — Threat Model (STRIDE)

## 1. Scope
- **In scope:** the `cydui` library (core, backends, tools) and the **usage profile** of a typical firmware (CYD + WiFi + HTTP backend in the homelab). Network/storage controls belong to the app, but the library must not get in the way and should provide safe hooks.
- **Out of scope:** advanced physical security (chip decapping), vulnerabilities in the ESP-IDF/WiFi stack, security of the homelab itself.

## 2. Assets
| ID | Asset | Value |
|----|-------|-------|
| A1 | User data (tasks, calendar) | Privacy |
| A2 | Credentials (WiFi PSK, backend token) | Access to network/service |
| A3 | Firmware integrity | Trust in the device |
| A4 | UI availability | Usefulness |
| A5 | Integrity of what is displayed | Avoids misleading the user |

## 3. Data flow diagram and boundaries
```
 [User]──touch──▶(P1 Touch driver / event parser)
     ▲                          │
     │ pixels                   ▼
 [Display]◀──SPI/DMA──(P4 Rasterizer)◀──(P3 Command list)◀──(P2 UI core: IDs, state, layout, anim)
                                                                     ▲
                                                                     │ model data
 [HTTP Backend]◀──WiFi/HTTP(S)──▶(P5 App network client)──▶(P6 App model)
                                        │
                                  [DS1 NVS/Flash: SSID, token, calibration]
 [Dev host]◀──USB/UART──▶(P7 Bootloader/serial/log)

 Trust boundaries:
  TB1  Physical world ↔ firmware (touch, USB)
  TB2  Firmware ↔ network (WiFi)
  TB3  Firmware ↔ flash (external access via USB)
  TB4  App ↔ library (the app is "trusted", but its data is not)
```

## 4. STRIDE per component

### P1 — Touch driver / event parser (TB1)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | "Forged" touch from electrical interference (phantom clicks) | AB-04 | Medium |
| T | Out-of-range or NaN coordinates reach hit-testing | AB-05 | Medium |
| R | n/a (no action log on the device) | — | Low |
| I | n/a | — | — |
| D | Event flood fills the queue and hangs | AB-04 | Medium |
| E | n/a | — | — |

### P2 — UI core (IDs, state, layout, animation) (TB4)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | ID collision inherits another widget's state (action on the wrong item) | AB-06 | Medium |
| T | Dev/data corrupts the state table (invalid pointers, multi-task without lock) | AB-15 | Medium |
| R | n/a | — | — |
| I | Residual widget state (typed text) stays in RAM after recycling | — | Low |
| D | Too many items / deep tree / infinite tweens exhaust arenas or CPU | AB-07, AB-08 | **High** |
| E | Memory corruption from arena overflow → code execution | AB-01, AB-15 | Medium (high impact) |

### P3 — Command list
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T | Command with invalid pointers/dimensions (bitmap, font) | AB-16, AB-17 | Medium |
| D | Command buffer overflow | AB-01, AB-07 | High |
| E | Out-of-bounds read/write in the raster | AB-17 | Medium (high impact) |

### P4 — Rasterizer / text / fonts
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | Text misleads the user (bidi, control characters, homoglyphs) | AB-03 | Medium |
| T | Malformed font/theme | AB-16 | Low (local artifacts, build-time) |
| I | Read past the glyph buffer exposes adjacent memory as pixels | AB-02 | Low |
| D | Huge text / quadratic line wrapping | AB-01 | High |
| E | OOB in UTF-8 decode | AB-02 | Medium (high impact) |

### P5 — App network client (TB2)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | Fake backend (DNS/ARP spoof) | AB-09 | **High** on a home LAN |
| T | Response tampered in transit (plain HTTP) | AB-10 | **High** |
| R | Backend cannot tell which device acted | — | Low |
| I | Sniffing of tasks/calendar and token | AB-10 | **High** |
| D | Huge response exhausts heap; slow backend blocks UI | AB-07 | Medium |
| E | Vulnerable JSON/HTTP parser | — | Medium |

### P6 — App model
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T/S | Malicious task content (long text, control characters, bidi) reaches the UI | AB-01..03 | Medium |

### DS1 — NVS / flash (TB3)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T | Rewriting calibration/config by someone with USB | AB-12 | Medium |
| I | Extracting SSID/token via `esptool read_flash` | AB-11 | **High** with physical access |
| D | NVS corruption prevents the app from booting | — | Low |

### P7 — Serial/log/bootloader (TB1/TB3)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T/E | Flashing malicious firmware | AB-12 | High with physical access (accept or mitigate with secure boot) |
| I | Logs with sensitive data | AB-13 | Medium |
| R | No audit trail of who flashed firmware | — | Low |

## 5. Prioritized threats
1. **High:** DoS from arena/CPU exhaustion (P2/P3/P4), app network without TLS/authentication (P5), credential extraction from flash (DS1).
2. **Medium / high impact:** memory corruption from hostile input (UTF-8, bitmap, arena overflow).
3. **Consciously accepted:** full physical access without secure boot (document it), visual spoofing with homoglyphs (partial mitigation: filter control/bidi).

Controls and residual risks are in `06-controls-and-risks.md`.
