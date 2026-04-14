Netzwerk-Aufbau

Arduino Uno (WiFi-Hotspot)
  SSID: "AALeC-Quiz"
  Pass: "12345678"
  IP des APs: 192.168.4.1
       │
       ├── AALeC (ESP8266) verbindet sich → bekommt z.B. 192.168.4.2
       │
       └── Dein Mac verbindet sich → bekommt z.B. 192.168.4.3
                │
                └── Mosquitto :1883 + :9001
                    Game Master (Docker)
Schritt 1 — Mac mit dem Arduino-Hotspot verbinden
Mac WLAN → AALeC-Quiz (Passwort: 12345678)
Deine neue IP herausfinden:

ipconfig getifaddr en0
Das Ergebnis (z.B. 192.168.4.3) ist die Broker-IP für den AALeC.

Schritt 2 — config.h prüfen / anpassen
Die config.h hat bereits:


#define WIFI_SSID     "AALeC-Quiz"
#define WIFI_PASSWORD "12345678"
#define MQTT_BROKER   "192.168.4.3"   // ← ggf. deine echte IP eintragen
#define MQTT_PORT     1883
Falls deine IP abweicht, einfach anpassen und neu flashen.

Schritt 3 — Docker-Services auf der richtigen Netzwerkschnittstelle erreichbar machen
Die Container binden bereits auf 0.0.0.0:1883 — das bedeutet sie sind auf allen Netzwerkinterfaces erreichbar, also auch über das WLAN-Interface wenn du im Hotspot bist.

Kurz prüfen ob der Broker von außen antwortet:


# mosquitto_pub/sub ist im Container — teste von deinem Mac aus
# (mosquitto-clients installieren falls nicht vorhanden)
brew install mosquitto

# Auf einem Terminal: alle Quiz-Topics lauschen
mosquitto_sub -h 127.0.0.1 -p 1883 -t "quiz/#" -v

# AALeC-Registrierung simulieren (zweites Terminal):
mosquitto_pub -h 127.0.0.1 -p 1883 \
  -t "quiz/connect/aAlec-TEST01" \
  -m '{"device_id":"aAlec-TEST01","name":"Testplayer"}'
Du solltest im Game-Master-Log sehen:


[register] aAlec-TEST01 → 'Testplayer'  (1 players)
Schritt 4 — AALeC flashen und verbinden
AALeC per USB an den Mac
In PlatformIO: Upload (oder pio run -t upload)
Serial Monitor öffnen (115200 Baud) — du siehst:

Connecting to AALeC-Quiz ...
WiFi connected. IP: 192.168.4.2
[MQTT] connected as aAlec-XXXXXX
Schritt 5 — Spiel starten
Das Spiel startet aktuell automatisch (wegen --auto-start), wartet aber auf Spieler. Du brauchst einen MQTT-Start-Trigger. Einfachste Methode — direkt per mosquitto_pub ein Spiel-Start-Signal injizieren:

Da der Game Master derzeit nur über [s]-Tastatur oder Code startet, kannst du einen schnellen Workaround nutzen — ein quiz/cmd-Topic. Ich kann das einbauen wenn du willst. Alternativ: den Container kurz interaktiv starten:


# Game-Master-Container interaktiv mit Terminal:
podman exec -it its_game-master_1 python game_master.py \
  --broker mosquitto --questions questions.json
Dann s + Enter → Spiel startet.

Schritt 6 — Ablauf beobachten

# Alle MQTT-Nachrichten live sehen:
mosquitto_sub -h 127.0.0.1 -p 1883 -t "quiz/#" -v
Du siehst dann den kompletten Ablauf:


quiz/state   {"state":"QUESTION","question_id":1,"remaining_s":20}
quiz/question {"id":1,"text":"...","options":{...},"time_limit_s":20}
quiz/state   {"state":"VOTING",...}
quiz/answer/aAlec-XXXXXX {"question_id":1,"answer":"B","elapsed_ms":4200}
quiz/reveal  {"question_id":1,"correct":"B","counts":{...}}
quiz/scores  {"scores":[...]}
Kurzfassung
Was	Wie
Mac ins Hotspot-WLAN	SSID AALeC-Quiz, PW 12345678
Broker-IP prüfen	ipconfig getifaddr en0
config.h anpassen	MQTT_BROKER auf deine Mac-IP setzen
AALeC flashen	PlatformIO Upload
Topics lauschen	mosquitto_sub -h 127.0.0.1 -p 1883 -t "quiz/#" -v
Spiel starten	podman exec -it its_game-master_1 python game_master.py --broker mosquitto --questions questions.json → s
Willst du, dass ich noch ein quiz/cmd-Topic in den Game Master einbaue, damit du das Spiel per MQTT starten kannst (ohne interaktives Terminal)?

