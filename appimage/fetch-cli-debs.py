#!/usr/bin/env python3
"""
Query the ProtonVPN public apt repository and print download URLs for
proton-vpn-cli at the requested version and all of its ProtonVPN Python
dependencies (those not available on PyPI).

Usage:
    python3 fetch-cli-debs.py <cli_version>

Output: one HTTPS URL per line, suitable for wget.
"""

import re
import sys
import urllib.request

REPO_BASE = "https://repo.protonvpn.com/debian"

# ProtonVPN Python packages to pull from the apt repo (not on PyPI).
# Packages are resolved in this order; each one's .deb is output.
PROTON_PYTHON_DEPS = [
    "python3-proton-core",
    "python3-proton-keyring-linux",
    "python3-proton-keyring-linux-secretservice",
    "python3-proton-vpn-api-core",
    "python3-proton-vpn-session",
    "python3-proton-vpn-logger",
    "python3-proton-vpn-connection",
    "python3-proton-vpn-local-agent",   # native amd64 package
]


def fetch_text(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "Debian APT-HTTP/1.3"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read().decode()


def parse_packages(text: str) -> dict[str, list[dict]]:
    """Parse a Debian Packages file into {name: [entry, ...]}."""
    pkgs: dict[str, list[dict]] = {}
    for block in text.split("\n\n"):
        block = block.strip()
        if not block:
            continue
        entry: dict[str, str] = {}
        for line in block.split("\n"):
            if ":" in line:
                k, _, v = line.partition(":")
                entry[k.strip()] = v.strip()
        name = entry.get("Package", "")
        if name:
            pkgs.setdefault(name, []).append(entry)
    return pkgs


def version_key(v: str) -> tuple:
    """Coerce a Debian-style version into a sortable tuple."""
    return tuple(int(x) if x.isdigit() else x for x in re.split(r"[.\-~]", v))


def latest_satisfying(entries: list[dict], min_ver: str | None,
                      max_ver: str | None = None) -> dict | None:
    valid = entries
    if min_ver:
        mv = version_key(min_ver)
        valid = [e for e in valid if version_key(e.get("Version", "0")) >= mv]
    if max_ver:
        xv = version_key(max_ver)
        valid = [e for e in valid if version_key(e.get("Version", "0")) <= xv]
    if not valid:
        valid = entries
    return max(valid, key=lambda e: version_key(e.get("Version", "0")))


def deb_url(entry: dict) -> str:
    return f"{REPO_BASE}/{entry['Filename']}"


def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <cli_version>", file=sys.stderr)
        sys.exit(1)

    cli_version = sys.argv[1]

    all_pkgs  = parse_packages(fetch_text(f"{REPO_BASE}/dists/stable/main/binary-all/Packages"))
    amd64_pkgs = parse_packages(fetch_text(f"{REPO_BASE}/dists/stable/main/binary-amd64/Packages"))

    # Find the exact CLI .deb
    cli_entries = [e for e in all_pkgs.get("proton-vpn-cli", [])
                   if e.get("Version") == cli_version]
    if not cli_entries:
        print(f"ERROR: proton-vpn-cli {cli_version} not found in repo", file=sys.stderr)
        sys.exit(1)
    cli_entry = cli_entries[0]
    print(deb_url(cli_entry))

    # Parse minimum version requirements from the CLI's Depends field
    min_versions: dict[str, str | None] = {}
    for dep in cli_entry.get("Depends", "").split(","):
        dep = dep.strip()
        m = re.match(r"([\w\-]+)\s*\(>=\s*([\d.]+)\)", dep)
        if m:
            min_versions[m.group(1)] = m.group(2)
        else:
            pkg_name = dep.split()[0] if dep else ""
            if pkg_name:
                min_versions.setdefault(pkg_name, None)

    # Resolve each ProtonVPN Python dependency
    for dep_name in PROTON_PYTHON_DEPS:
        entries = all_pkgs.get(dep_name) or amd64_pkgs.get(dep_name)
        if not entries:
            print(f"WARNING: {dep_name} not found in repo — skipping", file=sys.stderr)
            continue
        entry = latest_satisfying(entries, min_versions.get(dep_name))
        if entry:
            print(deb_url(entry))


if __name__ == "__main__":
    main()
