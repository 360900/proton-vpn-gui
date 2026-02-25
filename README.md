# proton-vpn-qt-app

A community-built Qt GUI front-end for the [Proton VPN Linux CLI](https://protonvpn.com/support/linux-vpn-tool/).

> **Disclaimer:** This project is an independent community effort and is in no way affiliated with, endorsed by, or associated with Proton AG or any of its products. "Proton" and "ProtonVPN" are trademarks of Proton AG.

---

## Features

- Sign in with your Proton VPN credentials, including two-factor authentication (2FA)
- Detects whether the VPN is already connected on launch (via `ip a` / `proton0` interface)
- Displays the active server name (via `nmcli`) and your current public IP (via `curl ifconfig.me`) when already connected
- Browse and search the full country list
- Browse cities per country with feature tags (P2P, Tor, Secure Core, etc.)
- Connect to the fastest server, or pick a specific country/city
- Connection elapsed timer (only shown when you connect through the app)
- Account page showing your username
- Animated spinners during all loading states
- Dark Proton-branded color scheme

## Requirements

| Dependency | Purpose |
|---|---|
| `protonvpn` CLI | Core VPN control (sign in, connect, disconnect, country/city lists) |
| Qt 6 (Core, Gui, Widgets, Svg, SvgWidgets) | UI framework |
| `ip` (`iproute2`) | Detecting whether the VPN tunnel is active |
| `nmcli` (NetworkManager, optional) | Showing the active server name on launch |
| `curl` (optional) | Fetching your public IP address on launch |

The app will work without `nmcli` and `curl` — those features simply degrade gracefully.

## Building

```bash
git clone https://github.com/your-username/proton-vpn-qt-app.git
cd proton-vpn-qt-app/src
cmake -B build
cmake --build build
```

The resulting binary is `build/proton_vpn_qt`.

### Dependencies (Arch Linux)

```bash
sudo pacman -S qt6-base qt6-svg
```

### Dependencies (Ubuntu/Debian)

```bash
sudo apt install qt6-base-dev qt6-svg-dev libqt6svg6-dev cmake
```

## Usage

```bash
./build/proton_vpn_qt
```

The app requires the `protonvpn` CLI to be installed and accessible in your `PATH`. See the [Proton VPN Linux documentation](https://protonvpn.com/support/linux-vpn-tool/) for installation instructions.

## Contributing

Pull requests and issues are welcome. Please note that this project has no access to Proton VPN's internal APIs — it can only do what the public `protonvpn` CLI exposes.

## License

GPLv3
