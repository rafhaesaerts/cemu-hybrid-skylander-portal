# Hybrid Skylanders Portal — Cemu integration

Adds a **hybrid** mode to Cemu's emulated Skylander portal: figures placed on a **real
Portal of Power** (USB `1430:0150`) are merged into the same 16-slot view as **virtual**
(dump-backed) figures, so a game sees one portal carrying both at once.

This builds *on top of* Cemu's existing `SkylanderUSB` (it reuses the real 2-bit status
word, the `queuedStatus` add/remove animation, `QueryBlock`/`WriteBlock`, and the figure
list) rather than replacing it. The only genuinely new component is a libusb bridge to the
physical portal.

## How it works

```
   game ── HID ──> SkylanderPortalDevice ──> g_skyportal (SkylanderUSB, 16 slots)
                                                 │  ▲   each slot is virtual OR physical
                                                 │  │
                              physical writes /  │  │  OnPhysicalAdd / OnPhysicalRemove
                              LED colour         ▼  │  (+ cached block data)
                                            PhysicalPortalBridge  ── libusb ──> real portal
                                            (own context + thread)              1430:0150
```

* **Arrival/removal** — the bridge polls the real portal's interrupt-IN stream, parses its
  status word, and on a new figure caches all `0x40` blocks (via `Q`) then calls
  `SkylanderUSB::OnPhysicalAdd`, which drops it into the lowest free slot and queues the
  `ADDED → READY` animation — identical to how a virtual figure is loaded. Removal queues
  `REMOVING → REMOVED`.
* **Reads (`Q`)** — served from the cached block data, exactly like virtual figures (no code
  change in `QueryBlock`).
* **Writes (`W`)** — for a physical slot, forwarded to the real portal via the bridge
  instead of being saved to a dump file (`WriteBlock`).
* **LEDs (`C`)** — mirrored onto the real portal.
* **Exclusive ownership** — `BackendLibusb` is told to skip `1430:0150` while hybrid mode is
  on, so the bridge is the sole owner of the device (otherwise two paths would fight over it).

## Files

New:
* `src/Cafe/OS/libs/nsyshid/PhysicalPortalBridge.{h,cpp}` — libusb bridge (own context+thread).

Changed:
* `Skylander.{h,cpp}` — `physical`/`portalIndex` per slot; `StartHybrid`/`StopHybrid`/
  `OnPhysicalAdd`/`OnPhysicalRemove`; physical write + LED forwarding.
* `BackendEmulated.cpp` — start hybrid when enabled.
* `BackendLibusb.cpp` — skip `1430:0150` while hybrid active.
* `config/CemuConfig.{h,cpp}` — `emulate_skylander_portal_hybrid` (XML key `EmulateSkylanderPortalHybrid`).
* `gui/.../EmulatedUSBDeviceFrame.{h,cpp}` — "Hybrid (also use real portal)" checkbox.
* `src/Cafe/CMakeLists.txt` — build the bridge.

## Enable it

1. Build Cemu normally **with libusb** (`-DENABLE_LIBUSB=ON`, the default where supported).
2. Set up the WinUSB/libusb driver for `1430:0150` (Windows: Zadig — same requirement as
   Cemu's existing physical-portal passthrough).
3. In *Emulated USB Devices → Skylanders Portal*, tick **Emulate Skylander Portal** and
   **Hybrid (also use real portal)**. (Or set `EmulateSkylanderPortalHybrid=true` in
   `settings.xml`.) Takes effect on the next game load / backend re-attach.

## Verification status — read this

* ✅ `PhysicalPortalBridge.cpp` was compiled `-fsyntax-only -Wall -Wextra` (both with and
  without `HAS_LIBUSB`) against minimal libusb/precompiled shims — clean.
* ⚠️ **The full Cemu project was NOT built here** (no vcpkg/Vulkan/wxWidgets toolchain in
  the dev environment). Please build it to confirm the `Skylander.cpp`/backend/GUI edits.
* ⚠️ **Not tested against real hardware.** The live USB behaviour is modelled on Cemu's
  emulated `SkylanderUSB` and the published protocol. Points likely to need a one-line tweak
  during hardware bring-up are marked **`VERIFY:`** in `PhysicalPortalBridge.cpp`:
  * the **status-word layout** parsed in `HandleStatus` (2 bits/slot, slot 0 lowest);
  * the **index byte** sent in `Q`/`W` requests (`portalIndex` vs `0x10|portalIndex`);
  * **packet sizes** (32-byte commands, interrupt-IN read).

## Test plan (with a real portal)

1. Enable hybrid, boot a Skylanders game, place **one physical** figure → it should appear
   and the game should play the "new Skylander" cinematic.
2. Load **one virtual** figure via the dialog → both physical and virtual usable at once.
3. Level both up → physical change persists to the real figure's NFC; virtual to its dump.
4. Lift the physical figure mid-game → it should disappear; virtual stays.
5. Toggle hybrid off → behaviour returns to stock (emulated-only or libusb passthrough).

## Known limitations

* Toggling the checkbox applies on next backend re-attach, not instantly (same as the
  existing *Emulate Skylander Portal* toggle).
* Block pre-cache delivers after all blocks arrive or a short timeout (partial on timeout).
* LED forwarding covers the common `C` command; `J`/`L` variants aren't forwarded yet.
* Audio (Trap Team) still uses Cemu's emulated mono speaker path; not routed to the real
  portal's speaker.
