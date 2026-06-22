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

def download_latest_release(local_file_path):
    repo = "XplorodoX/ITS"
    url = f"https://api.github.com/repos/{repo}/releases/latest"
    print(f"\n--- GitHub Release Download ---")
    print(f"Frage neuestes Release von GitHub ab ({repo})...")
    
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Python-esptool-downloader"})
        with urllib.request.urlopen(req, timeout=10) as response:
            release_data = json.loads(response.read().decode('utf-8'))
            
        tag_name = release_data.get("tag_name", "unknown")
        print(f"Neueste Version auf GitHub: {tag_name}")
        
        assets = release_data.get("assets", [])
        download_url = None
        for asset in assets:
            if asset.get("name") == "firmware.bin":
                download_url = asset.get("browser_download_url")
                break
                
        if not download_url:
            raise Exception("Keine 'firmware.bin' im neuesten GitHub Release gefunden.")
            
        print(f"Lade firmware.bin herunter...")
        
        def report_progress(block_num, block_size, total_size):
            read_so_far = block_num * block_size
            if total_size > 0:
                percent = min(100, int(read_so_far * 100 / total_size))
                sys.stdout.write(f"\rFortschritt: {percent}% ({read_so_far // 1024} KB / {total_size // 1024} KB)")
            else:
                sys.stdout.write(f"\rFortschritt: {read_so_far // 1024} KB")
            sys.stdout.flush()
            
        urllib.request.urlretrieve(download_url, local_file_path, report_progress)
        print("\nDownload erfolgreich abgeschlossen.")
        return True
        
    except Exception as e:
        print(f"\nFehler beim Herunterladen von GitHub: {e}")
        if os.path.exists(local_file_path):
            print(f"Verwende die bereits lokal vorhandene Datei: {local_file_path}")
            return True
        return False

def find_local_firmware():
    paths = [
        "firmware.bin",
        "src/firmware/Controller/.pio/build/esp12e/firmware.bin",
        "src/firmware/Controller/firmware.bin"
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
    
    args = parser.parse_args()
    
    print("==================================================")
    print("   AALeC-V3 ESP8266 Firmware Flasher & Downloader")
    print("==================================================")
    
    firmware_file = args.file
    
    if args.local:
        print(f"Verwende lokalen Modus. Suche nach {firmware_file}...")
        local_path = find_local_firmware()
        if local_path and local_path != firmware_file:
            print(f"Gefundene lokale Firmware: {local_path}")
            use_found = input(f"Diese Firmware verwenden? ({local_path}) [Y/n]: ").strip().lower()
            if use_found in ("", "y", "yes"):
                firmware_file = local_path
                
        if not os.path.exists(firmware_file):
            print(f"Fehler: Firmware-Datei {firmware_file} existiert nicht!")
            sys.exit(1)
    else:
        success = download_latest_release(firmware_file)
        if not success:
            print("\nGitHub Download fehlgeschlagen.")
            local_path = find_local_firmware()
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
