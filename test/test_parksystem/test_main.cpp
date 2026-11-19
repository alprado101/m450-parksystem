#include <unity.h>
#include "Parksystem.h"

// --- 1. Test Doubles ---

// Mock: Merkt sich Aufrufe und aktuellen Zustand (gemäss T03-02 Kap. 4.4)
class SchrankeMock : public ISchranke {
public:
    int oeffnenAufrufe = 0;
    int schliessenAufrufe = 0;
    bool offen = false;

    void oeffnen() override {
        oeffnenAufrufe++;
        offen = true;
    }

    void schliessen() override {
        schliessenAufrufe++;
        offen = false;
    }

    bool istOffen() const override {
        return offen;
    }
};

// Stub: Liefert konfigurierbare Rückgabewerte ohne echte Kassen-Logik (gemäss T03-02 Kap. 4.3)
class KasseStub : public IKasse {
public:
    KassenStatus naechsterStatus = KassenStatus::PAID;
    std::string letzteRegistrierteEinfahrt = "";

    void registriereEinfahrt(const std::string& kennzeichen) override {
        letzteRegistrierteEinfahrt = kennzeichen;
    }

    KassenStatus pruefeZahlung(const std::string& kennzeichen) override {
        return naechsterStatus;
    }
};

// Stub: Steuert die Zeit deterministisch (gemäss T03-02 Kap. 4.3)
class UhrStub : public IUhr {
public:
    unsigned long aktuelleZeitMs = 0;
    unsigned long jetzt() const override { return aktuelleZeitMs; }
};

// Dummy: Platzhalter, wenn Zeit im Test nicht relevant ist (gemäss T03-02 Kap. 4.1)
class DummyUhr : public IUhr {
public:
    unsigned long jetzt() const override { return 0; }
};

// --- 2. Automatisierte Unit-Tests (Bezug zum Testkonzept TC-01 bis TC-16) ---

void test_TC01_einfahrt_gueltiges_kennzeichen() {
    SchrankeMock einfahrt, ausfahrt;
    KasseStub kasse;
    ParkhausSteuerung sys(einfahrt, ausfahrt, kasse, 3);

    std::string res = sys.einfahrtErkannt("LU123456");

    TEST_ASSERT_EQUAL_STRING("[OK] Einfahrt fuer LU123456", res.c_str());
    TEST_ASSERT_EQUAL(1, einfahrt.oeffnenAufrufe);
    TEST_ASSERT_EQUAL(1, einfahrt.schliessenAufrufe);
    TEST_ASSERT_EQUAL_STRING("LU123456", kasse.letzteRegistrierteEinfahrt.c_str());
    TEST_ASSERT_EQUAL(1, sys.getAnzahlFahrzeuge());
}

void test_TC02_ersatzticket_bei_unlesbarem_kennzeichen() {
    SchrankeMock einfahrt, ausfahrt;
    KasseStub kasse;
    ParkhausSteuerung sys(einfahrt, ausfahrt, kasse, 3);

    std::string res = sys.einfahrtErkannt("UNLESBAR");

    TEST_ASSERT_EQUAL_STRING("[ERSATZTICKET] TICKET1", res.c_str());
    TEST_ASSERT_EQUAL(1, einfahrt.oeffnenAufrufe);
    TEST_ASSERT_EQUAL(1, sys.getAnzahlFahrzeuge());
}

void test_TC03_TC04_TC05_belegungsanzeige_frei_und_besetzt() {
    SchrankeMock einfahrt, ausfahrt;
    KasseStub kasse;
    kasse.naechsterStatus = KassenStatus::PAID;
    ParkhausSteuerung sys(einfahrt, ausfahrt, kasse, 3);

    // TC-03: Frei bei 0/3
    TEST_ASSERT_EQUAL_STRING("FREI", sys.getBelegungsStatus().c_str());

    sys.einfahrtErkannt("LU111111");
    sys.einfahrtErkannt("LU222222");
    sys.einfahrtErkannt("LU333333");

    // TC-04: Besetzt bei 3/3 und Sperre für weiteres Fahrzeug
    TEST_ASSERT_EQUAL_STRING("BESETZT", sys.getBelegungsStatus().c_str());
    std::string vollRes = sys.einfahrtErkannt("LU999999");
    TEST_ASSERT_EQUAL_STRING("[STATUS: BESETZT] Parkhaus voll!", vollRes.c_str());
    TEST_ASSERT_EQUAL(3, einfahrt.oeffnenAufrufe); // Keine 4. Öffnung

    // TC-05: Ausfahrt lässt Anzeige wieder auf FREI springen
    sys.ausfahrtErkannt("LU111111");
    TEST_ASSERT_EQUAL_STRING("FREI", sys.getBelegungsStatus().c_str());
    TEST_ASSERT_EQUAL(2, sys.getAnzahlFahrzeuge());
}

void test_TC06_TC07_gebuehrenberechnung_und_bezahlung() {
    UhrStub uhr;
    KassenLogik kasse(uhr);

    uhr.aktuelleZeitMs = 10000; // Einfahrt bei 10s
    kasse.einfahrtSpeichern("LU123456");

    // Nach 15s Parkdauer -> 1.00 + (15/10)*1.00 = 2.00 CHF
    uhr.aktuelleZeitMs = 25000;
    TEST_ASSERT_FLOAT_WITHIN(0.01, 2.00, kasse.berechneGebuehr("LU123456"));

    // Nach 35s Parkdauer -> 1.00 + (35/10)*1.00 = 4.00 CHF
    uhr.aktuelleZeitMs = 45000;
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.00, kasse.berechneGebuehr("LU123456"));

    // TC-07: Bezahlung durchführen
    TEST_ASSERT_TRUE(kasse.bezahle("LU123456"));
    TEST_ASSERT_EQUAL(KassenStatus::PAID, kasse.pruefeStatusUndAusfahrt("LU123456"));
}

void test_TC08_ausfahrt_bezahlt_oeffnet_schranke() {
    SchrankeMock einfahrt, ausfahrt;
    KasseStub kasse;
    kasse.naechsterStatus = KassenStatus::PAID;
    ParkhausSteuerung sys(einfahrt, ausfahrt, kasse, 3);

    sys.einfahrtErkannt("LU123456");
    std::string res = sys.ausfahrtErkannt("LU123456");

    TEST_ASSERT_EQUAL_STRING("[ERFOLG] Ausfahrt freigegeben", res.c_str());
    TEST_ASSERT_EQUAL(1, ausfahrt.oeffnenAufrufe);
    TEST_ASSERT_EQUAL(0, sys.getAnzahlFahrzeuge());
}

void test_TC09_TC14_TC15_ausfahrt_blockiert_bei_unbezahlt_unbekannt_offline() {
    SchrankeMock einfahrt, ausfahrt;
    KasseStub kasse;
    ParkhausSteuerung sys(einfahrt, ausfahrt, kasse, 3);

    // TC-09: Unbezahlt
    kasse.naechsterStatus = KassenStatus::UNPAID;
    TEST_ASSERT_EQUAL_STRING("[STOPP] Bitte zuerst an der Kasse bezahlen!", sys.ausfahrtErkannt("LU111111").c_str());

    // TC-14: Unbekanntes Kennzeichen
    kasse.naechsterStatus = KassenStatus::NOTFOUND;
    TEST_ASSERT_EQUAL_STRING("[FEHLER] Kein Parkvorgang gefunden!", sys.ausfahrtErkannt("LU888888").c_str());

    // TC-15: Kasse offline
    kasse.naechsterStatus = KassenStatus::OFFLINE;
    TEST_ASSERT_EQUAL_STRING("[FEHLER] Kassensystem nicht erreichbar!", sys.ausfahrtErkannt("LU123456").c_str());

    // Ausfahrtsschranke darf kein einziges Mal geöffnet worden sein
    TEST_ASSERT_EQUAL(0, ausfahrt.oeffnenAufrufe);
}

void test_TC11_TC12_notfall_oeffnung() {
    SchrankeMock einfahrt, ausfahrt;
    KasseStub kasse;
    ParkhausSteuerung sys(einfahrt, ausfahrt, kasse, 3);

    // TC-11: Notfall Einfahrt
    sys.notfallEinfahrtOeffnen();
    TEST_ASSERT_TRUE(einfahrt.istOffen());
    TEST_ASSERT_FALSE(ausfahrt.istOffen());

    // TC-12: Notfall Ausfahrt
    sys.notfallAusfahrtOeffnen();
    TEST_ASSERT_TRUE(ausfahrt.istOffen());

    sys.notfallSchliessen();
    TEST_ASSERT_FALSE(einfahrt.istOffen());
    TEST_ASSERT_FALSE(ausfahrt.istOffen());
}

void test_TC13_TC16_ungueltiges_kennzeichen_und_dublette() {
    SchrankeMock einfahrt, ausfahrt;
    KasseStub kasse;
    ParkhausSteuerung sys(einfahrt, ausfahrt, kasse, 3);

    // TC-13: Leer und Sonderzeichen
    TEST_ASSERT_EQUAL_STRING("[FEHLER] Ungueltiges oder leeres Kennzeichen!", sys.einfahrtErkannt("").c_str());
    TEST_ASSERT_EQUAL_STRING("[FEHLER] Ungueltiges oder leeres Kennzeichen!", sys.einfahrtErkannt("@@123").c_str());
    TEST_ASSERT_EQUAL(0, einfahrt.oeffnenAufrufe);

    // TC-16: Doppelte Einfahrt verhindern
    sys.einfahrtErkannt("LU123456");
    std::string dupRes = sys.einfahrtErkannt("LU123456");
    TEST_ASSERT_EQUAL_STRING("[FEHLER] Fahrzeug bereits im Parkhaus registriert!", dupRes.c_str());
    TEST_ASSERT_EQUAL(1, einfahrt.oeffnenAufrufe);
    TEST_ASSERT_EQUAL(1, sys.getAnzahlFahrzeuge());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_TC01_einfahrt_gueltiges_kennzeichen);
    RUN_TEST(test_TC02_ersatzticket_bei_unlesbarem_kennzeichen);
    RUN_TEST(test_TC03_TC04_TC05_belegungsanzeige_frei_und_besetzt);
    RUN_TEST(test_TC06_TC07_gebuehrenberechnung_und_bezahlung);
    RUN_TEST(test_TC08_ausfahrt_bezahlt_oeffnet_schranke);
    RUN_TEST(test_TC09_TC14_TC15_ausfahrt_blockiert_bei_unbezahlt_unbekannt_offline);
    RUN_TEST(test_TC11_TC12_notfall_oeffnung);
    RUN_TEST(test_TC13_TC16_ungueltiges_kennzeichen_und_dublette);
    return UNITY_END();
}