#!/usr/bin/env python3
"""Schreibt manifest.json fuer den Web-Installer und prueft die Sicherheitsregeln.

Aufruf (im Repo-Hauptordner):  python3 tools/make_manifest.py

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

PARTS = [("firmware/bootloader.bin", 0x0), ("firmware/partitions.bin", 0x8000),
         ("firmware/boot_app0.bin", 0xE000), ("firmware/app.bin", 0x10000)]


def sha(p):
    return hashlib.sha256((ROOT / p).read_bytes()).hexdigest()


def main():
    if sha("firmware/partitions.bin") != PARTITIONS_SHA256:
        sys.exit("FEHLER: partitions.bin weicht von TamaPoke ab - nicht veroeffentlichen!")
    ino = (ROOT / "firmware-src/KleineWelt/KleineWelt.ino").read_text(encoding="utf-8")
    version = re.search(r'#define FW_VERSION "([0-9.]+)"', ino).group(1)
    manifest = {
        "name": f"Kleine Welt {version}",
        "version": version,
        # PFLICHT: zeigt die Frage "Erase device" (Kaestchen standardmaessig leer).
        # Fehlt das, loescht esp-web-tools bei Neuinstallation alles.
        "new_install_prompt_erase": True,
        "builds": [{
            "chipFamily": "ESP32-S3",
            # ?v=<hash>: GitHub Pages cached; so passt app.bin immer zum Manifest
            "parts": [{"path": f"{p}?v={sha(p)[:16]}", "offset": off} for p, off in PARTS],
        }],
    }
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"manifest.json fuer Kleine Welt {version} geschrieben")


if __name__ == "__main__":
    main()
