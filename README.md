# proton-vpn-qt-app

A community-built Qt 6 GUI front-end for the [Proton VPN Linux CLI](https://protonvpn.com/support/linux-vpn-tool/).

> **Disclaimer:** This project is an independent community effort and is in no way affiliated with, endorsed by, or associated with Proton AG or any of its products. "Proton" and "ProtonVPN" are trademarks of Proton AG.

---

## Features

- One-click **connect / disconnect** with a large power button and system tray controls
- **Location picker** on the main page — shows the fastest server or a specific city for your detected country, with country flag and feature icons (P2P, Tor, Secure Core)
- Automatic connection detection on launch with active server and public IP display
- Secure login with **interactive 2FA support** and inline validation
- Browse and search countries and cities with feature tags (P2P, Tor, Secure Core, Streaming, etc.)
- **Country flags** in the countries list, detected from system locale/timezone
- Connect to the fastest server or choose a specific location
- Confirmation dialog when quitting while the VPN is active — leave it running or disconnect cleanly
- Account page with username display and sign-out
- Startup options including **launch on login** (systemd user service) and optional auto-connect
- Single-instance protection to prevent duplicate launches
- Proton-inspired dark theme with KDE Breeze (when available) or Fusion styling
- About dialog with version, CLI compatibility information, and open-source credits

---

## Requirements

| Dependency | Purpose |
|---|---|
| `protonvpn` CLI | Core VPN control (sign in, connect, disconnect, country/city lists) |
| Qt 6 (Core, Gui, Widgets, Svg, SvgWidgets) | UI framework |
| `ip` (`iproute2`) | Detecting whether the VPN tunnel is active |
| `nmcli` (NetworkManager) | **Optional** — shows the active server name when already connected on launch |
| `curl` | **Optional** — fetches your public IP address when already connected on launch |
| `systemd` (user session) | **Optional** — required for the "Launch on Startup" feature |

The app works without `nmcli` and `curl`; those features degrade gracefully.

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

Pull requests and issues are welcome. Please note that this project has no access to Proton VPN's internal APIs — it can only do what the public `protonvpn` CLI exposes.

---

## License

GPLv3
