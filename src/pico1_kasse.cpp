#include <Arduino.h>

struct Vehicle {
  String plate;
  unsigned long entryTime;
  bool isPaid;
  bool isActive;
};

const int MAX_CARS = 20;
Vehicle parkingLot[MAX_CARS];

// Tarifmodell: 1 CHF Grundgebuehr + 1 CHF pro 10 Sekunden (Demo-Modus)
float calculateFee(unsigned long durationMs) {
  unsigned long seconds = durationMs / 1000;
  if (seconds < 5) return 0.0;
  return 1.00 + (seconds / 10) * 1.00;
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); 

  delay(1500);

  for (int i = 0; i < MAX_CARS; i++) {
    parkingLot[i].isActive = false;
  }

  Serial.println("\n============================================");
  Serial.println("  Pico 1 (Kasse) BEREIT");
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
      for (int i = 0; i < MAX_CARS; i++) {
        if (!parkingLot[i].isActive) {
          parkingLot[i].plate = plate;
          parkingLot[i].entryTime = millis();
          parkingLot[i].isPaid = false;
          parkingLot[i].isActive = true;
          Serial.println("[KASSE] Auto registriert: " + plate);
          Serial1.println("OK:ENTRY");
          break;
        }
      }
    } else if (msg.startsWith("CHECK:")) {
      String plate = msg.substring(6);
      int foundIndex = -1;
      for (int i = 0; i < MAX_CARS; i++) {
        if (parkingLot[i].isActive && parkingLot[i].plate == plate) {
          foundIndex = i;
          break;
        }
      }

      if (foundIndex == -1) {
        Serial1.println("NOTFOUND");
      } else if (parkingLot[foundIndex].isPaid) {
        Serial1.println("PAID");
        parkingLot[foundIndex].isActive = false; // Platz freigeben
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
      int count = 0;
      for (int i = 0; i < MAX_CARS; i++) {
        if (parkingLot[i].isActive) {
          count++;
          unsigned long dur = millis() - parkingLot[i].entryTime;
          float fee = calculateFee(dur);
          Serial.print("Kennzeichen: " + parkingLot[i].plate);
          Serial.print(" | Zeit: " + String(dur / 1000) + "s");
          Serial.print(" | Gebuehr: CHF " + String(fee, 2));
          Serial.println(parkingLot[i].isPaid ? " | Status: BEZAHLT" : " | Status: OFFEN");
        }
      }
      if (count == 0) Serial.println("Keine Fahrzeuge im Parkhaus.");
      Serial.println("----------------------------------\n");

    } else if (cmd.startsWith("PAY ")) {
      String plate = cmd.substring(4);
      plate.trim();
      bool found = false;
      for (int i = 0; i < MAX_CARS; i++) {
        if (parkingLot[i].isActive && parkingLot[i].plate.equalsIgnoreCase(plate)) {
          found = true;
          parkingLot[i].isPaid = true;
          float fee = calculateFee(millis() - parkingLot[i].entryTime);
          Serial.println("[KASSE] CHF " + String(fee, 2) + " fuer " + parkingLot[i].plate + " bezahlt. Ausfahrt freigegeben!");
          break;
        }
      }
      if (!found) Serial.println("[FEHLER] Auto nicht gefunden oder bereits bezahlt!");
    }
  }
}