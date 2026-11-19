#include <Arduino.h>
#include <Servo.h>
#include "Parksystem.h"

// Produktive Hardware-Implementierung für den SG90-Servomotor
class ServoSchranke : public ISchranke {
private:
    Servo servo;
    int pin;
    bool offen;
    const int GATE_CLOSED = 90;
    const int GATE_OPEN   = 0;

public:
    explicit ServoSchranke(int gpioPin) : pin(gpioPin), offen(false) {}

    void init() {
        servo.attach(pin);
        servo.write(GATE_CLOSED);
    }

    void oeffnen() override {
        offen = true;
        servo.write(GATE_OPEN);
        delay(3000);
    }

    void schliessen() override {
        servo.write(GATE_CLOSED);
        offen = false;
    }

    bool istOffen() const override {
        return offen;
    }
};

// Produktive UART-Kommunikation zu Pico 1 (Kasse)
class UartKasse : public IKasse {
public:
    void registriereEinfahrt(const std::string& kennzeichen) override {
        Serial1.println(("ENTRY:" + kennzeichen).c_str());
    }

    KassenStatus pruefeZahlung(const std::string& kennzeichen) override {
        while (Serial1.available() > 0) Serial1.read();
        Serial1.println(("CHECK:" + kennzeichen).c_str());

        unsigned long timeout = millis() + 2000;
        while (millis() < timeout) {
            if (Serial1.available() > 0) {
                String resp = Serial1.readStringUntil('\n');
                resp.trim();
                if (resp == "PAID") return KassenStatus::PAID;
                if (resp == "UNPAID") return KassenStatus::UNPAID;
                return KassenStatus::NOTFOUND;
            }
        }
        return KassenStatus::OFFLINE;
    }
};

ServoSchranke einfahrtHardware(8); // GP8
ServoSchranke ausfahrtHardware(9); // GP9
UartKasse kasseHardware;
ParkhausSteuerung parkhaus(einfahrtHardware, ausfahrtHardware, kasseHardware, 3);

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);
    delay(1500);

    einfahrtHardware.init();
    ausfahrtHardware.init();

    Serial.println("Pico 2 (SmartPark mit Dependency Injection) BEREIT");
}

void loop() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        std::string cmd = input.c_str();

        if (input.equalsIgnoreCase("STATUS")) {
            Serial.println(("[ANZEIGE] " + parkhaus.getBelegungsStatus() + " (" + std::to_string(parkhaus.getAnzahlFahrzeuge()) + "/3)").c_str());
        } else if (input.equalsIgnoreCase("TICKET")) {
            Serial.println(parkhaus.ersatzTicketAnfordern().c_str());
        } else if (input.equalsIgnoreCase("IN") || input.startsWith("IN ")) {
            String plate = (input.length() > 3) ? input.substring(3) : "";
            plate.trim();
            plate.toUpperCase();
            Serial.println(parkhaus.einfahrtErkannt(plate.c_str()).c_str());
        } else if (input.startsWith("OUT ")) {
            String plate = input.substring(4);
            plate.trim();
            plate.toUpperCase();
            Serial.println(parkhaus.ausfahrtErkannt(plate.c_str()).c_str());
        } else if (input.equalsIgnoreCase("NOTFALL_EINFAHRT")) {
            parkhaus.notfallEinfahrtOeffnen();
            Serial.println("[NOTFALL] Einfahrtsschranke manuell GEOEFFNET!");
        } else if (input.equalsIgnoreCase("NOTFALL_AUSFAHRT")) {
            parkhaus.notfallAusfahrtOeffnen();
            Serial.println("[NOTFALL] Ausfahrtsschranke manuell GEOEFFNET!");
        } else if (input.equalsIgnoreCase("NOTFALL_ZU")) {
            parkhaus.notfallSchliessen();
            Serial.println("[NOTFALL] Schranken GESCHLOSSEN.");
        }
    }
}