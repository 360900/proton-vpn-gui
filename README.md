# proton-vpn-qt-app

A Qt GUI front-end for the [Proton VPN Linux CLI](https://protonvpn.com/support/linux-vpn-tool/).

> **Disclaimer:** This project is an independent community effort and is in no way affiliated with, endorsed by, or associated with Proton AG or any of its products. "Proton" and "ProtonVPN" are trademarks of Proton AG.

---

## Features

### General
- One-click **connect / disconnect** with a large power button and system tray controls
- Automatic connection detection on launch using `protonvpn status`, with active server and public IP display
- **Background polling** every 15 seconds — detects external state changes (CLI disconnect, reconnect, or location switch) and updates the UI without any user action
- Secure login with **interactive 2FA support** and inline validation
- Confirmation dialog when quitting while the VPN is active — leave it running or disconnect cleanly
- Single-instance protection to prevent duplicate launches
- Proton-inspired dark theme with KDE Breeze (when available) or Fusion styling
- Informational banners for CLI version mismatches and pre-release builds

### Free & Plus Plan Awareness
- Detects the user's plan (Free or Plus) at login and on launch
- **Account page** shows plan type (Free / VPN Plus) with a direct upgrade link for Free users
- **Countries page**: the Connect button is locked with a tooltip for Free users — Proton picks the server automatically
- **VPN page**: location picker is disabled for Free users (forbidden cursor + tooltip); recent connections picker is hidden
- **Settings page**: Plus-only features (NAT Type, VPN Accelerator, NetShield, Port Forwarding, Custom DNS, Recent Connections) are grouped under a `✦ Available to Plus Members` divider and rendered at reduced opacity for Free users

### Location & Country Selection *(Plus)*
- Browse and search countries and cities with feature tags (P2P, Tor, Secure Core, etc.)
- **Country flags** in the countries list, detected from system locale/timezone
- **Location picker** on the main VPN page — choose the fastest server or a specific city, with country flag and feature icons
- **Recent connections** picker — quick access to previously used locations, with configurable history depth

### Settings
**App tab**
- Launch on Startup via a systemd user service, with optional Auto-connect
- Desktop Notifications for connect/disconnect events
- *(Plus)* Recent Connections count (0–20) and one-click history clear

**VPN tab**
- Anonymous Crash Reports, IPv6, Kill Switch — available on all plans
- *(Plus)* NAT Type, VPN Accelerator, NetShield Ad-blocker, Port Forwarding, Custom DNS

---

## Screenshots
<img width="670" height="629" alt="Screenshot_20260318_142949" src="https://github.com/user-attachments/assets/0ceaabad-f436-4363-9916-96dbdfd957ed" />
<div>Country List - Wide View</div>
<img width="670" height="629" alt="Screenshot_20260318_142959" src="https://github.com/user-attachments/assets/01ac1c89-825e-475c-83b2-db0d90001158" />
<div>Country List - Narrow View</div>
<img width="571" height="629" alt="Screenshot_20260318_143126" src="https://github.com/user-attachments/assets/4c25b20e-71d6-4c6d-a02e-40829c36c59e" />
<img width="535" height="630" alt="Screenshot_20260307_151135" src="https://github.com/user-attachments/assets/687d6bde-039e-4ac4-8fb7-c7de7f2374a6" />
<img width="670" height="629" alt="Screenshot_20260318_143011" src="https://github.com/user-attachments/assets/d590e188-0a05-42f4-9e45-35bbf2c6bbf0" />
<img width="571" height="629" alt="Screenshot_20260318_143126" src="https://github.com/user-attachments/assets/f87d3d76-b70b-44b6-87a9-3a6e8c6643ac" />

---

## Requirements

| Dependency | Purpose |
|---|---|
| `protonvpn` CLI | Core VPN control — sign in, connect, disconnect, status, country/city lists, settings |
| Qt 6 (Core, Gui, Widgets, Svg, SvgWidgets) | UI framework |
| `curl` | **Optional** — fetches your public IP address when already connected on launch |
| `systemd` (user session) | **Optional** — required for the "Launch on Startup" feature |

The app communicates exclusively with the `protonvpn` CLI. `curl` degrades gracefully if absent.

---

## Building

```bash
git clone https://github.com/wheat32/proton-vpn-qt-app.git
cd proton-vpn-qt-app/src
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The resulting binary is `build/proton_vpn_qt`.

### Dependencies — Arch Linux

```bash
sudo pacman -S qt6-base qt6-svg cmake ninja
```

### Dependencies — Ubuntu / Debian

```bash
sudo apt install qt6-base-dev qt6-svg-dev libqt6svg6-dev cmake ninja-build
```

---

## Usage

```bash
./build/proton_vpn_qt
```

The app requires the `protonvpn` CLI to be installed and accessible in your `PATH`. See the [Proton VPN Linux documentation](https://protonvpn.com/support/linux-vpn-tool/) for installation instructions.

---

## CI / Releases

- **Pull requests** to `main` automatically trigger a build via GitHub Actions to verify the code compiles.
- **Pushes to `main`** automatically build a release binary and publish a [GitHub Release](../../releases) tagged with the version from `src/version.json` (e.g. `v1.1.0`).

The release archive (`proton-vpn-qt-app-linux-x86_64.tar.gz`) contains the single `proton_vpn_qt` binary:

```bash
tar -xzf proton-vpn-qt-app-linux-x86_64.tar.gz
./proton_vpn_qt
```

---

## Author & Credits

- **Nicholas Page** ([wheat32](https://github.com/wheat32)) — author
- Icons from [Bootstrap Icons](https://icons.getbootstrap.com/) (MIT License)
- Built with [Qt 6](https://www.qt.io/)
- Uses the [ProtonVPN Linux CLI](https://protonvpn.com/support/linux-vpn-tool/)
- Country flags from [Flag Icons](https://flagicons.lipis.dev/) (MIT License)

---

## Contributing

Pull requests and issues are welcome. Please note that this project has no access to Proton VPN's internal APIs. It can only do what the public `protonvpn` CLI exposes.

### Branching and Pull Requests

- All pull requests should target the `dev` branch, not `main`
- The `main` branch is reserved for stable, tested releases
- Pull requests submitted to `main` may be retargeted to `dev`

---

## License

GPLv3
