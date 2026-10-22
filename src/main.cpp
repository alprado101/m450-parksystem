#include <Arduino.h>
#include <Servo.h>

// 2 Servo-Objekte anlegen
Servo servoEntry;
Servo servoExit;

// Pin-Belegung fuer BEIDE Schranken
const int PIN_SERVO_ENTRY = 8; // GP8 (Pin 11) - Einfahrtsschranke
const int PIN_SERVO_EXIT  = 9; // GP9 (Pin 12) - Ausfahrtsschranke

// Winkel invertiert fuer SG90 (90 = unten, 0 = oben)
const int GATE_CLOSED = 90;
const int GATE_OPEN   = 0;

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
  Serial1.begin(9600); // UART Kommunikation zu Pico 1

  delay(1500); 

  // Beide Servos aktivieren
  servoEntry.attach(PIN_SERVO_ENTRY);
  servoExit.attach(PIN_SERVO_EXIT);

  // Beide Schranken sofort nach unten (zu) fahren beim Start
  servoEntry.write(GATE_CLOSED);
  servoExit.write(GATE_CLOSED);

  Serial.println("\n============================================");
  Serial.println("  Pico 2 (Schrankensteuerung) BEREIT");
  Serial.println("  - IN LU123456     (Einfahrt)");
  Serial.println("  - OUT LU123456    (Ausfahrt)");
  Serial.println("  - NOTFALL_AUF     (Beide Schranken geoeffnet)");
  Serial.println("============================================");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Einfahrt (IN)
    if (input.startsWith("IN ")) {
      String plate = input.substring(3);
      plate.trim();

      if (currentCars >= MAX_CAPACITY) {
        Serial.println("[STATUS: BESETZT] Parkhaus voll! Einfahrt gesperrt.");
      } else {
        Serial.println("[KAMERA] Einfahrt Kennzeichen: " + plate);
        Serial1.println("ENTRY:" + plate);
        openEntryGate(); // Steuert Servo 1
        currentCars++;
      }
    }

    // Ausfahrt (OUT)
    else if (input.startsWith("OUT ")) {
      String plate = input.substring(4);
      plate.trim();

      Serial.println("[KAMERA] Ausfahrt Kennzeichen: " + plate);
      Serial1.println("CHECK:" + plate);
      
      // Hinweis: Ohne Kommunikation zu Pico 1 öffnet die Schranke hier immer zur Demo
      Serial.println("[DEMO] Testmodus aktiv, öffne Ausfahrtsschranke.");
      openExitGate(); // Steuert Servo 2
      if (currentCars > 0) currentCars--;
    }

    // Notfallsteuerung
    else if (input.equalsIgnoreCase("NOTFALL_AUF")) {
      Serial.println("[NOTFALL] Beide Schranken AUF!");
      servoEntry.write(GATE_OPEN);
      servoExit.write(GATE_OPEN);
    }
    else if (input.equalsIgnoreCase("NOTFALL_ZU")) {
      Serial.println("[NOTFALL] Beide Schranken ZU!");
      servoEntry.write(GATE_CLOSED);
      servoExit.write(GATE_CLOSED);
    }
  }
}