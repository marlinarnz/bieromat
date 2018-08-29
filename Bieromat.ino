/* Referenzen:
 *  Geschrieben von Marlin Arnz (marlin-arnz@web.de)
 *  Ansatz für Bauteile und Verkabelung: https://www.glohbe.de/programmierung-arduino-brausteuerung/
 *  Temperatur auslesen: http://www.aeq-web.com/ds18b20-temperatur-messen-mit-dem-arduino/?ref=amzn
 *  Viele weitere Internetquellen für das Verständnis
 */

 /* Anmerkungen zum Motor (Scheibenwischermotor Valeo 3171A aus einem Opel Corsa):
  * Belegung volle Geschw.: + 12 V an Weiß; GND an Braun
  * - Leerlaufstrom: 2,24 A
  * - rpm im Leerlauf: 64 (Belegung andersherum: 54 rpm)
  * Belegung halbe Geschw.: + 12 V an Gelb; GND an Braun
  * - Leerlaufstrom: 1,5 A
  * - rpm im Leerlauf: 41
  */

/* =====> Bibliotheken einbinden */

#include <Wire.h>                 // LCD
#include <LiquidCrystal_I2C.h>    // LCD-I2C-Bibliothek
#include <OneWire.h>              // OneWire-Bibliothek
#include <DallasTemperature.h>    // DS18B20-Bibliothek

/* =====> Pins, Konstanten und Objekte definieren */

// Temperatursensoren
#define DS18B20_PIN 2                  // TemperaturPIN
OneWire oneWire(DS18B20_PIN);          // OneWire Referenz setzen
DallasTemperature sensors(&oneWire);   // DS18b20 initialisieren

// Display
// Lila (SDA) mit Pin A4; Grau (SCL) mit Pin A5
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);     // I2C initialisieren

// Motor
#define motorPWM 9        // PWM für Rührermotor
                          // Pin muss PWM unterstützen (~)
                          // Pin mit 0-5 V am PWM Board verbinden und 5 V mit 3. Pin des PWM Boards verbinden

// Relais
#define relais1 8         // Steuerung Relais für Heizung (an IN1)

// Knöpfe
#define buttonZurueck 3   // Knopf "Zurück" (rotes Kabel)
#define buttonWeiter 4    // Knopf "Weiter" (blaues Kabel)
#define buttonHoch 5      // Knopf "Hoch" (gelbes Kabel)
#define buttonRunter 6    // Knopf "Runter" (orangees Kabel)


/* =====> Variablen global */

// Für Temperatur
float tempMaische;       // Temperatur der Maische, Mittelwert
int anzahlTempSens = 2;  // Anzahl der angeschlossenen Temp.-Sensoren
int temperaturdifferenz = 2; // Für die Ausgabe der Temperaturdifferenz

// Für Motor
int motor = 0;           // Rührergeschwindigkeit vorgegeben (0 bis 255)
int motorJetzt = 0;      // Rührergeschw. aktuell durch Rampe
long motorRampeZeit;     // Zeitpunkt an dem die Rampe beginnt
float motorRPM = 0;      // Zur Ausgabe der rpm

// Für Heizung
bool heizung = false;    // Ist die Heizung an?

// Für Zeitangaben
long start = 0;          // Startzeit des Brauprogrammes nach Initialisierung
long startRast = 0;      // Startzeit der jeweiligen Rast

// Für Maischprogramm
int rast = 0;            // zum Anzeigen, welche Rast schon beendet ist

// Für Initialisierungsprogramm
int programmEbene = 0;   // Für die Navigation im Setup über die Knöpfe
int programmSubEbene = 0;// Für die Navigation auf Subebene im Setup über die Knöpfe
long letzterDruck = 0;   // Zeitpunkt, an dem ein Button das letzte Mal gedrückt wurde
bool durchgang = false;  // sind wir in dieser Programmebene geblieben?
int letzterWert;         // Für das Programm zum checken, ob eine Eingabe stattgefunden hat
long blinkAn;            // Zeitspeichervariable für Blinkfunktion

bool zurueck = false;    // Zustände der Buttons zur Zwischenspeicherung
bool weiter = false;
bool hoch = false;
bool runter = false;

String biertyp[] = {"Weizen", "Pils", "Ale"};    // Max. je 19 Zeichen!
                         // Ohne Anpassungen können nur drei Biere angezeigt werden
int bierDefault = 0;     // Der aus dem Array gewählte default-Biertyp

// Brauparameter
int einmaischenDauer;    // Einmaischen, Zeit in Minuten
int einmaischenTemp;     // Einmaischtemperatur in °C
int eiweissrastDauer;    // Eiweißrast, Zeit in Minuten
int eiweissrastTemp;     // Eiweißrast, Temperatur in °C
int maltoserastDauer;    // Maltoserast, Zeit in Minuten
int maltoserastTemp;     // Maltoserast, Temperatur in °C
int zuckerrast1Dauer;    // 1. Verzuckerungsrast, Zeit in Minuten
int zuckerrast1Temp;     // 1. Verzuckerungsrast, Temperatur in °C
int zuckerrast2Dauer;    // 2. Verzuckerungsrast, Zeit in Minuten
int zuckerrast2Temp;     // 2. Verzuckerungsrast, Temperatur in °C


/* =====> Setup */

void setup() {
  
  // Display
  Wire.begin();
  lcd.begin(20,4);       // Was für ein Display: 20x4 oder 12x2, ...
  //lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);  // Beleuchtung PIN festlegen, geht auch direkt in Initialisierung
  lcd.setBacklight(HIGH);
  lcd.home(); 
  lcd.print("Der Bieromat.");
  //lcd.setCursor(0,1);   // Cursor auf die 2te Zeile (=1te Zeile im Code) setzen

  // Relais
  pinMode(relais1, OUTPUT);            // Relaiseingang 1 definieren
  digitalWrite(relais1, LOW);          // Heizung aus

  // Temperatursensoren
  sensors.begin();                     // DS18B20 Sensoren starten

  // Rührermotor
  pinMode(motorPWM, OUTPUT);           // Motor PWM (gelbes Kabel)
  analogWrite(motorPWM, motor);        // Geschwindigkeit = 0 (von 255)

/* =====> Programmstart */
  lcd.setCursor(0,2);
  lcd.print("'Weiter' druecken");
  lcd.setCursor(0,3);
  lcd.print("zum starten");
  
  while (digitalRead(buttonWeiter) == LOW) {  // nur nach Drücken geht es weiter
    delay(50);
    letzterDruck = millis();
  }

  while (programmEbene < 7) {
    checkButtons();               // Einmal anfangs Knöpfe prüfen
    switch (programmEbene) {
      case 0:
        // Biertyp wählen

        if (!durchgang) {                // nur beim ersten Durchgang
          lcd.clear();
          lcd.home();
          lcd.print("Biertyp-Vorlage:");
          for (bierDefault = 0; bierDefault < 3; bierDefault++) { // Alle Default-Biere auflisten in Zeile 2 bis 4
            lcd.setCursor(1, bierDefault + 1);
            lcd.print(biertyp[bierDefault]);
          }
          bierDefault = 1;               // Bestimmt, welches Default-Bier genommen wird
          letzterWert = 0;
          durchgang = true;
        }
        
        bierDefault = buttonHochRunter(bierDefault, false);
        if (bierDefault == 0) {          // bierDefault bleibt immer zwischen 1 und 3
          bierDefault = 3;
        } else if (bierDefault == 4) {
          bierDefault = 1;
        }

        if (letzterWert != bierDefault) {
          for (int i = 1; i < 4; i++) {  // vorherige Auswahlmarkierungen löschen
            lcd.setCursor(0, i);
            lcd.print(" ");
          }
          lcd.setCursor(0, bierDefault); // Welches Default-Bier wurde gewählt
          lcd.print("-");
          letzterWert = bierDefault;
        }

        programmEbene = buttonZurueckWeiter(programmEbene);
        
        if (programmEbene == 1) {        // Brauparameter setzen
          switch (bierDefault) {
            case 1:
              einmaischenDauer = 10;
              einmaischenTemp = 35;
              eiweissrastDauer = 0;
              eiweissrastTemp = 50;
              maltoserastDauer = 60;
              maltoserastTemp = 63;
              zuckerrast1Dauer = 20;
              zuckerrast1Temp = 72;
              zuckerrast2Dauer = 0;
              zuckerrast2Temp = 75;
              break;
            case 2:
              einmaischenDauer = 10;
              einmaischenTemp = 35;
              eiweissrastDauer = 0;
              eiweissrastTemp = 55;
              maltoserastDauer = 80;
              maltoserastTemp = 63;
              zuckerrast1Dauer = 10;
              zuckerrast1Temp = 73;
              zuckerrast2Dauer = 0;
              zuckerrast2Temp = 75;
              break;
            case 3:
              einmaischenDauer = 10;
              einmaischenTemp = 35;
              eiweissrastDauer = 0;
              eiweissrastTemp = 55;
              maltoserastDauer = 80;
              maltoserastTemp = 63;
              zuckerrast1Dauer = 10;
              zuckerrast1Temp = 73;
              zuckerrast2Dauer = 0;
              zuckerrast2Temp = 75;
              break;
            default:
              programmEbene = 0;
              break;
          }
        }
        break;
        
      case 1:
        // Einmaischen Parameter einstellen

        if (!durchgang) {                // nur beim ersten Durchgang
          lcd.clear();
          lcd.home();
          lcd.print("Einmaischen Daten:");
          lcd.setCursor(1, 1);
          lcd.print("Temp.:    C");
          lcd.setCursor(1, 2);
          lcd.print("Dauer:    min");
          lcd.setCursor(1, 3);
          lcd.print("weiter oder zurueck");
          lcd.setCursor(8, 1);
          lcd.print(einmaischenTemp);   // Temperatur in Lücke einfügen
          lcd.setCursor(8, 2);
          lcd.print(einmaischenDauer);  // Dauer in Lücke einfügen
          programmSubEbene = 1;
          letzterWert = 0;
          durchgang = true;
        }

        // Ab in die Subebene! (Temperatur, Dauer oder Weiter)
        programmSubEbene = buttonHochRunter(programmSubEbene, false);
        switch (programmSubEbene) {
          case 1: // Temperatur
            markierenSubEbene();
            einmaischenTemp = wertAktualisieren(einmaischenTemp);
            letzterWert = programmSubEbene;
            break;
            
          case 2: // Dauer
            markierenSubEbene();
            einmaischenDauer = wertAktualisieren(einmaischenDauer);
            letzterWert = programmSubEbene;
            break;
            
          case 3: // Weiter in die nächste Programmebene
            markierenSubEbene();
            letzterWert = programmSubEbene;
            break;
            
          default:
            if (programmSubEbene < 1) {
              programmSubEbene = 3;
            } else {
              programmSubEbene = 1;
            }
            break;
        }

        programmEbene = buttonZurueckWeiter(programmEbene);
        break;
        
      case 2:
        // Eiweßrast Parameter einstellen

        if (!durchgang) {                // nur beim ersten Durchgang
          lcd.clear();
          lcd.home();
          lcd.print("Eiweissrast Daten:");
          lcd.setCursor(1, 1);
          lcd.print("Temp.:    C");
          lcd.setCursor(1, 2);
          lcd.print("Dauer:    min");
          lcd.setCursor(1, 3);
          lcd.print("weiter oder zurueck");
          lcd.setCursor(8, 1);
          lcd.print(eiweissrastTemp);   // Temperatur in Lücke einfügen
          lcd.setCursor(8, 2);
          lcd.print(eiweissrastDauer);  // Dauer in Lücke einfügen
          programmSubEbene = 1;
          letzterWert = 0;
          durchgang = true;
        }

        // Ab in die Subebene! (Temperatur, Dauer oder Weiter)
        programmSubEbene = buttonHochRunter(programmSubEbene, false);
        switch (programmSubEbene) {
          case 1: // Temperatur
            markierenSubEbene();
            eiweissrastTemp = wertAktualisieren(eiweissrastTemp);
            letzterWert = programmSubEbene;
            break;
            
          case 2: // Dauer
            markierenSubEbene();
            eiweissrastDauer = wertAktualisieren(eiweissrastDauer);
            letzterWert = programmSubEbene;
            break;
            
          case 3: // Weiter in die nächste Programmebene
            markierenSubEbene();
            letzterWert = programmSubEbene;
            break;
            
          default:
            if (programmSubEbene < 1) {
              programmSubEbene = 3;
            } else {
              programmSubEbene = 1;
            }
            break;
        }

        programmEbene = buttonZurueckWeiter(programmEbene);
        break;
        
      case 3:
        // Maltoserast Parameter einstellen

        if (!durchgang) {                // nur beim ersten Durchgang
          lcd.clear();
          lcd.home();
          lcd.print("Maltoserast Daten:");
          lcd.setCursor(1, 1);
          lcd.print("Temp.:    C");
          lcd.setCursor(1, 2);
          lcd.print("Dauer:    min");
          lcd.setCursor(1, 3);
          lcd.print("weiter oder zurueck");
          lcd.setCursor(8, 1);
          lcd.print(maltoserastTemp);   // Temperatur in Lücke einfügen
          lcd.setCursor(8, 2);
          lcd.print(maltoserastDauer);  // Dauer in Lücke einfügen
          programmSubEbene = 1;
          letzterWert = 0;
          durchgang = true;
        }

        // Ab in die Subebene! (Temperatur, Dauer oder Weiter)
        programmSubEbene = buttonHochRunter(programmSubEbene, false);
        switch (programmSubEbene) {
          case 1: // Temperatur
            markierenSubEbene();
            maltoserastTemp = wertAktualisieren(maltoserastTemp);
            letzterWert = programmSubEbene;
            break;
            
          case 2: // Dauer
            markierenSubEbene();
            maltoserastDauer = wertAktualisieren(maltoserastDauer);
            letzterWert = programmSubEbene;
            break;
            
          case 3: // Weiter in die nächste Programmebene
            markierenSubEbene();
            letzterWert = programmSubEbene;
            break;
            
          default:
            if (programmSubEbene < 1) {
              programmSubEbene = 3;
            } else {
              programmSubEbene = 1;
            }
            break;
        }

        programmEbene = buttonZurueckWeiter(programmEbene);
        break;
        
      case 4:
        // 1. Verzuckerungsrast Parameter einstellen

        if (!durchgang) {                // nur beim ersten Durchgang
          lcd.clear();
          lcd.home();
          lcd.print("1. Verzuckerungsrast");
          lcd.setCursor(1, 1);
          lcd.print("Temp.:    C");
          lcd.setCursor(1, 2);
          lcd.print("Dauer:    min");
          lcd.setCursor(1, 3);
          lcd.print("weiter oder zurueck");
          lcd.setCursor(8, 1);
          lcd.print(zuckerrast1Temp);   // Temperatur in Lücke einfügen
          lcd.setCursor(8, 2);
          lcd.print(zuckerrast1Dauer);  // Dauer in Lücke einfügen
          programmSubEbene = 1;
          letzterWert = 0;
          durchgang = true;
        }

        // Ab in die Subebene! (Temperatur, Dauer oder Weiter)
        programmSubEbene = buttonHochRunter(programmSubEbene, false);
        switch (programmSubEbene) {
          case 1: // Temperatur
            markierenSubEbene();
            zuckerrast1Temp = wertAktualisieren(zuckerrast1Temp);
            letzterWert = programmSubEbene;
            break;
            
          case 2: // Dauer
            markierenSubEbene();
            zuckerrast1Dauer = wertAktualisieren(zuckerrast1Dauer);
            letzterWert = programmSubEbene;
            break;
            
          case 3: // Weiter in die nächste Programmebene
            markierenSubEbene();
            letzterWert = programmSubEbene;
            break;
            
          default:
            if (programmSubEbene < 1) {
              programmSubEbene = 3;
            } else {
              programmSubEbene = 1;
            }
            break;
        }

        programmEbene = buttonZurueckWeiter(programmEbene);
        break;
        
      case 5:
        // 2. Verzuckerungsrast Parameter einstellen

        if (!durchgang) {                // nur beim ersten Durchgang
          lcd.clear();
          lcd.home();
          lcd.print("2. Verzuckerungsrast");
          lcd.setCursor(1, 1);
          lcd.print("Temp.:    C");
          lcd.setCursor(1, 2);
          lcd.print("Dauer:    min");
          lcd.setCursor(1, 3);
          lcd.print("weiter oder zurueck");
          lcd.setCursor(8, 1);
          lcd.print(zuckerrast2Temp);   // Temperatur in Lücke einfügen
          lcd.setCursor(8, 2);
          lcd.print(zuckerrast2Dauer);  // Dauer in Lücke einfügen
          programmSubEbene = 1;
          letzterWert = 0;
          durchgang = true;
        }

        // Ab in die Subebene! (Temperatur, Dauer oder Weiter)
        programmSubEbene = buttonHochRunter(programmSubEbene, false);
        switch (programmSubEbene) {
          case 1: // Temperatur
            markierenSubEbene();
            zuckerrast2Temp = wertAktualisieren(zuckerrast2Temp);
            letzterWert = programmSubEbene;
            break;
            
          case 2: // Dauer
            markierenSubEbene();
            zuckerrast2Dauer = wertAktualisieren(zuckerrast2Dauer);
            letzterWert = programmSubEbene;
            break;
            
          case 3: // Weiter in die nächste Programmebene
            markierenSubEbene();
            letzterWert = programmSubEbene;
            break;
            
          default:
            if (programmSubEbene < 1) {
              programmSubEbene = 3;
            } else {
              programmSubEbene = 1;
            }
            break;
        }

        programmEbene = buttonZurueckWeiter(programmEbene);
        break;
        
      case 6:
        // Eingaben überprüfen

        if (!durchgang) {                // nur beim ersten Durchgang
          lcd.clear();
          lcd.home();
          lcd.print("Eingaben pruefen!");
          lcd.setCursor(0, 1);
          lcd.print("Nach dem Start sind ");
          lcd.setCursor(0, 2);
          lcd.print("die Eingaben nicht ");
          lcd.setCursor(0, 3);
          lcd.print("mehr aenderbar.");
          
          durchgang = true;
        }

        programmEbene = buttonZurueckWeiter(programmEbene);

        break;

      default:
        if (programmEbene != 7) {        // zur Sicherheit neustarten, falls irgendwas schiefgeht
          programmEbene = 0;
        }
        break;
    }
  }


  start = millis();       // millis = Millisekunden ab Programmstart
  lcd.clear();            // LCD löschen
}

/* =====> Schleife */

void loop() {

  maischprogramm();
  ausgabe();
  motorRampe();
}

/* =====> Funktionen */

void maischprogramm() {
  // Einmaischen
  if (rast == 0) {
    heizen(einmaischenTemp);
    if (!heizung && (startRast == 0)) {   // Beim ersten mal, wenn die Temperatur = Rasttemperatur
      lcd.home();
      lcd.print("Einmaischen         ");
      startRast = millis();
      motor = 255;
    } else if (startRast > 0) {           // Wenn die Rast bereits begonnen hat
      motor = 255;
    } else {                              // Wenn die Rast noch nicht begonnen hat
      lcd.home();
      lcd.print("Aufheizen fuer Einm.");
    }
    if (((millis() - startRast) / 60000 >= einmaischenDauer) && (startRast != 0)) {
      rast++;
      startRast = 0;                      // Rast beenden
    }
  }
  
  // Eiweißrast
  else if (rast == 1) {
    heizen(eiweissrastTemp);
    if (!heizung && (startRast == 0)) {   // Beim ersten mal, wenn die Temperatur = Rasttemperatur
      lcd.home();
      lcd.print("Eiweissrast         ");
      startRast = millis();
      motor = 127;                        // Rührer mit halber Geschwindigkeit
    } else if (startRast > 0) {           // Wenn die Rast bereits begonnen hat
      motor = 127;
    } else {                              // Wenn die Rast noch nicht begonnen hat
      lcd.home();
      lcd.print("Aufheizen fuer ER.  ");
    }
    if (((millis() - startRast) / 60000 >= eiweissrastDauer) && (startRast != 0)) {
      rast++;
      startRast = 0;                      // Rast beenden
    }
  }
  
  // Maltoserast
  else if (rast == 2) {
    heizen(maltoserastTemp);
    if (!heizung && (startRast == 0)) {   // Beim ersten mal, wenn die Temperatur = Rasttemperatur
      lcd.home();
      lcd.print("Maltoserast         ");
      startRast = millis();
      motor = 127;                        // Rührer mit halber Geschwindigkeit
    } else if (startRast > 0) {           // Wenn die Rast bereits begonnen hat
      motor = 127;
    } else {                              // Wenn die Rast noch nicht begonnen hat
      lcd.home();
      lcd.print("Aufheizen fuer MR.  ");
    }
    if (((millis() - startRast) / 60000 >= maltoserastDauer) && (startRast != 0)) {
      rast++;
      startRast = 0;                      // Rast beenden
    }
  }
  
  // 1. Verzuckerungsrast
  else if (rast == 3) {
    heizen(zuckerrast1Temp);
    if (!heizung && (startRast == 0)) {   // Beim ersten mal, wenn die Temperatur = Rasttemperatur
      lcd.home();
      lcd.print("1. Verzuckerungsrast");
      startRast = millis();
      motor = 127;                        // Rührer mit halber Geschwindigkeit
    } else if (startRast > 0) {           // Wenn die Rast bereits begonnen hat
      motor = 127;
    } else {                              // Wenn die Rast noch nicht begonnen hat
      lcd.home();
      lcd.print("Aufheizen fuer VZR. 1");
    }
    if (((millis() - startRast) / 60000 >= zuckerrast1Dauer) && (startRast != 0)) {
      rast++;
      startRast = 0;                      // Rast beenden
    }
  }
  
  // 2. Verzuckerungsrast
  else if (rast == 4) {
    heizen(zuckerrast2Temp);
    if (!heizung && (startRast == 0)) {   // Beim ersten mal, wenn die Temperatur = Rasttemperatur
      lcd.home();
      lcd.print("2. Verzuckerungsrast");
      startRast = millis();
      motor = 127;                        // Rührer mit halber Geschwindigkeit
    } else if (startRast > 0) {           // Wenn die Rast bereits begonnen hat
      motor = 127;
    } else {                              // Wenn die Rast noch nicht begonnen hat
      lcd.home();
      lcd.print("Aufheizen fuer VZR. 2");
    }
    if (((millis() - startRast) / 60000 >= zuckerrast2Dauer) && (startRast != 0)) {
      rast++;
      startRast = 0;                      // Rast beenden
    }
  }
 
  // Ende
  else if (rast == 5) {
    // Abmaischen
    heizen(80);                 // Auf 80°C aufheizen
    lcd.home();
    lcd.print("Abmaischen           ");

    if (!heizung) {
      motor = 0;
      motorRampe();      // Motor abschalten macht die Rampe
      lcd.clear();
      lcd.print("Ende");
      lcd.setCursor(0,1);
      lcd.print("Temp Maische: ");
      lcd.print(getTempMaische(),1);   // Endtemperatur anzeigen
      lcd.print(" C");
      lcd.setCursor(0,2);
      lcd.print("Maischzeit ges.: ");
      lcd.print((millis() - start) / 60000);  // Gesamtzeit anzeigen
      lcd.setCursor(0,3);
      lcd.print("Bieromat ausschalten");
      
      while ((digitalRead(buttonWeiter) == LOW) && (digitalRead(buttonZurueck) == LOW)) {
        delay(50);
      }
      lcd.clear();
      lcd.setBacklight(LOW);
      delay(999999);
    }
  }
  else {
    lcd.home();
    lcd.print("Es ist ein Fehler aufgetreten.");
  }
  
}

void ausgabe() {
  // Temperatur anzeigen
  lcd.setCursor(0,2);
  lcd.print("Temp Maische: ");
  lcd.print(getTempMaische(),1);    // eine Stelle hinter dem Komma
  lcd.print(" C");
  
  // Temperaturdifferenz anzeigen
  if (temperaturdifferenz % 4 == 0) {
    lcd.setCursor(0,3);
    lcd.print("Temp.Diff.: ");
    lcd.print(abs((getTempMaische() - sensors.getTempCByIndex(0)) * 2),1);
    lcd.print(" K");    // mathematisch nur korrekt für 2 Sensoren
  }

  // Motoraktivität anzeigen
  if (temperaturdifferenz % 8 == 0) {
    lcd.setCursor(0,3);
    lcd.print("Ruehrer: ");
    motorRPM = motorJetzt / 255 * 100;
    lcd.print(motorRPM,0);  // Mit PWM auf 255: 62 rpm (Belegung: + 12 V an Weiß; GND an Braun)
    lcd.print(" %       ");
  }

  // Zeit
  lcd.setCursor(0,1);
  lcd.print("                    "); // vorherige Minutenangabe löschen
  lcd.setCursor(0,1);
  if (startRast == 0) {
    lcd.print("Maischzeit ges.: "); // bis jetzt vergangene Minuten
    lcd.print((millis() - start) / 60000);
  } else {
    lcd.print("Minute der Rast: ");// bis jetzt vergangene Rastzeit
    lcd.print((millis() - startRast) / 60000);
  }

/*
  // Heizung
  lcd.setCursor(0,3);
  lcd.print("Heizung: ");
  if (heizung) {
    lcd.print("an");
  } else {
    lcd.print("aus");
  }
*/

temperaturdifferenz++;
}

void heizen(float wunschTemp) {     // Die Rasttemperatur wird übergeben
  float x = 0;
  if (heizung && startRast == 0) {  // Wenn in der Aufheizphase, dann Heizung 2K vorher abschalten
    x = 2;
  } else {
    x = 0.5;
  }
  if (getTempMaische() < (wunschTemp - x)) {  // Heizung schaltet erst an, wenn die Temperatur x K unter der gewünschten liegt
    if (!heizung) {
      digitalWrite(relais1, HIGH);  // Heizung an (Relaiseingang 1)
      heizung = true;
      motor = 255;
    }
  } else {
    digitalWrite(relais1, LOW);     // Heizung aus
    heizung = false;
  }
}

float getTempMaische() {
  tempMaische = 0;
  sensors.requestTemperatures();
  for (int i = 0; i < anzahlTempSens; i++) {
    tempMaische = tempMaische + sensors.getTempCByIndex(i);
  }
  return(tempMaische = tempMaische / anzahlTempSens); // Mittelwert bilden
}

void motorRampe(){
  motorRampeZeit = millis();
  while ((millis() - motorRampeZeit) < 1000) {  // Somit wird ein delay(1000) erzeugt
    if (motor > motorJetzt) {
      motorJetzt = max(50, motorJetzt) + 1; // 50 ist die Mindestgeschwindigkeit, sonst passiert nichts
      if (motorJetzt >= motor) {  // Korrektur
        motorJetzt = motor;
      }
      analogWrite(motorPWM, motorJetzt);
    }
    else if (motor < motorJetzt) {
      motorJetzt = motorJetzt - 1;
      if ((motorJetzt <= motor) || (motorJetzt < 50)) {
        motorJetzt = motor;
      }
      analogWrite(motorPWM, motorJetzt);
    }
    delay(100);  // bestimmt die Rampengeschwindigkeit
  }
}

void checkButtons() {
  if ((millis() - letzterDruck) > 333) {    // debounce ohne delay(). 333ms = debounce-Zeit
  
  if (digitalRead(buttonZurueck) == HIGH) {
    zurueck = true;
  }
  if (digitalRead(buttonWeiter) == HIGH) {
    weiter = true;
  }
  if (digitalRead(buttonHoch) == HIGH) {
    hoch = true;
  }
  if (digitalRead(buttonRunter) == HIGH) {
    runter = true;
  }
  }
}

int buttonZurueckWeiter(int parameter) {
    if (zurueck) {
      parameter = parameter - 1;
      letzterDruck = millis();
      durchgang = false;
      zurueck = false;
    }
    if (weiter) {
      parameter++;
      letzterDruck = millis();
      durchgang = false;
      weiter = false;
    }
    if (parameter < 0) {
      parameter = 0;
    }
  return parameter;
}

int buttonHochRunter(int parameter, bool nichtMenue) {
  if (nichtMenue) {
    if (hoch) {
      parameter++;
      letzterDruck = millis();
      hoch = false;
    }
    if (runter) {
      parameter = parameter - 1;
      letzterDruck = millis();
      runter = false;
    }
    
  } else { // Wenn parameter ein Menünavigationsparameter (nichtMenue = false) ist, funktionieren die Tasten andersherum
    if (hoch) {
      parameter = parameter - 1;
      letzterDruck = millis();
      hoch = false;
    }
    if (runter) {
      parameter++;
      letzterDruck = millis();
      runter = false;
    }
  }
  if (parameter < 0) {
    parameter = 0;
  }
  
  return parameter;
}

void markierenSubEbene() {
  if (letzterWert != programmSubEbene) {
    for (int i = 1; i < 4; i++) {  // vorherige Auswahlmarkierungen löschen
      lcd.setCursor(0, i);
      lcd.print(" ");
    }
    lcd.setCursor(0, programmSubEbene);
    lcd.print("-");
  }
}

int wertAktualisieren(int wert) {       // Nur zum Einstellen der Brauparameter im Submenü
  if (1 == buttonZurueckWeiter(0)) {    // Wenn "Weiter" gedrückt wurde
    bool an = false;
    while (0 != buttonZurueckWeiter(1)) {   // Solange nicht "Zurück" gedrückt wurde
      checkButtons();       // Knöpfe nochmal prüfen, weil wir in einer Schleife sind
      wert = buttonHochRunter(wert, true);
      if (letzterWert != wert) {   // neuen Wert in Lücke einfügen
        lcd.setCursor(8, programmSubEbene);
        if (wert > 9) {   // einstellige Zahlen anpassen
          lcd.print(wert);
        } else {
          lcd.print(" ");
          lcd.print(wert);
        }
      }
      letzterWert = einmaischenTemp;

      // blinkenden Pfeil einfügen, damit man weiß wo man ist
      if (!an) {
        if ((millis() - blinkAn) > 500) {
          lcd.setCursor(15, programmSubEbene);  // Pfeil malen
          lcd.print("<--");
          an = true;
          blinkAn = millis();
        }
      } else {
        if ((millis() - blinkAn) > 500) {
          lcd.setCursor(15, programmSubEbene);  // Pfeil wieder löschen
          lcd.print("   ");
          an = false;
          blinkAn = millis();
        }
      }

    }
    durchgang = true;
    lcd.setCursor(15, programmSubEbene);  // Pfeil wieder löschen
    lcd.print("   ");
  }
  return wert;
}

