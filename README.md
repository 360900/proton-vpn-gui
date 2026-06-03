# proton-vpn-qt-app

A Qt GUI front-end for the [Proton VPN Linux CLI](https://protonvpn.com/support/linux-vpn-tool/).

> **Disclaimer:** This project is an independent community effort and is in no way affiliated with, endorsed by, or associated with Proton AG or any of its products. "Proton" and "ProtonVPN" are trademarks of Proton AG.

---

[![Packaging status](https://repology.org/badge/vertical-allrepos/proton-vpn-qt-app.svg)](https://repology.org/project/proton-vpn-qt-app/versions)

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
- Launch on Startup via XDG autostart, with optional Auto-connect
- Desktop Notifications for connect/disconnect events
- *(Plus)* Recent Connections count (0–20) and one-click history clear

**VPN tab**
- Anonymous Crash Reports, IPv6, Kill Switch — available on all plans
- *(Plus)* NAT Type, VPN Accelerator, NetShield Ad-blocker, Port Forwarding, Custom DNS

### Port Forwarding *(Plus)*

When Port Forwarding is enabled in Settings and you are connected to a P2P server, the app automatically manages the NAT-PMP lease using `natpmpc`:

- The **forwarded port number** is displayed on the main VPN page with a one-click **Copy** button for easy use with torrent clients (e.g. Transmission, qBittorrent)
- A **keep-alive loop** runs every 45 seconds to renew the 60-second NAT-PMP lease — the port stays valid as long as the app is open
- If the app is closed while port forwarding is active, the lease will lapse within 60 seconds. A warning is shown in the quit dialog when this applies
- If `natpmpc` is not installed, a banner is shown when you connect to a P2P server with port forwarding enabled. Install it with:
  - **Debian / Ubuntu:** `sudo apt install natpmpc`
  - **Fedora:** `sudo dnf install libnatpmp`
  - **Arch:** `sudo pacman -S libnatpmp`

`natpmpc` is an **optional** dependency — users who do not use port forwarding are unaffected by its absence.

---

## Screenshots

### Main Page

<table>
  <tr>
    <td><img width="571" alt="Main page narrow" src="https://github.com/user-attachments/assets/ed419e10-124c-4533-b194-16d067cbad0d" /></td>
    <td><img width="773" alt="Main page normal width" src="https://github.com/user-attachments/assets/8c4e8a0a-213c-4404-955e-9610cb084100" /></td>
  </tr>
</table>

### Country List

<table>
  <tr>
    <td><img width="468" alt="Country list narrow" src="https://github.com/user-attachments/assets/f886d61a-b1bb-4185-a5ae-bf8ee40b71ee" /></td>
    <td><img width="626" alt="Country list wide" src="https://github.com/user-attachments/assets/37fb89c2-821a-449a-9ce5-7d603dfe4088" /></td>
  </tr>
</table>

### Account Page

<img width="626" alt="Account page" src="https://github.com/user-attachments/assets/95860a4e-156b-47f3-adce-41b9c57a7f28" />

### Settings Page

<table>
  <tr>
    <td><img width="626" alt="Settings — App tab 1" src="https://github.com/user-attachments/assets/1ccab72f-c56b-4b9e-9ac6-b0da97c294e7" /></td>
    <td><img width="626" alt="Settings — App tab 2" src="https://github.com/user-attachments/assets/acdbf5dc-e951-4fe3-b2fa-a818af4db170" /></td>
  </tr>
  <tr>
    <td><img width="626" alt="Settings — Appearance tab" src="https://github.com/user-attachments/assets/ad0d4d5b-bf2c-4552-a53f-f713bd0ea031" /></td>
    <td><img width="626" alt="Settings — VPN tab" src="https://github.com/user-attachments/assets/a05708fd-8e4e-4141-ab89-02dbd90a2043" /></td>
  </tr>
</table>

---

## Requirements

| Dependency | Purpose |
|---|---|
| `protonvpn` CLI | Core VPN control — sign in, connect, disconnect, status, country/city lists, settings |
| Qt 6 (Core, Gui, Widgets, Svg, SvgWidgets) | UI framework |
| `curl` | **Optional** — fetches your public IP address when already connected on launch |
| XDG autostart (`~/.config/autostart/`) | **Optional** — required for the "Launch on Startup" feature; supported by all major desktop environments |

The app communicates exclusively with the `protonvpn` CLI. `curl` degrades gracefully if absent.

> [!WARNING]
> **The official Proton VPN GTK app is not supported alongside this app and will cause problems.** Both apps will conflict over connection state and session management. The CLI will produce errors when two front-ends are running at the same time. If you have the GTK app installed, it is advised that you uninstall it before using this app.

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

## D-Bus Interface

While running, the app registers a D-Bus service on the **session bus** that any other application or script can query.

| | Value |
|---|---|
| **Service** | `com.protonvpn.app` |
| **Object path** | `/com/protonvpn/app` |
| **Interface** | `com.protonvpn.app.Status` |

### Properties (read-only)

| Property | Type | Description |
|---|---|---|
| `Status` | `string` | Current VPN state: `connected`, `connecting`, `disconnected`, `disconnecting`, `error`, or `unknown` |
| `ConnectedServer` | `string` | Active server name (e.g. `US-NJ#189`) while connected; empty otherwise |

### Signals

| Signal | Arguments | Description |
|---|---|---|
| `StatusChanged` | `status: string` | Emitted on every VPN state transition |

### Example usage

```bash
# Read current status
gdbus call --session \
  --dest com.protonvpn.app \
  --object-path /com/protonvpn/app \
  --method org.freedesktop.DBus.Properties.Get \
  com.protonvpn.app.Status Status

# Read all properties at once
gdbus call --session \
  --dest com.protonvpn.app \
  --object-path /com/protonvpn/app \
  --method org.freedesktop.DBus.Properties.GetAll \
  com.protonvpn.app.Status

# Monitor live state change signals
gdbus monitor --session --dest com.protonvpn.app

# Introspect the full interface
gdbus introspect --session \
  --dest com.protonvpn.app \
  --object-path /com/protonvpn/app
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

See [CONTRIBUTING.md](CONTRIBUTING.md).

---

## License

GPLv3
