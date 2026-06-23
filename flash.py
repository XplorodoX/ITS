#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
AALeC-V3 Firmware Flasher & Release Downloader
Automates downloading the latest firmware.bin from GitHub and flashing it to ESP8266.
Supports macOS, Linux, and Windows. Automatically detects the correct serial port.
"""

import sys
import os
import platform
import subprocess
import json
import urllib.request
import argparse
import time

def install_and_import(package_name, import_name=None):
    if import_name is None:
        import_name = package_name
    try:
        return __import__(import_name)
    except ImportError:
        print(f"Installiere fehlende Abhängigkeit: {package_name}...")
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", package_name])
        except Exception:
            try:
                subprocess.check_call([sys.executable, "-m", "pip", "install", "--user", package_name])
            except Exception as e:
                print(f"Fehler bei der Installation von {package_name}: {e}")
                print(f"Bitte installieren Sie {package_name} manuell mit: pip install {package_name}")
                sys.exit(1)
        return __import__(import_name)

# Ensure dependencies are available
serial = install_and_import("pyserial", "serial")
import serial.tools.list_ports

# Check if esptool is installed
try:
    import esptool
except ImportError:
    print("Installiere esptool...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "esptool"])
    except Exception as e:
        print(f"Fehler bei der Installation von esptool: {e}")
        print("Bitte installieren Sie esptool manuell mit: pip install esptool")
        sys.exit(1)

def find_esp_ports():
    ports = list(serial.tools.list_ports.comports())
    candidates = []
    
    # Common keywords for USB-to-UART bridges
    keywords = ["usb", "uart", "serial", "cp210", "ch34", "ftdi", "pl2303", "silicon", "prolific", "wch", "qinheng"]
    
    for p in ports:
        device = p.device
        description = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        
        is_candidate = any(kw in description or kw in hwid for kw in keywords)
        
        system = platform.system()
        if system == "Darwin":
            if "cu.usbserial" in device or "cu.wchusbserial" in device or "cu.usbmodem" in device:
                is_candidate = True
        elif system == "Linux":
            if "ttyUSB" in device or "ttyACM" in device:
                is_candidate = True
        elif system == "Windows":
            if "com" in device.lower() and device.lower() != "com1":
                is_candidate = True
                
        if is_candidate:
            candidates.append(p)
            
    return candidates

def select_port(cli_port):
    if cli_port:
        print(f"Verwende manuell angegebenen Port: {cli_port}")
        return cli_port
        
    print("\n--- Port-Auswahl ---")
    print("Suche nach angeschlossenen USB-Serial-Geräten...")
    candidates = find_esp_ports()
    
    if not candidates:
        all_ports = list(serial.tools.list_ports.comports())
        if not all_ports:
            print("\nKeine seriellen Schnittstellen gefunden!")
            print("Stellen Sie sicher, dass Ihr ESP8266 per USB-Datenkabel angeschlossen ist.")
            port = input("Bitte geben Sie den Port manuell ein (z.B. COM3 oder /dev/cu.usbserial-110): ").strip()
            return port
        else:
            print("\nKeine typischen USB-Serial-Adapter erkannt. Gefundene System-Ports:")
            for i, p in enumerate(all_ports):
                print(f"[{i+1}] {p.device} ({p.description})")
            print(f"[{len(all_ports)+1}] Port manuell eingeben")
            
            while True:
                try:
                    choice = input(f"Auswahl (1-{len(all_ports)+1}): ").strip()
                    if not choice:
                        continue
                    idx = int(choice) - 1
                    if 0 <= idx < len(all_ports):
                        return all_ports[idx].device
                    elif idx == len(all_ports):
                        return input("Port eingeben: ").strip()
                except ValueError:
                    print("Ungültige Eingabe. Bitte eine Zahl eingeben.")
    
    if len(candidates) == 1:
        port = candidates[0].device
        print(f"\nAutomatisch erkanntes USB-Gerät: {port} ({candidates[0].description})")
        confirm = input("Diesen Port verwenden? [Y/n]: ").strip().lower()
        if confirm in ("", "y", "yes"):
            return port
            
    print("\nVerfügbare USB-Serial-Geräte:")
    for i, p in enumerate(candidates):
        print(f"[{i+1}] {p.device} ({p.description})")
    print(f"[{len(candidates)+1}] Alle Systemports anzeigen")
    print(f"[{len(candidates)+2}] Port manuell eingeben")
    
    while True:
        try:
            choice = input(f"Auswahl (1-{len(candidates)+2}): ").strip()
            if not choice:
                continue
            idx = int(choice) - 1
            if 0 <= idx < len(candidates):
                return candidates[idx].device
            elif idx == len(candidates):
                all_ports = list(serial.tools.list_ports.comports())
                print("\nAlle verfügbaren Ports:")
                for j, p in enumerate(all_ports):
                    print(f"[{j+1}] {p.device} ({p.description})")
                sub_choice = input(f"Auswahl (1-{len(all_ports)}): ").strip()
                return all_ports[int(sub_choice)-1].device
            elif idx == len(candidates) + 1:
                return input("Port manuell eingeben: ").strip()
        except (ValueError, IndexError):
            print("Ungültige Auswahl. Bitte erneut wählen.")

def get_git_repo():
    try:
        # Führe Git-Befehl aus, um die Remote-URL zu ermitteln
        result = subprocess.run(
            ["git", "remote", "get-url", "origin"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        url = result.stdout.strip()
        # Entferne .git am Ende, falls vorhanden
        if url.endswith(".git"):
            url = url[:-4]
        # Extrahiere owner/repo
        if "github.com/" in url:
            parts = url.split("github.com/")[-1].split("/")
            if len(parts) >= 2:
                return f"{parts[0]}/{parts[1]}"
        elif "github.com:" in url:
            parts = url.split("github.com:")[-1].split("/")
            if len(parts) >= 2:
                return f"{parts[0]}/{parts[1]}"
    except Exception:
        pass
    return "XplorodoX/ITS"  # Fallback

def download_release_from_github(local_file_path, repo=None, releases_count=3, asset_name="firmware.bin"):
    if not repo:
        repo = get_git_repo()
        
    url = f"https://api.github.com/repos/{repo}/releases"
    print(f"\n--- GitHub Release Auswahl ---")
    print(f"Frage die neuesten Releases von GitHub ab ({repo})...")
    
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Python-esptool-downloader"})
        with urllib.request.urlopen(req, timeout=10) as response:
            releases = json.loads(response.read().decode('utf-8'))
            
        if not isinstance(releases, list) or len(releases) == 0:
            raise Exception("Keine Releases auf GitHub gefunden.")
            
        # Filter die neuesten Releases, die das gewünschte Asset besitzen
        valid_releases = []
        for release in releases:
            assets = release.get("assets", [])
            for asset in assets:
                if asset.get("name") == asset_name:
                    valid_releases.append({
                        "tag_name": release.get("tag_name", "unknown"),
                        "name": release.get("name", ""),
                        "download_url": asset.get("browser_download_url")
                    })
                    break
            if len(valid_releases) >= releases_count:
                break
                
        if not valid_releases:
            raise Exception(f"Keine Releases mit dem Asset '{asset_name}' gefunden.")
            
        print(f"\nVerfügbare Firmware-Releases auf GitHub ({repo}):")
        for idx, rel in enumerate(valid_releases):
            name_str = f" ({rel['name']})" if rel['name'] else ""
            print(f"[{idx + 1}] {rel['tag_name']}{name_str}")
            
        while True:
            try:
                choice = input(f"Bitte wählen Sie ein Release (1-{len(valid_releases)}): ").strip()
                if not choice:
                    continue
                choice_idx = int(choice) - 1
                if 0 <= choice_idx < len(valid_releases):
                    selected_release = valid_releases[choice_idx]
                    break
                else:
                    print(f"Bitte eine Zahl zwischen 1 und {len(valid_releases)} eingeben.")
            except ValueError:
                print("Ungültige Eingabe. Bitte eine Zahl eingeben.")
                
        print(f"\nAusgewähltes Release: {selected_release['tag_name']}")
        download_url = selected_release['download_url']
        
        print(f"Lade {asset_name} herunter...")
        
        def report_progress(block_num, block_size, total_size):
            read_so_far = block_num * block_size
            if total_size > 0:
                percent = min(100, int(read_so_far * 100 / total_size))
                sys.stdout.write(f"\rFortschritt: {percent}% ({read_so_far // 1024} KB / {total_size // 1024} KB)")
            else:
                sys.stdout.write(f"\rFortschritt: {read_so_far // 1024} KB")
            sys.stdout.flush()
            
        urllib.request.urlretrieve(download_url, local_file_path, report_progress)
        print(f"\nDownload von {asset_name} erfolgreich abgeschlossen.")
        return True
        
    except Exception as e:
        print(f"\nFehler beim Herunterladen von GitHub: {e}")
        if os.path.exists(local_file_path):
            print(f"Verwende die bereits lokal vorhandene Datei: {local_file_path}")
            return True
        return False

def find_local_firmware(filename="firmware.bin"):
    paths = [
        filename,
        f"src/firmware/Controller/.pio/build/esp12e/{filename}",
        f"src/firmware/Controller/{filename}"
    ]
    for p in paths:
        if os.path.exists(p):
            return p
    return None

def main():
    parser = argparse.ArgumentParser(description="Automatisches Herunterladen und Flashen des AALeC-V3 ESP8266-Controllers.")
    parser.add_argument("--port", help="Serieller Port (z.B. COM3 oder /dev/cu.usbserial-110). Wenn nicht angegeben, wird er gesucht.")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate zum Flashen (Standard: 115200 für hohe Zuverlässigkeit, bei Timeout reduzieren oder erhöhen)")
    parser.add_argument("--local", action="store_true", help="Kein GitHub-Download, verwende eine lokale firmware.bin")
    parser.add_argument("--file", default="firmware.bin", help="Pfad zur lokalen Firmware-Datei (Standard: firmware.bin)")
    parser.add_argument("--repo", default=None, help="GitHub Repository (z.B. XplorodoX/ITS). Wenn nicht angegeben, wird es über Git ermittelt.")
    parser.add_argument("--releases-count", type=int, default=3, help="Anzahl der anzuzeigenden Releases (Standard: 3)")
    parser.add_argument("--asset", default="firmware.bin", help="Name des Firmware-Assets auf GitHub (Standard: firmware.bin)")
    
    args = parser.parse_args()
    
    print("==================================================")
    print("   AALeC-V3 ESP8266 Firmware Flasher & Downloader")
    print("==================================================")
    
    firmware_file = args.file
    # Falls der Standardwert verwendet wird, aber ein anderes Asset gewählt wurde,
    # passen wir den lokalen Dateinamen an.
    if args.file == "firmware.bin" and args.asset != "firmware.bin":
        firmware_file = args.asset
    
    if args.local:
        print(f"Verwende lokalen Modus. Suche nach {firmware_file}...")
        local_path = find_local_firmware(firmware_file)
        if local_path and local_path != firmware_file:
            print(f"Gefundene lokale Firmware: {local_path}")
            use_found = input(f"Diese Firmware verwenden? ({local_path}) [Y/n]: ").strip().lower()
            if use_found in ("", "y", "yes"):
                firmware_file = local_path
                
        if not os.path.exists(firmware_file):
            print(f"Fehler: Firmware-Datei {firmware_file} existiert nicht!")
            sys.exit(1)
    else:
        success = download_release_from_github(
            firmware_file,
            repo=args.repo,
            releases_count=args.releases_count,
            asset_name=args.asset
        )
        if not success:
            print("\nGitHub Download fehlgeschlagen.")
            local_path = find_local_firmware(firmware_file)
            if local_path:
                print(f"Eine lokale Firmware-Datei wurde gefunden: {local_path}")
                use_local = input("Möchten Sie diese stattdessen flashen? [Y/n]: ").strip().lower()
                if use_local in ("", "y", "yes"):
                    firmware_file = local_path
                else:
                    sys.exit(1)
            else:
                print("Fehler: Keine Firmware-Datei verfügbar.")
                sys.exit(1)
                
    port = select_port(args.port)
    if not port:
        print("Fehler: Kein Port ausgewählt.")
        sys.exit(1)
        
    print(f"\nBereite Flashen vor...")
    print(f"Datei: {firmware_file}")
    print(f"Port:  {port}")
    print(f"Baud:  {args.baud}")
    print("--------------------------------------------------")

    # EEPROM löschen (optional)
    erase = input("\nEEPROM vorher löschen? (Namen werden zurückgesetzt) [y/N]: ").strip().lower()
    if erase in ("y", "yes"):
        erase_cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "esp8266",
            "--port", port,
            "erase_flash"
        ]
        print("Lösche Flash (inkl. EEPROM) …")
        erase_result = subprocess.run(erase_cmd)
        if erase_result.returncode != 0:
            print("Fehler beim Löschen des Flash. Abbruch.")
            sys.exit(erase_result.returncode)
        print("Flash erfolgreich gelöscht.\n")

    # Run esptool using the python interpreter
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp8266",
        "--port", port,
        "--baud", str(args.baud),
        "write_flash", "0x0", firmware_file
    ]
    
    print(f"Führe Befehl aus: {' '.join(cmd)}")
    print("Flashen startet in 2 Sekunden. Bitte Controller per USB verbunden lassen...")
    time.sleep(2)
    
    try:
        # Run esptool process
        result = subprocess.run(cmd)
        if result.returncode == 0:
            print("\n==================================================")
            print("         SUCCESS: Flashen erfolgreich!            ")
            print("==================================================")
            print("Starten Sie den Controller neu (Reset-Knopf drücken).")
        else:
            print("\n==================================================")
            print("         ERROR: Flashen fehlgeschlagen.           ")
            print("==================================================")
            print("\nTipps zur Behebung bei Verbindungs-Timeout:")
            print("1. Halten Sie die 'FLASH'-Taste am Controller gedrückt.")
            print("2. Drücken Sie kurz die 'RST'-Taste (Reset).")
            print("3. Lassen Sie die 'FLASH'-Taste los.")
            print("4. Starten Sie dieses Skript erneut.")
            print("5. Versuchen Sie es mit einer niedrigeren Baudrate, z.B.:")
            print(f"   python {sys.argv[0]} --baud 96000")
            print("6. Nutzen Sie ein hochwertigeres oder kürzeres USB-Kabel.")
            sys.exit(result.returncode)
    except KeyboardInterrupt:
        print("\nAbgebrochen durch Benutzer.")
        sys.exit(1)
    except Exception as e:
        print(f"\nEin unerwarteter Fehler ist aufgetreten: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
