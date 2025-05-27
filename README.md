# 🌤️ Wetterstation 2025

> Tamino Beckmann & Marcel Ebner  
> 📅 Abgabedatum: 27.05.2025

## 🔧 Projektübersicht

Im Projekt „Wetterstation 2025“ wurde ein ESP32-Mikrocontroller verwendet, um Temperatur, Luftfeuchtigkeit und Erschütterungen zu messen. Die Werte werden über ein OLED-Display, Webserver, Blynk-App und Discord angezeigt.

## ⚙️ Verwendete Komponenten

- ESP32 Mini C3
- DHT11 Sensor
- B23 Shock Sensor
- OLED-Display (SSD1306)
- RGB-LED
- Webserver mit HTML/JS
- MariaDB + PHP-API
- Discord Webhook
- Blynk App

## 📷 Aufbau

![Aufbau](Protokoll-Bilder/Aufbau.png)

## 🧪 Features

- 🌡 Temperaturmessung (DHT11)
- 💧 Luftfeuchtigkeitsmessung
- 🚨 Erschütterungserkennung (B23 Sensor)
- 🖥️ OLED-Display mit Uhrzeit und Messwerten
- 🌐 Webserver mit Live-Werten & Steuerung
- ☁️ Cloudanbindung über Blynk & Discord
- 🔌 Sleep-Mode für Stromsparen

## 📁 Webserver

Weboberfläche mit Anzeige der Sensordaten und Schaltflächen:

![Webserver](Protokoll-Bilder/Startseite.png)

## 💾 Datenbank

Daten werden alle 10 Sekunden in eine MariaDB-Datenbank eingetragen über eine `insert_temp.php`-API.

![DataGrip](Protokoll-Bilder/Webserver.png)

## 🔴 Status-LED Logik

- 🟢 **Grün** – alles OK  
- 🔴 **Rot** – kein WLAN oder Erschütterung  
- 🟠 **Orange** – Temperatur über 28 °C

![Status LED](Protokoll-Bilder/LED.png)

## 📱 Discord & Blynk

### Discord-Benachrichtigung (alle 5 Minuten):

![Discord](Protokoll-Bilder/Discord.png)

### Anzeige & Steuerung per Blynk App:

![Blynk](Protokoll-Bilder/Blynk.png)

## 🧠 Sleep Mode

Webgesteuerter Soft-Sleep-Modus: Display und LEDs werden deaktiviert, Sensoren und Netz bleiben aktiv.

---

📜 Finaler Code ist in `Wetterstation-Code.ino` enthalten.

---

## 🔚 Abschluss

Die vollständige technische Dokumentation mit Codebeispielen, Troubleshooting und Setup-Anleitung findet ihr in `WetterstationProtkoll_BeckmannEbner.pdf`.

