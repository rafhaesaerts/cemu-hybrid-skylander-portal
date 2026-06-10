# 🛡️ Hybrid Skylanders Portal for Cemu

A modified build of **Cemu** that lets you use a **real Skylanders Portal of Power** *and* Cemu's
built‑in emulated portal **at the same time**. Real figures you place on the physical portal and
virtual figures you load from `.sky` dumps all show up together in the game.

> ⚠️ This is a **separate Cemu build**, not an add‑on. Run this build instead of (or alongside)
> your normal Cemu — it doesn't touch your existing Cemu install, saves, or games.

## What you need

- A **Wii U Skylanders game** that already works in normal Cemu (Trap Team, Swap Force, Giants, …)
- A **real Skylanders Portal of Power** (the USB one) + its USB cable
- Windows 10/11, 64‑bit · ~5 minutes

## Step 1 — Get the hybrid Cemu

**Option A — Download the ready‑made build (easiest)**
1. Go to the **[Releases page](../../releases)** and download the latest **`Cemu-Hybrid-Portal.zip`**.
2. Extract it anywhere (e.g. `C:\Cemu-Hybrid`). It contains `Cemu_release.exe` and a `resources` folder — that's the whole program.
3. Run **`Cemu_release.exe`**, finish the short first‑launch setup, and point it at your games (same as normal Cemu).

**Option B — Build it yourself (advanced)**
1. `git clone https://github.com/rhsts/cemu-hybrid-skylander-portal.git`
2. Follow Cemu's standard [build instructions](BUILD.md). The hybrid changes are already on the `hybrid-portal` branch — nothing extra to do.

> 💡 Your normal Cemu and this one don't interfere. You can keep both.

## Step 2 — Let the real portal talk to Cemu (one‑time driver setup)

Cemu reads the portal through a generic USB driver, so you swap its driver once with a free tool.

1. **Plug in** the Portal of Power. (No figure needed yet.)
2. Download **[Zadig](https://zadig.akeo.ie/)** and run it.
3. Menu **Options → List All Devices**.
4. In the dropdown, pick **Spyro Portal** (USB ID `1430 0150`).
5. Set the driver on the right to **WinUSB** (libusbK also works), then click **Replace Driver** / **Install Driver**.
6. Done — you only do this once. *(To use the portal with other software later you'd switch it back to the default driver; for this build, leave it on WinUSB.)*

## Step 3 — Turn on Hybrid mode

> ⚠️ **Set this BEFORE starting the game.** The Hybrid switch only takes effect when a game starts — toggling it while a game is running does nothing. Changed your mind mid-game? Change the checkbox, then restart the game.

1. Open this Cemu build (no game running yet).
2. Top menu: **Tools → Emulated USB Devices**.
3. Open the **Skylanders Portal** tab.
4. Tick **☑ Hybrid (also use real portal)** — that's the only box you need (Hybrid already includes the emulated portal).
5. Start your Skylanders game. The real portal **lights up with the game's colours** once it's running — that means it's connected. 🎉

## Step 4 — Use it

- **Physical figures:** place a figure on the real portal — it appears in the game and fills the lowest free slot. Remove it and it leaves. Level‑ups/changes are written back to the real figure, just like on a console.
- **Virtual figures:** in the same tab, click **Load** on an empty slot and pick a `.sky` dump.
- **Unplugged the portal mid‑game?** No problem — plug it back in and the figures come back by themselves within a few seconds.
- **Both at once:** keep a figure on the portal **and** Load a virtual trap/item — they show up together. In some games (e.g. Trap Team) the first virtual you load onto a portal that already has a physical figure triggers a quick one‑time re‑scan so the game notices it; after that the portal stays stable as you load, swap, or clear more virtuals.

## Troubleshooting

| Problem | Fix |
|---|---|
| Portal doesn't light up / figures ignored | Make sure it's **plugged in** and the LED can power on. Re‑check **Zadig** shows `1430 0150` on **WinUSB**. Toggle the **Hybrid** checkbox off and on, then restart the game (the switch only applies at game launch). |
| "A toy on the portal has a problem" | The game is rejecting bad figure **data** — usually a corrupt `.sky` dump. Try a known‑good dump. (Physical figures are read directly and are fine.) |
| Portal worked, then stopped after re‑plugging | Re‑open Zadig and confirm the driver is still **WinUSB** on `1430 0150`. |
| Nothing in **Tools → Emulated USB Devices** | You're running normal Cemu, not this build. Launch `Cemu_release.exe` from the hybrid folder. |

📄 Full guide: **[INSTALL.md](INSTALL.md)** · Technical details: **[HYBRID_PORTAL_INTEGRATION.md](HYBRID_PORTAL_INTEGRATION.md)**

---

<details>
<summary><b>About the underlying Cemu emulator</b> (click to expand)</summary>

# **Cemu - Wii U emulator**

[![Build Process](https://github.com/cemu-project/Cemu/actions/workflows/build.yml/badge.svg)](https://github.com/cemu-project/Cemu/actions/workflows/build.yml)
[![Discord](https://img.shields.io/discord/286429969104764928?label=Cemu&logo=discord&logoColor=FFFFFF)](https://discord.gg/5psYsup)
[![Matrix Server](https://img.shields.io/matrix/cemu:cemu.info?server_fqdn=matrix.cemu.info&label=cemu:cemu.info&logo=matrix&logoColor=FFFFFF)](https://matrix.to/#/#cemu:cemu.info)

This is the code repository of Cemu, a Wii U emulator that is able to run most Wii U games and homebrew in a playable state.
It's written in C/C++ and is being actively developed with new features and fixes.

Cemu is currently only available for 64-bit Windows, Linux & macOS devices.

### Links:
 - [Open Source Announcement](https://www.reddit.com/r/cemu/comments/wwa22c/cemu_20_announcement_linux_builds_opensource_and/)
 - [Official Website](https://cemu.info)
 - [Compatibility List/Wiki](https://wiki.cemu.info/wiki/Main_Page)
 - [Official Subreddit](https://reddit.com/r/Cemu)
 - [Official Discord](https://discord.gg/5psYsup)
 - [Official Matrix Server](https://matrix.to/#/#cemu:cemu.info)
 - [Setup Guide](https://cemu.cfw.guide)

#### Other relevant repositories:
 - [Cemu-Language](https://github.com/cemu-project/Cemu-Language)
 - [Cemu's Community Graphic Packs](https://github.com/cemu-project/cemu_graphic_packs)

## Download

You can download the latest Cemu releases for Windows, Linux and Mac from the [GitHub Releases](https://github.com/cemu-project/Cemu/releases/). For Linux you can also find Cemu on [flathub](https://flathub.org/apps/info.cemu.Cemu).

On Windows, Cemu is available both as an installer and in a portable format, where no installation is required besides extracting it in a safe place.

The native macOS build is currently purely experimental and should not be considered stable or ready for issue-free gameplay. There are also known issues with degraded performance due to the use of MoltenVK and Rosetta for ARM Macs. We appreciate your patience while we improve Cemu for macOS.

Pre-2.0 releases can be found on Cemu's [changelog page](https://cemu.info/changelog.html).

## Build Instructions

To compile Cemu yourself on Windows, Linux or macOS, view [BUILD.md](/BUILD.md).

## Issues

Issues with the emulator should be filed using [GitHub Issues](https://github.com/cemu-project/Cemu/issues).  
The old bug tracker can be found at [bugs.cemu.info](https://bugs.cemu.info) and still contains relevant issues and feature suggestions.

## Contributing

Pull requests are very welcome. For easier coordination you can visit the developer discussion channel on [Discord](https://discord.gg/5psYsup) or alternatively the [Matrix Server](https://matrix.to/#/#cemu:cemu.info).
Before submitting a pull request, please read and follow our code style guidelines listed in [CODING_STYLE.md](/CODING_STYLE.md).

If coding isn't your thing, testing games and making detailed bug reports or updating the (usually outdated) compatibility wiki is also appreciated!

Questions about Cemu's software architecture can also be answered on Discord (or through the Matrix bridge).

## License
Cemu is licensed under [Mozilla Public License 2.0](/LICENSE.txt). Exempt from this are all files in the dependencies directory for which the licenses of the original code apply as well as some individual files in the src folder, as specified in those file headers respectively.

</details>
