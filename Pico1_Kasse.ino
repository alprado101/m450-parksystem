/*
 * SmartPark System - Pico 1: Kasse & Gebührenverwaltung
 * Autoren: Alondra & Lucy
 */

#include <Arduino.h>

struct Vehicle {
  String plate;
  unsigned long entryTime; // Zeitstempel in ms
  bool isPaid;
  bool isActive;
};

const int MAX_CARS = 20;
Vehicle parkingLot[MAX_CARS];

// Tarifmodell: 1 CHF Grundgebühr + 1 CHF pro angefangene 10 Sekunden (zu Demonstrationszwecken)
float calculateFee(unsigned long durationMs) {
  unsigned long seconds = durationMs / 1000;
  if (seconds < 5) return 0.0; // Erste 5 Sekunden gratis (Kulanz)
  return 1.00 + (seconds / 10) * 1.00;
}

void setup() {
  Serial.begin(115200);   // USB-Schnittstelle zur Benutzerbedienung (Kasse)
  Serial1.begin(9600);   // UART-Kommunikation zu Pico 2 (GP0 = TX, GP1 = RX)

  for (int i = 0; i < MAX_CARS; i++) {
    parkingLot[i].isActive = false;
  }

  Serial.println("============================================");
  Serial.println("  Pico 1 (Kasse) betriebsbereit");
  Serial.println("  Befehle ueber Konsole:");
  Serial.println("  - LIST                (Alle aktiven Autos)");
  Serial.println("  - PAY <KENNZEICHEN>   (Gebuehr bezahlen)");
  Serial.println("============================================");
}

void loop() {
  // 1. Befehle vom Schranken-Pico (Pico 2) verarbeiten
  if (Serial1.available() > 0) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();

    if (msg.startsWith("ENTRY:")) {
      String plate = msg.substring(6);
      bool added = false;
      for (int i = 0; i < MAX_CARS; i++) {
        if (!parkingLot[i].isActive) {
          parkingLot[i].plate = plate;
          parkingLot[i].entryTime = millis();
          parkingLot[i].isPaid = false;
          parkingLot[i].isActive = true;
          added = true;
          Serial.println("[KASSE] Auto registriert: " + plate);
          Serial1.println("OK:ENTRY");
          break;
        }
      }
      if (!added) Serial1.println("ERROR:FULL");

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
        parkingLot[foundIndex].isActive = false; // Parkvorgang nach Ausfahrt beenden
        Serial.println("[KASSE] Auto " + plate + " ist ausgefahren. Platz freigegeben.");
      } else {
        Serial1.println("UNPAID");
      }
    }
  }

  // 2. Bedienung an der Kasse ueber USB-Seriell
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("LIST")) {
      Serial.println("\n--- Aktuell parkende Fahrzeuge ---");
      int count = 0;
      for (int i = 0; i < MAX_CARS; i++) {
        if (parkingLot[i].isActive) {
          count++;
          unsigned long duration = (millis() - parkingLot[i].entryTime);
          float fee = calculateFee(duration);
          Serial.print("Kennzeichen: " + parkingLot[i].plate);
          Serial.print(" | Dauer: " + String(duration / 1000) + "s");
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
          float fee = calculateFee(millis() - parkingLot[i].entryTime);
          parkingLot[i].isPaid = true;
          Serial.println("[KASSE] Zahlung erfolgreich fuer: " + parkingLot[i].plate);
          Serial.println("Betrag von CHF " + String(fee, 2) + " beglichen. Ausfahrt freigeschaltet!");
          break;
        }
      }
      if (!found) Serial.println("[FEHLER] Kennzeichen nicht gefunden oder bereits bezahlt!");
    }
  }
}