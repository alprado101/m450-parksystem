#include <Arduino.h>
#include "Parksystem.h"

// Produktive Hardware-Implementierung für die Zeitmessung auf dem Pico
class ArduinoUhr : public IUhr {
public:
  unsigned long jetzt() const override {
    return millis();
  }
};

ArduinoUhr uhrHardware;
KassenLogik kasse(uhrHardware);

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); // UART zu Pico 2 (GP0 = TX, GP1 = RX)
  delay(1500);

  Serial.println("\n============================================");
  Serial.println("  Pico 1 (Kasse mit Dependency Injection) BEREIT");
  Serial.println("  Befehle im Terminal:");
  Serial.println("  - LIST                (Alle Autos & Gebuehren)");
  Serial.println("  - PAY LU123456        (Gebuehr bezahlen)");
  Serial.println("============================================");
}

void loop() {
  // Anfragen von Pico 2 (Schranken) empfangen
  if (Serial1.available() > 0) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();

    if (msg.startsWith("ENTRY:")) {
      String plate = msg.substring(6);
      plate.trim();
      plate.toUpperCase();
      kasse.einfahrtSpeichern(plate.c_str());
      Serial.println("[KASSE] Auto registriert: " + plate);
      Serial1.println("OK:ENTRY");
    } 
    else if (msg.startsWith("CHECK:")) {
      String plate = msg.substring(6);
      plate.trim();
      plate.toUpperCase();
      KassenStatus status = kasse.pruefeStatusUndAusfahrt(plate.c_str());

      if (status == KassenStatus::NOTFOUND) {
        Serial1.println("NOTFOUND");
      } else if (status == KassenStatus::PAID) {
        Serial1.println("PAID");
        Serial.println("[KASSE] Auto " + plate + " ist ausgefahren.");
      } else {
        Serial1.println("UNPAID");
      }
    }
  }

  // Eingaben an der Kasse via Konsole
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("LIST")) {
      Serial.println("\n--- Aktuell parkende Fahrzeuge ---");
      const auto& liste = kasse.getVorgaenge();
      if (liste.empty()) {
        Serial.println("Keine Fahrzeuge im Parkhaus.");
      } else {
        for (const auto& v : liste) {
          unsigned long dur = (kasse.getAktuelleZeitMs() - v.einfahrtMs) / 1000;
          double fee = kasse.berechneGebuehr(v.kennzeichen);
          Serial.print("Kennzeichen: ");
          Serial.print(v.kennzeichen.c_str());
          Serial.print(" | Zeit: ");
          Serial.print(dur);
          Serial.print("s | Gebuehr: CHF ");
          Serial.print(fee, 2);
          Serial.println(v.bezahlt ? " | Status: BEZAHLT" : " | Status: OFFEN");
        }
      }
      Serial.println("----------------------------------\n");
    } 
    else if (cmd.startsWith("PAY ")) {
      String plate = cmd.substring(4);
      plate.trim();
      plate.toUpperCase();
      std::string kz = plate.c_str();

      double fee = kasse.berechneGebuehr(kz);
      if (kasse.bezahle(kz)) {
        Serial.print("[KASSE] CHF ");
        Serial.print(fee, 2);
        Serial.print(" fuer ");
        Serial.print(plate);
        Serial.println(" bezahlt. Ausfahrt freigegeben!");
      } else {
        Serial.println("[FEHLER] Auto nicht gefunden oder bereits bezahlt!");
      }
    }
  }
}