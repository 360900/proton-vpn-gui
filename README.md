# Proton VPN GUI

A friendly Linux desktop app for [Proton VPN](https://protonvpn.com).

You sign in once, pick a country, hit the big button, and you're online through a
Proton VPN server. No terminal, no command-line flags, no fiddling with config
files. The app handles signing in, picking a fast server, reconnecting when the
link drops, and remembering your favorites so the next time is one click.

> Heads up: this project is made by the community. Proton AG made Proton VPN,
> the brand, and the CLI. We are not Proton AG. Proton and ProtonVPN are
> trademarks of Proton AG.

## What you get

- A clear main screen with a big Connect button and the country you're in.
- A searchable list of every Proton VPN country, with flags and feature tags
  like P2P, Tor, and Secure Core.
- A small map view that animates while you connect and again on disconnect.
- Favorites and a recent-connections list so returning to the same place takes
  one click.
- The same controls in a tray icon: connect or disconnect from the menu bar.
- Adjustable appearance that respects your system's light or dark theme, with
  a reduced-motion setting for calmer animations.
- Toggles for Proton-supported features that the CLI exposes, including kill
  switch, IPv6, NAT type, VPN accelerator, NetShield, and port forwarding.
- Desktop notifications when you connect or disconnect.
- Auto-launch on login, with an optional auto-connect to your last server.

## Install on Linux

Pick the one that matches your setup.

### Arch Linux and Manjaro (from source)

The package is not published on the AUR yet, but the repo includes a ready-to-use
`PKGBUILD`. Build and install it locally with:

```bash
git clone https://github.com/360900/proton-vpn-gui.git
cd proton-vpn-gui/packaging/aur
makepkg -si
```

A `Proton VPN GUI` launcher appears in your application menu. Pushing it to the
AUR as an official package is a manual step the maintainers have not done yet.

### Build a Flatpak yourself (most other distros)

The Flatpak is not published on Flathub yet. While the submission is prepared,
you can build and install it from this repository with the included script.

```bash
git clone https://github.com/360900/proton-vpn-gui.git
cd proton-vpn-gui
./build-flatpak.sh --local --install
flatpak run io.github._360900.ProtonVpnGui
```

This builds inside the KDE runtime and is the same pipeline that produces the
official bundle, so what you run is exactly what will land on Flathub once the
review is complete.

### Native build (Debian, Ubuntu, Fedora, others)

You will need the Proton VPN CLI, a handful of build tools, and CMake 3.31 or
newer with Qt 6.8 or newer (the QML interface relies on recent Qt Quick APIs).

Debian or Ubuntu (recent releases such as Debian trixie or Ubuntu 26.04; older
LTS releases ship too-old Qt):

```bash
sudo apt install cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qt6-svg-dev qt6-tools-dev
git clone https://github.com/360900/proton-vpn-gui.git
cd proton-vpn-gui/src
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/proton_vpn_gui
```

Fedora:

```bash
sudo dnf install cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel \
  qt6-qtsvg-devel qt6-qttools-devel
git clone https://github.com/360900/proton-vpn-gui.git
cd proton-vpn-gui/src
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/proton_vpn_gui
```

The build produces a single `proton_vpn_gui` binary that you can run directly.

## Before you start

This app is a friendly face for the **Proton VPN Linux CLI**. The CLI does the
real work: signing in, opening the tunnel, switching servers, and applying your
settings. You need to install it once before the GUI can do anything, even on
Flatpak. The Flatpak version calls the CLI on your host through Flatpak's
bridge, so the CLI has to be on your host system, not inside the Flatpak.

If you don't have it yet, follow Proton's official Linux guide:
[protonvpn.com/support/linux-vpn-tool](https://protonvpn.com/support/linux-vpn-tool/).

> Warning: the official Proton VPN GTK app and this app cannot run at the same
> time. They both talk to the same CLI and will trip over each other. Pick
> this one and uninstall the GTK app.

## Day-to-day tips

- First launch walks you through signing in. After that, opening the app takes
  you straight to the main screen.
- The globe animates while a connection is being established and again when you
  disconnect, so you can tell at a glance what's happening even if you have
  the window minimized.
- Right-click the tray icon for quick connect, disconnect, or quit options.
- Free tier users can still connect, but Proton picks the server for you and
  the country picker is hidden. To pick a country yourself you need a paid
  Plus plan.
- Connecting to a server that supports port forwarding enables automatic
  port forwarding. You need the optional `libnatpmp` package (Debian, Ubuntu,
  Fedora, Arch) and a Plus plan.

## Common questions

**Does this app collect my data?**
No. The app delegates everything to the Proton VPN CLI on your machine and
exposes a session D-Bus interface for status and basic control. There is no
extra telemetry from this GUI.

**Will my settings and servers carry over?**
Yes. Your favorites, recent connections, and preferences live under
`~/.config/ProtonVPN-GUI/` (or the equivalent Flatpak config path) and stay
between upgrades.

**Why does it ask for a 2FA code?**
That's Proton asking, not us. Proton sometimes needs a second factor on sign
in. The app passes it through to the CLI for you.

**The connection failed with a confusing message. What now?**
Try the same action from the Proton VPN CLI directly. The error message will
usually be the same one. If that works, the GUI will work on the next attempt.

**Can other tools talk to this app?**
Yes. It publishes a session-bus interface at
`io.github._360900.ProtonVpnGui` with status and basic connect, disconnect,
and raise-window methods. Scripts and other apps can use it to react to the
VPN state.

**When will the Flatpak be on Flathub?**
The submission is queued. Until then, build it locally with
`./build-flatpak.sh --local --install`.

## For developers

The code is split into a small C++ core and a Qt Quick interface. The core
wraps the CLI, parses its output, runs status polling, and tracks connection
state with a small state machine. The QML side reads the core and renders the
UI.

```bash
cmake -S src -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build
ctest --output-on-failure
```

Thirteen tests cover parsers, the connection state machine, the process
runner, status polling, the VPN service, NAT-PMP, configuration, connection
history, geo helpers, the Flatpak bridge, and UI helpers. Native, Flatpak,
and AppImage builds are wired into CI on every pull request to `main` or
`dev`.

## Credits

This project is a community-maintained Qt rewrite of the original
**ProtonVPN Qt App** by Nicholas Page ([wheat32](https://github.com/wheat32)).
We started from that codebase and rebuilt the core and the UI on top of
their foundations. Without them, this project would not exist.

Large parts of the file layout, the config and history paths, the tray
controller, the autostart hook, the original Flatpak packaging, the
proton-vpn-sign icon derivative, and the initial CI workflows come from the
upstream repository at
[github.com/wheat32/proton-vpn-qt-app](https://github.com/wheat32/proton-vpn-qt-app).
Their work is licensed under GPL-3.0-only, the same license this project
shipped with. If you are reading code or comments and spot a familiar
pattern, it is almost certainly because we kept it on purpose.

What this project adds on top:

- A fully Qt Quick UI under `src/qml/`, replacing the legacy QtWidgets
  scaffolding.
- A new `src/core/` library that isolates the Proton VPN CLI interaction, the
  connection state machine, status polling, settings management, and D-Bus
  interfaces. Everything in this layer has unit tests.
- A session D-Bus control interface at
  `io.github._360900.ProtonVpnGui`.
- Light and dark theme support with live system-theme following and a
  reduced-motion option.
- New AppStream metadata, an AUR package, and modernized CI workflows.

## License

This project is licensed under the GNU General Public License, version 3. See
[LICENSE](LICENSE).
