#include "Parksystem.h"
#include <algorithm>
#include <cctype>

ParkhausSteuerung::ParkhausSteuerung(ISchranke& einfahrt, ISchranke& ausfahrt, IKasse& kasse, int maxKapa)
    : einfahrtSchranke(einfahrt), ausfahrtSchranke(ausfahrt), kasse(kasse),
      maxKapazitaet(maxKapa), ticketZaehler(1) {}

bool ParkhausSteuerung::istGueltigesKennzeichen(const std::string& kz) const {
    if (kz.length() < 2 || kz.length() > 10) return false;
    for (char c : kz) {
        if (!std::isalnum(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

bool ParkhausSteuerung::istBereitsImParkhaus(const std::string& kz) const {
    return std::find(aktiveFahrzeuge.begin(), aktiveFahrzeuge.end(), kz) != aktiveFahrzeuge.end();
}

std::string ParkhausSteuerung::einfahrtErkannt(const std::string& kennzeichen) {
    if (kennzeichen == "UNLESBAR") {
        return ersatzTicketAnfordern();
    }
    if (!istGueltigesKennzeichen(kennzeichen)) {
        return "[FEHLER] Ungueltiges oder leeres Kennzeichen!";
    }
    if (static_cast<int>(aktiveFahrzeuge.size()) >= maxKapazitaet) {
        return "[STATUS: BESETZT] Parkhaus voll!";
    }
    if (istBereitsImParkhaus(kennzeichen)) {
        return "[FEHLER] Fahrzeug bereits im Parkhaus registriert!";
    }

    aktiveFahrzeuge.push_back(kennzeichen);
    kasse.registriereEinfahrt(kennzeichen);
    einfahrtSchranke.oeffnen();
    einfahrtSchranke.schliessen();
    return "[OK] Einfahrt fuer " + kennzeichen;
}

std::string ParkhausSteuerung::ersatzTicketAnfordern() {
    if (static_cast<int>(aktiveFahrzeuge.size()) >= maxKapazitaet) {
        return "[STATUS: BESETZT] Parkhaus voll!";
    }
    std::string ticket = "TICKET" + std::to_string(ticketZaehler++);
    aktiveFahrzeuge.push_back(ticket);
    kasse.registriereEinfahrt(ticket);
    einfahrtSchranke.oeffnen();
    einfahrtSchranke.schliessen();
    return "[ERSATZTICKET] " + ticket;
}

std::string ParkhausSteuerung::ausfahrtErkannt(const std::string& kennzeichen) {
    KassenStatus status = kasse.pruefeZahlung(kennzeichen);

    if (status == KassenStatus::OFFLINE) {
        return "[FEHLER] Kassensystem nicht erreichbar!";
    }
    if (status == KassenStatus::UNPAID) {
        return "[STOPP] Bitte zuerst an der Kasse bezahlen!";
    }
    if (status == KassenStatus::NOTFOUND) {
        return "[FEHLER] Kein Parkvorgang gefunden!";
    }

    ausfahrtSchranke.oeffnen();
    ausfahrtSchranke.schliessen();

    auto it = std::find(aktiveFahrzeuge.begin(), aktiveFahrzeuge.end(), kennzeichen);
    if (it != aktiveFahrzeuge.end()) {
        aktiveFahrzeuge.erase(it);
    }
    return "[ERFOLG] Ausfahrt freigegeben";
}

void ParkhausSteuerung::notfallEinfahrtOeffnen() {
    einfahrtSchranke.oeffnen();
}

void ParkhausSteuerung::notfallAusfahrtOeffnen() {
    ausfahrtSchranke.oeffnen();
}

void ParkhausSteuerung::notfallSchliessen() {
    einfahrtSchranke.schliessen();
    ausfahrtSchranke.schliessen();
}

std::string ParkhausSteuerung::getBelegungsStatus() const {
    return (static_cast<int>(aktiveFahrzeuge.size()) >= maxKapazitaet) ? "BESETZT" : "FREI";
}

int ParkhausSteuerung::getAnzahlFahrzeuge() const {
    return static_cast<int>(aktiveFahrzeuge.size());
}

// --- KassenLogik ---

KassenLogik::KassenLogik(IUhr& uhr) : uhr(uhr) {}

void KassenLogik::einfahrtSpeichern(const std::string& kennzeichen) {
    vorgaenge.push_back({kennzeichen, uhr.jetzt(), false});
}

double KassenLogik::berechneGebuehr(const std::string& kennzeichen) const {
    for (const auto& v : vorgaenge) {
        if (v.kennzeichen == kennzeichen) {
            unsigned long sekunden = (uhr.jetzt() - v.einfahrtMs) / 1000;
            return 1.00 + (sekunden / 10) * 1.00;
        }
    }
    return -1.0;
}

bool KassenLogik::bezahle(const std::string& kennzeichen) {
    for (auto& v : vorgaenge) {
        if (v.kennzeichen == kennzeichen && !v.bezahlt) {
            v.bezahlt = true;
            return true;
        }
    }
    return false;
}

KassenStatus KassenLogik::pruefeStatusUndAusfahrt(const std::string& kennzeichen) {
    for (auto it = vorgaenge.begin(); it != vorgaenge.end(); ++it) {
        if (it->kennzeichen == kennzeichen) {
            if (it->bezahlt) {
                vorgaenge.erase(it);
                return KassenStatus::PAID;
            }
            return KassenStatus::UNPAID;
        }
    }
    return KassenStatus::NOTFOUND;
}