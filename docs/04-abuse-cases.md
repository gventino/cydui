# 04 — Abuse Cases

Abuse = what a hostile agent (or a gross mistake) can attempt against the library or against firmware built with it. Context: home device, on the local network, possibly physically accessible to third parties (guests, shared desk).

## Hostile actors
- **Network:** someone on the user's LAN/WiFi (guest, compromised IoT device).
- **Physical:** someone with access to the board (USB, screen).
- **Content:** malicious data coming from the backend or typed in (e.g. a task title imported from another system).
- **Careless dev:** library user who misuses the API (also a source of failures).

## Cases
| ID | Abuse | Target | Potential impact |
|----|-------|--------|------------------|
| AB-01 | Huge text (MBs) or with thousands of lines passed to `label`/`text_input`. | Text/layout | Arena overflow, render hang, watchdog reset. |
| AB-02 | Invalid UTF-8 / overlong / surrogates / truncated sequences. | UTF-8 decoder | Out-of-buffer read, nonexistent glyph indexed. |
| AB-03 | Strings with control characters or bidi (RTL override) to forge what is displayed (e.g. a "fake" task). | Render | Visual spoofing / misleading the user. |
| AB-04 | Touch event flood (electrical noise or physical attack). | Input queue | Queue overflows; UI hangs; phantom clicks on destructive actions. |
| AB-05 | Out-of-range coordinates, NaN in the calibration matrix. | Calibration / hit-test | Out-of-bounds access; touches mapped off-screen. |
| AB-06 | Widget ID collision (identical labels in lists) to inherit another item's state. | State table | One item's checkbox changes another; cross-wired state. |
| AB-07 | Too many widgets/items per frame (list of 10⁵ items from the backend). | Command list / state | DoS via arena overflow; FPS → 0. |
| AB-08 | Never-ending animation loop or huge number of tweens. | Animation | 100% CPU, power drain, screen always "dirty". |
| AB-09 | Fake backend on the network (ARP/DNS spoof) answering the app. | App/network | Fake data shown; token exfiltration. |
| AB-10 | Intercepting plaintext HTTP traffic (sniffing) to read tasks/calendar. | App/network | Leak of personal data. |
| AB-11 | Reading flash over USB (`esptool read_flash`) to extract SSID/password/token. | Flash/NVS | Credential leak. |
| AB-12 | Flashing malicious firmware over USB. | Device | Full device takeover. |
| AB-13 | Serial console logs sensitive data (tokens, content). | Logs | Leak via USB/monitor. |
| AB-14 | Shoulder surfing / always-on screen with personal data. | Display | Visual leak. |
| AB-15 | Dev passes invalid pointers, tiny arenas, or calls the API from several tasks without a lock. | API | Memory corruption, hard-to-debug crashes. |
| AB-16 | Malformed font/theme (corrupt glyph table, null paint pointer). | Theme/font | Crash during rasterization. |
| AB-17 | Bitmap with declared dimensions larger than the actual data. | Bitmap raster | Out-of-buffer read. |
| AB-18 | Wrong Kconfig for the backend (bad pins) → electrical short or output on a strapping pin. | Hardware | Physical damage/unstable boot. |
| AB-19 | (Added in It8, for text input) A source of typed characters and keys (the keyboard on the screen, the SDL keyboard, an application that forwards text from a phone) sends floods, control characters, bidi marks, invalid code points or `\n` into a one-line field. | Key queue, text field | Overflow, forged text, a field that grows without a limit. |
| AB-20 | (It8) A frame that costs far more than a normal one: hundreds of full-screen translucent or rounded commands (a bug, not a remote input: widgets draw small rectangles). | Rasterizer | The UI loop stalls for seconds; other tasks starve. |
| AB-21 | (It8) Code the library runs on behalf of the application misbehaves: a post-effect, a painter, a backend, a custom widget (a write past its strip, a loop). | Plug-ins | Memory corruption or a hang inside the "library". |
| AB-22 | (It8) A modal popup, a focus or the keyboard that nothing can dismiss (the application never clears `open`, or calls `cydui_focus_set` every frame). | Popup, focus | The screen stays blocked or typing goes to a field the user did not choose. |

## "Handled case" criterion
Each AB must have, in `06-controls-and-risks.md`, at least one control and, for library-level ones (AB-01, 02, 04–08, 15–17, 19), a **test** (unit or fuzz) that exercises it.
