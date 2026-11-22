# Firmware PulsLab

Prima versiune pentru Arduino Uno R3 si MAX30100, scrisa cu asistenta Codex.
Se foloseste doar biblioteca Wire inclusa in Arduino AVR Boards.
Procesarea PPG, BPM si SpO2 ramane in aplicatia web.

## Incarcare

1. In Arduino IDE 2.x instaleaza/actualizeaza **Arduino AVR Boards** din Boards Manager.
   Este necesara o versiune Wire care include `setWireTimeout`.
2. Deschide `PulsLab/PulsLab.ino`, selecteaza Arduino Uno si portul placii.
3. Apasa Verify, apoi Upload.
4. Deschide Serial Monitor la **115200 baud**, cu terminator Newline sau Both NL & CR.
   Apar `#BOOT`, `#READY`, `#CFG`, apoi linii `D,0,...`.
5. Inchide Serial Monitor inainte de conectarea din aplicatia web.

## Conectare

| Modul MAX30100 | Uno |
| --- | --- |
| VIN | 5V doar pentru modulul cu regulatoare, conform specificatiei sale |
| GND | GND |
| SDA | A4 |
| SCL | A5 |
| INT | neconectat in aceasta versiune |

Firmware-ul verifica PART_ID=0x11. MAX30102 nu este compatibil cu acest driver.
Alimentarea circuitului integrat simplu nu este echivalenta cu alimentarea modulului.

## Configuratie si protocol

I2C 0x57, magistrala 100 kHz. MODE 0x03; SPO2_CONFIG 0x47:
100 esantioane/s, impuls 1600 us, ADC 16 biti. LED_CONFIG contine RED
in nibble-ul superior si IR in cel inferior; implicit ambii au indexul 7
(aproximativ 24 mA, valoare care se ajusteaza pe montaj).

| Comanda terminata cu newline | Efect |
| --- | --- |
| `?` | `#CFG` cu configuratia dorita si indicatorul `ready` |
| `L7,7` | indici IR,RED 0–15; aplicare imediata daca senzorul este pregatit |
| `L5,3` | IR index 5, RED index 3; curentii nu au pasi liniari |
| `P` | opreste liniile D; citirea senzorului continua |
| `S` | reia liniile D, fara resetarea secventei |

Date: `D,<nr>,<ir>,<red>\n`. Valorile sunt brute, fara filtrare.
In pauza secventa continua sa creasca; aplicatia poate raporta aceste esantioane
netransmise ca pierderi. Nu folosi pauza in testul ratei/pierderilor pe 5 minute.
Secventa uint32 revine la zero dupa aproximativ 497 zile de achizitie continua.

Initializarea este o masina de stari, fara delay(). Resetul are timeout 100 ms,
erorile initiaza o noua incercare dupa 1 s, iar lipsa datelor timp de 1 s
declanseaza reinitializarea. Comenzile raman disponibile in timpul recuperarii.
Setarile LED acceptate cat timp senzorul lipseste se aplica la reconectare.
`ready=0` indica faptul ca setarile nu sunt confirmate pe senzor.

FIFO este verificat la 5 ms; se citesc perechile IR/RED existente, nu se genereaza
esantioane cu un temporizator software. La overflow se arunca FIFO si se avanseaza
secventa cu 16 + contorul overflow. Contorul saturat la 15 nu permite recuperarea
numarului exact de pierderi. La erori I2C/reinitializare, durata intreruperii si
pierderile exacte nu sunt reconstruite; jurnalul trebuie inclus in interpretarea
testului. Daca bufferul serial nu are loc pentru o linie completa, esantionul este
omis si urmatorul numar de ordine arata pierderea.

## Verificare pe montaj

| Cerinta | Verificare |
| --- | --- |
| F1 | `#READY`; readback intern MODE, SPO2_CONFIG si LED_CONFIG |
| F2–F3 | linii D valide, canale distincte, secventa crescatoare, circa 100 Hz |
| F4 | pornire fara senzor, apoi reconectare; erori si revenire automata |
| F5 | `?`, `L5,3`, `P`, `S`; comenzi invalide, valori -1/16 si linie prea lunga |
| F6 | fara delay in loop, parser cu buffer fix, timeout I2C |

F7 (INT) nu este implementata. H5 (LED pentru fiecare bataie) nu este implementata;
bataile se detecteaza in browser.

Ruleaza cel putin 5 minute fara pauza in aplicatie, noteaza rata reala si pierderile,
verifica deconectarea USB, fara deget, miscarea si saturatia. Validarea fizica si
masuratorile comparative raman de facut. Codul nu transforma prototipul intr-un
dispozitiv medical.

Referinta registrelor: [datasheet oficial MAX30100](https://www.analog.com/media/en/technical-documentation/data-sheets/max30100.pdf),
sectiunile Register Map, FIFO, Mode Configuration, SpO2 Configuration si LED Configuration.
