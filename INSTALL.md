# Hybrid Skylanders Portal for Cemu — Install Guide

This is a modified build of **Cemu** that lets you use a **real Skylanders Portal of Power**
*and* Cemu's built‑in emulated portal **at the same time**. Real figures you place on the
physical portal and virtual figures you load from dumps all show up together in the game.

> ⚠️ This is a **separate Cemu build**, not an add‑on. You can't bolt it onto a normal Cemu —
> you run this build instead of (or alongside) your regular Cemu. It doesn't touch your existing
> Cemu install, saves, or games.

---

## What you need

- A **Wii U Skylanders game** that already works in normal Cemu (Trap Team, Swap Force, Giants, …)
- A **real Skylanders Portal of Power** (the USB one) + its USB cable
- Windows 10/11, 64‑bit
- 5 minutes

---

## Step 1 — Get the hybrid Cemu

**Option A — Download the ready‑made build (easiest)**
1. Go to the **[Releases page](../../releases)** of this repo.
2. Download the latest **`Cemu-Hybrid-Portal.zip`**.
3. Extract it anywhere you like (e.g. `C:\Cemu-Hybrid`). That folder contains `Cemu_release.exe`
   and a `resources` folder — that's the whole program.
4. Run **`Cemu_release.exe`**. First launch: finish the short setup, then point it at your games
   (same as normal Cemu).

**Option B — Build it yourself (advanced)**
1. `git clone https://github.com/rhsts/cemu-hybrid-skylander-portal.git`
2. Follow Cemu's standard [build instructions](BUILD.md) for your OS.
   The hybrid changes are already in the `hybrid-portal` branch — nothing extra to do.

> 💡 Your normal Cemu and this one don't interfere. You can keep both.

---

## Step 2 — Let the real portal talk to Cemu (one‑time driver setup)

Cemu reads the portal through a generic USB driver, so you swap its driver once with a free tool.

1. **Plug in** the Portal of Power. (No need for a figure yet.)
2. Download **Zadig** from <https://zadig.akeo.ie/> and run it.
3. In Zadig: menu **Options → List All Devices**.
4. In the dropdown, pick **Spyro Portal** (USB ID `1430 0150`).
5. Set the driver on the right to **WinUSB** (libusbK also works), then click
   **Replace Driver** / **Install Driver**.
6. Done. You only do this once. *(To use the portal with other software later, you'd switch it
   back to the default driver — but for this build, leave it on WinUSB.)*

---

## Step 3 — Turn on Hybrid mode in Cemu

> ⚠️ **Set this BEFORE starting the game.** The Hybrid switch only takes effect when a game
> starts — toggling it while a game is running does nothing. Changed your mind mid-game?
> Change the checkbox, then restart the game.

1. Open this Cemu build (no game running yet).
2. Top menu: **Tools → Emulated USB Devices**.
3. Open the **Skylanders Portal** tab.
4. Tick **☑ Hybrid (also use real portal)**.
   - That's the only box you need — Hybrid already includes the emulated portal.
5. Start your Skylanders game. The real portal **lights up with the game's colours** once it's
   running — that means it's connected. 🎉

---

## Step 4 — Use it

- **Physical figures:** just place a figure on the real portal — it appears in the game and fills
  the lowest free slot. Remove it and it leaves the game. Level‑ups/changes are written back to
  the real figure, exactly like on a console.
- **Virtual figures:** in the same Skylanders Portal tab, click **Load** on any empty slot and pick
  a `.sky` dump.
- **Unplugged the portal mid‑game?** No problem — plug it back in and the figures come back by
  themselves within a few seconds.
- **Both at once:** keep a figure on the portal **and** Load a dump — they all show up together.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Portal doesn't light up / figures ignored | Make sure it's **plugged in** and the LED can power on. Re‑check **Zadig** shows `1430 0150` on **WinUSB**. Toggle the **Hybrid** checkbox off and on, then restart the game (the switch only applies at game launch). |
| "A toy on the portal has a problem" | That's the game rejecting bad figure **data** — usually a corrupt `.sky` dump. Try a known‑good dump. (Physical figures are read directly and are fine.) |
| Portal worked, then stopped after re‑plugging | Re‑open Zadig and confirm the driver is still **WinUSB** on `1430 0150`. |
| Nothing in **Tools → Emulated USB Devices** | You're running normal Cemu, not this hybrid build. Launch `Cemu_release.exe` from the hybrid folder. |

---

*Built on [Cemu](https://github.com/cemu-project/Cemu) (MPL‑2.0). Hybrid portal support added in
the `hybrid-portal` branch; see `HYBRID_PORTAL_INTEGRATION.md` for technical details.*
