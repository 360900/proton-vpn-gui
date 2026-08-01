# Proton VPN GUI

Proton VPN GUI is an independent Qt 6 desktop client for the Proton VPN Linux
CLI. It is a complete Qt Quick rewrite with a tested C++ core, a responsive
server browser, connection history, favorites, tray controls, and light and
dark themes.

This project is not affiliated with or endorsed by Proton AG. Proton VPN and
the Proton logo are trademarks of Proton AG.

## Features

- Connect and disconnect from the system tray or the main window.
- Browse, search, favorite, and reconnect to Proton VPN locations.
- See the active server, connection duration, public IP, and connection state.
- Use country flags, server feature badges, and recent connections.
- Configure supported Proton VPN CLI settings, including DNS and kill switch.
- Use a session D-Bus interface for status queries and basic control.
- Run as a native Linux application or as a Flatpak that delegates CLI work to
  the host through `flatpak-spawn`.
- Respect the system color scheme, with an optional reduced-motion setting.

## Requirements

- Linux with Qt 6.8 or newer for building.
- The Proton VPN Linux CLI installed and available as `protonvpn`.
- A session D-Bus, required for tray integration and single-instance behavior.

The GUI does not implement VPN or network control itself. All VPN operations
are delegated to the Proton VPN CLI.

## Build From Source

### Debian or Ubuntu

```bash
sudo apt install cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qt6-svg-dev qt6-tools-dev libqt6dbus6
git clone https://github.com/360900/proton-vpn-gui.git
cd proton-vpn-gui/src
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/proton_vpn_gui
```

### Arch Linux

```bash
sudo pacman -S cmake ninja qt6-base qt6-declarative qt6-svg qt6-tools
git clone https://github.com/360900/proton-vpn-gui.git
cd proton-vpn-gui/src
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/proton_vpn_gui
```

Run the test suite with:

```bash
cmake --build build
cd build
ctest --output-on-failure
```

## Flatpak

The Flatpak package uses the KDE runtime and delegates every Proton VPN CLI
command to the host. The CLI must be installed outside the sandbox.

Build the current checkout and install it for the current user:

```bash
./build-flatpak.sh --local --install
flatpak run io.github._360900.ProtonVpnGui
```

The tracked manifest is `io.github._360900.ProtonVpnGui.yml`. For Flathub, use a
tagged release with a full commit pin in the manifest and submit the manifest
to the Flathub repository.

## Arch User Repository

The AUR packaging files are in `packaging/aur/`. They are kept separate from
the application source so the same release tag can be used for Flatpak, AUR,
and binary releases.

To build the package locally:

```bash
cd packaging/aur
makepkg -si
```

The package depends on the Proton VPN CLI and does not bundle it.

## D-Bus API

The application exposes the following session-bus service:

| Item | Value |
|---|---|
| Service | `io.github._360900.ProtonVpnGui` |
| Object path | `/io/github/360900/ProtonVpnGui` |
| Status interface | `io.github._360900.ProtonVpnGui.Status` |
| Control interface | `io.github._360900.ProtonVpnGui.Control` |

Read the current status with:

```bash
gdbus call --session \
  --dest io.github._360900.ProtonVpnGui \
  --object-path /io/github/360900/ProtonVpnGui \
  --method org.freedesktop.DBus.Properties.Get \
  io.github._360900.ProtonVpnGui.Status Status
```

## Project Layout

- `src/qml/`: Qt Quick interface.
- `src/core/`: CLI process, parser, polling, and state-machine code.
- `src/app/`: QML-facing application facade and models.
- `src/tests/`: Qt Test coverage for pure and near-pure logic.
- `io.github._360900.ProtonVpnGui.yml`: Flatpak manifest.
- `packaging/aur/`: Arch Linux package files.

## License

Proton VPN GUI is licensed under the GNU General Public License, version 3.
See [LICENSE](LICENSE).
