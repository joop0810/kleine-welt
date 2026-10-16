#!/usr/bin/env python3
"""Schreibt das Manifest fuer den Web-Installer und prueft die Sicherheitsregeln.

Aufruf (im Repo-Hauptordner):
  python3 tools/make_manifest.py            # Mallorca-Deko: firmware/ -> manifest.json
  python3 tools/make_manifest.py kumpel     # Kumpel: firmware-kumpel/ -> manifest-kumpel.json

Bricht ab, wenn
  - die Partitionstabelle nicht mehr exakt der von TamaPoke entspricht
    (sonst koennten Spielstand/Ruhmeshalle von TamaPoke verloren gehen),
  - "new_install_prompt_erase" fehlt (ohne diese Angabe LOESCHT esp-web-tools
    das Geraet bei einer Neuinstallation automatisch).
"""
import hashlib, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# sha256 von partitions.bin aus app3M_fat9M_16MB - identisch mit TamaPoke 4.1.6
PARTITIONS_SHA256 = "ace02503447d0f470692e65fa76002f2d77a92dc81cd3813d8aa66718d716da9"

VARIANTS = {
    # Name, Firmware-Ordner, Manifest, Quelldatei mit FW_VERSION, Zusatzfelder
    "kleinewelt": ("Kleine Welt", "firmware", "manifest.json", "firmware-src/KleineWelt/KleineWelt.ino", {}),
    # Kumpel spricht Improv: der Installer fragt nach dem Flashen direkt nach dem WLAN
    "kumpel": ("Kumpel", "firmware-kumpel", "manifest-kumpel.json", "firmware-src/Kumpel/Kumpel.ino",
               {"new_install_improv_wait_time": 20}),
}


def sha(p):
    return hashlib.sha256((ROOT / p).read_bytes()).hexdigest()


def main():
    key = sys.argv[1] if len(sys.argv) > 1 else "kleinewelt"
    name, fwdir, out, src, extra = VARIANTS[key]
    parts = [(f"{fwdir}/bootloader.bin", 0x0), (f"{fwdir}/partitions.bin", 0x8000),
             (f"{fwdir}/boot_app0.bin", 0xE000), (f"{fwdir}/app.bin", 0x10000)]
    if sha(f"{fwdir}/partitions.bin") != PARTITIONS_SHA256:
        sys.exit("FEHLER: partitions.bin weicht von TamaPoke ab - nicht veroeffentlichen!")
    ino = (ROOT / src).read_text(encoding="utf-8")
    version = re.search(r'#define FW_VERSION "([0-9.]+)"', ino).group(1)
    manifest = {
        "name": f"{name} {version}",
        "version": version,
        # PFLICHT: zeigt die Frage "Erase device" (Kaestchen standardmaessig leer).
        # Fehlt das, loescht esp-web-tools bei Neuinstallation alles.
        "new_install_prompt_erase": True,
        **extra,
        "builds": [{
            "chipFamily": "ESP32-S3",
            # ?v=<hash>: GitHub Pages cached; so passt app.bin immer zum Manifest
            "parts": [{"path": f"{p}?v={sha(p)[:16]}", "offset": off} for p, off in parts],
        }],
    }
    (ROOT / out).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"{out} fuer {name} {version} geschrieben")


if __name__ == "__main__":
    main()
