#ifndef PARKSYSTEM_H
#define PARKSYSTEM_H

#include <string>
#include <vector>

// --- 1. Schnittstellen (Interfaces) für Hardware & Umgebung ---

enum class KassenStatus {
    PAID,
    UNPAID,
    NOTFOUND,
    OFFLINE
};

class ISchranke {
public:
    virtual ~ISchranke() = default;
    virtual void oeffnen() = 0;
    virtual void schliessen() = 0;
    virtual bool istOffen() const = 0;
};

class IKasse {
public:
    virtual ~IKasse() = default;
    virtual void registriereEinfahrt(const std::string& kennzeichen) = 0;
    virtual KassenStatus pruefeZahlung(const std::string& kennzeichen) = 0;
};

class IUhr {
public:
    virtual ~IUhr() = default;
    virtual unsigned long jetzt() const = 0;
};

// --- 2. Geschäftslogik Schrankensteuerung (Pico 2) ---

class ParkhausSteuerung {
private:
    ISchranke& einfahrtSchranke;
    ISchranke& ausfahrtSchranke;
    IKasse& kasse;
    int maxKapazitaet;
    int ticketZaehler;
    std::vector<std::string> aktiveFahrzeuge;

    bool istGueltigesKennzeichen(const std::string& kz) const;
    bool istBereitsImParkhaus(const std::string& kz) const;

public:
    // Dependency Injection über Konstruktor
    ParkhausSteuerung(ISchranke& einfahrt, ISchranke& ausfahrt, IKasse& kasse, int maxKapa = 3);

    std::string einfahrtErkannt(const std::string& kennzeichen);
    std::string ersatzTicketAnfordern();
    std::string ausfahrtErkannt(const std::string& kennzeichen);

    void notfallEinfahrtOeffnen();
    void notfallAusfahrtOeffnen();
    void notfallSchliessen();

    std::string getBelegungsStatus() const;
    int getAnzahlFahrzeuge() const;
};

// --- 3. Geschäftslogik Kasse & Tarife (Pico 1) ---

struct ParkVorgang {
    std::string kennzeichen;
    unsigned long einfahrtMs;
    bool bezahlt;
};

class KassenLogik {
private:
    IUhr& uhr;
    std::vector<ParkVorgang> vorgaenge;

public:
    explicit KassenLogik(IUhr& uhr);
    
    const std::vector<ParkVorgang>& getVorgaenge() const { return vorgaenge; }
    unsigned long getAktuelleZeitMs() const { return uhr.jetzt(); }

    void einfahrtSpeichern(const std::string& kennzeichen);
    double berechneGebuehr(const std::string& kennzeichen) const;
    bool bezahle(const std::string& kennzeichen);
    KassenStatus pruefeStatusUndAusfahrt(const std::string& kennzeichen);
    
};

#endif