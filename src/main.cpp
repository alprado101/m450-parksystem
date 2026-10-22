#include <Arduino.h>
#include <Servo.h>

// Servo-Objekte anlegen
Servo servoEntry;
Servo servoExit;

// Pin-Belegung gemaess Aufgabenstellung
const int PIN_SERVO_ENTRY = 8; // GP8 (Pin 11 auf dem Pico)
const int PIN_SERVO_EXIT  = 9; // GP9 (Pin 12 auf dem Pico)

// Winkel fuer die Schrankenposition
const int GATE_CLOSED = 0;   // Schranke unten (geschlossen)
const int GATE_OPEN   = 90;  // Schranke oben (offen)

const int MAX_CAPACITY = 5;
int currentCars = 0;

void openEntryGate() {
  Serial.println("[SCHRANKE] Einfahrtsschranke OEFFNET...");
  servoEntry.write(GATE_OPEN);
  delay(3000); // 3 Sekunden Durchfahrtszeit
  servoEntry.write(GATE_CLOSED);
  Serial.println("[SCHRANKE] Einfahrtsschranke GESCHLOSSEN.");
}

void openExitGate() {
  Serial.println("[SCHRANKE] Ausfahrtsschranke OEFFNET...");
  servoExit.write(GATE_OPEN);
  delay(3000); // 3 Sekunden Durchfahrtszeit
  servoExit.write(GATE_CLOSED);
  Serial.println("[SCHRANKE] Ausfahrtsschranke GESCHLOSSEN.");
}

void setup() {
  Serial.begin(115200);  
  Serial1.begin(9600);  

  delay(1500); 

  // Servos den Pins zuweisen
  servoEntry.attach(PIN_SERVO_ENTRY);
  servoExit.attach(PIN_SERVO_EXIT);

  // In Grundstellung fahren
  servoEntry.write(GATE_CLOSED);
  servoExit.write(GATE_CLOSED);

  Serial.println("\n============================================");
  Serial.println("  Pico 2 (Schrankensteuerung) BEREIT");
  Serial.println("  Befehle im Terminal:");
  Serial.println("  - AUF             (Manuell oeffnen)");
  Serial.println("  - ZU              (Manuell schliessen)");
  Serial.println("  - IN LU123456     (Einfahrt simulieren)");
  Serial.println("  - OUT LU123456    (Ausfahrt simulieren)");
  Serial.println("  - NOTFALL_AUF     (Beide Schranken dauerhaft auf)");
  Serial.println("  - NOTFALL_ZU      (Schranken schliessen)");
  Serial.println("============================================");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Manueller Schranken-Schnelltest
    if (input.equalsIgnoreCase("AUF")) {
      openEntryGate();
    } 
    else if (input.equalsIgnoreCase("ZU")) {
      servoEntry.write(GATE_CLOSED);
      Serial.println("[SCHRANKE] Geschlossen.");
    }

    // US01: Einfahrt simulieren
    else if (input.startsWith("IN ")) {
      String plate = input.substring(3);
      plate.trim();

      if (currentCars >= MAX_CAPACITY) {
        Serial.println("[STATUS: BESETZT] Parkhaus voll! Einfahrt gesperrt.");
      } else {
        Serial.println("[KAMERA] Kennzeichen erkannt: " + plate);
        Serial1.println("ENTRY:" + plate); // Signal an Pico 1 via UART

        openEntryGate();
        currentCars++;
        Serial.println("Belegung: " + String(currentCars) + "/" + String(MAX_CAPACITY) + " Plaetze besetzt.");
      }
    }

    // US04: Ausfahrt simulieren
    else if (input.startsWith("OUT ")) {
      String plate = input.substring(4);
      plate.trim();

      Serial.println("[KAMERA] Ausfahrt fuer " + plate + " angefordert. Pruefe Bezahlung...");
      Serial1.println("CHECK:" + plate); // Status bei Pico 1 abfragen

      unsigned long timeout = millis() + 2000;
      bool received = false;
      String response = "";

      while (millis() < timeout) {
        if (Serial1.available() > 0) {
          response = Serial1.readStringUntil('\n');
          response.trim();
          received = true;
          break;
        }
      }

      if (!received) {
        Serial.println("[TESTMODUS] Kein Signal von Pico 1 empfangen. Oeffne Schranke testweise.");
        openExitGate();
      } else if (response == "PAID") {
        Serial.println("[ERFOLG] Gebuehr bezahlt! Schranke oeffnet.");
        openExitGate();
        if (currentCars > 0) currentCars--;
        Serial.println("Belegung: " + String(currentCars) + "/" + String(MAX_CAPACITY) + " Plaetze besetzt.");
      } else if (response == "UNPAID") {
        Serial.println("[STOPP] Bitte zuerst an der Kasse bezahlen! Schranke bleibt ZU.");
      } else {
        Serial.println("[FEHLER] Auto nicht gefunden!");
      }
    }

    // US06: Notfall-Override
    else if (input.equalsIgnoreCase("NOTFALL_AUF")) {
      Serial.println("[NOTFALL] Manueller Override: Schranken DAUERHAFT OFFEN!");
      servoEntry.write(GATE_OPEN);
      servoExit.write(GATE_OPEN);
    } 
    else if (input.equalsIgnoreCase("NOTFALL_ZU")) {
      Serial.println("[NOTFALL] Manueller Override beendet: Schranken geschlossen.");
      servoEntry.write(GATE_CLOSED);
      servoExit.write(GATE_CLOSED);
    }
  }
}