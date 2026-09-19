# Fluke NORMA 4000/5000 – TCP/Socket API (fjernstyring)

Dette dokumentet beskriver fjernstyrings-API-et for effektanalysatorene **Fluke NORMA 4000 og NORMA 5000**. Instrumentet fjernstyres med **SCPI 1999.0**-kommandoer (Standard Commands for Programmable Instruments) sendt som tekstlinjer over **TCP/IP** via Ethernet-grensesnittet, der TCP-porten er **fast satt til 23**. Nøyaktig samme kommandosett gjelder også for de øvrige grensesnittene RS-232, USB (virtuell COM-port) og GPIB (IEC/IEEE-buss) – bytter du transportlag, forblir kommandoene identiske.

Dokumentet er satt sammen for å kunne brukes direkte som referanse når du utvikler en egen applikasjon (TCP-klient) som snakker med instrumentet.

**Kilde:** Fluke NORMA 4000/5000 Remote Control Users Guide, juni 2007 Rev. 2, 5/12.

> ## Kom raskt i gang
>
> 1. Koble til instrumentets IP-adresse på **TCP-port 23** med en vanlig TCP-strømsocket (eller test med `telnet <ip> 23`). IP-adresse, subnettmaske og gateway konfigureres i instrumentets **General Setup**-skjermbilde.
> 2. Send SCPI-kommandoer som ren ASCII-tekst, én kommandolinje om gangen, terminert med linjeskift `<LF>` (`\n`, 0Ah).
> 3. For spørringer (kommandoer som slutter med `?`): les svaret tilbake som én tekstlinje avsluttet med `\n` (fjern eventuell `\r` foran).
> 4. Test forbindelsen med `*IDN?` – instrumentet svarer f.eks. `Fluke,NORMA4000,KN34512BA,01.00`.
> 5. Typisk måleoppsett: `*RST` → konfigurer (`ROUT:SYST`, `SYNC:SOUR`, områder, `APER`, `FUNC`) → `INIT:CONT ON` → hent verdier med `DATA?`.
> 6. Komplette, gjenbrukbare klienteksempler (inkludert rå TCP-socket uten VISA) finnes i kapittelet [Programmeringseksempler](#programmeringseksempler).

## Innhold

1. [Tilkobling og grensesnitt](#tilkobling-og-grensesnitt)
2. [Protokoll og SCPI-syntaks](#protokoll-og-scpi-syntaks)
3. [Felleskommandoer og målefunksjoner](#felleskommandoer-og-målefunksjoner)
4. [Subsystemer: ABORt til ROUTe](#subsystemer-abort-til-route)
5. [Subsystemer: SENSe, SENSe2 og SOURce](#subsystemer-sense-sense2-og-source)
6. [Subsystemer: SYNC til STATus](#subsystemer-sync-til-status)
7. [Hurtigreferanse: alle kommandoer](#hurtigreferanse-alle-kommandoer)
8. [Statusrapporteringssystemet](#statusrapporteringssystemet)
9. [Feilmeldinger](#feilmeldinger)
10. [Programmeringseksempler](#programmeringseksempler)

---

## Tilkobling og grensesnitt

Dette kapittelet beskriver maskinvaregrensesnittene for fjernstyring av Fluke NORMA 4000/5000 Power Analyzer. Som standard er instrumentet utstyrt med RS-232-grensesnitt. Som opsjon kan instrumentet også utstyres med IEC/IEEE-bussgrensesnitt (GPIB), IEEE 802.3 (Ethernet) og USB. **Samme SCPI-kommandosett gjelder uavhengig av hvilket grensesnitt som brukes** – for en TCP/Socket-applikasjon er Ethernet-grensesnittet det mest aktuelle.

### IEEE 802.3 (Ethernet) – opsjon (hovedgrensesnitt for TCP/Socket)

Instrumentet kan som opsjon utstyres med IEEE 802.3 (Ethernet)-grensesnitt. Kontakten for Ethernet-grensesnittet (RJ-45) er plassert på baksiden av instrumentet. En kontroller (PC) for fjernstyring kan kobles til via grensesnittet. Tilkobling skjer med tvunnet parkabel (twisted pair).

#### Grensesnittets egenskaper (Ethernet)

- Toveis TCP/IP-datatransmisjon
- 10/100 Mbps drift
- Half/full duplex
- Høy dataoverføringshastighet: maks. 240 kB/s (måledata), 1,3 MB/s (rådata)

#### Signallinjer (RJ-45)

| Pinne | Signal | Beskrivelse |
|-------|--------|-------------|
| 1 | TD+ (Transmit Data Plus) | Positivt signal i TD-differensialparet; inneholder den serielle utgående datastrømmen som instrumentet sender ut på nettverket. |
| 2 | TD− (Transmit Data Minus) | Negativt signal i TD-differensialparet; inneholder samme utgangsdata som pinne 1 (TD+). |
| 3 | RD+ (Receive Data Plus) | Positivt signal i RD-differensialparet; inneholder den serielle inngående datastrømmen instrumentet mottar fra nettverket. |
| 6 | RD− (Receive Data Minus) | Negativt signal i RD-differensialparet; inneholder samme inngangsdata som pinne 3 (RD+). |

#### Kabling mellom instrument og kontroller

Kabling mellom instrument og kontroller skjer med tvunnet parkabel med RJ-45-plugger. To tilkoblingsmåter støttes:

- **Via lokalnett (hub/switch):** vanlig rett («patch»/straight-through) kabel mellom instrumentet og hub/switch, og mellom hub/switch og kontrolleren (NIC).
- **Direkte tilkobling:** kontrolleren kobles direkte til instrumentet med krysset kabel (crossover), der TD+/RD+-parene krysses (pinne 1↔3, 2↔6).

#### Tilkoblingsinnstillinger (TCP/IP)

For å styre instrumentet over Ethernet-grensesnittet må det først opprettes en TCP/IP-forbindelse med disse innstillingene:

| Innstilling | Beskrivelse |
|-------------|-------------|
| IP address | Instrumentets Internet Protocol-adresse (for eksempel `192.168.1.100`). |
| TCP port number | Transmission Control Protocol-portnummer. Dette er for tiden fast satt til **23** (porten som er tildelt «telnet»-tjenesten). |
| IP subnet address mask | Internet Protocol-subnettmaske (for eksempel `255.255.255.0`). |
| IP gateway address | Internet Protocol-adressen til gatewayen (for eksempel `192.168.1.1`). |

På instrumentsiden konfigureres disse innstillingene i **General Setup**-skjermbildet. Når forbindelsen opprettes, må kontrolleren bruke instrumentets IP-adresse og TCP-port som destinasjonsadresse.

Eksempel på tilkobling (enhver TCP-socket/telnet-klient kan brukes):

```
telnet 192.168.1.100 23
```

### IEC/IEEE-bussgrensesnitt (GPIB) – opsjon

Instrumentet kan som opsjon utstyres med et IEC/IEEE-bussgrensesnitt. Kontakten for IEEE 488 er plassert på baksiden av instrumentet. En kontroller for fjernstyring kobles til via grensesnittet med skjermet kabel.

#### Grensesnittets egenskaper (GPIB)

- 8-bits parallell datatransmisjon
- Toveis datatransmisjon
- Trelednings-handshake (three-wire handshake)
- Høy dataoverføringshastighet: maks. 115 kB/s (måledata), 1,2 MB/s (rådata)
- Inntil 15 enheter kan kobles til
- Maksimal lengde på tilkoblingskabler 15 m (enkeltforbindelse 2 m)
- «Wired OR» dersom flere instrumenter kobles i parallell

#### Busslinjer

**Databuss med 8 linjer, DIO 1 til DIO 8**
Overføringen er bit-parallell og byte-seriell i ASCII/ISO-kode. DIO1 er minst signifikante bit, DIO8 er mest signifikante.

**Kontrollbuss med 5 linjer**

- `IFC` (Interface Clear): Aktiv LOW tilbakestiller grensesnittene til de tilkoblede instrumentene til standardinnstillingen.
- `ATN` (Attention): Aktiv LOW signaliserer overføring av grensesnittmeldinger. Inaktiv HIGH signaliserer overføring av enhetsmeldinger.
- `SRQ` (Service Request): Aktiv LOW lar instrumentet sende en serviceforespørsel til kontrolleren.
- `REN` (Remote Enable): Aktiv LOW muliggjør omkobling til fjernstyring.
- `EOI` (End or Identify): Har to funksjoner i kombinasjon med ATN:
  - ATN = HIGH: Aktiv LOW markerer slutten på en dataoverføring.
  - ATN = LOW: Aktiv LOW utløser parallell polling (parallel poll).

**Handshake-buss med 3 linjer**

- `DAV` (Data Valid): Aktiv LOW signaliserer en gyldig databyte på databussen.
- `NRFD` (Not Ready For Data): Aktiv LOW signaliserer at en av de tilkoblede enhetene ikke er klar til å motta data.
- `NDAC` (Not Data Accepted): Aktiv LOW så lenge instrumentet tar imot data som ligger på databussen.

#### Grensesnittfunksjoner

Instrumenter som kan fjernstyres via IEC/IEEE-bussen kan være utstyrt med ulike grensesnittfunksjoner. Tabell 3-1 viser grensesnittfunksjonene som er relevante for instrumentet.

**Tabell 3-1. Grensesnittfunksjoner**

| Kontrolltegn | Grensesnittfunksjon |
|--------------|---------------------|
| SH1 | Handshake-kildefunksjon (Source Handshake). |
| AH1 | Handshake-mottaksfunksjon (Acceptor Handshake). |
| L4 | Listener-funksjon. |
| T6 | Talker-funksjon, med evne til å svare på seriell polling. |
| SR1 | Serviceforespørselsfunksjon (Service Request). |
| PP1 | Parallell pollingfunksjon *(ikke implementert)* |
| RL1 | Remote/local-omkoblingsfunksjon *(ikke implementert)* |
| DC1 | Tilbakestillingsfunksjon (Device Clear) *(ikke implementert)* |
| DT1 | Triggerfunksjon (Device Trigger) *(ikke implementert)* |

#### Grensesnittmeldinger

Grensesnittmeldinger overføres til instrumentet på datalinjene mens ATN (Attention)-linjen er aktiv LOW. Disse meldingene brukes til kommunikasjon mellom kontrolleren og instrumentet.

##### Universalkommandoer

Universalkommandoer (se Tabell 3-2) ligger i kodeområdet 10 til 1F hex. De virker på alle instrumenter som er koblet til bussen, uten at de adresseres først.

**Tabell 3-2. Universalkommandoer**

| Kommando | QuickBASIC-kommando | Virkning på instrumentet |
|----------|--------------------|--------------------------|
| DCL (Device Clear) *(ikke implementert)* | `IBCMD (controller%, CHR$(20))` | Avbryter behandlingen av kommandoene som nettopp er mottatt, og setter kommandobehandlingsprogramvaren til en definert starttilstand. Endrer ikke instrumentinnstillingen. |
| IFC (Interface Clear) | `IBSIC (controller%)` | Tilbakestiller grensesnittene til standardtilstanden. |
| LLO (Local Lockout) *(ikke implementert)* | `IBCMD (controller%, CHR$(17))` | Manuell omkobling til LOCAL deaktiveres. |
| SPE (Serial Poll Enable) | `IBCMD (controller%, CHR$(24))` | Klar for seriell polling. |
| SPD (Serial Poll Disable) | `IBCMD (controller%, CHR$(25))` | Slutt på seriell polling. |
| PPU (Parallel Poll Unconfigure) | `IBCMD (controller%, CHR$(21))` | Slutt på parallell pollingtilstand. |

##### Adresserte kommandoer

Adresserte kommandoer ligger i kodeområdet 00 til 0F hex. De virker bare på instrumenter som er adressert som «listener».

**Tabell 3-3. Adresserte kommandoer**

| Kommando | QuickBASIC-kommando | Virkning på instrumentet |
|----------|--------------------|--------------------------|
| SDC (Selected Device Clear) *(ikke implementert)* | `IBCLR (device%)` | Avbryter behandlingen av kommandoene som nettopp er mottatt, og setter kommandobehandlingsprogramvaren til en definert starttilstand. Endrer ikke instrumentinnstillingen. |
| GET (Group Execute Trigger) *(ikke implementert)* | `IBTRG (device%)` | Trigger en tidligere aktiv instrumentfunksjon (for eksempel en sweep). Virkningen av kommandoen er identisk med en puls på den eksterne triggersignalinngangen. |
| GTL (Go to Local) *(ikke implementert)* | `IBLOC (device%)` | Overgang til LOCAL-tilstand (manuell betjening). |
| PPC (Parallel Poll Configure) *(ikke implementert)* | `IBPPC (device%, data%)` | Konfigurerer instrumentet for parallell polling. QuickBASIC-kommandoen utfører i tillegg PPE / PPD. |

### RS-232-C-grensesnitt (standard)

Instrumentet er utstyrt med et RS-232-C-grensesnitt som standard. Den 9-polede kontakten er plassert på baksiden av enheten. En kontroller for fjernstyring kan kobles til via grensesnittet.

#### Grensesnittets egenskaper (RS-232)

- Seriell datatransmisjon i asynkron modus
- Toveis datatransmisjon via to separate linjer
- Valgbar overføringshastighet fra 1200 til 115200 baud
- Logisk 0-signalnivå fra +3 V til +15 V
- Logisk 1-signalnivå fra −15 V til −3 V
- En ekstern enhet (kontroller) kan kobles til
- Programvare-handshake (XON, XOFF)
- Maskinvare-handshake

#### Signallinjer (9-polet kontakt)

| Pinne | Signal | Beskrivelse |
|-------|--------|-------------|
| 2 | TxD (Transmit Data) | Datalinje; overføring fra instrument til ekstern kontroller (DTE). |
| 3 | RxD (Receive Data) | Datalinje; overføring fra ekstern kontroller til instrument. |
| 4 | DSR (Data Set Ready) | Brukes ikke. |
| 5 | GND (Ground) | Grensesnittjord, koblet til instrumentjord. |
| 6 | DTR (Data Terminal Ready) | Brukes ikke. |
| 7 | CTS (Clear To Send) | Inngang fra DTE/kontroller. Enheten stopper å sende data til DTE/kontrolleren når den detekterer at CTS-linjen går lav. |
| 8 | RTS (Request To Send) | Utgang til DTE/kontroller. Enheten setter RTS-linjen lav (logisk 0) når den ikke kan ta imot mer data fra DTE/kontrolleren. |
| 9 | RI | – |

#### Transmisjonsparametre

For å sikre feilfri og korrekt dataoverføring må transmisjonsparametrene på instrumentet og kontrolleren ha samme innstillinger. Innstillingene gjøres i **General Setup**-skjermbildet på instrumentet.

| Parameter | Beskrivelse |
|-----------|-------------|
| Transmission rate (baud rate) | Åtte ulike baudrater kan stilles inn på instrumentet: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200. |
| Data bits | Dataoverføring skjer i 8-bits eller 7-bits ASCII-kode. LSB (minst signifikante bit) overføres som første bit. |
| Start bit | Overføringen av en databyte innledes med en startbit. Startbitens fallende flanke markerer begynnelsen på databyten. |
| Parity bit | Odd, Even, Zero, One, None |
| Stop bit | Overføringen av en databyte avsluttes med en stoppbit. |

Bitrekkefølge: bit 01 = startbit, bit 02 til 09 = databiter, bit 10 = stoppbit. Bitvarighet = 1/baudrate.

Eksempel fra manualen: overføring av tegnet `A` (41 hex) i 8-bits ASCII-kode.

#### Grensesnittfunksjoner (kontrolltegn)

For grensesnittstyring kan en rekke kontrolltegn definert fra 0 til 20 hex i ASCII-koden overføres via grensesnittet.

**Tabell 3-4. Kontrolltegn for RS-232-C-grensesnittet**

| Kontrolltegn | Grensesnittfunksjon |
|--------------|---------------------|
| `<Ctrl Q>` 11 hex | Aktiverer tegnutmating (XON). |
| `<Ctrl S>` 13 hex | Stopper tegnutmating (XOFF). |
| Break (minst 1 tegn logisk 0) | Tømmer instrumentets inngangsbuffer. Alle ventende spørringer avbrytes. Tilsvarer IFC på GPIB-grensesnittet. |
| 0A hex | Terminator `<LF>`. Instrumentet går til remote-tilstand ved mottak av dette tegnet sammen med en gyldig kommando. |

#### Handshake

**Programvare-handshake**
Programvare-handshake med XON/XOFF-protokollen styrer dataoverføringen. Hvis mottakeren (instrumentet) vil stanse inndata, sender den XOFF til senderen. Senderen avbryter da datautmatingen til den mottar XON fra mottakeren. Samme funksjon finnes også på sendersiden (kontrolleren).

> **Merk:** Programvare-handshake er ikke egnet for overføring av binærdata – maskinvare-handshake foretrekkes.

**Maskinvare-handshake**
Med maskinvare-handshake signaliserer instrumentet mottaksberedskap via linjene DTR og RTS. Logisk 0 betyr «klar», logisk 1 betyr «ikke klar». Om kontrolleren er klar til å motta, signaliseres til instrumentet via CTS- eller DSR-linjen (se «Signallinjer»). Instrumentets sender slås på med logisk 0 og av med logisk 1. RTS-linjen forblir aktiv så lenge det serielle grensesnittet er aktivt. DTR-linjen styrer instrumentets mottaksberedskap.

**Kabling mellom instrument og kontroller**
Kablingen mellom instrumentet og kontrolleren er en forlengelseskabel (ved 9-polet kontrollerkontakt), det vil si at data-, kontroll- og signallinjene har rett gjennomgående kabling (straight-through). Kablingsskjemaet gjelder for kontrollere med 9-polet eller 25-polet kontakt: signalparene DCD/DCD, TxD/RxD, RxD/TxD, DSR/DTR, GND/GND, DTR/DSR, CTS/RTS, RTS/CTS og RI/RI kobles mellom instrumentets 9-polede hunnkontakt og kontrolleren.

### Universal Serial Bus (USB) – opsjon

Instrumentet kan som opsjon utstyres med USB-grensesnitt. Kontakten for USB-grensesnittet (USB serie «B»-hunnkontakt) er plassert på baksiden av instrumentet. En kontroller for fjernstyring kan kobles til via grensesnittet. Tilkobling skjer med USB A-til-USB B-kabel (også kalt USB A/B-kabel eller serie «A»-plugg til serie «B»-plugg-kabel).

#### Grensesnittets egenskaper (USB)

- Toveis USB-datatransmisjon
- Kompatibel med USB 1.1 og USB 2.0
- Dataoverføringshastighet 800 kB/s (rådata)

#### Pinnetilordning (serie B-hunnkontakt)

| Pinne | Signal |
|-------|--------|
| 1 | VBUS (power) |
| 2 | D− (data minus) |
| 3 | D+ (data plus) |
| 4 | GND (ground) |

#### Tilkoblingsinnstillinger (VCP)

For å styre instrumentet over USB-grensesnittet må det først opprettes en serieportforbindelse med VCP-navnet (Virtual COM Port, for eksempel `COM3`) som er knyttet til instrumentets USB-grensesnitt. VCP-navnet finnes i Windows Enhetsbehandling under «Ports (COM & LPT)». Der vises en port med navnet «NORMA Power Analyzer USB Serial Port» når instrumentet er slått på og koblet til en PC med USB-kabel.

På instrumentsiden brukes **General Setup**-skjermbildet til å velge USB-grensesnittet. Ingen andre innstillinger trengs på instrumentsiden.

## Protokoll og SCPI-syntaks

Dette kapittelet gir grunnleggende informasjon om fjernstyring av instrumentet: grensesnitt- og enhetsmeldinger, kommandobehandling, statusrapporteringssystem osv. Instrumentet er utstyrt med RS-232-C-grensesnitt og valgfritt med et IEC/IEEE-bussgrensesnitt i henhold til standarden IEC 625.1/IEEE 488.1. Kontaktene sitter på baksiden av instrumentet og lar deg koble til en kontroller for fjernstyring.

Instrumentet støtter **SCPI versjon 1999.0** (Standard Commands for Programmable Instruments). SCPI-standarden er basert på standarden IEEE 488.2 og har som mål å standardisere enhetsspesifikke kommandoer, feilhåndtering og statusregistrene.

Det forutsettes at brukeren har grunnleggende kunnskap om IEC/IEEE-bussprogrammering og betjening av kontrolleren. Programmeringseksemplene for IEC-buss i manualen er alle skrevet med VISA C API.

### Komme i gang

En kort betjeningssekvens som raskt viser instrumentets grunnfunksjoner.

**Forutsetninger:**

- Instrumentet kobles til port COM1 på den styrende datamaskinen. Fabrikkinnstillinger: Baud Rate = 115200, Data Bits = 8, Stop Bits = 1, Parity = None, Handshake = RTS/CTS.
- Programmet HyperTerminal brukes til å kommunisere med instrumentet.

**Prosedyre:**

1. Koble sammen instrumentet og den styrende datamaskinen.
2. Start HyperTerminal på datamaskinen (Start > Programs > Accessories > Communication > HyperTerminal). HyperTerminal er en standard del av Windows.
3. Hvis HyperTerminal aldri har vært brukt/konfigurert før:
   - I vinduet *Connection Description*: skriv inn navnet `Fluke` og trykk OK.
   - I vinduet *Connect To*: velg `COM1` under *Connect using* (eller en annen port hvis du bruker den) og trykk OK.
   - I vinduet *COM1 Properties*: sett riktige egenskaper og trykk OK:
     - Bits per second = 115200
     - Data bits = 8
     - Parity = None
     - Stop bits = 1
     - Flow control = None
4. Gå til File > Properties > Settings > ASCII Setup og huk av følgende, og trykk OK to ganger:
   - Send line ends with line feeds
   - Echo typed characters locally
   - Append line feeds to incoming line ends
5. Skriv `*IDN?` i hovedvinduet (det hvite) og trykk Enter. (Ikke skriv feil — alle tegn sendes umiddelbart til instrumentet når du trykker en tast. Backspace sletter ikke feilskrevne tegn. Gjør du en feil, trykk Enter flere ganger; det setter ting i orden igjen.)
6. Instrumentet returnerer identifikasjonsstrengen, for eksempel:

   ```
   Fluke,NORMA4000,KN34512BA,01.00
   ```

7. Skriv `DATA? "POW"` i hovedvinduet og trykk Enter. Dette ber instrumentet returnere siste gyldige effektmåling.
8. Instrumentet returnerer siste gyldige effektmåling, for eksempel:

   ```
   +1.23456E+02
   ```

### Omkobling til fjernstyring

Ved oppstart er instrumentet alltid i manuell betjeningsmodus ("LOCAL"-tilstand) og kan betjenes via frontpanelet.

Instrumentet kobles om til fjernstyring ("REMOTE"-tilstand) slik:

- **IEC/IEEE-buss:** når det mottar en adressert kommando fra kontrolleren med REN-linjen satt.
- **Andre grensesnitt:** når det mottar en gyldig kommando terminert med line feed `<LF>` (= 0Ah) fra kontrolleren i tilstanden `SYSTem:KLOCk REM`, eller eksplisitt via denne kommandoen.

Under fjernstyring er betjening via frontpanelet deaktivert. Instrumentet forblir i fjernstyringstilstand til det settes tilbake til manuell tilstand via frontpanelet eller via fjernstyringen. Omkobling fra manuell til fjernstyring og omvendt påvirker ikke instrumentinnstillingene.

#### Indikasjoner under fjernstyring

Fjernstyringstilstanden vises med et toveisradio-ikon i cellen lengst til venstre i statuslinjen på instrumentets skjerm. Et nøkkelikon i tredje celle i statuslinjen indikerer at [LOCAL]-tasten (F6/Esc) er deaktivert, og at omkobling til manuell betjening bare kan gjøres via fjernstyring. Vises ikke nøkkelikonet, kan omkobling til manuell betjening gjøres med [LOCAL]-tasten (F6/Esc).

### Retur til manuell betjening

Retur til manuell betjening kan gjøres via frontpanelet eller IEC/IEEE-bussen.

**Manuelt:** Trykk [LOCAL]-tasten.

Merk:

- Før omkobling må kommandobehandlingen være fullført, ellers skjer omkobling til fjernstyring umiddelbart igjen.
- [LOCAL]-tasten kan deaktiveres med kommandoen `SYSTem:KLOCk ON` eller universalkommandoen LLO (kun GPIB) for å hindre utilsiktet omkobling. Da er omkobling til manuell betjening bare mulig via fjernstyring.
- [LOCAL]-tasten kan aktiveres igjen med kommandoen `SYSTem:KLOCk OFF` eller ved å deaktivere REN-kontrollinjen (kun GPIB).

**Fjernstyrt:**

- GTL-grensesnittmelding (kun GPIB)
- Med kommandoen `SYSTem:KLOCk OFF`

### Kommandoer og instrumentresponser

Instrumentkommandoer overføres via det valgte grensesnittet. Med unntak av enkelte enhetsresponser (binærdata) brukes ASCII-kode. På IEC/IEEE-buss (GPIB) omtales kommandoer og instrumentresponser som enhetsmeldinger (device messages). Kommandoer og responser er i all hovedsak identiske for alle grensesnittyper. Det skilles etter hvilken retning enhetsmeldingene sendes på grensesnittet.

**Kommandoer** er meldinger kontrolleren sender til instrumentet. De betjener enhetsfunksjonene og ber om informasjon. Kommandoer inndeles etter to kriterier:

1. Etter virkningen de har på instrumentet:
   - *Innstillingskommandoer* (setting commands) forårsaker instrumentinnstillinger, for eksempel tilbakestilling av instrumentet eller setting av utgangsnivået til 1 V.
   - *Spørringer* (queries) gjør at data legges klar for utlesing på grensesnittet, for eksempel enhetsidentifikasjon eller avlesning av aktiv inngang.
2. Etter definisjonen i standarden IEEE 488.2:
   - *Common Commands* (fellesskommandoer) er eksakt definert med hensyn til funksjon og notasjon i IEEE 488.2. De gjelder funksjoner som håndtering av de standardiserte statusregistrene, tilbakestilling og selvtest.
   - *Enhetsspesifikke kommandoer* gjelder funksjoner som avhenger av instrumentets egenskaper, for eksempel frekvensinnstilling. Flertallet av disse kommandoene er også standardisert av SCPI-komiteen.

**Enhetsresponser** (device responses) er meldinger instrumentet sender til kontrolleren som svar på en spørring. De kan inneholde måleresultater eller informasjon om instrumentets status.

### Struktur og syntaks for enhetsmeldinger

#### Introduksjon til SCPI

SCPI (Standard Commands for Programmable Instruments) beskriver et standard kommandosett for programmering av instrumenter, uavhengig av instrumenttype eller produsent. Målet til SCPI-konsortiet er å standardisere de enhetsspesifikke kommandoene i størst mulig grad. Det er utviklet en modell som definerer identiske funksjoner i en enhet eller i forskjellige enheter, og kommandosystemer er laget slik at identiske funksjoner kan adresseres med identiske kommandoer. Kommandosystemene har hierarkisk struktur (trestruktur).

SCPI er basert på standarden IEEE 488.2 og bruker de samme grunnleggende syntakselementene og fellesskommandoene som er definert der. Deler av syntaksen for enhetsresponsene er definert mer detaljert enn i IEEE 488.2 (se avsnittet "Responser på spørringer").

#### Kommandostruktur

Kommandoene består av en *header* og, i de fleste tilfeller, én eller flere *parametre*. Header og parametre skilles med et "white space" (ASCII-kode 0 til 9, 11 til 32 desimalt, blank). Headere kan bestå av flere nøkkelord. Spørringer dannes ved å legge et spørsmålstegn direkte etter headeren.

#### Common Commands (fellesskommandoer)

Fellesskommandoer (enhetsuavhengige) består av en header innledet med en asterisk `*`, og eventuelt én eller flere parametre.

Eksempler:

```
*RST      RESET, tilbakestiller instrumentet
*ESE 253  EVENT STATUS ENABLE, setter bitene i event status enable-registeret
*ESR?     EVENT STATUS QUERY, leser innholdet i event status-registeret
```

#### Enhetsspesifikke kommandoer

##### Hierarki

Enhetsspesifikke kommandoer har hierarkisk struktur. De ulike nivåene representeres av sammensatte headere. Headere på det høyeste nivået (rotnivået) har bare ett nøkkelord, som betegner et helt kommandosystem.

Eksempel:

```
:SYSTem
```

Dette nøkkelordet betegner kommandosystemet `:SYSTem`. For kommandoer på lavere nivåer må hele stien angis, fra venstre og nedover med høyeste nivå først, og de enkelte nøkkelordene skilles med kolon `:`.

Eksempel:

```
INPut:COUPling AC
```

Denne kommandoen ligger på andre nivå i `INPut`-subsystemet og velger AC-kobling for inngangskanalen.

Trestrukturen i `INPut`-systemet (manualens figur 1-1) ser slik ut:

```
INPut
├── COUPling
├── GAIN
├── SHUNt
└── FILTer
    ├── STATe
    └── LPASs
        ├── STATe
        └── FREQuency
```

##### Valgfritt nøkkelord

Noen kommandosystemer tillater at visse nøkkelord settes inn i headeren eller utelates. Disse nøkkelordene er markert med hakeparenteser i beskrivelsen. Instrumentet må gjenkjenne full kommandolengde for kompatibilitet med SCPI-standarden. Noen kommandoer kan forkortes betraktelig ved å utelate valgfrie nøkkelord.

Eksempel:

```
INPut:FILTer[:STATe] ON
```

Denne kommandoen aktiverer antialiasingfilteret som settes inn i signalveien før signalet behandles av `SENSe`-subsystemet. Følgende kommando har samme effekt:

```
INPut:FILTer ON
```

> **Merk:** Et valgfritt nøkkelord må ikke utelates hvis virkningen spesifiseres nærmere med et numerisk suffiks.

##### Lang og kort form

Eksempel:

```
STATus:QUEStionable:ENABle 1
STAT:QUES:ENAB 1
```

> **Merk:** Kortformen kjennetegnes av store bokstaver, langformen tilsvarer det komplette ordet. Store og små bokstaver tjener kun dette formålet i dokumentasjonen — instrumentet selv skiller ikke mellom store og små bokstaver.

##### Parametre

En parameter må skilles fra headeren med et "white space". Hvis en kommando har flere parametre, skilles de med komma `,`.

Eksempel:

```
FORMat:READings:DATA REAL,32
```

Denne kommandoen velger binært 32-bits flyttallsformat for dataoverføringer.

##### Numerisk suffiks

Hvis en enhet har flere funksjoner eller egenskaper av samme type, for eksempel innganger, kan ønsket funksjon velges ved å legge et suffiks til kommandoen. Angivelser uten suffiks tolkes som suffiks 1, med mindre annet er eksplisitt angitt.

Eksempel:

```
INPut:COUPling DC
```

Denne kommandoen setter inngangskoblingen på kanal 1 til DC.

Målefunksjoner (parametre til kommandoen `SENSe:FUNCtion`) bruker numerisk suffiks for å velge fase. Hvis ikke noe suffiks angis, konfigureres totalverdien.

#### Struktur på kommandolinjer

En kommandolinje kan inneholde én eller flere kommandoer.

**Terminering av meldinger — en kommandolinje termineres med ett av følgende:**

- `<New Line>` (line feed, `<LF>` = 0Ah)
- `<New Line>` sammen med EOI
- EOI sammen med siste databyte (EOI gjelder kun GPIB-grensesnittet)

VISA produserer automatisk EOI sammen med siste databyte.

Flere kommandoer i én kommandolinje skilles med semikolon `;`. Hvis neste kommando tilhører et annet kommandosystem, følges semikolonet av et kolon.

Eksempel:

```
INPut1:COUPling DC;:SENSe:CURRent1:DC:RANGe 1.0
```

Denne kommandolinjen inneholder to kommandoer. Den første tilhører `INPut`-subsystemet og setter inngangskoblingen for kanal 1. Den andre tilhører `SENSe`-subsystemet og setter strømområdet på fase 1 til 1.0 A. (`DC` kunne vært utelatt siden det er et valgfritt nøkkelord. For dette instrumentet er det ingen forskjell mellom AC- og DC-område — begge setter samme område.)

Hvis påfølgende kommandoer tilhører samme system og har ett eller flere nivåer felles, kan kommandolinjen forkortes. Da startes den andre kommandoen (etter semikolonet) med nivået som ligger under de felles nivåene. Kolonet etter semikolonet må da utelates.

Eksempel (full lengde — to kommandoer i `INPut`-subsystemet med ett felles nivå):

```
INPut1:SHUNt EXTernal;:INPut1:GAIN 25.0
```

Forkortet form av kommandolinjen:

```
INPut1:SHUNt EXTernal;GAIN 25.0
```

En ny kommandolinje må imidlertid alltid startes med den komplette stien:

```
INPut1:SHUNt EXTernal
INPut1:GAIN 25.0
```

#### Responser på spørringer

For hver innstillingskommando er det definert en spørring, med mindre annet er eksplisitt angitt. Spørringen dannes ved å legge et spørsmålstegn til den aktuelle innstillingskommandoen. Responser på spørringer etter SCPI-standarden er delvis underlagt strengere regler enn responser etter IEEE 488.2:

1. **Den etterspurte parameteren overføres uten header.**

   ```
   INPut:COUPling?
   Respons: AC
   ```

2. **Numeriske verdier returneres uten enhet.** Fysiske størrelser refererer til grunnenhetene eller til enhetene satt med Unit-kommandoen.

   ```
   INPut:FILTer:LPASs:FREQuency?
   Respons: 3.0E5 for 300 kHz
   ```

3. **Sannhetsverdier (boolske parametre) returneres som 0 (Off) og 1 (On).**

   ```
   INPut:FILTer:STATe?
   Respons: 1
   ```

4. **Tekst (character data) returneres i kortform.**

   ```
   INPut:SHUNt?
   Respons: EXT
   ```

5. **Ved flere spørringer i samme kommandolinje returneres responsene i samme rekkefølge som spørringene, skilt med semikolon.**

   ```
   INPut:FILTer:STATe?;:INPut:FILTer:LPASs:FREQuency?
   Respons: 1;1.0E+04
   ```

#### Parametertyper

De fleste kommandoer krever at en parameter angis. Parametre må skilles fra headeren med et "white space". Tillatte parametre er numeriske verdier, boolske parametre, tekst, tegnstrenger og blokkdata. Parametertypen og tillatt verdiområde for en gitt kommando er angitt i kommandobeskrivelsen.

##### Numeriske verdier

Numeriske verdier kan angis i enhver form: fortegn, desimalpunktum og eksponent. Verdier som overskrider instrumentets oppløsning, rundes opp eller ned. Mantissen kan bestå av inntil 15 tegn, og eksponenten må ligge i verdiområdet -307 til 307. Eksponenten innledes med `E` eller `e`. Å angi eksponenten alene er ikke tillatt. For fysiske størrelser med enhet aksepteres ingen enhet — grunnenheten brukes.

```
SENSe:VOLTage1:RANGe 1000.0    setter område 1000 V
```

##### Boolske parametre

Boolske parametre representerer to tilstander. ON-tilstanden (logisk sann) representeres av `ON` eller en numerisk verdi ulik 0. OFF-tilstanden (logisk usann) representeres av `OFF` eller den numeriske verdien 0. Ved spørring returneres 0 eller 1.

```
Innstillingskommando: SYNC:STATe ON
Spørring:             SYNC:STATe?
Respons:              1
```

##### Tekst

Tekstparametre følger de syntaktiske reglene for nøkkelord. De kan angis i kort eller lang form. Som alle andre parametre må de skilles fra headeren med et "white space". Ved spørring returneres kortformen av teksten.

```
Innstillingskommando: INPut1:SHUNt EXTernal
Spørring:             INPut1:SHUNt?
Respons:              EXT
```

##### Strenger

Strenger må alltid angis i anførselstegn (`'` eller `"`).

```
ROUTe:SYSTem "3W"
ROUTe:SYSTem '3W'
```

##### Blokkdata

Blokkdata er et overføringsformat som egner seg for overføring av store datamengder fra instrumentet til kontrolleren. Blokkdata har følgende struktur:

```
#40008xxxxxxxx
```

Datablokken innledes med ASCII-tegnet `#`. Neste tall angir hvor mange av de påfølgende sifrene som beskriver lengden på datablokken. I eksempelet angir de fire påfølgende sifrene at lengden er 8 byte (foranstilte nuller ignoreres). Deretter følger databytene. Under overføringen av databytene ignoreres alle End- og andre kontrolltegn til alle byte er overført. Dataelementer som består av mer enn én byte, overføres med den byten først som er spesifisert av SCPI-kommandoen `FORMat:BORDer`. Den interne strukturen på dataene i blokken avhenger av den faktiske instrumentkonfigurasjonen.

#### Oversikt over syntakselementer

| Element | Betydning |
|---------|-----------|
| `:` | Kolon skiller nøkkelordene i en kommando. I en kommandolinje markerer det etter skille-semikolonet det øverste kommandonivået. |
| `;` | Semikolon skiller to kommandoer i en kommandolinje. Det endrer ikke stien. |
| `,` | Komma skiller flere parametre i en kommando. |
| `?` | Spørsmålstegn danner en spørring. |
| `*` | Asterisk markerer en fellesskommando (common command). |
| `"` | Anførselstegn innleder og avslutter en streng. |
| `#` | ASCII-tegnet `#` innleder blokkdata. |
| white space | Et "white space" (ASCII-kode 0 til 9, 11 til 32 desimalt, blank) skiller header og parameter. |

### Instrumentmodell og kommandoprosessering

Behandlingen av grensesnittkommandoene skjer i flere komponenter som arbeider uavhengig av hverandre og samtidig, og som kommuniserer med hverandre via meldinger (manualens figur 1-2):

```
Grensesnitt ──> Input unit (med input buffer) ──> Command recognition ──> Data set ──> Instrument hardware
                                                          │                  │
                                                          v                  v
Grensesnitt <── Output unit (med output buffer) <── Status reporting system
```

#### Input unit (inngangsenhet)

Inngangsenheten mottar kommandoer, tegn for tegn, fra grensesnittet og lagrer dem i inngangsbufferet. **Inngangsbufferet har en størrelse på 2048 tegn.** Inngangsenheten sender en melding til kommandogjenkjenningen når inngangsbufferet er fullt, eller når den mottar en terminator, `<PROGRAM MESSAGE TERMINATOR>` som definert i IEEE 488.2, eller grensesnittmeldingen DCL (kun GPIB).

Hvis inngangsbufferet er fullt, stoppes grensesnittrafikken og dataene som er mottatt til da behandles; deretter fortsetter trafikken. Hvis bufferet ikke er fullt ved mottak av en terminator, kan inngangsenheten motta neste kommando mens kommandogjenkjenning og -utførelse pågår. Mottak av DCL (kun GPIB) tømmer inngangsbufferet og sender umiddelbart en melding til kommandogjenkjenningen.

#### Command recognition (kommandogjenkjenning)

Kommandogjenkjenningen analyserer dataene fra inngangsenheten i mottaksrekkefølge. Bare DCL-kommandoer (kun GPIB) behandles med prioritet; GET-kommandoer (Group Execute Trigger, kun GPIB) behandles først etter tidligere mottatte kommandoer. Hver gjenkjent kommando overføres umiddelbart til datasettet, men uten å bli utført der med én gang.

Syntaktiske feil i kommandoer oppdages her og overføres til statusrapporteringssystemet. Resten av en kommandolinje etter en syntaksfeil analyseres og behandles videre så langt det er mulig.

Når kommandogjenkjenningen gjenkjenner en terminator eller en DCL-kommando (kun GPIB), ber den datasettet om å sette kommandoene også i instrumentmaskinvaren. Deretter er den umiddelbart klar til å fortsette kommandobehandlingen. Det betyr at nye kommandoer kan behandles mens maskinvaren settes ("overlappende utførelse"). **Merk: For dette instrumentet utføres for tiden alle kommandoer ikke-overlappende (= sekvensielt).**

#### Data set og instrument hardware (datasett og instrumentmaskinvare)

Begrepet "instrumentmaskinvare" betegner den delen av instrumentet som faktisk utfører instrumentfunksjonene: signalgenerering, måling osv. Kontrolleren er ikke inkludert.

Datasettet er en detaljert gjengivelse av instrumentmaskinvaren i programvaren. Innstillingskommandoer fra grensesnittet fører til endring av datasettet. Datasettforvaltningen legger de nye verdiene (for eksempel frekvens) inn i datasettet, men gir dem videre til maskinvaren først på forespørsel fra kommandogjenkjenningen. Siden dette bare skjer ved slutten av en kommandolinje, er rekkefølgen på innstillingskommandoene i kommandolinjen ikke relevant.

Dataene sjekkes for kompatibilitet med hverandre og med instrumentmaskinvaren først umiddelbart før de overføres til maskinvaren. Hvis det viser seg at utførelse ikke er mulig, signaliseres en "execution error" til statusrapporteringssystemet. Alle endringer i datasettet forkastes, og instrumentmaskinvaren tilbakestilles ikke. På grunn av den forsinkede kontrollen og maskinvareinnstillingen er det tillatt at ugyldige instrumenttilstander kortvarig settes innenfor en kommandolinje uten at det gis feilmelding — men ved slutten av kommandolinjen må en gyldig instrumenttilstand være oppnådd.

Før dataene gis videre til maskinvaren, settes settling-biten i `STATus:OPERation`-registeret. Maskinvaren gjør innstillingene og nullstiller biten når den nye tilstanden har stabilisert seg. Denne mekanismen kan brukes til synkronisering av kommandobehandlingen.

#### Status reporting system (statusrapporteringssystem)

Statusrapporteringssystemet samler informasjon om instrumenttilstanden og gjør den tilgjengelig for utgangsenheten på forespørsel. En detaljert beskrivelse av struktur og funksjon gis i manualens kapittel 2.

#### Output unit (utgangsenhet)

Utgangsenheten samler informasjonen som kontrolleren har bedt om og som datasettforvaltningen leverer. Den behandler informasjonen etter SCPI-reglene og gjør den tilgjengelig i utgangsbufferet. **Utgangsbufferet har en størrelse på 2048 tegn.** Hvis den etterspurte informasjonen overskrider denne størrelsen, gjøres den tilgjengelig i porsjoner uten at kontrolleren merker det.

Hvis instrumentet adresseres som talker uten at utgangsbufferet inneholder data eller venter på data fra datasettforvaltningen, returnerer utgangsenheten feilmeldingen "Query UNTERMINATED" til statusrapporteringssystemet. Ingen data sendes på grensesnittet, og kontrolleren venter til dens tidsgrense (timeout) er nådd. Denne fremgangsmåten er spesifisert av SCPI.

Grensesnittspørringer gjør at datasettforvaltningen sender de ønskede dataene til utgangsenheten.

### Kommandosekvens og kommandosynkronisering

Som nevnt ovenfor er overlappende utførelse mulig for alle kommandoer. Likeledes behandles innstillingskommandoene i en kommandolinje ikke nødvendigvis i den rekkefølgen de er mottatt. For å sikre at kommandoer utføres i en bestemt rekkefølge, må hver kommando sendes i en egen kommandolinje med et eget viWrite-kall (viPrintf, viQueryf).

For å hindre overlappende utførelse av kommandoer må én av kommandoene `*OPC`, `*OPC?` eller `*WAI` brukes. Hver av de tre kommandoene utløser en bestemt handling først etter at maskinvaren er satt og har stabilisert seg. Kontrolleren kan programmeres til å vente på den respektive handlingen (se tabell 1-1).

**Tabell 1-1. Synkronisering med *OPC, *OPC? og *WAI**

| Kommando | Handling etter at maskinvaren har stabilisert seg | Programmering av kontrolleren |
|----------|---------------------------------------------------|-------------------------------|
| `*OPC` | Setter operation-complete-biten i ESR | - Sette bit 0 i ESE<br>- Sette bit 5 i SRE<br>- Vente på service request (SRQ) |
| `*OPC?` | Skriver en "1" i utgangsbufferet | Adressere instrumentet som talker |
| `*WAI` | Fortsetter IEC/IEEE-buss-handshaken. Handshaken stoppes ikke. | Sende neste kommando |

Et eksempel på kommandosynkronisering finnes i manualens kapittel 6.

> **Merk:** Kommandosynkroniseringskommandoene fungerer, men er for tiden ikke nødvendige, siden instrumentet utfører alle kommandoer sekvensielt.

## Felleskommandoer og målefunksjoner

Dette kapittelet beskriver alle kommandoer som er implementert i instrumentet. Kommandoene listes først i tabeller og beskrives deretter i detalj, ordnet etter kommandosubsystemene. Notasjonen følger SCPI-standarden, og SCPI-konformitetsinformasjon er inkludert i den enkelte kommandobeskrivelsen.

**Alle kommandoer kan brukes for styring via alle grensesnitt** (inkludert TCP/socket-grensesnittet).

### Felleskommandoer (Common Commands)

Felleskommandoene er hentet fra standarden IEEE 488.2 (IEC 625-2). En gitt kommando har samme effekt på ulike enheter. Headeren til disse kommandoene består av en asterisk `*` etterfulgt av tre bokstaver. Mange felleskommandoer refererer til statusrapporteringssystemet som er beskrevet i kapittel 2.

#### Oversikt

| Kommando | Parameter | Funksjon | Kommentar |
|---|---|---|---|
| `*CLS` | – | Clear Status | ingen spørring |
| `*ESE` | 0 til 255 | Event Status Enable | |
| `*ESR?` | – | Standard Event Status Query | kun spørring |
| `*IDN?` | – | Identification Query | kun spørring |
| `*OPC` | – | Operation Complete | |
| `*OPC?` | – | Operation Complete Query | |
| `*OPT?` | – | Option Identification Query | kun spørring |
| `*RST` | – | Reset | ingen spørring |
| `*SRE` | 0 til 255 | Service Request Enable | |
| `*STB?` | – | Status Byte Query | kun spørring |
| `*WAI` | – | Wait to continue | ingen spørring |
| `*SAV` | 10 til 24 | Save User Setup | ingen spørring |
| `*RCL` | 1 til 9 / 10 til 24 | Recall Standard Setup / Recall User Setup | ingen spørring |
| `*LRN?` | – | Learn Setup String | kun spørring |
| `*TRG` | – | Trigger | ingen spørring |

#### Detaljert beskrivelse

##### `*CLS`

CLEAR STATUS setter statusbyten (STB), standard hendelsesregister (ESR) og EVENt-delen av QUEStionable- og OPERation-registrene til null. Kommandoen endrer ikke maske- og transisjonsdelene av registrene. Den tømmer utgangsbufferen.

##### `*ESE 0 to 255`

EVENT STATUS ENABLE setter event status enable-registeret til den angitte verdien. Spørreformen `*ESE?` returnerer innholdet i event status enable-registeret i desimalform.

##### `*ESR?`

STANDARD EVENT STATUS QUERY returnerer innholdet i hendelsesstatusregisteret i desimalform (0 til 255) og setter deretter registeret til null.

##### `*IDN?`

IDENTIFICATION QUERY spør etter instrumentidentifikasjonen. Responsen er for eksempel:

```
"Fluke,NORMA4000,KN34512BA,01.00"
```

- `KN34512BA` = instrumentets serienummer
- `01.00` = fastvareversjonsnummer

##### `*OPC`

OPERATION COMPLETE setter bit 0 i hendelsesstatusregisteret når alle foregående kommandoer er utført. Denne biten kan brukes til å utløse en service request.

##### `*OPC?`

OPERATION COMPLETE QUERY skriver meldingen `"1"` til utgangsbufferen så snart alle foregående kommandoer er utført.

##### `*OPT?`

OPTION IDENTIFICATION QUERY spør etter opsjonene som er inkludert i instrumentet og returnerer en liste over installerte opsjoner. Opsjonene skilles fra hverandre med komma. Kommandoen ber om identifikasjon av enhetens opsjoner. Eksempel på respons fra enheten:

```
"Option1,Option2"
```

##### `*RST`

RESET setter instrumentet til en definert standardtilstand. Standardinnstillingen er angitt i beskrivelsen av de enkelte kommandoene.

##### `*SRE 0 to 255`

SERVICE REQUEST ENABLE setter service request enable-registeret til den angitte verdien. Bit 6 (MSS-maskebit) forblir 0. Denne kommandoen bestemmer under hvilke betingelser en service request genereres. Spørreformen `*SRE?` leser innholdet i service request enable-registeret i desimalform. Bit 6 er alltid 0.

##### `*STB?`

READ STATUS BYTE QUERY leser ut innholdet i statusbyten i desimalform.

##### `*TRG`

TRIGGER starter målingen umiddelbart hvis instrumentet er i single-shot-modus (`INITiate:CONTinuous OFF`). Denne kommandoen tilsvarer `INITiate:IMMediate` (se avsnittet «TRIGger subsystem»). Hvis minneopptak (memory recording) er konfigurert, forbikobles ARM- og TRIGger-lagene, og instrumentet begynner umiddelbart å lagre data. Synkroniseringsbetingelsen må være oppfylt hvis synkronisering er ON.

##### `*WAI`

WAIT-to-CONTINUE tillater behandling av påfølgende kommandoer først etter at alle foregående kommandoer er utført og alle signaler har stabilisert seg.

##### `*SAV 10 to 24`

SAVE SETUP lagrer instrumentoppsettet i det angitte brukerkonfigurasjonsminnet.

##### `*RCL 1 to 24`

RECALL SETUP henter instrumentoppsett fra det angitte konfigurasjonsminnet (1 til 9 = standardoppsett, 10 til 24 = brukeroppsett).

##### `*LRN?`

LEARN SETUP STRING spør etter komplett instrumentoppsett. Oppsettet returneres som en sekvens av semikolonseparerte kommandoer. Hvis denne sekvensen sendes tilbake til instrumentet, gjenopprettes instrumentkonfigurasjonen fullstendig.

### Målefunksjoner (Measurement Functions)

`<function>` er en hierarkisk målefunksjon som angir hvilken type midlet elektrisk størrelse instrumentet skal konfigureres til å måle. Én verdi av `<function>` beregnes over én midlingssyklus i instrumentet. Ved bruk av `[SENSe:]FUNCtion`-subsystemet kan flere funksjoner måles/beregnes samtidig. `<function>` har følgende syntaks:

```
<function> ::= "<function_name>"
```

Brukt med `[SENSe:]FUNCtion`-subsystemet er `<function_name>` STRING PROGRAM DATA, dvs. funksjonsnavnene omsluttes av doble anførselstegn. Hvis flere funksjoner angis, må hvert funksjonsnavn stå i egne anførselstegn.

Eksempel:

```
FUNC "VOLT1:DC"                          Mål True RMS på fase 1
FUNC "VOLT1:AC", "VOLT2:AC", "VOLT3:AC"  Mål RMS på fase 1, 2 og 3
```

Når en `<function>` returneres som svar på en spørring, inneholder den ingen mellomrom. Mnemonikkene i spørreresponsen bruker kortform med standardnoder utelatt. Alle bokstaver i responsen er store (uppercase).

#### Fasesuffikser

Den første noden i funksjonsnavnet har et numerisk heltallssuffiks som brukes til å skille mellom fasene i et flerfasesystem. Gyldige suffikser:

| Suffiks | Fase |
|---|---|
| 1 | L1 |
| 2 | L2 |
| 3 | L3 |
| 4 | L4 på NORMA 5000-modellen |
| 5 | L5 på NORMA 5000-modellen |
| 6 | L6 på NORMA 5000-modellen |
| 12 | fase-til-fase-spenning L1 – L2 |
| 13 | fase-til-fase-spenning L1 – L3 (kun W2-system) |
| 23 | fase-til-fase-spenning L2 – L3 |
| 31 | fase-til-fase-spenning L3 – L1 (kun W3-system) |
| 45 | fase-til-fase-spenning L4 – L5 (N5000-modellen) |
| 56 | fase-til-fase-spenning L5 – L6 (N5000-modellen) |
| 64 | fase-til-fase-spenning L6 – L4 (N5000-modellen) |
| (uten suffiks) | Gjennomsnitts-/total-/samleverdi fra kanalene i 1. trefasesystem (fase 1 … fase 3), eller 1. tofasesystem (fase 1 … fase 2) når to-wattmeter-konfigurasjon (2W/Aron) er aktiv |
| 460 | Gjennomsnitts-/total-/samleverdi fra kanalene i 2. trefasesystem (fase 4 … fase 6) |
| 123 | Gjennomsnittlig fase-til-fase-spenning i 1. trefasesystem (fase 1 … fase 3), eller 1. tofasesystem (fase 1 … fase 2) når to-wattmeter-konfigurasjon (2W/Aron) er aktiv |
| 456 | Gjennomsnittlig fase-til-fase-spenning i 2. trefasesystem (fase 4 … fase 6) |

#### MINimum/MAXimum og IPOSitive/INEGative/INTegral

Den valgfrie **MINimum/MAXimum**-delen av funksjonsnavnet angir at ekstremverdien til funksjonen skal returneres. Etter hver midlingssyklus sammenlignes den nye middelverdien mot MIN/MAX-registrene, slik at ekstremverdiene akkumuleres over mange midlingssykluser inntil de nullstilles med en egen kommando.

MINimum/MAXimum-egenskapen er ikke det samme som PHIGH/PLOW: PHIGH/PLOW returnerer høyeste/laveste samplede verdi funnet innenfor det gjeldende midlingsintervallet.

MINimum/MAXimum-egenskapen må aktiveres i CALCulate-subsystemet før den kan brukes som del av en `<function>`.

> Merk: MINimum/MAXimum-opsjonene er per manualen foreløpig ikke implementert («currently unimplemented»).

Den valgfrie **IPOSitive/INEGative/INTegral**-delen av funksjonsnavnet angir at summert verdi (integral) av funksjonen skal returneres:

- `IPOSitive` – kun de positive verdiene av funksjonen summeres
- `INEGative` – kun de negative verdiene av funksjonen summeres
- `INTegral` – summen av begge

IPOSitive/INEGative/INTegral-egenskapen må aktiveres i CALCulate-subsystemet før den kan brukes som del av en `<function>`. De integrerte verdiene kan nullstilles med kommandoen `CALCulate:INTegral:CLEar[:IMMediate]`.

#### Grunninstrumentets målefunksjoner (Base Instrument Measurement Functions)

> Merk: Hvis W2-system er valgt, erstattes `VOLTage31` av `VOLTage13`.

| Funksjon (størrelse) | Kommando | Uten suffiks (1. system) eller med 460 (2. system) |
|---|---|---|
| True RMS Voltage | `VOLTage[1..6\|460][:DC][:MINimum\|MAXimum]` | Average Voltage trms |
| RMS uten DC-komponent | `VOLTage[1..6\|460]:AC[:MINimum\|MAXimum]` | Average Voltage rms |
| True RMS fase-til-fase-spenning | `VOLTage[12\|23\|31\|45\|56\|64][:DC][:MINimum\|MAXimum]` | |
| Rectified Mean fase-til-fase-spenning | `VOLTage[12\|23\|31\|45\|56\|64]:RMEAN[:MINimum\|MAXimum]` | |
| Rectified Mean fase-til-fase-spenning, korrigert | `VOLTage[12\|23\|31\|45\|56\|64]:RMCORR[:MINimum\|MAXimum]` | |
| Fase-til-fase spenningsharmonisk | `VOLTage[12\|23\|31\|45\|56\|64]:HAR[:MINimum\|MAXimum]` | |
| Fase-til-fase spenning, formfaktor | `VOLTage[12\|23\|31\|45\|56\|64]:FFACtor[:MINimum\|MAXimum]` | |
| Fase-til-fase spennings-THD | `VOLTage[12\|23\|31\|45\|56\|64]:THD[:MINimum\|MAXimum]` | |
| Fase-til-fase spenning, harmonisk innhold | `VOLTage[12\|23\|31\|45\|56\|64]:HCONTent[:MINimum\|MAXimum]` | |
| Fase-til-fase spenning, grunnharmonisk innhold | `VOLTage[12\|23\|31\|45\|56\|64]:FCONTent[:MINimum\|MAXimum]` | |
| Fase-til-fase absolutt spenningsfase | `VOLTage[12\|23\|31\|45\|56\|64]:PHASe[:MINimum\|:MAXimum]` | |
| Gjennomsnittlig true RMS fase-til-fase-spenning | `VOLTage123\|456[:DC][:MINimum\|MAXimum]` | |
| Gjennomsnittlig Mean fase-til-fase-spenning | `VOLTage123\|456:MEAN[:MINimum\|MAXimum]` | |
| Gjennomsnittlig Rectified Mean fase-til-fase-spenning | `VOLTage123\|456:RMEAN[:MINimum\|MAXimum]` | |
| Gjennomsnittlig Rectified Mean fase-til-fase-spenning, korrigert | `VOLTage123\|456:RMCORR[:MINimum\|MAXimum]` | |
| Gjennomsnittlig fase-til-fase spenningsharmonisk | `VOLTage123\|456:HAR[:MINimum\|MAXimum]` | |
| Middelverdi av spenning | `VOLTage[1..6\|460]:MEAN[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Average Mean Voltage |
| Rectified Mean Voltage | `VOLTage[1..6\|460]:RMEAN[:MINimum\|MAXimum]` | Average Rectified Mean Voltage |
| Rectified Mean Voltage, korrigert | `VOLTage[1..6\|460]:RMCORR[:MINimum\|MAXimum]` | Average Rectified Mean Voltage Corrected |
| Peak-to-peak-spenning | `VOLTage[1..6]:PTP[:MINimum\|MAXimum]` | |
| Høyeste verdi innen midlingsintervallet | `VOLTage[1..6]:PHIGH[:MINimum\|MAXimum]` | |
| Laveste verdi innen midlingsintervallet | `VOLTage[1..6]:PLOW[:MINimum\|MAXimum]` | |
| Spenningsharmonisk | `VOLTage[1..6\|460]:HAR[:MINimum\|MAXimum]` (orden velges med `CALCulate:HARMonic:ORDer`) | Average Voltage Harm |
| Spenning, crestfaktor | `VOLTage[1..6]:CFACtor[:MINimum\|MAXimum]` | |
| Spenning, absolutt fase | `VOLTage[1..6]:PHASe[:MINimum\|MAXimum]` (relativt til synkroniseringssignalet) | |
| Spenning, formfaktor | `VOLTage[1..6]:FFACtor[:MINimum\|MAXimum]` | |
| Spenning, harmonisk innhold | `VOLTage[1..6]:HCONTent[:MINimum\|MAXimum]` | |
| Spenning, grunnharmonisk innhold | `VOLTage[1..6]:FCONTent[:MINimum\|MAXimum]` | |
| Spennings-THD | `VOLTage[1..6]:THD[:MINimum\|MAXimum]` | |
| True RMS Current | `CURRent[1..6\|460][:DC][:MINimum\|MAXimum]` | Average Current trms |
| RMS uten DC-komponent | `CURRent[1..6\|460]:AC[:MINimum\|MAXimum]` | Average Current rms |
| Middelverdi av strøm | `CURRent[1..6\|460]:MEAN[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Average Mean Current |
| Rectified Mean Current | `CURRent[1..6\|460]:RMEAN[:MINimum\|MAXimum]` | Average Rectified Mean Current |
| Rectified Mean Current, korrigert | `CURRent[1..6\|460]:RMCORR[:MINimum\|MAXimum]` | Average Rectified Mean Current Corrected |
| Peak-to-peak-strøm | `CURRent[1..6]:PTP[:MINimum\|MAXimum]` | |
| Høyeste verdi innen midlingsintervallet | `CURRent[1..6]:PHIGH[:MINimum\|MAXimum]` | |
| Laveste verdi innen midlingsintervallet | `CURRent[1..6]:PLOW[:MINimum\|MAXimum]` | |
| Strømharmonisk | `CURRent[1..6\|460]:HAR[:MINimum\|MAXimum]` | Average Current Harm |
| Strøm, crestfaktor | `CURRent[1..6]:CFACtor[:MINimum\|MAXimum]` | |
| Strøm, absolutt fase | `CURRent[1..6]:PHASe[:MINimum\|MAXimum]` (relativt til synkroniseringssignalet) | |
| Strøm, formfaktor | `CURRent[1..6]:FFACtor[:MINimum\|MAXimum]` | |
| Strøm, harmonisk innhold | `CURRent[1..6]:HCONTent[:MINimum\|MAXimum]` | |
| Strøm, grunnharmonisk innhold | `CURRent[1..6]:FCONTent[:MINimum\|MAXimum]` | |
| Strøm-THD | `CURRent[1..6]:THD[:MINimum\|MAXimum]` | |
| Aktiv effekt | `POWer[1..6\|460][:ACTive][:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Active Power |
| Tilsynelatende effekt | `POWer[1..6\|460]:APParent[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Apparent Power |
| Reaktiv effekt | `POWer[1..6\|460]:REACtive[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Reactive Power |
| Effektfaktor | `POWer[1..6\|460]:FACTor[:MINimum\|MAXimum]` | Total Power Factor |
| Korrigert effekt | `POWer[1..6\|460]:CORRected[:MINimum\|MAXimum]` | Total Corrected Power |
| Elektrisk virkningsgrad | `POWer[460]:EFFiciency[:MINimum\|MAXimum]` | Total Electrical Efficiency |
| Fasevinkel mellom U og I (arccos[PF]) | `PHASe[1..6\|460][:MINimum\|MAXimum]` | Total Phase Angle (arccos[PF]) |
| Tilsynelatende impedans | `IMPedance[1..6\|460][:APParent][:MINimum\|MAXimum]` | Total App. Impedance |
| Seriell resistans | `RESistance[1..6\|460]:SERial[:MINimum\|MAXimum]` | Total Serial Resistance |
| Parallell resistans | `RESistance[1..6\|460]:PARallel[:MINimum\|MAXimum]` | Total Parallel Resistance |
| Seriell reaktans | `REACTance[1..6\|460]:SERial[:MINimum\|MAXimum]` | Total Serial Reactance |
| Parallell reaktans | `REACTance[1..6\|460]:PARallel[:MINimum\|MAXimum]` | Total Parallel Reactance |
| Aktiv effekt, harmonisk | `POWer[1..6\|460][:ACTive]:HAR[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Active Power Harm. |
| Tilsynelatende effekt, harmonisk | `POWer[1..6\|460]:APParent:HAR[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Apparent Power Harm. |
| Reaktiv effekt, harmonisk | `POWer[1..6\|460]:REACtive:HAR[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Reactive Power Harm. |
| Effektfaktor, harmonisk | `POWer[1..6\|460]:FACTor:HAR[:MINimum\|MAXimum]` | Total Power Factor Harm. |
| Elektrisk virkningsgrad, harmonisk | `POWer[460]:EFFiciency:HAR[:MINimum\|MAXimum]` | Total Electrical Efficiency Harm. |
| Fasevinkel U til I, harmonisk (arccos[PF]) | `PHASe[1..6\|460]:HAR[:MINimum\|MAXimum]` | Total Phase Angle Har. (arccos[PF]) |
| Tilsynelatende impedans, harmonisk | `IMPedance[1..6\|460][:APParent]:HAR[:MINimum\|MAXimum]` | Total App. Impedance Harmonic |
| Seriell resistans, harmonisk | `RESistance[1..6\|460]:SERial:HAR[:MINimum\|MAXimum]` | Total Serial Resistance Harmonic |
| Parallell resistans, harmonisk | `RESistance[1..6\|460]:PARallel:HAR[:MINimum\|MAXimum]` | Total Par. Resistance Harmonic |
| Seriell reaktans, harmonisk | `REACTance[1..6\|460]:SERial:HAR[:MINimum\|MAXimum]` | Total Serial Reactance Harmonic |
| Parallell reaktans, harmonisk | `REACTance[1..6\|460]:PARallel:HAR[:MINimum\|MAXimum]` | Total Par. Reactance Harmonic |
| SYNC-frekvens | `FREQuency[:MINimum\|MAXimum]` | |
| Midlingsintervallets lengde i sekunder | `TIME[:INTerval][:MINimum\|MAXimum]` | |
| Tid [sek] siden timer-nullstilling | `TIME:RELative` | |

#### Tilleggsfunksjoner tilgjengelig med prosessgrensesnitt PI1 installert

| Funksjon (størrelse) | Kommando | Uten suffiks |
|---|---|---|
| Akselmoment (Shaft torque) | `TORQue[1..4][:MINimum\|MAXimum]` | `TORQue1` |
| Rotasjonshastighet | `SPEed[1..4][:MINimum\|MAXimum]` | `SPEed1` |
| Mekanisk effekt | `POWer[1..4]:MECHanical[:MINimum\|MAXimum]` | `POWer1:MECHanical` |
| Slip (sakking) | `SLIP[1..4][:MINimum\|MAXimum]` | `SLIP1` |
| Mekanisk virkningsgrad | `EFFiciency[1..4][:MINimum\|MAXimum]` | `EFFiciency1` |
| Rå inngangsverdi | `GPINput[1..8][:MINimum\|MAXimum]` | `GPINput1` |

## Subsystemer: ABORt til ROUTe

Dette kapittelet dokumenterer subsystemene ABORt, CALCulate, DISPlay, FORMat, HARDcopy (HCOPy), INITiate, INPut, OUTPut og ROUTe. SCPI-kortformen vises med store bokstaver i kommandonavnet (f.eks. `CALCulate` = kortform `CALC`).

### ABORt-subsystemet

ABORt-subsystemet inneholder kommandoene for å avbryte handlinger som er trigget. Etter at en handling er avbrutt, kan den umiddelbart trigges på nytt. Alle kommandoene utløser en hendelse (event) og har ingen *RST-verdi.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:ABORt` | – | – | Ingen query |

#### `ABORt`

Nullstiller trigger-/synkroniseringssystemet. Når målingen eller datalagringen er startet etter at trigger-/synkbetingelsen er oppfylt, har denne kommandoen ingen effekt. Kommandoen tar instrumentet fra tilstanden «venter på trigger/synk» tilbake til tilstanden før `INITiate[:IMMediate]:SEQuence`-kommandoen ble sendt.

- **Parametre:** ingen
- **Respons:** ingen query
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
ABOR
```

### CALCulate-subsystemet

CALCulate-subsystemet inneholder kommandoer for spektrumberegning, brukerdefinert beregning av elektrisk virkningsgrad og integrasjon av de midlede verdiene.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:CALCulate:TRANsform:FREQuency[:STATe]` | `ONCE` | – | Ingen query |
| `:CALCulate:TRANsform:FREQuency:MODE` | `FFT \| DFT \| STD` | `FFT` | |
| `:CALCulate:TRANsform:FREQuency:FUNCtion` | `<function list>` | – | |
| `:CALCulate:TRANsform:FREQuency:STARt` | `0` | – | Kun for FFT og DFT |
| `:CALCulate:TRANsform:FREQuency:STOP` | `<value>` | – | Kun for FFT og DFT |
| `:CALCulate:TRANsform:FREQuency:CYCLes` | `4 \| 6 \| 8 \| 10 \| 12` | `10` | Kun for STD |
| `:CALCulate:TRANsform:FREQuency:GROuping` | `COMPonent \| HARMonic \| HGRoup \| HSGRoup \| ISGRoup \| SGRoup` | – | Kun for STD |
| `:CALCulate:DATA?` | `[<count>,[<offset>]]` | – | Kun query |
| `:CALCulate:DATA:PREamble?` | – | – | Kun query |
| `:CALCulate:DATA:THD?` | – | – | Kun query |
| `:CALCulate:INTegral[:STATe]` | `ON \| OFF` | `OFF` | |
| `:CALCulate:INTegral:FUNCtion` | `<function list>` | – | |
| `:CALCulate:INTegral:CLEar[:IMMediate]` | – | – | |
| `:CALCulate:INTegral:CLEar:AUTO` | `ON \| OFF` | `ON` | |
| `:CALCulate:INTegral:STARt:SOURce` | `CMD \| TIME \| MAN` | `CMD` | |
| `:CALCulate:INTegral:STARt[:IMMediate]` | – | – | |
| `:CALCulate:INTegral:STARt:TIME` | `yyyy,mm,dd,hh,mm,ss` | – | |
| `:CALCulate:INTegral:STOP:SOURce` | `CMD \| TIME \| MAN \| TINTerval` | `CMD` | |
| `:CALCulate:INTegral:STOP[:IMMediate]` | – | – | |
| `:CALCulate:INTegral:STOP:TIME` | `yyyy,mm,dd,hh,mm,ss` | – | |
| `:CALCulate:INTegral:STOP:TINTerval` | `0 to ...` | – | |
| `:CALCulate:HARMonic:ORDer` | `0 to 40` | `1` | |
| `:CALCulate:POWer:EFFiciency:REFerence` | `<function 1>, <function 2>` | – | |
| `:CALCulate:POWer:CORRected` | `STAR \| DELTa` | – | |

#### `CALCulate:TRANsform:FREQuency[:STATe] ONCE`

Starter en enkelt beregning av frekvenstransformen (spektrum), dvs. instrumentet beregner spektrum kun ved mottak av denne kommandoen. Et forsøk på å utføre spektrumberegning mens `SENSe:SWEep1[:STATe]` er `ON` (minnelagring av samples) genererer feilen «-221, Settings conflict».

- **Parametre:** `ONCE`
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
CALC:TRAN:FREQ ONCE
```

#### `CALCulate:TRANsform:FREQuency:MODE FFT | DFT | STD`

Velger beregningsmetode for harmoniske.

| Parameter | Betydning |
|---|---|
| `FFT` | Beregner et FFT-amplitudespektrum. Antall linjer bestemmes av instrumentet og avhenger av stoppfrekvensen, innstillingen for anti-alias-filteret og instrumentmodellen. |
| `DFT` | Beregner et DFT-amplitudespektrum, dvs. frekvensen og amplituden til grunnharmonisk og dens heltallsmultipler fra FFT-spekteret. Antall linjer (harmoniske) avhenger av valgt frekvensområde og grunnfrekvensen (maks. 41 inkludert DC-komponent). |
| `STD` | Beregner harmoniske i henhold til standarden EN61000-4-7 Ed 2.1. Instrumentet må være synkronisert på grunnfrekvensen. |

- **\*RST-tilstand:** `FFT`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:TRAN:FREQ:MODE FFT
CALC:TRAN:FREQ:MODE?    Respons: FFT
```

#### `CALCulate:TRANsform:FREQuency:FUNCtion <function>{,<function>}`

Velger funksjonen(e) `<function>` som instrumentet skal bruke i spektrumberegningen. `<function>` angis som en streng i anførselstegn, for eksempel: `"VOLTage1"`. En kommaseparert liste av `<sensor_function>` kan sendes som parametre. Når en ny liste sendes, invalideres den tidligere aktive listen (hvis noen).

Query-responsen returnerer en kommaseparert liste av funksjoner, der hver funksjon er `<STRING RESPONSE DATA>`, dvs. omsluttet av anførselstegn. Queryen returnerer kortform-mnemonikkene og utelater eventuelle standardnoder (default nodes) i `<function>`.

Denne funksjonslisten lagres ikke med `*SAV` og tømmes med `*RST`.

- **\*RST-tilstand:** tom liste = ingen verdier definert
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:TRAN:FREQ:FUNC "VOLT1","CURR1","POW1"
CALC:TRAN:FREQ:FUNC?    Respons: "VOLT1","CURR1","POW1"
```

#### `CALCulate:TRANsform:FREQuency:STARt <frequency>`

Angir startfrekvensen for beregningen av harmoniske, i hertz. Kommandoen aksepterer kun 0 og returnerer alltid 0. Kommandoen er implementert av kompatibilitetshensyn.

- **Parametre:** `<frequency>`
- **\*RST-tilstand:** 0.0 Hz
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:TRAN:FREQ:STAR 0.0
CALC:TRAN:FREQ:STAR?    Respons: 0.0
```

#### `CALCulate:TRANsform:FREQuency:STOP <frequency>`

Angir øvre frekvens for beregningen av harmoniske (frekvensen til den ytterste FFT- eller DFT-spektrumlinjen).

- **Parametre:** `<frequency>`
  - Minimum: 10 Hz
  - Maksimum: samplerate / 2
  - Sampleraten kan hentes med `[:SENSe]:SWEep:FREQuency?`.
  - Instrumentet avrunder (coercer) den angitte verdien til nærmeste høyere eksakte frekvens.
- **\*RST-tilstand:** høyest mulige verdi (avhengig av instrumentet)
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:TRAN:FREQ:STOP 625.0
CALC:TRAN:FREQ:STOP?    Respons: 625.0
```

#### `CALCulate:TRANsform:FREQuency:CYCLes 4 | 6 | 8 | 10 | 12`

Definerer lengden på analyseintervallet. Innstillingen gjelder kun i STD-modus.

- **Parametre:** `<cycles>` – velger antall grunnharmoniske perioder (cycles) for analyseintervallet (begrenset liste). Standardverdiene er 10 for 50 Hz og 12 for 60 Hz nominell frekvens (tilsvarer en intervallengde på 200 ms ved fnom).
- **\*RST-tilstand:** `10`
- **Invaliderer / invalideres av:** –

#### `CALCulate:TRANsform:FREQuency:GROuping COMPonent | HARMonic | HGRoup | HSGRoup | ISGRoup | SGRoup`

Setter grupperingsmodus for etterbehandling av harmonisk analyse. Modusen kan endres selv etter en trigget analyse (`CALC:TRAN:FREQ ONCE`) for å lese dataene fra samme intervall på nytt med en annen grupperingsmodus. Innstillingen gjelder kun i STD-modus.

| Parameter | Betydning |
|---|---|
| `COMPonent` | Ingen gruppering, kun spektralkomponentene fra basis-FFT leveres (Y C,k) |
| `HARMonic` | De harmoniske komponentene beregnes (Y H,h) |
| `HGRoup` | De harmoniske gruppene beregnes (Y g,h) |
| `HSGRoup` | De harmoniske subgruppene beregnes (Y sg,h) |
| `ISGRoup` | De interharmoniske subgruppene beregnes (Y isg,h) |
| `SGRoup` | Både harmoniske og interharmoniske subgrupper beregnes (Ysg,h, Yisg,h) |

- **\*RST-tilstand:** `COMPonent`
- **Invaliderer / invalideres av:** –

#### `CALCulate:DATA? [<count>[,<offset>]]`

Returnerer spektrumdata i formatet definert av FORMat-kommandoene. Når kommandoen sendes uten argumenter, returneres alle data i henhold til preamble-informasjonen. Første returnerte spektrumlinje tilsvarer DC-komponenten i signalet.

Hvis en harmonisk ikke kunne beregnes på grunn av brudd på sampleteoremet for en frekvens i DFT-modus, returneres NaN for denne harmoniske.

Hvis en måling av harmoniske ennå ikke er utført, eller hvis denne queryen sendes mens en måling av harmoniske pågår (bit 12 i `STATus:OPERation` er satt), genereres feilen «-230, Data corrupt or stale» og ingen data returneres.

- **Parametre (valgfrie):**
  - `<count>` – angir antall linjer/harmoniske som skal returneres per funksjon.
  - `<offset>` – hvis ikke angitt, returneres spektrumdata fra og med linje/harmonisk 0 (DC-komponenten). Ellers har første returnerte linje indeksen gitt av `<offset>`.
  - Hvis `<offset>` + `<count>` overstiger antall tilgjengelige linjer, genereres feilen «-222, Data out of range». Bruk `CALCulate:DATA:PREamble?` for å finne det faktiske antallet tilgjengelige harmoniske/linjer.
- **Respons:**
  - Når `FORMat:TRANspose` er `ON`, grupperes punktene per funksjon:
    `<line1>,<line2>,<line3>,... (func1) ... <line1>,<line2>,<line3>,... (func2)`
  - Når `FORMat:TRANspose` er `OFF`, grupperes punktene per linje:
    `<func1>,<func2>,<func3>,... (line1) ... <func1>,<func2>,<func3>,... (line2)`
- **\*RST-tilstand:** ingen respons på denne kommandoen etter reset.
- **Invaliderer / invalideres av:** –

Eksempel:

```
CALC:DATA?    Respons: 221.56,0.056,15.456,0.075,5.24,0.034...
```

#### `CALCulate:DATA:PREamble?`

Leser preamble (innledningsdata) for spektrumdataene. Preamble-informasjonen er kun gyldig for spektrumdata fra den samme enkeltberegningen av spektrum som ble startet med `CALCulate:TRANsform:FREQuency[:STATe] ONCE`.

Hvis denne queryen sendes mens en måling av harmoniske pågår (bit 12 i `STATus:OPERation` er satt), genereres feilen «-230, Data corrupt or stale» og ingen preamble-data returneres.

- **Respons:** `<start_time>,<line_count>,<function_count>,<freq>[,<freq>,...]`
  - `<start_time>` – angir tidsintervallet mellom `TIMer:RESet:TIME?` og første punkt i de samplede dataene som ble brukt i den siste spektrumberegningen. Hvis en måling av harmoniske ennå ikke er utført, returneres ASCII-ekvivalenten av NaN (+9.91E+37) for dette elementet.
  - `<line_count>` – angir antall spektrumlinjer beregnet per funksjon.
  - `<function_count>` – angir antall funksjoner i spektrumfunksjonslisten.
  - `<freq>[,<freq>,...]` – en liste med frekvenssteg (FFT), grunnfrekvens (DFT) eller synkroniseringsfrekvens (STD) for hver funksjon i spektrumlisten. Første frekvens tilsvarer første funksjon i funksjonslisten, andre frekvens tilsvarer andre funksjon, osv. For FFT (frekvenssteg) og STD (synkroniseringsfrekvens) er `<freq>`-verdiene identiske for alle funksjoner; for DFT kan hver funksjon ha en individuell grunnfrekvens. Hvis ingen grunnfrekvens kan finnes (DFT), eller synkroniseringsfrekvensen ikke er gyldig eller utenfor området (STD), returneres en ASCII-ekvivalent av NaN (+9.91E37).
- **Invaliderer / invalideres av:** –

Eksempel:

```
CALC:DATA:PRE?    Respons: 11.32, 40, 3, 50.0,50.0,50.0
```

#### `CALCulate:DATA:THD?`

Leser THD-verdiene i henhold til valgt gruppering i STD-modus. Verdiene er kun gyldige i STD-modus (unntatt med `COMPonent`-gruppering).

- **Respons:** `<THD>[,<THD>,...]` – en liste med THD-verdier for hver funksjon i spektrumlisten, oppgitt som relativ % av grunnharmonisk. NaN-verdier returneres i følgende tilfeller:
  - modusinnstillingen er FFT, DFT eller STD med `COMPonent`-gruppering
  - grunnharmonisk for en funksjon er mindre enn 5 % av måleområdet
  - synkroniseringen er ikke låst, eller er utenfor analyseområdet for harmoniske
- **Invaliderer / invalideres av:** –

Eksempel:

```
CALC:DATA:THD?    Respons: +2.36780E+00,+5.12943E+00,+9.91000E+37
```

#### `CALCulate:INTegral[:STATe] ON | OFF`

Styrer tilstanden til instrumentets integrasjonsfunksjonalitet. Når den er aktivert, kan instrumentet integrere over enkelte midlede målefunksjoner.

| Parameter | Betydning |
|---|---|
| `ON` | Integrasjon aktivert. |
| `OFF` | Integrasjon deaktivert. |

- **\*RST-tilstand:** `OFF`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT ON
CALC:INT?    Respons: ON
```

#### `CALCulate:INTegral:FUNCtion <function>{,<function>}`

Angir funksjonslisten for integralberegning. `<function>` angis som en streng i anførselstegn, for eksempel: `"POWer1"`. En kommaseparert liste av `<function>` kan sendes som parametre. Når en ny liste sendes, invalideres den tidligere aktive listen (hvis noen).

Query-responsen returnerer en kommaseparert liste av funksjoner, der hver funksjon er `<STRING RESPONSE DATA>`, dvs. omsluttet av anførselstegn. Queryen returnerer kortform-mnemonikkene og utelater eventuelle standardnoder i `<function>`.

Midlede POWer- og VOLTage/CURRent:MEAN-verdier kan integreres. Spesifikatorene `INTegral | PINTegral | NINTegral` kan da brukes i `SENSe:FUNCtion`- / `SENSe:DATA?`-listen for å lese de integrerte verdiene. Hvis integrasjonsfunksjonslisten endres eller integrasjon slås `OFF`, returnerer verdiene som tilsvarer de fjernede funksjonene NaN som respons på `SENSe:DATA?`.

`SENSe:FUNCtion` genererer feilen «-221, Settings conflict» hvis det gjøres forsøk på å sette en `SENSe:FUNCtion`-liste som inneholder en integrert funksjon som ikke er del av integrasjonsfunksjonslisten, eller mens INTegral-beregning er `OFF`. Maksimalt antall integrerte funksjoner er 6.

`*RCL` og `*RST` nullstiller alle integrerte verdier.

- **Parametre:** `<function>` – angir den integrerte funksjonen. Liste over gyldige funksjoner:

```
VOLTage1..6:MEAN
CURRent1..6:MEAN
POWer[1..6|460][:ACTive]
POWer[1..6|460]:APParent
POWer[1..6|460]:REACtive
POWer[1..6|460][:ACTive]:HAR
POWer[1..6|460]:APParent:HAR
POWer[1..6|460]:REACtive:HAR
```

- **\*RST-tilstand:** avhenger av antall installerte faser:

| Antall installerte faser | Funksjonsliste |
|---|---|
| 1 | `"POW1"` |
| 2 | `"POW1","POW2"` |
| 3 | `"POW1","POW2","POW3","POW"` |
| 4 | `"POW1","POW2","POW3","POW","POW4"` |
| 5 | `"POW1","POW2","POW3","POW","POW4","POW5"` |
| 6 | `"POW1","POW2","POW3","POW","POW460"` |

- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT:FUNC "POW1:ACT"
CALC:INT:FUNC?    Respons: "POW1"
```

#### `CALCulate:INTegral:CLEar[:IMMediate]`

Setter verdiene til alle de integrerte funksjonene til null. Alle verdier nullstilles samtidig.

- **Parametre:** –
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
CALC:INT:CLE
```

#### `CALCulate:INTegral:CLEar:AUTO ON | OFF`

Styrer automatisk nullstilling av de integrerte funksjonene.

| Parameter | Betydning |
|---|---|
| `ON` | Integrerte verdier nullstilles ved start av integrasjonen. Alle verdier settes til null ved integrasjonsstart. |
| `OFF` | Automatisk nullstilling av integrerte verdier er deaktivert. |

- **\*RST-tilstand:** `ON`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT:CLE:AUTO ON
CALC:INT:CLE:AUTO?    Respons: ON
```

#### `CALCulate:INTegral:STARt:SOURce CMD | TIME | MAN`

Angir startbetingelsen for integrasjon.

| Parameter | Betydning |
|---|---|
| `CMD` | Integrasjonen starter ved mottak av kommandoen `CALCulate:INTegral:STARt[:IMMediate]`. |
| `TIME` | Integrasjonen starter på et tidspunkt angitt med kommandoen `CALCulate:INTegral:STARt:TIME`. |
| `MAN` | Integrasjonen starter når brukeren trykker F1-tasten på frontpanelet fra integrasjonsmåleskjermen på instrumentet. |

- **\*RST-tilstand:** `CMD`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT:STAR:SOUR CMD
CALC:INT:STAR:SOUR?    Respons: CMD
```

#### `CALCulate:INTegral:STARt[:IMMediate]`

Starter integrasjonen umiddelbart. Kommandoen setter også de integrerte verdiene til null hvis `CALCulate:INTegral:CLEar:AUTO` er satt til `ON`.

Forsøk på å starte integrasjon med denne kommandoen når `CALCulate:INTegral:STARt:SOURce` ikke er satt til `CMD`, genererer feilen «-221, Settings conflict».

- **Parametre:** –
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
CALC:INT:STAR
```

#### `CALCulate:INTegral:STARt:TIME <yyyy,MM,dd,hh,mm,ss>`

Angir starttidspunkt for integrasjonen. Integrasjonen starter når instrumentets interne dato/klokkeslett er lik tiden angitt med denne kommandoen.

| Parameter | Betydning |
|---|---|
| `yyyy` | År |
| `MM` | Måned |
| `dd` | Dag |
| `hh` | Timer i 24-timers notasjon |
| `mm` | Minutter |
| `ss` | Sekunder (heltallsverdi) |

- **\*RST-tilstand:** `2002,1,1,0,0,0`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT:STAR:TIME 2002,01,12,12,30,00
CALC:INT:STAR:TIME?    Respons: 2002,01,12,12,30,00
```

#### `CALCulate:INTegral:STOP:SOURce CMD | TIME | MAN | TINTerval`

Angir stoppbetingelsen for integrasjon. Å stoppe integrasjonen nullstiller ikke de integrerte verdiene.

| Parameter | Betydning |
|---|---|
| `CMD` | Integrasjonen stopper ved mottak av kommandoen `CALCulate:INTegral:STOP[:IMMediate]`. |
| `TIME` | Integrasjonen stopper på et tidspunkt angitt med kommandoen `CALCulate:INTegral:STOP:TIME`. |
| `MAN` | Integrasjonen stopper når brukeren trykker F2-tasten på frontpanelet fra integrasjonsmåleskjermen på instrumentet. |
| `TINTerval` | Integrasjonen stopper etter at intervallet angitt med `CALCulate:INTegral:STOP:TINTerval` har utløpt, regnet fra integrasjonsstart. |

- **\*RST-tilstand:** `CMD`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT:STOP:SOUR CMD
CALC:INT:STOP:SOUR?    Respons: CMD
```

#### `CALCulate:INTegral:STOP[:IMMediate]`

Stopper integrasjonen umiddelbart. Å stoppe integrasjonen nullstiller ikke de integrerte verdiene.

Forsøk på å bruke denne kommandoen når `CALCulate:INTegral:STARt:SOURce` ikke er satt til `CMD`, genererer feilen «-221, Settings conflict». *(Slik står det i manualen.)*

- **Parametre:** –
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
CALC:INT:STOP
```

#### `CALCulate:INTegral:STOP:TIME <yyyy,MM,dd,hh,mm,ss>`

Angir stopptidspunkt for integrasjonen. Integrasjonen stopper når instrumentets interne dato/klokkeslett er lik tiden angitt med denne kommandoen.

| Parameter | Betydning |
|---|---|
| `yyyy` | År |
| `MM` | Måned |
| `dd` | Dag |
| `hh` | Timer i 24-timers notasjon |
| `mm` | Minutter |
| `ss` | Sekunder (heltallsverdi) |

- **\*RST-tilstand:** `2010,1,1,0,0,0`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT:STOP:TIME 2002,01,12,12,30,00
CALC:INT:STOP:TIME?    Respons: 2002,01,12,12,30,00
```

#### `CALCulate:INTegral:STOP:TINTerval <interval>`

Angir integrasjonsintervallet i sekunder. Integrasjonen stopper etter at dette intervallet har utløpt, regnet fra integrasjonsstart.

- **Parametre:** `1.0e-3` til `9.99e+6`
- **\*RST-tilstand:** `6.00000E+01`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:INT:STOP:TINT 1.0
CALC:INT:STOP:TINT?    Respons: 1.0
```

#### `CALCulate:HARMonic:ORDer <order>`

Angir harmonisk orden for målefunksjonen `VOLTage[1..6|460]:HAR[:MINimum|MAXimum]`.

- **Parametre:** `0` til `40` (for øyeblikket er kun `1` gyldig)
- **\*RST-tilstand:** `1`
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:HARM:ORD 1
CALC:HARM:ORD?    Respons: 1
```

#### `CALCulate:POWer[460]:EFFiciency:REFerence <function1>,<function2>`

Angir to funksjoner for brukerdefinert beregning av elektrisk virkningsgrad.

- `CALCulate:POWer:EFFiciency:REFerence` angir variabler for elektrisk virkningsgrad for det 1. 2- eller 3-fasesystemet (`POWer:EFFiciency`).
- `CALCulate:POWer460:EFFiciency:REFerence` angir variabler for elektrisk virkningsgrad for det 2. 3-fasesystemet (`POWer460:EFFiciency`).

- **Parametre:** `<function1>` og `<function2>` kan være hvilken som helst av de midlede aktive effektene instrumentet måler: `"POWer[1..6|460][:ACTive]"`
- **\*RST-tilstand:** avhenger av instrumenttype og antall installerte faser.
- **Invaliderer / invalideres av:** –

Eksempler:

```
CALC:POW:EFF:REF "POW460", "POW1"
CALC:POW:EFF:REF?    Respons: "POW460", "POW1"
```

#### `CALCulate:POWer:CORRected STAR | DELTa`

Velger fase-til-nøytral- eller fase-til-fase-spenninger for beregning av tomgangstapsmålinger (no load loss) på transformatorer i henhold til IEC60076-1 (målefunksjonen `POWer[1|2|3|4|5|6|460]:CORRected`).

I et 3-faseinstrument er `STAR` ikke mulig sammen med W2 (Aron); parameteren settes da automatisk til `DELTa`. Hvis det er flere enn 3 faser, kan `STAR` fortsatt velges i modus W2, men den gjelder da kun fase 4 og oppover. Pcorr fra det første systemet er da ikke tilgjengelig.

Kommandoen aksepteres kun av firmwareversjon V1.4 og nyere.

| Parameter | Betydning |
|---|---|
| `STAR` | Bruk fase-til-nøytral-spenninger (stjernekobling/wye). |
| `DELTa` | Bruk fase-til-fase-spenninger (deltakobling). |

- **\*RST-tilstand:** `CALCulate:POWer:CORRected?`: `STAR`
- **Invaliderer:** –
- **Invalideres av:** `ROUTe:SYST "2W"` (kun 3-faseinstrumenter)

Eksempler:

```
CALC:POW:CORR DELT
CALC:POW:CORR?    Respons: DELT
```

### DISPlay-subsystemet

DISPlay-subsystemet inneholder kommandoer for å styre displayet.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:DISPlay[:WINDow][:STATe]` | `ON \| OFF` | `OFF` | |
| `:DISPlay:USER:FUNCtion` | `<function list>` | – | |

#### `:DISPlay[:WINDow][:STATe] ON | OFF`

Styrer om instrumentets prosessor oppdaterer displayet. Lysstyrke eller strømforbruk for displayet påvirkes ikke av denne kommandoen.

| Parameter | Betydning |
|---|---|
| `ON` | Displayet oppdateres av prosessoren. |
| `OFF` | Instrumentets prosessor oppdaterer ikke displayet, noe som frigjør mer prosessorkraft til omfattende beregninger. |

- **\*RST-tilstand:** `OFF`
- **Invaliderer / invalideres av:** –

Eksempler:

```
DISP ON
DISP?    Respons: ON
```

#### `:DISPlay:USER:FUNCtion <function>{,<function>}`

Angir funksjonslisten for den brukerdefinerte måleskjermen.

- **Parametre:** `<function>{,<function>}`
- **\*RST-tilstand:** tom liste = ingen verdier definert
- **Invaliderer / invalideres av:** –

Eksempler:

```
DISP:USER:FUNC "VOLT1:PHIGH","VOLT1:PLOW","VOLT1:PTP"
DISP:USER:FUNC?    Respons: "VOLT1:PHIGH","VOLT1:PLOW","VOLT1:PTP"
```

### FORMat-subsystemet

FORMat-subsystemet setter dataformatet for overføring av numeriske måledata og målearrayer. Dette dataformatet brukes for responsdata av de kommandoene som spesifikt er angitt å påvirkes av FORMat-subsystemet.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:FORMat[:DATA]` | `ASCii \| REAL`, `0 to 8 \| 32 \| 64` | `ASCii` | Standardlengde for REAL er 64 |
| `:FORMat[:DATA]:STATus` | `ASCii \| INTeger, 8 \| 16 \| 32` | `ASCii` | Standardlengde for INTeger er 8 |
| `:FORMat:BORDer` | `NORMal \| SWAPped` | `NORMal` | Lengde kun for INTeger |
| `:FORMat:TRANspose` | `ON \| OFF` | `OFF` | |

#### `FORMat[:DATA] ASCii | INTeger | REAL, [0..8] | 16 | [32 | 64]`

Angir dataformatet for overføring av måleverdiene fra instrumentet. Kommandoen gjelder alle typer måledata: midlede målinger, minneopptak, spektrum-/FFT-data osv. Hvis `<length>` ikke angis, bruker instrumentet siste gyldige innstilling.

Denne kommandoen er koblet med `FORMat[:DATA]:STATus`. En endring fra ASCii- (tekst-) format til REAL | INTeger (binært) format eller omvendt via enten `FORMat[:DATA]` eller `FORMat[:DATA]:STATus`, endrer begge formatene – både måledata- og statusinformasjonsformatet. Disse formatene er alltid enten begge tekst eller begge binære. Siste gyldige lengde brukes for det indirekte endrede formatet.

**Parametre – type:**

| Type | Betydning |
|---|---|
| `ASCii` | Data overføres som flyttallsverdi formatert som streng. Flere ASCii-dataverdier skilles med komma. Lengden angir antall mantissesifre i vitenskapelig notasjon. Hvis målestatusen er feil (8), er måleverdien NaN = +9.91E+37. |
| `REAL` | Data overføres som flyttall av angitt lengde i «definite-length block»-binærformat: `#abbbb....` der ett tegn ('0'–'9') angir antall b-tegn som gir antall databytes, deretter antall databytes som følger umiddelbart, og så selve databytene. REAL-data er som standard i big-endian-byterekkefølge. Byterekkefølgen kan endres med kommandoen `FORMat:BORDer`. |

**Parametre – lengde [bits]:**

| Lengde | Betydning |
|---|---|
| `0 to 8` | Gjelder ASCii. Lengden angir antall mantissesifre i vitenskapelig notasjon. For lengder ulik null formateres verdiene med C-formatstrengen `"%+.(length-1)e"`. En `<length>`-verdi på null betyr at enheten selv velger antall signifikante sifre som returneres. Maksimal lengde for ASCii er 8. Standardlengden er 6. |
| `16` | Gjelder INTeger. Angir antall biter som representerer det fortegnsbestemte heltallet. |
| `32 \| 64` | Gjelder REAL. Angir lengden på den binære representasjonen av flyttallet i biter (standard er 64). |

Hvis målestatusen er feil (8), er måleverdien IEEE 754 NaN:

```
FORMat:BORDer SWAPped
REAL,32 = {0, 0, 0xC0, 0x7F}
REAL,64 = {0, 0, 0, 0, 0, 0, 0xF8, 0x7F}
FORMat:BORDer NORMal
REAL,32 = {0x7F, 0xC0, 0, 0}
REAL,64 = {0x7F, 0xF8, 0, 0, 0, 0, 0, 0}
```

- **Respons:** `<type>,[<length>]`
- **\*RST-tilstand:** `ASCii,6`
- **Invaliderer:** `FORMat[:DATA]:STATus`
- **Invalideres av:** `FORMat[:DATA]:STATus`

Eksempler:

```
FORM ASC,6
FORM REAL,32
FORM?    Respons: REAL,64
```

#### `FORMat[:DATA]:STATus ASCii | INTeger, [8] | 16 | 32`

Angir formatet for statusinformasjonen ved overføring av måleverdiene fra instrumentet. Statusinformasjon er et heltall.

Kommandoen gjelder midlede målinger og minneopptak av midlede data. Hvis `<length>` ikke angis, bruker instrumentet siste gyldige innstilling.

Denne kommandoen er koblet med `FORMat[:DATA]`. En endring fra ASCii- (tekst-) format til REAL | INTeger (binært) format eller omvendt via enten `FORMat[:DATA]` eller `FORMat[:DATA]:STATus`, endrer begge formatene – både måledata- og statusinformasjonsformatet. Disse formatene er alltid enten begge tekst eller begge binære. Siste gyldige lengde brukes for det indirekte endrede formatet.

| Parameter | Betydning |
|---|---|
| `ASCii` | Statusinformasjonen overføres som heltallsverdi formatert som streng. Flere statusinformasjonsverdier skilles med komma. Lengde er ikke gyldig for ASCii-format av statusinformasjon. |
| `INTeger,[8]\|16\|32` | Statusinformasjonsverdien overføres som binært heltall av angitt lengde. INTeger-statusinformasjonsverdier er i big-endian-byterekkefølge. For standardlengden på 8 biter kan lengden utelates. |

- **Respons:** `<type>,[<length>]`
- **\*RST-tilstand:** `ASCii`
- **Invaliderer:** `FORMat[:DATA]`
- **Invalideres av:** `FORMat[:DATA]`

Eksempler:

```
FORM:STAT ASC
FORM:STAT INT,8
FORM?    Respons: INT,8
```

#### `FORMat:BORDer NORMal | SWAPped`

Angir om binærdataene som overføres over grensesnittet er i normal (Motorola) eller byttet (swapped, Intel) byterekkefølge. Kommandoen gjelder alle typer binære måledata: midlede målinger, minneopptak, spektrum-/FFT-data osv.

| Parameter | Betydning |
|---|---|
| `NORMal` | Big-endian-dataformat (Motorola). |
| `SWAPped` | Little-endian-dataformat (Intel). |

- **Respons:** `<format>`
- **\*RST-tilstand:** `NORMal`
- **Invaliderer / invalideres av:** –

Eksempler:

```
FORM:BORD SWAP
FORM:BORD?    Respons: SWAP
```

#### `FORMat:TRANspose ON | OFF`

Kommandoen gjelder minneopptak (TRACe) og utmating av spektrumdata. Dataene kan betraktes som en 2D-array (matrise). Kommandoen velger om rader og kolonner i matrisen skal byttes om.

| Parameter | Betydning |
|---|---|
| `ON` | Verdiene grupperes per målefunksjon: alle verdier for funksjon 1, alle verdier for funksjon 2, ... |
| `OFF` | Verdiene grupperes per intervall/spektrumlinje: alle verdier fra intervall 1, alle verdier fra intervall 2, ... |

- **\*RST-tilstand:** `OFF`
- **Invaliderer / invalideres av:** –

Eksempler:

```
FORM:TRAN ON
FORM:TRAN?    Respons: ON
```

### HARDcopy-subsystemet (HCOPy)

HARDcopy-subsystemet inneholder kommandoer for å hente ut bildet av instrumentskjermen.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:HCOPy:SDUMp:DATA?` | – | – | Kun query |

#### `HCOPy:SDUMp:DATA?`

Returnerer skjermdump-data (i internt format som aksepteres av PC-programmet for overføring av instrumentskjermbilder).

- **Parametre:** –
- **Respons:** blokk med RLE-kodede skjermdata
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
HCOP:SDUM:DATA?
Respons: Blokk med RLE-kodede skjermdata
```

### INITiate-subsystemet

INITiate-subsystemet styrer driften av instrumentets midlingsfunksjonalitet (averaging). Hvis minneopptak er aktivert, initierer det også trigger-/synkroniseringssubsystemet.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:INITiate:CONTinuous` | `ON \| OFF` | `ON` | |
| `:INITiate[:IMMediate]` | – | – | Ingen query |
| `:INITiate[:IMMediate]:SEQuence1` | – | – | |
| `:INITiate[:IMMediate]:NAME` | `STARt` | – | |

#### `INITiate:CONTinuous ON | OFF`

Styrer kontinuerlig-tilstanden for midlingsfunksjonaliteten. Hvis satt til `ON`, starter instrumentet automatisk en ny midlingssyklus når den forrige er ferdig. `INITiate:CONTinuous ON` bør brukes for målinger uten hull (gap-free).

| Parameter | Betydning |
|---|---|
| `ON` | Fri-løp-modus (free-run). Instrumentet starter automatisk ny midlingssyklus når den forrige er ferdig. |
| `OFF` | Enkeltskudd-modus (single-shot). Instrumentet utfører én midlingssyklus ved mottak av enten `INIT[:IMMediate]` eller `*TRG`. Instrumentet settes deretter tilbake i IDLE-tilstand. |

- **\*RST-tilstand:** `ON`
- **Invaliderer / invalideres av:** –

Eksempler:

```
INIT:CONT ON
INIT:CONT?    Respons: 1
```

#### `INITiate[:IMMediate]`

Forlater IDLE-tilstanden og starter én enkelt midlingssyklus. Når denne midlingssyklusen er fullført, settes instrumentet tilbake i IDLE-tilstand. Hvis enheten ikke er i IDLE, eller hvis `INITiate:CONTinuous` er satt til `ON`, har en IMM-kommando ingen effekt og feilen -213 genereres.

- **Parametre:** –
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
INIT
```

#### `INITiate[:IMMediate]:SEQuence1` / `INITiate[:IMMediate]:NAME STARt`

Initierer starttriggeren for minneopptak (memory acquisition). Etter initiering fylles pretriggeren (hvis > 0). Pretriggeren er fylt når «waiting for trigger»-biten i OPER:STAT-registeret er satt til 1. `STARt` er et alias for `SEQuence1`.

- **Parametre:** –
- **\*RST-tilstand:** –
- **Invaliderer / invalideres av:** –

Eksempel:

```
INITiate:NAME STARt
```

### INPut-subsystemet

INPut-subsystemet styrer egenskapene til inngangskanalene. Numeriske suffikser på INPut-noden tilsvarer maskinvareinngangskanal på instrumentet. For 6-kanalsmodeller er de gyldige elektriske kanalsuffiksene 1 til 6. For 12-kanalsmodeller er de gyldige elektriske kanalsuffiksene 1 til 12. Det elektriske INPut-subsystemet skiller ikke mellom strøm- og spenningskanaler. Inngangsfilterinnstillingene er felles for alle elektriske kanaler, så kanalsuffiksene kan utelates, selv om de er implementert av kompatibilitetshensyn. Hvis kanalsuffikset utelates, gjelder kommandoen inngang 1.

For Process Interface-opsjonen er de gyldige mekaniske kanalsuffiksene 21 til 24 (momentinngang 1..4) og 25 til 28 (turtallsinngang 1..4). Se avsnitt 2.1.10 SENSe2 Subsystem for detaljert beskrivelse.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:INPut[1..12]:COUPling` | `AC \| DC` | `DC` | |
| `:INPut[1..12]:GAIN` | `0 to 1.0e12` | `1.0` | Kun strømkanaler |
| `:INPut[1..12]:FILTer[:STATe]` | `ON \| OFF` | `ON` | |
| `:INPut[1..12]:FILTer[:LPASs]:FREQuency?` | – | Enhetsavhengig | Kun query |
| `:INPut[1..12]:SHUNt` | `INTernal \| EXTernal` | `INTernal` | Kun strømkanaler |
| `:INPut[21..28]:TYPE` | `VOLTage \| FREQuency` | – | Opsjon Process Interface |

#### `INPut[1..12]:COUPling AC | DC`

Setter inngangskoblingen for den valgte inngangskanalen.

| Parameter | Betydning |
|---|---|
| `AC` | DC-komponenten fjernes fra signalet før videre behandling. Denne koblingen kompenserer for eventuelle DC-forskyvninger (offsets) på signalet. |
| `DC` | Signalet forblir urørt og alle komponentene sendes videre til behandling. Denne koblingen bør brukes for ekte RMS-beregninger (true RMS). |

- **\*RST-tilstand:** `DC` for alle kanaler
- **Invaliderer / invalideres av:** –

Eksempler:

```
INP1:COUP AC
INP2:COUP?    Respons: DC
```

#### `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:GAIN <gain>`

Setter shuntfaktoren for strøminngangene. Innstillingen gjelder når EXTernal inngangsshunt er valgt. De partallige (spennings-) kanalnumrene er kun tilgjengelige på instrumenter utstyrt med PP59/PP69 power phase.

- **Parametre:** `1.0e-7` til `1.0e+7`
  - Denne enhetsløse forsterkningsfaktoren angir V/A-omsetningsforholdet til den eksterne shunten koblet til den angitte kanalen. Negative verdier er ikke tillatt.
- **Respons:** `<gain>`
- **\*RST-tilstand:** `1.0` for alle kanaler
- **Invaliderer / invalideres av:** –

Eksempler:

```
INP1:GAIN 10.0
INP3:GAIN?    Respons: 251.65
```

#### `INPut[1..12]:FILTer[:STATe] ON | OFF`

Slår anti-alias-filtrene på inngangene på eller av. Filtrene på alle kanaler er koblet sammen, dvs. å aktivere/deaktivere filteret på én kanal aktiverer/deaktiverer filtrene på alle kanaler.

| Parameter | Betydning |
|---|---|
| `ON` | Anti-alias-filter aktivert. |
| `OFF` | Anti-alias-filter deaktivert. |

- **\*RST-tilstand:** `ON` for alle kanaler
- **Invaliderer / invalideres av:** –

Eksempler:

```
INP:FILT ON
INP:FILT?    Respons: 1
```

#### `INPut[1..12]:FILTer[:LPASs]:FREQuency?`

Henter grensefrekvensen (cutoff) til anti-alias-lavpassfilteret. Alle inngangskanaler er utstyrt med de samme filtrene, så den returnerte verdien er alltid identisk for alle kanaler. Grensefrekvensen til anti-alias-filteret kan ikke endres.

- **Respons:** `<frequency>` i Hz
- **\*RST-tilstand:** enhetsavhengig for alle kanaler
- **Invaliderer / invalideres av:** –

Eksempel:

```
INP:FILT:FREQ?    Respons: 300.0e3
```

#### `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:SHUNt INTernal | EXTernal`

Velger shunten som brukes på strømkanalen. De partallige (spennings-) kanalnumrene er kun tilgjengelige på instrumenter utstyrt med PP59/PP69 power phase.

| Parameter | Betydning |
|---|---|
| `INTernal` | Interne shunter opptil 10 A brukes. |
| `EXTernal` | Ekstern shunt er tilkoblet. Shuntfaktoren må angis med kommandoen `INPut:GAIN`. |

- **\*RST-tilstand:** `INTernal` for alle kanaler
- **Invaliderer / invalideres av:** –

Eksempler:

```
INP1:SHUN EXT
INP3:SHUNt?    Respons: INT
```

#### `INPut[21..28]:TYPe VOLTage | FREQuency` (opsjon Process Interface)

Setter inngangstypen slik at den samsvarer med sensortypen.

| Parameter | Betydning |
|---|---|
| `VOLTage` | Et DC-signal i området +/- 10 V forventes på inngangen. |
| `FREQuency` | Et AC-signal med frekvens i området 1 Hz til 200 kHz forventes på inngangen. |

- **\*RST-tilstand:** `FREQuency`
- **Invaliderer / invalideres av:** –

Eksempler:

```
INP21:TYP FREQ
INP21:TYP?    Respons: FREQ
```

### OUTPut-subsystemet

OUTPut-subsystemet styrer egenskapene til SYNC-utgangen.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:OUTPut9[:STATe]` | `ON \| OFF` | `OFF` | SYNC-utgang |

#### `OUTPut9[:STATe] ON | OFF`

Setter tilstanden til synkroniseringsutgangen. Kan bare settes til `ON` hvis `SYNC:SOURce` ikke er satt til `EXTernal`.

| Parameter | Betydning |
|---|---|
| `ON` | Synkroniseringspulsene sendes ut på sync-inn-/utgangskontakten på baksiden. |
| `OFF` | Ingen synkroniseringspulser sendes ut. |

- **\*RST-tilstand:** `OFF`
- **Invaliderer:** –
- **Invalideres av:** `SYNC:SOURce EXTernal`

Eksempler:

```
OUTP9 ON
OUTP9?    Respons: 0
```

### ROUTe-subsystemet

ROUTe-subsystemet velger tilkoblingstypen instrumentet bruker for å måle på et trefasesystem.

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:ROUTe:SYSTem` | `"3W" \| "2W"` | `"3W"` | |

#### `ROUTe:SYSTem "3W" | "2W"`

Velger tilkoblingstypen instrumentet bruker for å måle på et trefasesystem.

**Merk:** Parameteren `2W` aksepteres kun av firmwareversjon 1.4 og høyere.

| Parameter | Betydning |
|---|---|
| `"3W"` | Tre-wattmeter-konfigurasjon. |
| `"2W"` | To-wattmeter-konfigurasjon. |

- **\*RST-tilstand:** `"3W"`
- **Invaliderer / invalideres av:** –

Eksempler:

```
ROUT:SYST "3W"
ROUT:SYST?    Respons: "3W"
```

## Subsystemer: SENSe, SENSe2 og SOURce

### SENSe-subsystemet

SENSe-subsystemet styrer instrumentets midlingsfunksjon (averaging) og beregningen av de grunnleggende midlede verdiene. Numeriske suffikser på nodene `VOLTage|CURRent` tilsvarer elektriske faser. Kanalene i INPut-subsystemet kombineres til faser i `SENSe:VOLTage|CURRent`-subsystemet som vist i tabellen nedenfor. Hvis kanalsuffikset utelates, gjelder kommandoen fase 1.

| Inngangskanal (suffiks)              | Elektrisk fase | Fasesuffiks i SENSe-subsystemet |
|--------------------------------------|----------------|---------------------------------|
| INPut1 (strøm), INPut2 (spenning)    | L1             | `SENSe:VOLTage|CURRent1`        |
| INPut3 (strøm), INPut4 (spenning)    | L2             | `SENSe:VOLTage|CURRent2`        |
| INPut5 (strøm), INPut6 (spenning)    | L3             | `SENSe:VOLTage|CURRent3`        |
| INPut7 (strøm), INPut8 (spenning)    | L4             | `SENSe:VOLTage|CURRent4`        |
| INPut9 (strøm), INPut10 (spenning)   | L5             | `SENSe:VOLTage|CURRent5`        |
| INPut11 (strøm), INPut12 (spenning)  | L6             | `SENSe:VOLTage|CURRent6`        |

SENSe-noden er standardnoden (default node) på rotnivået i kommandotreet. Standardnoden i SENSe-subsystemet er `POWer`. Alle faserelaterte SENSe-innstillinger er felles for både AC- og DC-kobling, så `AC | DC`-noden kan utelates (DC antas brukt). Noden `:AC[|:DC]` er kun implementert av kompatibilitetshensyn.

#### Kommandooversikt (SENSe)

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `[:SENSe]` | | | |
| `  :CURRent[1..6]|VOLTage[1..6]` | | | |
| `    :AC|[:DC]` | | | |
| `      :RANGe` | | | |
| `        [:UPPer]` | 0.3 to 1000 V | - [V] | VOLTage |
| | 0.03 to 10 | - [A] | CURRent med INTernal shunt |
| | 0.03 to 20 V | | CURRent med EXTernal shunt |
| `        :AUTO` | ON \| OFF \| ONCE | ON | ONCE foreløpig ikke implementert |
| `        :LIST?` | | | Kun spørring |
| `      :SCALe` | 0.9 to 1.0e+7 | 1.0 [-] | |
| `  [:POWer[1..6]]|CURRent[1..6]|VOLTage[1..6]` | | | |
| `    :AC|[:DC]` | | | |
| `      :APERture` | | | |
| `        [:TIME]` | 15.0e-3 to 3.6e3 | 0.3 s | |
| `  :SWEep` | | | |
| `    :FREQuency?` | | enhetsavhengig | Kun spørring |
| `  :FUNCtion` | | | |
| `    [:ON]` | list of sens func | "" | |
| `      :ALL` | | - | Ingen spørring |
| `      :COUNt?` | | 0 | Kun spørring |
| `    :OFF` | | | |
| `      :ALL` | | | |
| `    :CONCurrent` | ON \| OFF | ON | |
| `  :DATA?` | list of sens func | "" | Kun spørring |
| `    :STATus?` | list of sens func | "" | Kun spørring |
| `  :SWEep1|2` | | | |
| `    :TIME` | \<value\> \| MAX | | |
| `      :MAX?` | | | Kun spørring |
| `    :POINTS?` | | | Kun spørring |
| `    :OFFSet` | | | |
| `      :TIME` | 0 \| \<value\> \| MAX | | |
| `      :POINTS?` | | | Kun spørring |
| `    [:STATe]` | ON \| OFF | OFF | |
| `    :COUNt` | 1 to 65535 | 1 | |
| `    :SFACtor` | 1 to 65535 | 1 | |
| `    :FUNCtion` | \<function list\> | | |

#### Skalering

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:SCALe <value>`**

Setter skaleringsfaktoren for spenning/strøm, som gjenspeiler omsetningsforholdet til eventuelle spennings-/strømtransformatorer eller spenningsdelere som benyttes. Spenning eller strøm på den angitte kanalen multipliseres med denne skaleringsfaktoren før all videre behandling. Alle signalstørrelser som beregnes på grunnlag av strøm og/eller spenning, skaleres med denne faktoren.

- **Parametre:** `0.9 to 1.0e+7`. Negative verdier er ikke tillatt.
- **Tilstand etter `*RST`:** `1.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
VOLT3:SCAL 10.0
CURR2:SCAL?          -> 10.0
```

#### Områdevalg (Ranging)

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC|[:DC]:RANGe[:UPPer] <value>`**

Setter spennings-/strømområdet. Det angitte området er det faktiske området for instrumentets inngangskanal. Kanal-skaleringsfaktorer og eksterne shunter (`INPut:GAIN`) er ikke inkludert. Kommandoen setter RMS-verdien av området; det faktiske toppområdet (peak) er to ganger høyere. En områdeverdi innenfor det gyldige intervallet rundes opp til nærmeste høyere mulige verdi.

- **Parametre:**
  - `0.3 to 1000.0 V` – Område for spenningskanal. Gjelder spenningskanalene (2, 4, 6, 8, 10, 12).
  - `0.03 to 10.0 A` – Område for strømkanal når `INPut:SHUNt` er satt til `INTernal`. Gjelder kanalene (1, 3, 5, 7, 9, 11).
  - `0.03 to 10.0 V` – Område for strømkanal når `INPut:SHUNt` er satt til `EXTernal`. Området settes som spenning på spenningsinngangen til strømkanalen. Det faktiske strømområdet i ampere finnes ved å multiplisere denne verdien med `INPut:GAIN` for tilhørende kanal. Gjelder kanalene (1, 3, 5, 7, 9, 11).
- **Tilstand etter `*RST`:** Etter reset er autorange PÅ, så det er ikke satt noe fast område.
- **Ugyldiggjør:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe:AUTO ON`
- **Ugyldiggjøres av:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe:AUTO ON`

```scpi
VOLT3:RANG 25.0
CURR2:RANG?          -> 3.0
```

#### Midling og områdelister

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:RANGe[:UPPer]:LIST?`**

Spør etter en liste over gyldige spennings-/strømområder tilgjengelig på kanalen angitt med fasesuffikset. Den returnerte listen inneholder de faktiske områdene for instrumentets inngangskanaler. Kanal-skaleringsfaktorer og eksterne shunter (`INPut:GAIN`) er ikke inkludert. For strømkanaler avhenger listen av den aktuelle innstillingen av `INPut:SHUNt` (`EXTernal` eller `INTernal`).

- **Respons:** `<range_list>` – kommaseparert liste over områdeverdier.
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
CURR2:RANG:LIST?
-> 0.03, 0.1, 0.3, 1, 3, 10   (for innstillingen INPut:SHUNt INTernal)
```

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:RANGe[:UPPer]:AUTO ON|OFF|ONCE`**

Styrer autorange for spenning/strøm.

- **Parametre:**
  - `ON` – Autorange er permanent PÅ. Overvåking av `STATus:OPERation`-registeret vil oppdage at området endres.
  - `OFF` – Autorange er permanent AV.
  - `ONCE` – ONCE er foreløpig ikke implementert.
- **Tilstand etter `*RST`:** `ON`
- **Ugyldiggjør:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe[:UPPer]`
- **Ugyldiggjøres av:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe[:UPPer]`

```scpi
VOLT3:RANG:AUTO ON
CURR2:RANG:AUTO?     -> 0
```

**`[SENSe:][:POWer[1..6]]|CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:APERture[:TIME] <avgtime>`**

Setter det nominelle midlingsintervallet. Spørring returnerer det innstilte nominelle midlingsintervallet. I synkron modus endres det faktiske midlingsintervallet fortløpende («on-the-fly»): det nominelle midlingsintervallet forlenges til neste hele signalperiode.

For å spørre etter den faktiske midlingsperioden må kommandoen `:SENSe:DATA? "TIME[:INTerval]"` brukes.

Hvis det nominelle midlingsintervallet endres med denne kommandoen, settes synkroniseringstimeouten (`SYNC:TIMeout`) til det nominelle midlingsintervallet eller 0.3 sekunder, avhengig av hva som er størst.

> **Merk:** Nodene `[:POWer[1..6]]|CURRent[1..6]|VOLTage[1..6]:AC[|:DC]` er kun implementert for SCPI-kompatibilitet. Instrumentet arbeider med bare ett midlingsintervall for alle midlede målinger.

- **Parametre:** `15 ms ... 3600 s`. Oppløsning 1 ms. Enheten er sekunder.
- **Tilstand etter `*RST`:** `0.3 s`
- **Ugyldiggjør:** `SYNC:TIMeout`
- **Ugyldiggjøres av:** –

```scpi
APER 0.2
APER?                -> 1.5
```

#### Samplingsfrekvens

**`[SENSe:]SWEep:FREQuency?`**

Spør etter samplingsfrekvensen til instrumentets ADC-er. Samplingsfrekvensen er fast og kan ikke endres.

- **Respons:** `<sample_rate>` – den faktiske samplingsfrekvensen instrumentet bruker for datainnsamling. Samplingsfrekvensen er felles for alle kanaler. Enheten er Hz.
- **Tilstand etter `*RST`:** Enhetsavhengig:
  - Norma 3000: 102.4 kHz
  - Norma 4000: 341.33 kHz eller 1.024 MHz
  - Norma 5000: 341.33 kHz eller 1.024 MHz
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWEep:FREQ?          -> 3.4133E+05
```

#### Målefunksjoner

**`[SENSe:]FUNCtion[:ON] <function>{,<function>}`**

Kommandoen `FUNCtion[:ON]` velger hvilke(n) `<function>`(er) instrumentet skal måle (SENSe). `<function>` angis som en streng i anførselstegn, for eksempel: `FUNCtion "VOLTage:AC"`. Hvis `CONCurrent` er `OFF`, sendes én enkelt `<function>` som parameter; denne funksjonen velges som den som skal måles. Hvis mer enn én funksjon sendes, genereres feilen `-108 (Parameter not allowed)`. Hvis `CONCurrent` er `ON`, kan en kommaseparert liste av `<sensor_function>` sendes som parametre; disse funksjonene slås på, mens alle andre funksjoner slås av.

Spørringen `FUNCtion[:ON]?` returnerer en kommaseparert liste over funksjoner som er PÅ, hver som `<STRING RESPONSE DATA>`. Hvis ingen funksjoner er PÅ, returneres en tom streng. Spørringen returnerer kortformene og utelater eventuelle standardnoder i `<function>`.

Denne funksjonslisten lagres ikke med `*SAV` og nullstilles med `*RST`.

- **Parametre:** `<function>{,<function>}`
- **Tilstand etter `*RST`:** Tom liste = ingen verdier definert.
- **Ugyldiggjør:** `[SENSe:]FUNCtion[:ON]:COUNt?`
- **Ugyldiggjøres av:** `[SENSe:]FUNCtion[:ON]:ALL`, `[SENSe:]FUNCtion:OFF:ALL`, `[SENSe:]FUNCtion:CONCurrent OFF`, `[SENSe:]DATA? <function>{,<function>}`, `[SENSe:]DATA:STATUS? <function>{,<function>}`

```scpi
FUNC "VOLT","CURR","POW"
FUNC?                -> "VOLT","CURR","POW"
```

**`[SENSe:]FUNCtion[:ON]:ALL`**

Slår PÅ alle `<sensor_function>`-er som instrumentet kan måle samtidig.

- **Tilstand etter `*RST`:** –
- **Ugyldiggjør:** `[SENSe:]FUNCtion[:ON]:COUNt?`
- **Ugyldiggjøres av:** `[SENSe:]FUNCtion[:ON]`

```scpi
FUNC:ALL
```

**`[SENSe:]FUNCtion[:ON]:COUNt?`**

Spørringen returnerer antall `<sensor_function>`-er som er PÅ.

- **Respons:** `<count>` – antall midlede målinger som for øyeblikket er konfigurert.
- **Tilstand etter `*RST`:** `0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** `[SENSe:]FUNCtion[:ON]`, `[SENSe:]FUNCtion[:OFF]`, `[SENSe:]DATA? <function>{,<function>}`, `[SENSe:]DATA:STATUS? <function>{,<function>}`, `[SENSe:]FUNCtion:CONCurrent OFF`

```scpi
FUNC:COUN?           -> 3
```

**`[SENSe:]FUNCtion:OFF:ALL`**

Slår AV alle `<sensor_function>`-er som instrumentet kan måle samtidig.

- **Tilstand etter `*RST`:** –
- **Ugyldiggjør:** `[SENSe:]FUNCtion[:ON]:COUNt?`
- **Ugyldiggjøres av:** `[SENSe:]FUNCtion[:ON]`

```scpi
FUNC:OFF:ALL
```

**`[SENSe:]FUNCtion:CONCurrent ON | OFF`**

`CONCurrent`-kommandoen angir om SENSor-blokken skal konfigureres til å måle én funksjon om gangen, eller mer enn én funksjon om gangen (samtidig).

- **Parametre:**
  - `ON` – Funksjonen(e) angitt som parametre til `FUNCtion[:ON]`-kommandoen slås på, mens tilstanden til andre funksjoner settes til av.
  - `OFF` – `FUNCtion[:ON]`-kommandoen fungerer som en «én-av-n»-bryter som velger den angitte funksjonen som den eneste målte funksjonen.
- **Tilstand etter `*RST`:** `ON`
- **Ugyldiggjør:** `[SENSe:]FUNCtion[:ON]:COUNt?`, `[SENSe:]FUNCtion[:ON]`
- **Ugyldiggjøres av:** –

```scpi
FUNC:CONC ON
FUNC:CONC?           -> 1
```

#### Dataspørring

**`[SENSe:]DATA? [<function,function...>]`**

Returnerer data i formatet definert av FORMat-kommandoene. Uten argumenter: antall returnerte verdier er lik antall argumenter til kommandoen `SENSe:FUNCtion[:ON]`. Hvis `SENSe:FUNCtion:CONCurrent` er `OFF`, kan bare én funksjon/måling konfigureres og returneres. Hvis den er `ON`, kan flere funksjoner konfigureres og spørres etter måleresultater.

- **Parametre:** `[<function>,<function>,...]`
- **Respons:** `<measurement_value>[,<measurement_value>,…]`
- **Tilstand etter `*RST`:** Tom liste = ingen verdier definert.
- **Ugyldiggjør:** `[SENSe:]FUNCtion[:ON]:COUNt?`, `[SENSe:]FUNCtion[:ON]`
- **Ugyldiggjøres av:** –

```scpi
DATA? "VOLT","CURR","POW"
-> 221.56,1.056,230.65
```

**`[SENSe:]DATA:STATus? [<function,function...>]`**

Returnerer midlede måling(er) etterfulgt av statusinformasjon for målingen. Statusinformasjonen angir gyldigheten av målingen og legges til etter settet med måleverdier. Antall statusverdier er likt antall returnerte måleverdier. Formatet på statusinformasjonen styres av `FORMat:STATus`-kommandoene.

- **Parametre:** `[<function>,<function>,...]` – se `SENSe:FUNCtion` for detaljert beskrivelse av tilgjengelige funksjoner.

**Statusverdier:** De returnerte statusverdiene er heltall og legges til på slutten av måleresultatene. Statusverdien er en bitmaske og kan være en kombinasjon av én eller flere av følgende verdier (biter) kombinert med logisk OR. For eksempel representerer verdien 3 både underrange- og overrange-tilstand.

| Verdi | Navn | Betydning |
|---|---|---|
| 0 | Normal | Gyldig måling, ingen tvilsom tilstand. |
| 1 | Underrange | Den returnerte verdien er gyldig, men signalamplituden er for lav for det gitte området, slik at målepresisjonen er redusert. |
| 2 | Overrange | Instrumentet returnerer en måleverdi, men inngangssignalets amplitude er for høy for det gitte området og klippes til en amplitude innenfor gjeldende område. Den returnerte verdien kan ligge mer eller mindre utenfor spesifikasjonen. |
| 8 | Undefined | Instrumentet klarte ikke å beregne en gyldig verdi. Dette kan f.eks. skyldes tap av synkronisering (ingen gyldig frekvens, harmoniske, ...). Instrumentet returnerer Not A Number for målingen. |
| 16 | Not available | Den forespurte funksjonen er ikke, eller ikke lenger, tilgjengelig (f.eks. opsjon ikke installert, funksjon slått av). Instrumentet returnerer Not A Number for målingen. |
| 128 | Power Factor capacitive | For Power Factor-funksjonen angir dette kapasitiv faseforskjell mellom spenning og strøm (0 = induktiv). |

- **Respons:** `<measurement_value1>[,<measurement_value2>,…],<measurement_status1>,[<measurement_status2>,…]`
- **Tilstand etter `*RST`:** Tom liste = ingen verdier definert.
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
DATA:STATUS? "VOLT","CURR","POW"
-> 221.56,1.056,230.65,0,0,0
```

#### Minneopptak (Memory Recording)

Kommandoene som konfigurerer minneopptak bruker obligatoriske suffikser 1 og 2 etter SWEep-noden:

- `[:SENSe]:SWEep1` – Konfigurerer minneopptak av samplede verdier (REALtime).
- `[:SENSe]:SWEep2` – Konfigurerer minneopptak av midlede verdier (AVERage).

Innstillingene for minneopptak av midlede og samplede verdier deler samme konfigurasjonsområde, og alle innstillinger må settes på nytt når man bytter fra opptak av midlede til samplede verdier eller omvendt. Kommandoen `[SENSe:]SWEep1|2[:STATe] OFF` tilbakestiller innstillingene til standardverdier, bortsett fra triggere.

**`[SENSe:]SWEep1|2:TIME <value> | MAX`**

Angir maksimal lengde på minneopptaket i sekunder. Opptakslengden inkluderer pretrigger. Hvis synkronisering er på, er maksimal opptaksvarighet for SWEep2 direkte avhengig av det eksakte antallet midlingsintervaller som vil bli tatt opp, beregnet som angitt opptakslengde / nominelt midlingsintervall.

- **Parametre:** `<value> | MAX`
- **Tilstand etter `*RST`:** `MAX`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:TIME 1.0
SWE1:TIME?           -> 1.0
```

**`[SENSe:]SWEep1|2:TIME:MAX?`**

Returnerer maksimal opptakstid i sekunder i henhold til gjeldende innstillinger for minneopptak (total mengde tilgjengelig minne, sett av variabler som skal tas opp, samplingsfaktor og instrumentets samplingsfrekvens).

- **Respons:** `<time>`
- **Tilstand etter `*RST`:** Maksimal opptakstid i henhold til `*RST`-innstillingene for minneopptak.
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:TIME:MAX?
```

**`[SENSe:]SWEep1|2:POINTS?`**

Før opptaket er startet eller fullført, returnerer denne kommandoen maks. antall punkter per funksjon som vil bli tatt opp. For synkroniserte SWEep2-opptak (AVERage) beregnes verdien som:

```
Konfigurert opptakstid / nominelt midlingsintervall / samplingsfaktor
( SWEep2:TIME? / APER? / SWEep2:SFACtor? )
```

Det faktiske maks. antall punkter som blir tatt opp, avhenger av variasjonene i frekvensen til det målte signalet. Når opptaket er fullført, returnerer kommandoen faktisk antall registrerte punkter per funksjon.

- **Respons:** `<count>`
- **Tilstand etter `*RST`:** Maksimalt tilgjengelig antall punkter. Avhenger av mengden tilgjengelig minne i instrumentet.
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:POINTS?
```

**`[SENSe:]SWEep1|2:OFFSet:TIME <value> | MAX`**

Angir pretriggerlengden i sekunder.

- **Parametre:** `<value> | MAX`. Pretriggerlengden må være større enn eller lik null og mindre enn opptakslengden angitt med `[SENSe:]SWEep1|2:TIME`.
- **Tilstand etter `*RST`:** `0.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:OFFS:TIME 1.0
```

**`[SENSe:]SWEep1|2:OFFSet:POINTS?`**

Før opptaket er startet eller fullført, returnerer denne kommandoen maks. antall punkter per funksjon som vil bli tatt opp i pretriggeren. For synkroniserte SWEep2-opptak (AVERage) beregnes verdien som:

```
Konfigurert pretriggertid / nominelt midlingsintervall / samplingsfaktor
( SWEep2:OFFSet:TIME? / APER? / SWEep2:SFACtor? )
```

Det faktiske maks. antall punkter som blir tatt opp i pretriggeren, avhenger av variasjonene i frekvensen til det målte signalet. Når opptaket er fullført, returnerer kommandoen faktisk antall punkter per funksjon som ble tatt opp i pretriggeren.

- **Parametre:** –
- **Respons:** `<count>`
- **Tilstand etter `*RST`:** `0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:OFFS:POINTS?    -> 0
```

**`[SENSe:]SWEep1|2[:STATe] ON | OFF`**

Aktiverer/deaktiverer minneopptak av samplede/midlede verdier. Bare én av sweepene kan være aktivert om gangen. Forsøk på å aktivere begge sweepene genererer feilen `-221 Settings conflict`, dvs. SWEep1-opptak (REALtime) og SWEep2-opptak (AVERage) kan ikke kjøre samtidig. Overgang fra OFF til ON tømmer minnet som om `TRACe:DELete:ALL` var utført. Overgang fra ON til OFF tilbakestiller alle innstillinger relatert til minneopptak, bortsett fra triggere.

- **Parametre:**
  - `ON` – Aktiverer minneopptak.
  - `OFF` – Deaktiverer minneopptak.
- **Tilstand etter `*RST`:** `OFF`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1 ON
SWE1?                -> 1
```

**`[SENSe:]SWEep1|2:COUNt <count>`**

Antall blokker som skal registreres.

- **Parametre:** `1 to 65535` (foreløpig er bare 1 gyldig).
- **Tilstand etter `*RST`:** `1`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:COUN 1
SWE1:COUN?           -> 1
```

**`[SENSe:]SWEep1|2:SFACtor <value>`**

Setter samplingsfaktoren (sample factor). Den angir at hver n-te verdi produsert av `[SENSe:]SWEep1|2`-blokken lagres i minnet. Hvis den angitte samplingsfaktoren ville gi en tid mellom to lagrede sampler som er større enn `SENSe:SWEep1|2:TIME` eller `SENSe:SWEep1|2:OFFSet:TIME`, genereres feilen `-221 Settings conflict`.

- **Parametre:** `1 to 65535`. Når samplingsfaktoren er satt til 1, lagres alle sampler.
- **Tilstand etter `*RST`:** `1`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:SFAC 1
```

**`[SENSe:]SWEep1|2:FUNCtion <function>{,<function>}`**

Angir funksjonslisten for minneopptak. Maksimalt antall funksjoner er 20. Hvis listen overskrider enhetens kapasitet, økes samplingsfaktoren automatisk.

For SWEep1-funksjonslisten (REALtime) kan bare samplede verdier angis. For SWEep2-funksjonslisten (AVERage) kan alle funksjoner fra instrumentets standard funksjonsliste angis.

Funksjonslisten lagres i en lagret konfigurasjon og lastes inn igjen ved PowerOn eller med `*RCL`.

- **Parametre:** `<function>`
  - Gyldige funksjoner for SWEep1 (REALtime):
    - `VOLTage1..6[:DC]`
    - `CURRent1..6[:DC]`
    - `POWer1..6[:ACTive]`
    - `TORQue[1..4]`
    - `SPEed[1..4]`
    - `POWer[1..4]:MECHanical`
  - Gyldige funksjoner for SWEep2 (AVERage): alle funksjoner fra instrumentets standard funksjonsliste.
- **Tilstand etter `*RST`:** `"VOLTage1"`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SWE1:FUNC "VOLT1","VOLT2","VOLT3"
```

### SENSe2-subsystemet (krever opsjonen Process Interface)

> **Merk:** Hele SENSe2-subsystemet gjelder kun instrumenter med opsjonen **Process Interface** installert.

SENSe2-subsystemet styrer innstillingene for inngangene på den valgfrie Process Interface-opsjonen. Numeriske suffikser på nodene `TORQue|SPEed|POLepairs|TYPe|REFerence` tilsvarer indeksen til de 4 motorene/generatorene som støttes. Kanalene i INPut-subsystemet kombineres til drivverksindeks (drive index) i `SENSe2:xxx`-subsystemet som vist i tabellen nedenfor.

| Inngangskanal (suffiks)                   | Drivverksindeks | Suffiks i SENSe2-subsystemet |
|-------------------------------------------|-----------------|------------------------------|
| INPut21 (moment), INPut25 (turtall)        | 1 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence1[:POWer]` |
| INPut22 (moment), INPut26 (turtall)        | 2 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence2[:POWer]` |
| INPut23 (moment), INPut27 (turtall)        | 3 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence3[:POWer]` |
| INPut24 (moment), INPut28 (turtall)        | 4 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence4[:POWer]` |

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:INPut[21..28]` | | | |
| `  :TYPe` | VOLTage \| FREQuency | FREQuency | Analog/digital sensor |

**`INPut[21..28]:TYPe VOLTage | FREQuency`**

Velger signaltypen som måles på en Process Interface-inngang.

- **Parametre:**
  - `VOLTage` – Inngangssignalet er spenning.
  - `FREQuency` – Inngangssignalet er frekvens.
- **Tilstand etter `*RST`:** `FREQuency` for alle Process Interface-innganger.
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
INP21:TYP VOLT
INP25:TYP?           -> FREQ
```

SENSe2-noden skiller det mekaniske systemet fra det elektriske (SENSe1 for det elektriske systemet er standardnoden på rotnivået i kommandotreet). `SENSe2:xxx:VOLTage`-nodene gjelder når tilhørende inngangstype er satt til `INPutx:TYPe VOLTage`, og `SENSe2:xxx:FREQuency`-nodene gjelder når tilhørende inngangstype er satt til `INPutx:TYPe FREQuency`.

#### Kommandooversikt – moment (TORQue)

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:SENSe2` | | | |
| `  :TORQue[1..4]` | | | |
| `    :VOLTage` | | | |
| `      :SCALe` | -1e6 to 1e6 | 1 [Nm/V] | Analog momentsensor |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 0 [V] | Inngangsspenning for 0 [Nm] |
| `        :IMMediate` | | | Sett offset fra inngangsverdi; ingen spørring |
| `    :FREQuency` | | | |
| `      :SCALe` | -1e6 to 1e6 | [Nm/Hz] | Digital momentsensor |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 10000 [Hz] | Inngangsfrekvens for 0 [Nm] |
| `        :IMMediate` | | | Sett offset fra inngangsverdi; ingen spørring |

#### Kommandooversikt – turtall (SPEed) og drivverk

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:SENSe2` | | | |
| `  :SPEed[1..4]` | | | |
| `    :VOLTage` | | | |
| `      :SCALe` | | | |
| `        [:DEFault]` | -1e6 to 1e6 | 1 [rpm/V] | Analog turtallssensor |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 0 [V] | Inngangsspenning for 0 [rpm] |
| `        :IMMediate` | | | Sett offset fra inngangsverdi; ingen spørring |
| `    :FREQuency` | | | |
| `      :SCALe` | | | |
| `        [:DEFault]` | -1e6 to 1e6 | 60 [rpm/Hz] | Digital turtallssensor |
| `        :PULSe` | 1 to 100000 | 1 [pul/rev] | Alternativ innstilling |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 0 [Hz] | Inngangsfrekvens for 0 [rpm] |
| `        :IMMediate` | | | Sett offset fra inngangsverdi; ingen spørring |
| `  :TYPe[1..4]` | MOTor \| GENerator | MOTor | |
| `  :POLepairs[1..4]` | 1 to 999 | 1 | |
| `  :REFerence[1..4]` | | | |
| `    [:POWer]` | "POWer[1..6][:ACTive]" | "POWer" | For virkningsgradsberegning |

#### Skalering – moment og turtall

**`SENSe2:TORQue[1..4]:VOLTage:SCALe <value>`**

Setter momentskaleringsfaktoren for inngang av spenningstype, som gjenspeiler omsetningsforholdet til momentsensorene som benyttes. Differansen mellom spenningen på den aktuelle inngangen og den angitte offsetverdien multipliseres med denne skaleringsfaktoren før all videre behandling.

- **Parametre:** `-1.0e6 to 1.0e6` [Nm/V]
- **Tilstand etter `*RST`:** `1.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SENS2:TORQ3:VOLT:SCAL 10.0
SENS2:TORQ1:VOLT:SCAL?   -> 25.0
```

**`SENSe2:TORQue[1..4]:VOLTage:OFFSet[:VALue] <value>`**

Setter inngangsspenningen som tilsvarer moment lik null. Denne spenningen trekkes fra den målte spenningen på inngangen før differansen multipliseres med skaleringsfaktoren.

- **Parametre:** `-1.0e6 to 1.0e6` [V]
- **Tilstand etter `*RST`:** `0.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** `SENSe2:TORQue[1..4]:VOLTage:OFFSet:IMMediate`

```scpi
SENS2:TORQ3:VOLT:OFFS 0.0
SENS2:TORQ2:VOLT:OFFS?   -> 0.0
```

**`SENSe2:TORQue[1..4]:VOLTage:OFFSet:IMMediate`**

Setter offsetverdien til den momentspenningen som måles i øyeblikket. Målingen må være gyldig (ingen overlast).

- **Parametre:** –
- **Tilstand etter `*RST`:** –
- **Ugyldiggjør:** `SENSe2:TORQue[1..4]:VOLTage:OFFSet`
- **Ugyldiggjøres av:** –

```scpi
SENS2:TORQ3:VOLT:OFFS:IMM
```

**`SENSe2:TORQue[1..4]:FREQuency:SCALe <value>`**

Setter momentskaleringsfaktoren for inngang av frekvenstype, som gjenspeiler omsetningsforholdet til momentsensorene som benyttes. Differansen mellom frekvensen på den aktuelle inngangen og den angitte offsetverdien multipliseres med denne skaleringsfaktoren før all videre behandling.

- **Parametre:** `-1.0e6 to 1.0e6` [rpm/Hz] *(slik enheten er oppgitt i manualen; kommandooversikten angir [Nm/Hz] for momentskala)*
- **Tilstand etter `*RST`:** `1.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SENS2:TORQ2:FREQ:SCAL 0.001
SENS2:TORQ1:FREQ:SCAL?   -> 0.001
```

**`SENSe2:TORQue[1..4]:FREQuency:OFFSet[:VALue] <value>`**

Setter inngangsfrekvensen som tilsvarer moment lik null. Denne frekvensen trekkes fra den målte frekvensen på inngangen før differansen multipliseres med skaleringsfaktoren.

- **Parametre:** `-1.0e6 to 1.0e6` [Hz]
- **Tilstand etter `*RST`:** `0.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** `SENSe2:TORQue[1..4]:FREQuency:OFFSet:IMMediate`

```scpi
SENS2:TORQ3:FREQ:OFFS 1000.0
SENS2:TORQ2:FREQ:OFFS?   -> 1000.0
```

**`SENSe2:TORQue[1..4]:FREQuency:OFFSet:IMMediate`**

Setter offsetverdien for momentfrekvens fra den verdien som måles i øyeblikket. Målingen må være gyldig (ingen overlast / udefinert verdi).

- **Parametre:** –
- **Tilstand etter `*RST`:** –
- **Ugyldiggjør:** `SENSe2:TORQue[1..4]:FREQuency:OFFSet`
- **Ugyldiggjøres av:** –

```scpi
SENS2:TORQ3:FREQ:OFFS:IMM
```

**`SENSe2:SPEed[1..4]:VOLTage:SCALe[:DEFault] <value>`**

Setter turtallsskaleringsfaktoren for inngang av spenningstype, som gjenspeiler omsetningsforholdet til turtallssensorene som benyttes. Differansen mellom spenningen på den aktuelle inngangen og den angitte offsetverdien multipliseres med denne skaleringsfaktoren før all videre behandling.

- **Parametre:** `-1.0e6 to 1.0e6` [Nm/V] *(slik enheten er oppgitt i manualen; kommandooversikten angir [rpm/V] for turtallsskala)*
- **Tilstand etter `*RST`:** `1.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SENS2:SPE3:VOLT:SCAL 10.0
SENS2:SPE1:VOLT:SCAL?    -> 25.0
```

**`SENSe2:SPEed[1..4]:VOLTage:OFFSet[:VALue] <value>`**

Setter inngangsspenningen som tilsvarer turtall lik null. Denne spenningen trekkes fra den målte spenningen på inngangen før differansen multipliseres med skaleringsfaktoren.

- **Parametre:** `-1.0e6 to 1.0e6` [V]
- **Tilstand etter `*RST`:** `0.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** `SENSe2:SPEed[1..4]:VOLTage:OFFSet:IMMediate`

```scpi
SENS2:SPE3:VOLT:OFFS 0.0
SENS2:SPE2:VOLT:OFFS?    -> 0.0
```

**`SENSe2:SPEed[1..4]:VOLTage:OFFSet:IMMediate`**

Setter offsetverdien til den turtallsspenningen som måles i øyeblikket. Målingen må være gyldig (ingen overlast).

- **Parametre:** –
- **Tilstand etter `*RST`:** –
- **Ugyldiggjør:** `SENSe2:SPEed[1..4]:VOLTage:OFFSet[:VALue]`
- **Ugyldiggjøres av:** –

```scpi
SENS2:SPEed3:VOLT:OFFS:IMM
```

**`SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault] <value>`**

Setter turtallsskaleringsfaktoren for inngang av frekvenstype, som gjenspeiler omsetningsforholdet til turtallssensorene som benyttes. Differansen mellom frekvensen på den aktuelle inngangen og den angitte offsetverdien multipliseres med denne skaleringsfaktoren før all videre behandling.

- **Parametre:** `-1.0e6 to 1.0e6` [rpm/Hz]
- **Tilstand etter `*RST`:** `1.0`
- **Ugyldiggjør:** `SENSe2:SPEed[1..4]:FREQuency:SCALe:PULS`
- **Ugyldiggjøres av:** `SENSe2:SPEed[1..4]:FREQuency:SCALe:PULS`

```scpi
SENS2:SPE2:FREQ:SCAL 0.001
SENS2:SPE1:FREQ:SCAL?    -> 0.001
```

**`SENSe2:SPEed[1..4]:FREQuency:SCALe:PULSe <value>`**

Setter turtallsskaleringsfaktoren for inngang av frekvenstype, som gjenspeiler omsetningsforholdet til turtallssensorene som benyttes. Denne alternative metoden gjør det mulig å sende spesifikasjonen til en digital turtallssensor direkte til enheten. Tilhørende offsetverdi bør settes til null.

- **Parametre:** `1 to 100000` [pulses/revolution]
- **Tilstand etter `*RST`:** `1`
- **Ugyldiggjør:** `SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault]`
- **Ugyldiggjøres av:** `SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault]`

```scpi
SENS2:SPE2:FREQ:SCAL:PULS 1024
SENS2:SPE1:FREQ:SCAL:PULS?   -> 256
```

**`SENSe2:SPEed[1..4]:FREQuency:OFFSet[:VALue] <value>`**

Setter inngangsfrekvensen som tilsvarer turtall lik null. Denne frekvensen trekkes fra den målte frekvensen på inngangen før differansen multipliseres med skaleringsfaktoren.

- **Parametre:** `-1.0e6 to 1.0e6` [Hz]
- **Tilstand etter `*RST`:** `0.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** `SENSe2:SPEed[1..4]:FREQuency:OFFSet:IMMediate`

```scpi
SENS2:SPE3:FREQ:OFFS 1000.0
SENS2:SPE2:FREQ:OFFS?    -> 1000.0
```

**`SENSe2:SPEed[1..4]:FREQuency:OFFSet:IMMediate`**

Setter offsetverdien for turtallsfrekvens fra den verdien som måles i øyeblikket. Målingen må være gyldig (ingen overlast / udefinert verdi).

- **Tilstand etter `*RST`:** –
- **Ugyldiggjør:** `SENSe2:SPEed[1..4]:FREQuency:OFFSet[:VALue]`
- **Ugyldiggjøres av:** –

```scpi
SENS2:TORQ3:FREQ:OFFS:IMM
```

*(Eksempelet er gjengitt slik det står i manualen.)*

#### Drivverksinnstillinger (Drive Settings)

**`SENSe2:TYPe[1..4] MOTor | GENerator`**

Setter typen drivverk som brukes. Innstillingen påvirker beregningen av sakking (slip) og virkningsgrad.

- **Parametre:**
  - `MOTor` – Drivverkstype satt til motor.
  - `GENerator` – Drivverkstype satt til generator.
- **Tilstand etter `*RST`:** `MOT`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SENS2:TYP1 MOT
SENS2:TYP3?          -> GEN
```

**`SENSe2:POLepairs[1..4] <value>`**

Angir antall polpar for drivverket. Innstillingen brukes til beregning av sakking (slip).

- **Parametre:** `<value>`, gyldig område: `1 to 999`
- **Tilstand etter `*RST`:** `1`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SENS2:POL3 2
SENS2:POL1?          -> 1
```

**`SENSe2:REFerence[1..4][:POWer] <function>`**

Angir hvilken målt elektrisk effekt som brukes til virkningsgradsberegning.

- **Parametre:** `<function>` kan være hvilken som helst av de midlede aktive effektene som instrumentet måler: `"POWer[1..6|460][:ACTive]"`
- **Tilstand etter `*RST`:** `"POW"`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SENS2:REF3 "POW1"
SENS2:REF2?          -> "POW"
```

### SOURce-subsystemet (krever opsjonen Process Interface)

> **Merk:** Hele SOURce-subsystemet gjelder kun instrumenter med opsjonen **Process Interface** installert.

SOURce-subsystemet styrer innstillingene for de analoge utgangene på den valgfrie Process Interface-opsjonen. Numeriske suffikser på VOLTage-noden tilsvarer indeksen til de 4 utgangene som støttes.

#### Kommandooversikt (SOURce)

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `:SOURce` | | | |
| `  :VOLTage[1..4]` | | | |
| `    [:LEVel]` | | | |
| `      [:IMMediate]` | | | |
| `        [:AMPLitude]` | -10.3 to 10.3 | 0.0 V | Kun for FIXed-modus |
| `    :MODE` | FIXed \| VARiable | FIXed | |
| `    :FEED` | \<function\> | VOLTage1 | Kun for VARiable-modus |
| `    :GAIN` | -1.0e6 to 1.0e6 | 1.0 V/Ref unit | |
| `    :ZERO` | -1.0e6 to 1.0e6 | 0.0 Ref unit | |

#### Utgangskonfigurasjon

**`SOURce:VOLTage[1..4]:MODE FIXed | VARiable`**

Velger driftsmodus for de analoge utgangene.

- **Parametre:**
  - `FIXed` – Utgangsspenningen angis direkte med kommandoen `SOURce:VOLTage[1..4][:LEVel][:IMMediate][:AMPLitude]`.
  - `VARiable` – Etter hver måling beregnes utgangsspenningen fra målefunksjonen valgt med FEED, ved hjelp av de angitte GAIN- og ZERO-verdiene.
- **Tilstand etter `*RST`:** `FIXed`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SOUR:VOLT3:MODE VAR
SOUR:VOLT2:MODE?     -> FIX
```

**`SOURce:VOLTage[1..4][:LEVel][:IMMediate][:AMPLitude] <value>`**

Velger utgangsspenningen for FIXed-modus.

- **Parametre:** `<value>`, gyldig område: `-10.3 to 10.3 V`
- **Tilstand etter `*RST`:** `0.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SOUR:VOLT4 5.3
SOUR:VOLT1?          -> -2.5
```

**`SOURce:VOLTage[1..4]:FEED <function>`**

Angir referansefunksjonen for utgangen i VARiable-modus.

- **Parametre:** `<function>` – enhver gyldig midlet målefunksjon i instrumentet.
- **Tilstand etter `*RST`:** `"VOLTage1"`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SOUR:VOLT2:FEED "POW2:APP"
SOUR:VOLT4:FEED?     -> "CURR3:MEAN"
```

#### Skalering av utganger

**`SOURce:VOLTage[1..4]:GAIN <value>`**

Angir skaleringen for utgangen. Differansen mellom den aktuelle verdien av referansefunksjonen og ZERO-verdien multipliseres med denne faktoren for å beregne utgangsspenningen.

- **Parametre:** `<gain>`, gyldig område: `-1.0e6 to 1.0e6 V/Ref unit`
- **Tilstand etter `*RST`:** `1.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SOUR:VOLT2:GAIN 5.0
SOUR:VOLT3:GAIN?     -> 1.0e-3
```

**`SOURce:VOLTage[1..4]:ZERO <value>`**

Angir offset for utgangen. Denne verdien trekkes fra den aktuelle verdien av referansefunksjonen før differansen multipliseres med GAIN-innstillingen for å beregne utgangsspenningen.

- **Parametre:** `<value>`, gyldig område: `-1.0e6 to 1.0e6 Ref unit`
- **Tilstand etter `*RST`:** `0.0`
- **Ugyldiggjør:** –
- **Ugyldiggjøres av:** –

```scpi
SOUR:VOLT1:ZERO 225.0
SOUR:VOLT3:ZERO?     -> 50.0
```

## Subsystemer: SYNC til STATus

### SYNC-subsystemet

SYNC-subsystemet styrer instrumentets synkroniseringsevne. Når synkronisering er aktivert, tilpasser instrumentet midlingssyklusene til frekvensen på signalet som mates til synkroniseringskilden. Hvis minneopptak av samplede data pågår, kan synkroniseringssignalet brukes som en spesiell form for triggering.

#### Kommandooversikt (SYNC)

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `SYNC:STATe` | ON \| OFF | ON | |
| `SYNC:LEVel:UNIT` | ABSolute \| PCT | PCT | |
| `SYNC[:SOURce]\|VOLTage[1..6]\|CURRent[1..6]` | VOLTage[1..6] \| CURRent[1..6] \| EXTernal | VOLTage1 | SOURce = gjeldende sync-kilde. \<foreløpig er kun SOURce-noden implementert\> |
| `SYNC...:LEVel` | -150 %...150 % av området | 0.0 | Ikke for EXTernal |
| `SYNC...:SLOPe` | POSitive \| NEGative | POSitive | |
| `SYNC...:FILTer[:LPASs][:STATe]` | ON \| OFF | OFF | Ikke for EXTernal-kilde |
| `SYNC...:FILTer[:LPASs]:FREQuency` | 1.0e2, 1.0e3, 1.0e4 | 1.0e4 Hz | Ikke for EXTernal-kilde |
| `SYNC:TIMeout` | 0.015 til 3600.0 | 0.3 s | |

#### `SYNC:STATe ON | OFF`

**Beskrivelse:** Angir om midlingsintervallet skal styres av signalfrekvensen på valgt inngang eller ikke. Hvis synkronisering er slått på, holdes den faktiske midlingsperioden til det første heltallsmultiplum av sync-signalet som er større enn brukerangitt nominell midlingsperiode. Hvis synkronisering er slått av, er den faktiske midlingsperioden lik den brukerangitte nominelle midlingsperioden avrundet til et heltallsmultiplum av sampleperioder.

**Parametre:**

| Parameter | Betydning |
|---|---|
| ON | Synkronisering kreves. Instrumentet vil alltid forsøke å synkronisere til frekvensen på sync-kildesignalet. |
| OFF | Synkronisering er deaktivert. Bruk dette alternativet for målinger på DC-signaler. |

**Eksempel:**

```
SYNC:STAT ON
SYNC:STAT?          Respons: 1
```

- **\*RST-tilstand:** ON

#### `SYNC:LEVel:UNIT ABSolute | PCT`

**Beskrivelse:** Setter enheten for kommandoen `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:LEVel`.

**Parametre:**

| Parameter | Betydning |
|---|---|
| ABSolute | Nivå angis i absolutte enheter. |
| PCT | Nivå angis i prosent av nominelt inngangsområde. |

**Eksempel:**

```
SYNC:LEV:UNIT ABS
SYNC:LEVel:UNIT?    Respons: PCT
```

- **\*RST-tilstand:** PCT
- **Invaliderer:** `SYNC:LEVel:UNIT`

#### `SYNC[:SOURce] VOLTage[1..6] | CURRent[1..6] | EXTernal`

**Beskrivelse:** Velger signalkilden for synkronisering og frekvensmåling.

**Parametre:**

| Parameter | Betydning |
|---|---|
| VOLTage[1..6] | En av spenningskanalene er sync-kilde. |
| CURRent[1..6] | En av strømkanalene er sync-kilde. |
| EXTernal | Ekstern TTL-sync-inngang er sync-kilde. |

**Eksempel:**

```
SYNC:SOUR VOLT1
SYNC:SOUR?          Respons: VOLT1
```

- **\*RST-tilstand:** VOLTage1
- **Invaliderer:** `SYNC[:SOUR]:AUTO`
- **Invalideres av:** `SYNC[:SOUR]:AUTO`

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:LEVel <level>`

**Beskrivelse:** Setter sync-nivået der perioden til valgt inngangssignal måles av instrumentets synkroniseringskretser. `SYNC:SOURce:LEVel` setter sync-nivået til den aktive triggerkilden (ikke for EXTernal). \<foreløpig er kun SOURce-noden implementert\>

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<level>` | Gyldig område: -150 % til 150 % av nominelt inngangsområde på angitt kanal, i IEEE 488.2 \<NON-DECIMAL NUMERIC PROGRAM DATA\>-format. Enheten velges med kommandoen `SYNC:LEVel:UNIT`. |

**Eksempel:**

```
SYNC:VOLT1:LEV 10.0
SYNC:VOLT1:LEV?     Respons: 0.0
```

- **\*RST-tilstand:** 0.0
- **Invaliderer:** `SYNC[:SOURce]:VOLTage[1..6]|CURRent[1..6]:LEVel:AUTO`
- **Invalideres av:** `SYNC[:SOURce]:VOLTage[1..6]|CURRent[1..6]:LEVel:AUTO`, `[SENSe:]VOLTage[1..6]|CURRent[1..6]:AC[:|DC]:RANGe[:UPPer]`, `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:SHUNt`, `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:GAIN`, `SYNC:LEVel:UNIT`

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:SLOPe POSitive | NEGative`

**Beskrivelse:** Setter den aktive flanken for synkroniseringssignalet. \<foreløpig er kun SOURce-noden implementert\>

**Parametre:**

| Parameter | Betydning |
|---|---|
| POSitive | Instrumentet synkroniserer på positiv flanke av synkroniseringssignalet. |
| NEGative | Instrumentet synkroniserer på negativ flanke av synkroniseringssignalet. |

**Eksempel:**

```
SYNC:SLOP POS
SYNC:SLOP?          Respons: POS
```

- **\*RST-tilstand:** POSitive

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:FILTer:[LPASs[:STATe]] ON | OFF`

**Beskrivelse:** Styrer filteret for synkroniseringssignalet. Filtreringen brukes på signalet på inngangskanalen som er valgt som synkroniseringskilde. Denne kommandoen har ingen effekt hvis valgt sync-kilde er EXTernal. \<foreløpig er kun SOURce-noden implementert\>

**Parametre:**

| Parameter | Betydning |
|---|---|
| ON | Filter er PÅ. |
| OFF | Filter er AV. |

**Eksempel:**

```
SYNC:FILT ON
SYNC:FILT?          Respons: 0
```

- **\*RST-tilstand:** OFF

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:FILTer:[LPASs]:FREQuency 10.0e3 | 1.0e3 | 100.0`

**Beskrivelse:** Setter lavpassfrekvensen for synkroniseringssignalfilteret. Denne kommandoen har ingen effekt hvis valgt sync-kilde er EXTernal. Frekvensenheten er Hz. \<foreløpig er kun SOURce-noden implementert\>

**Parametre:**

| Parameter | Betydning |
|---|---|
| 10.0e3 | 10 kHz |
| 1.0e3 | 1 kHz |
| 100.0 | 100 Hz |

Enhver annen verdi mellom 100 Hz og 10 kHz tvinges (coerces) til nærmeste høyere eksakte verdi.

**Eksempel:**

```
SYNC:FILT:FREQ 100.0
SYNC:FILT:FREQ?     Respons: 1000.0
```

- **\*RST-tilstand:** 10000.0

#### `SYNC:TIMeout <timeout>`

**Beskrivelse:** Setter synkroniseringstimeout i sekunder. Instrumentet starter midling etter timeout hvis ikke noe sync-signal er tilgjengelig. Timeout er kun aktiv når synkronisering er på. Hvis det nominelle midlingsintervallet endres med kommandoen `[SENSe:]{CURRent[1..6]|VOLTage[1..6]|[POWer]}:{AC|[DC]}:APERture[:TIME]`, settes synkroniseringstimeouten til det nominelle midlingsintervallet eller 0.3 sekunder, avhengig av hva som er størst.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<timeout>` | 0.015 til 3600 s |

**Eksempel:**

```
SYNC:TIMeout 5.0
SYNC:TIMeout?       Respons: 5.0
```

- **\*RST-tilstand:** 0.3
- **Invalideres av:** `[SENSe:]{CURRent[1..6]|VOLTage[1..6]|[POWer]}:{AC|[DC]}:APERture[:TIME]`

### TIMer-subsystemet

TIMer-subsystemet inneholder kommandoer for å styre instrumentets interne timer. Denne timeren gir tidsstemplingsinformasjon for midlede målinger.

#### Kommandooversikt (TIMer)

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `TIMer:RESet` | | - | Ingen spørring |
| `TIMer:RESet:AUTO` | ON \| OFF | ON | \<foreløpig ikke implementert\> |
| `TIMer:RESet:TIME?` | | | Kun spørring |

#### `TIMer:RESet`

**Beskrivelse:** Nullstiller instrumentets interne timer. Timeren brukes til å måle minneopptakstid og antall midlingssykluser. Ved nullstilling settes både timerens tid og telleren for midlingssykluser til null. Absolutt tid for siste timer-nullstilling kan hentes med kommandoen `TIMer:RESet:TIME?`. Hvis denne kommandoen sendes mens midlede verdier lagres i minnet, blir tidsinformasjonen inkonsistent, siden timeren begynner å telle fra null midt i dataene.

Timeren nullstilles automatisk ved oppstart (Power On) — `TIMer:RESet:TIME?` gir da oppstartstidspunktet.

**Eksempel:**

```
TIM:RES
```

- **\*RST-tilstand:** Ingen nullstillingsbetingelse
- **Invaliderer:** `TIM:RES:TIME?`

#### `TIMer:RESet:AUTO ON | OFF`

**Beskrivelse:** Styrer om instrumentets interne timer nullstilles automatisk ved ARMing. For å beholde absolutt tidsbase for sekvenserte minnemålinger må `TIMer:RESet:AUTO` settes til OFF, slik at påfølgende `INITiate[:IMMediate]:NAME:STARt`-kommandoer ikke nullstiller timeren. \<foreløpig ikke implementert\>

**Parametre:**

| Parameter | Betydning |
|---|---|
| ON | `INITiate[:IMMediate]:NAME:STARt` nullstiller timeren. |
| OFF | `INITiate[:IMMediate]:NAME:STARt` nullstiller ikke timeren. |

**Eksempel:**

```
TIM:RES:AUTO ON
TIM:RES:AUTO?       Respons: 1
```

- **\*RST-tilstand:** ON
- **Invaliderer:** `TIM:RESet:TIME?`

#### `TIMer:RESet:TIME?`

**Beskrivelse:** Spør etter absolutt tid for siste timer-nullstilling.

**Respons:**

```
<year>,<month>,<day>,<hours>,<minutes>,<seconds>
```

Året er i firesifret numerisk format. Timer er i 24-timers notasjon.

**Eksempel:**

```
TIM:RES:TIME?
```

- **\*RST-tilstand:** Har ingen nullstillingsverdi

### TRACe-subsystemet

TRACe-subsystemet inneholder kommandoer for å lese minneopptak.

#### Kommandooversikt (TRACe)

| Kommando | Parameter | Merknad |
|---|---|---|
| `TRACe[:DATA]:PREamble?` | block | Kun spørring |
| `TRACe[:DATA]?` | \<block\>, \<number_of_points\>, \<offset\>, \<sparsing\> | Kun spørring |
| `TRACe[:DATA]:STATus?` | \<block\>, \<number_of_points\>, \<offset\>, \<sparsing\> | Kun spørring |
| `TRACe:FREE?` | | Kun spørring |
| `TRACe:CATalog:LENgth?` | | Kun spørring |
| `TRACe:DELete:ALL` | | |

#### `TRACe[:DATA]:PREamble? [<block>]`

**Beskrivelse:** Leser dataheaderen for gitt blokk. Hvis `<block>`-parameteren utelates, sendes alle headere.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<block>` = 1 | Denne valgfrie parameteren angir hvilken blokk i minnet preamblen skal returneres for. Foreløpig kan kun én blokk tas opp, og eneste gyldige verdi er 1. |

**Respons:**

| Felt | Type | Betydning |
|---|---|---|
| `<points_per_func>` | heltall | Angir mengden data tatt opp for hver funksjon. |
| `<num_of_func>` | heltall | Antall funksjoner konfigurert for minneopptak. |
| `<trigger_index>` | heltall | Indeks for datapunktet som svarer til trigger. |
| `<first_point_time_relative_to_trigger>` | flyttall | Tidsdifferansen mellom første registrerte datapunkt og trigger, i sekunder. |
| `<record_duration>` | flyttall | Angir tidsdifferansen mellom første og siste registrerte datapunkt, i sekunder. |
| `<trigger_time_relative_to_timer_reset_time>` | flyttall | Angir lengden på tidsintervallet mellom timer-nullstilling og trigger, i sekunder. Triggertiden som denne verdien er avledet fra, svarer til TIME:RELative-funksjonsverdien til datapunktet som kommer rett før dataene ved indeks `<trigger_index>` (TIME:RELative-verdiene er tidsstempler for når tilhørende midlingsintervaller ble fullført). |
| `<average_time_between_two_points>` | flyttall | Angir sampleintervall i sekunder. For SWEep1 (REALtime)-opptak og ikke-synkroniserte SWEep2 (AVERage)-opptak er denne verdien det eksakte sampleintervallet. For synkroniserte SWEep2 (AVERage)-opptak er verdien et gjennomsnittlig sampleintervall beregnet som `<record_duration> / <points_per_func>`. Det faktiske intervallet mellom enkeltstående påfølgende samples avhenger av variasjonene i frekvensen til det målte signalet. |

**Eksempel:**

```
TRAC:DATA:PRE?
Respons: 0,0,0,0.00000E+00,0.00000E+00,0.00000E+00,0.00000E+00
```

- **\*RST-tilstand:** `0,0,0,0.00000E+00,0.00000E+00,0.00000E+00,0.00000E+00`

#### `TRACe[:DATA]? [<block>[,<count>[,<offset>[,<sparsing>]]]]`

**Beskrivelse:** Leser data fra minnet.

Standard dataformat er lesbare ASCii-verdier (`FORMat[:DATA] ASCii, 8`). For bedre ytelse kan du bruke binær utdata med 32-bits brede flyttall (`FORMat[:DATA] REAL,32`) og normal (instrumentets egen) byterekkefølge (`FORMat:BORDer NORMal`).

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<block>` | Angir blokken som skal leses. Satt til 0 leses alle blokker. Satt til 1 leses den første innsamlede blokken, osv. |
| `<count>` | Angir antall punkter som skal leses for angitt blokk, med start ved offset-indeksen. |
| `<offset>` | Angir indeksen for det innsamlede datapunktet der overføringen skal starte. |
| `<sparsing>` | Angir at hvert n-te datapunkt overføres. Satt til 1 overføres alle punkter. Satt til 2 overføres annethvert punkt. |

Hvis ingen parametre angis, overføres alle registrerte data. Parametre kan utelates fra høyre mot venstre.

Standardverdier: `<block>=1`, `<count>=alle`, `<offset>=0`, `<sparsing>=1`

**Respons:**

Når `FORMat:TRANspose` er ON, grupperes verdiene etter funksjoner:

```
<interval1>,<interval2>,<interval3>,...   <interval1>,<interval2>,<interval3>,...
—— func1 ——                               —— func2 ——
```

Når `FORMat:TRANspose` er OFF, grupperes verdiene etter intervaller:

```
<func1>,<func2>,<func3>,...   <func1>,<func2>,<func3>,...
—— interval1 ——               —— interval2 ——
```

**Eksempel:**

```
TRAC:DATA?          Respons: 1.2345E+01,2.3456E+01,....
```

- **\*RST-tilstand:** Det er ingen respons på denne kommandoen etter reset.

#### `TRACe[:DATA]:STATus? [<block>[,<count>[,<offset>[,<sparsing>]]]]`

**Beskrivelse:** Leser data og status fra minnet.

Data returneres først, deretter følger statusinformasjonen.

Målestatusinformasjonen angir gyldigheten til målingen. Statusinformasjonen legges til etter settet med målte verdier. Antall statusverdier er likt antallet returnerte måleverdier. Formatet på statusinformasjonen styres av `FORMat:STATus`-kommandoene.

For best ytelse, bruk binær utdata med 32-bits brede flyttall (`FORMat[:DATA] REAL,32`), normal (instrumentets egen) byterekkefølge (`FORMat:BORDer NORMal`) og 8-bits brede heltalls-statusverdier (`FORMat[:DATA]:STATus INT,8`).

**Parametre:** Se `TRACe[:DATA]?`.

**Statusverdier:**

De returnerte statusverdiene er heltall. Målestatusverdiene legges til på slutten av måleresultatene. Statusverdien er en bitmaske (heltall) og kan være en kombinasjon av én eller flere av følgende verdier (biter) kombinert med logisk ELLER:

| Verdi | Navn | Betydning |
|---|---|---|
| 0 | Normal | Gyldig måling, ingen tvilsom (questionable) tilstand. |
| 1 | Underrange | Den returnerte verdien er gyldig, men signalamplituden er for lav for gitt område, slik at målepresisjonen er redusert. |
| 2 | Overrange | Instrumentet returnerer en måleverdi, men inngangssignalets amplitude er for høy for gitt område. Dette fører til at inngangssignalet klippes til en amplitude innenfor gjeldende område. Fordi måleverdien beregnes fra klippede sampledata, kan den returnerte verdien være mer eller mindre utenfor spesifikasjonen. |
| 8 | Undefined | Instrumentet klarte ikke å beregne en gyldig verdi. Dette kan f.eks. skyldes tap av synkronisering (ingen gyldig frekvens, harmoniske, ...). Instrumentet returnerer Not A Number for målingen. |
| 16 | Not available | Den forespurte funksjonen er ikke, eller ikke lenger, tilgjengelig (f.eks. opsjon ikke installert, funksjon slått av). Instrumentet returnerer Not A Number for målingen. |
| 128 | Power Factor capacitive | For effektfaktor-funksjonen angir dette kapasitiv faseforskjell mellom spenning og strøm (0 = induktiv). |

**Respons:**

Når `FORMat:TRANspose` er ON, grupperes verdiene etter funksjoner:

```
<interv1>,<interv2>,...  <interv1>,<interv2>,...     <interv1>,<interv2>,...  <interv1>,<interv2>,...
— func1 —                — func2 —                   — func1 —                — func2 — ...
—————————— data ——————————                           —————————— status ——————————
```

Når `FORMat:TRANspose` er OFF, grupperes verdiene etter intervaller:

```
<func1>,<func2>,...  <func1>,<func2>,...     <func1>,<func2>,...  <func1>,<func2>,...
— interval1 —        — interval2 —           — interval1 —        — interval2 — ...
—————————— data ——————————                   —————————— status ——————————
```

**Eksempel:**

```
TRAC:DATA:STAT?     Respons: 1.2345E+01,2.3456E+01,....,0,0,...
```

- **\*RST-tilstand:** Det er ingen respons på denne kommandoen etter reset.

#### `TRACe:FREE?`

**Beskrivelse:** Returnerer antall ledige byte i minnet.

**Respons:**

```
<bytes>
```

**Eksempel:**

```
TRAC:FREE?          Respons: 4194176
```

- **\*RST-tilstand:** Returnerer maksimalt tilgjengelig minne hvis ingen data er tatt opp. Denne verdien er instrumentavhengig.

#### `TRACe:CATalog:LENgth?`

**Beskrivelse:** Returnerer faktisk antall blokker innsamlet i minnet (kun 1 returneres).

**Respons:**

```
<number_of_blocks>
```

**Eksempel:**

```
TRAC:CAT:LEN?       Respons: 1
```

- **\*RST-tilstand:** 1

#### `TRACe:DELete:ALL`

**Beskrivelse:** Sletter alt minne.

**Eksempel:**

```
TRAC:DEL:ALL
```

- **\*RST-tilstand:** Dette er en handling og har ingen reset-tilstand.
- **Invaliderer:** `TRACe:FREE?`

### TRIGger-subsystemet

TRIGger-subsystemet inneholder kommandoer for å definere betingelsen på en midlet måling som skal utløse en handling. TRIGger-subsystemet har kun effekt hvis minneopptak er aktivert.

#### Kommandooversikt (TRIGger)

| Kommando | Parameter | Standardverdi/enhet |
|---|---|---|
| `TRIGger:STARt:SOURce` | BUS \| TIME \| IMMediate \| MANual \| SYNC \| \<function\> | IMMediate |
| `TRIGger:STARt:TIME` | yyyy,mm,dd,hh,mm,ss | |
| `TRIGger:STARt:LEVel` | value | 0.0 |
| `TRIGger:STARt:SLOPe` | POSitive \| NEGative | POSitive |
| `TRIGger:STOP:SOURce` | TIME \| IMMediate \| MANual \| \<function\> | IMMediate |
| `TRIGger:STOP:TIME` | yyyy,mm,dd,hh,mm,ss | |
| `TRIGger:STOP:LEVel` | \<value\> | 0.0 |
| `TRIGger:STOP:SLOPe` | POSitive \| NEGative | POSitive |

#### `TRIGger:STARt:SOURce BUS | TIME | IMMediate | MANual | SYNC | <function>`

**Beskrivelse:** Angir starttriggerkilden.

**Parametre:**

| Parameter | Betydning |
|---|---|
| BUS | Trigger når en `*TRG`-kommando mottas. |
| TIME | Trigger ved eksakt tidspunkt. |
| IMMediate | Ingen venting på en hendelse. |
| MANual | Signalet genereres av brukeren ved å trykke på frontpanelets "MEM"-tast. |
| SYNC | Trigger inntreffer hver gang en flanke av synkroniseringssignalet detekteres. Denne kilden er kun gyldig for REALtime-sweep (for å bruke EXTernal-signalkontakten som trigger må SYNC-kilden settes til EXTernal). |
| `<function>` | En betingelse på en midlet målefunksjon utløser triggeren. |

**Eksempel:**

```
TRIG:STAR:SOUR IMM
TRIG:STAR:SOUR?     Respons: IMM
```

- **\*RST-tilstand:** IMM

#### `TRIGger:STARt:TIME <yyyy,MM,dd,hh,mm,ss>`

**Beskrivelse:** Minneopptaket starter når instrumentets interne tid når angitt verdi.

**Parametre:**

| Parameter | Betydning |
|---|---|
| yyyy | År |
| MM | Måned |
| dd | Dag |
| hh | Timer i 24-timers notasjon |
| mm | Minutter |
| ss | Sekunder (heltallsverdi) |

**Eksempel:**

```
TRIG:STAR:TIME 2002,01,01,11,00,00
TRIG:STAR:TIME?     Respons: 2002,01,01,11,00,00
```

- **\*RST-tilstand:** 1970,1,1,0,0,0

#### `TRIGger:STARt:LEVel <level>`

**Beskrivelse:** Når startkilden for opptak er en midlet målefunksjon, angir denne innstillingen målefunksjonsnivået som skal utløse opptaket.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<level>` | Området for denne innstillingen er ikke definert. |

**Eksempel:**

```
TRIG:STARt:LEV 50.0
TRIG:STARt:LEV?     Respons: 25.0
```

- **\*RST-tilstand:** 0.0

#### `TRIGger:STARt:SLOPe POSitive | NEGative`

**Beskrivelse:** Når kilden for opptak er en midlet målefunksjon, angir denne innstillingen flanken. (Manualen omtaler her stoppkilden; kommandoen gjelder starttrigger.)

**Parametre:**

| Parameter | Betydning |
|---|---|
| POSitive | Trigger på positiv flanke. |
| NEGative | Trigger på negativ flanke. |

**Eksempel:**

```
TRIG:STAR:SLOP POS
TRIG:STAR:SLOP?     Respons: POS
```

- **\*RST-tilstand:** POS

#### `TRIGger:STOP:SOURce TIME | IMMediate | MANual | <function>`

**Beskrivelse:** Stopptriggerkilde. Innsamlingen stopper enten ved angitt dato/klokkeslett, når minnet er fullt, når opptakstiden er nådd, eller når valgt stoppbetingelse nedenfor er oppfylt — det som inntreffer først.

**Parametre:**

| Parameter | Betydning |
|---|---|
| TIME | Innsamlingen stopper enten når minnet er fullt, opptakstiden er nådd eller ved angitt dato/klokkeslett — det som inntreffer først. |
| IMMediate | Ingen ekstra stoppbetingelse settes. Innsamlingen stopper enten når minnet er fullt eller opptakstiden er nådd — det som inntreffer først. |
| MANual | Innsamlingen stopper enten når minnet er fullt, opptakstiden er nådd eller frontpanelets MEM-tast trykkes — det som inntreffer først. For å stoppe minneopptaket umiddelbart, slå av minnesubsystemet. |
| `<function>` | Innsamlingen stopper enten når minnet er fullt, opptakstiden er nådd, eller betingelsen på valgt funksjon er oppfylt — det som inntreffer først. |

**Eksempel:**

```
TRIG:STOP:SOUR MAN
TRIG:STOP:SOUR?     Respons: MAN
```

- **\*RST-tilstand:** MAN

#### `TRIGger:STOP:TIME <yyyy,MM,dd,hh,mm,ss>`

**Beskrivelse:** Minneopptaket stopper når instrumentets interne tid når angitt verdi.

**Parametre:**

| Parameter | Betydning |
|---|---|
| yyyy | År |
| MM | Måned |
| dd | Dag |
| hh | Timer i 24-timers notasjon |
| mm | Minutter |
| ss | Sekunder (heltallsverdi) |

**Eksempel:**

```
TRIG:STOP:TIME 2002,01,01,11,00,00
TRIG:STOP:TIME?     Respons: 2002,01,01,11,00,00
```

- **\*RST-tilstand:** 1970,1,1,0,0,0

#### `TRIGger:STOP:LEVel <level>`

**Beskrivelse:** Når stoppkilden for opptak er en midlet målefunksjon, angir denne innstillingen målefunksjonsnivået som skal stoppe opptaket.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<level>` | Området for denne innstillingen er ikke definert. |

**Eksempel:**

```
TRIG:STOP:LEV 50.0
TRIG:STOP:LEV?      Respons: 25.0
```

- **\*RST-tilstand:** 0.0

#### `TRIGger:STOP:SLOPe POSitive | NEGative`

**Beskrivelse:** Når stoppkilden for opptak er en midlet målefunksjon, angir denne innstillingen flanken.

**Parametre:**

| Parameter | Betydning |
|---|---|
| POSitive | Stopper opptak på positiv flanke. |
| NEGative | Stopper opptak på negativ flanke. |

**Eksempel:**

```
TRIG:STOP:SLOP POS
TRIG:STOP:SLOP?     Respons: POS
```

- **\*RST-tilstand:** POS

### SYSTem-subsystemet

I dette subsystemet er en rekke kommandoer for generelle funksjoner som ikke er direkte knyttet til effektanalyse, samlet. Her ligger blant annet kommunikasjonsinnstillingene (`SYSTem:COMMunicate...`).

#### Kommandooversikt (SYSTem)

| Kommando | Parameter | Standardverdi/enhet | Merknad |
|---|---|---|---|
| `SYSTem:COMMunicate:GPIB[:SELF]:ADDRess` | 1 to 30 | 5 | |
| `SYSTem:COMMunicate:SERial:BAUD` | 1200 \| 2400 \| 4800 \| 9600 \| 19200 \| 38400 \| 57600 \| 115200 | 115200 bd | |
| `SYSTem:COMMunicate:SERial:BITS` | 7 \| 8 | 8 bits | \<foreløpig ikke implementert\> |
| `SYSTem:COMMunicate:SERial:SBITs` | 1 \| 2 | 1 bit | \<foreløpig ikke implementert\> |
| `SYSTem:COMMunicate:SERial:CONTrol:RTS` | ON \| IBFull \| RFR | RFR | \<foreløpig ikke implementert\> |
| `SYSTem:COMMunicate:SERial:PACE` | XON \| NONE | NONE | \<foreløpig ikke implementert\> |
| `SYSTem:COMMunicate:SERial:PARity` | EVEN \| ODD \| ZERO \| ONE \| NONE \| IGNore | NONE | \<foreløpig ikke implementert\> |
| `SYSTem:DATE` | Year,month,day | | |
| `SYSTem:TIME` | Hours,minutes,seconds | | |
| `SYSTem:ERRor[:NEXT]?` | | | Kun spørring |
| `SYSTem:ERRor:ALL?` | | | Kun spørring |
| `SYSTem:KLOCk` | ON \| OFF \| REMote | OFF | |
| `SYSTem:LANGuage` | "DEFault" \| "D5255S" \| "D5255T" \| "D5255M" | "DEFault" | |
| `SYSTem:VERSion?` | | | Kun spørring |

#### `SYSTem:COMMunicate:GPIB[:SELF]:ADDRess <addr>`

**Beskrivelse:** Setter primæradressen til det valgfrie GPIB-grensesnittet.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<addr>` | 1 til 30 |

**Respons:** `<addr>`

**Eksempel:**

```
SYST:COMM:GPIB:ADDR 10
SYST:COMM:GPIB:ADDR?    Respons: 5
```

- **\*RST-tilstand:** Påvirkes ikke av `*RST`

#### `SYSTem:COMMunicate:SERial:BAUD <value>`

**Beskrivelse:** Setter baudraten for RS232-grensesnittet.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<value>` | 1200 \| 2400 \| 4800 \| 9600 \| 19200 \| 38400 \| 57600 \| 115200 |

**Respons:** `<value>`

**Eksempel:**

```
SYST:COMM:SER:BAUD 9600
SYST:COMM:SER:BAUD?     Respons: 115200
```

- **\*RST-tilstand:** Påvirkes ikke av `*RST`

#### `SYSTem:DATE <year>,<month>,<day>`

**Beskrivelse:** Setter datoen i instrumentets interne klokke.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<year>` | Må være \<numeric_value\>. Året er i firesifret numerisk format. |
| `<month>` | Må være \<numeric_value\>. Området er 1 til 12 inklusive. Tallet 1 svarer til måneden januar, 2 til februar, og så videre. |
| `<day>` | Må være \<numeric_value\>. Området er 1 til antall dager i måneden fra forrige parameter. |

**Eksempel:**

```
SYST:DATE 2001,2,5
SYST:DATE?          Respons: 2001,2,5
```

- **\*RST-tilstand:** Påvirkes ikke av reset.

#### `SYSTem:TIME <hours>,<minutes>,<seconds>`

**Beskrivelse:** Setter klokkeslettet i instrumentets interne klokke.

**Parametre:**

| Parameter | Betydning |
|---|---|
| `<hours>` | Må være \<numeric_value\>. Timene er i 24-timers notasjon. |
| `<minutes>` | Må være \<numeric_value\>. Området er 0 til 59 inklusive. |
| `<seconds>` | Må være \<numeric_value\>. Området er 0 til 59 inklusive. |

**Eksempel:**

```
SYST:TIME 15,45,23
SYST:TIME?          Respons: 15,45,23
```

- **\*RST-tilstand:** Påvirkes ikke av reset.

#### `SYSTem:ERRor[:NEXT]?`

**Beskrivelse:** Spør feil-/hendelseskøen etter neste element og fjerner det fra køen. Responsen returnerer hele køelementet bestående av et heltall og en streng. Hvis det ikke er noen feil i køen, returneres `0,"No error"`.

**Respons:**

```
<code>,<text description>
```

**Eksempel:**

```
SYST:ERR?           Respons: -100,"Command Error"
```

- **\*RST-tilstand:** Påvirkes ikke av reset.

#### `SYSTem:ERRor:ALL?`

**Beskrivelse:** Spør feil-/hendelseskøen etter alle elementer og fjerner dem fra køen. Responsen returnerer en semikolonseparert liste av hele køelementer bestående av heltall/streng-par. Hvis det ikke er noen feil i køen, returneres `0,"No error"`.

**Respons:**

```
<code>,<text description>[;<code>,<text description>[; ...]]
```

**Eksempel:**

```
SYST:ERR:ALL?
Respons: -102,"Syntax Error";-113,"Undefined Header"
```

- **\*RST-tilstand:** Påvirkes ikke av reset.

#### `SYSTem:KLOCk ON | OFF | REMote`

**Beskrivelse:** Denne kommandoen låser de lokale betjeningsorganene på instrumentet. Dette inkluderer frontpanel, tastatur og andre lokale grensesnitt.

**Parametre:**

| Parameter | Betydning |
|---|---|
| ON | Alle frontpanelkontroller er låst. |
| OFF | Alle frontpanelkontroller kan betjenes av brukeren. |
| REMote | Alle frontpanelkontroller unntatt F6/Esc låses når en fjernstyringskommando mottas. |

**Eksempel:**

```
SYST:KLOC ON
SYST:KLOC?          Respons: 1
```

- **\*RST-tilstand:** OFF

#### `SYSTem:LANGuage "DEFault" | "D5255S" | "D5255T" | "D5255M"`

**Beskrivelse:** Bytter til et annet kommandospråk. Det standard SCPI-kommandosettet forstås til enhver tid.

**Parametre:**

| Parameter | Betydning |
|---|---|
| "DEFault" | Standard SCPI-kommandosett. |
| "D5255S" | Eldre (legacy) kommandosett brukt av Norma D5255 Standard. |
| "D5255T" | Eldre (legacy) kommandosett brukt av Norma D5255 Transformer / Rectified Mean. |
| "D5255M" | Eldre (legacy) kommandosett brukt av D5255 Motor. |

**Eksempel:**

```
SYST:LANGuage "D5255S"
```

- **\*RST-tilstand:** "DEFault"

#### `SYSTem:VERSion?`

**Beskrivelse:** Denne spørringen returnerer en \<NR2\>-formatert numerisk verdi som svarer til SCPI-versjonsnummeret instrumentet er i samsvar med. Responsen har formen YYYY.V, der Y-ene representerer årsversjonen (f.eks. 1990) og V representerer et godkjent revisjonsnummer for det året.

**Respons:**

```
<version>
```

**Eksempel:**

```
SYST:VERS?          Respons: 1999.0
```

- **\*RST-tilstand:** Påvirkes ikke av reset.

### STATus-subsystemet

STATus-subsystemet inneholder kommandoene for statusrapporteringssystemet (se avsnittet "Status Reporting System"). `*RST` påvirker ikke statusregistrene.

#### Kommandooversikt (STATus)

| Kommando | Parameter | Merknad |
|---|---|---|
| `STATus:OPERation[:EVENt]?` | | Kun spørring |
| `STATus:OPERation:CONDition?` | | Kun spørring |
| `STATus:OPERation:ENABle` | 0 to 65535 | |
| `STATus:OPERation:PTRansition` | 0 to 65535 | |
| `STATus:OPERation:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable[:EVENt]?` | | Kun spørring |
| `STATus:QUEStionable:CONDition?` | | Kun spørring |
| `STATus:QUEStionable:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage[:EVENt]?` | | Kun spørring |
| `STATus:QUEStionable:VOLTage:CONDition?` | | Kun spørring |
| `STATus:QUEStionable:VOLTage:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent[:EVENt]?` | | Kun spørring |
| `STATus:QUEStionable:CURRent:CONDition?` | | Kun spørring |
| `STATus:QUEStionable:CURRent:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:NTRansition` | 0 to 65535 | |

#### `STATus:QUEStionable:VOLTage:CONDition?`

**Beskrivelse:** Returnerer innholdet i tilstandsregisteret (condition register) knyttet til statusstrukturen definert i kommandoen. Lesing av tilstandsregisteret er ikke-destruktiv. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767).

**Respons:** `<value>` er et 16-bits heltall i desimalnotasjon.

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Spenningskanaler (1, 3, 5, 7, 9, 11) overlast (overload). |
| bit 8 til 13 | Spenningskanaler (1, 3, 5, 7, 9, 11) underlast (underload). |

**Eksempel:**

```
STAT:QUES:VOLT:COND?    Respons: 2 (spenningsoverlast på fase 2)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:QUEStionable:VOLTage:PTRansition <value>`

**Beskrivelse:** Setter det positive transisjonsfilteret. Å sette en bit i det positive transisjonsfilteret gjør at en 0-til-1-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Spenningskanaler (1, 3, 5, 7, 9, 11) overlast, positiv transisjon. |
| bit 8 til 13 | Spenningskanaler (1, 3, 5, 7, 9, 11) underlast, positiv transisjon. |

**Eksempel:**

```
STAT:QUES:VOLT:PTR 16191
STAT:QUES:VOLT:PTR?     Respons: 16191
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable:VOLTage:NTRansition <value>`

**Beskrivelse:** Setter det negative transisjonsfilteret. Å sette en bit i det negative transisjonsfilteret gjør at en 0-til-1-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Spenningskanaler (1, 3, 5, 7, 9, 11) overlast, negativ transisjon. |
| bit 8 til 13 | Spenningskanaler (1, 3, 5, 7, 9, 11) underlast, negativ transisjon. |

**Eksempel:**

```
STAT:QUES:VOLT:NTR 16191
STAT:QUES:VOLT:NTR?     Respons: 16191
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable:VOLTage[:EVENt]?`

**Beskrivelse:** Denne spørringen returnerer innholdet i hendelsesregisteret (event register) knyttet til statusstrukturen definert i kommandoen. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767). Merk at lesing av hendelsesregisteret nullstiller det.

**Respons:** `<value>` er et 16-bits heltall i desimalnotasjon.

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Spenningskanaler (1, 3, 5, 7, 9, 11) overlast-hendelse. |
| bit 8 til 13 | Spenningskanaler (1, 3, 5, 7, 9, 11) underlast-hendelse. |

**Eksempel:**

```
STAT:QUES:VOLT?         Respons: 2 (spenningsoverlast på fase 2)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:QUEStionable:VOLTage:ENABle <value>`

**Beskrivelse:** Setter aktiveringsmasken (enable mask) som tillater at sanne tilstander i hendelsesregisteret rapporteres i sammendragsbiten (summary bit). Hvis en bit er 1 i enable-registeret og den tilknyttede hendelsesbiten går til sann, inntreffer en positiv transisjon i den tilknyttede sammendragsbiten. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Spenningskanaler (1, 3, 5, 7, 9, 11), aktiver overlast-hendelse. |
| bit 8 til 13 | Spenningskanaler (1, 3, 5, 7, 9, 11), aktiver underlast-hendelse. |

**Eksempel:**

```
STAT:QUES:VOLT:ENAB 16191
STAT:QUES:VOLT:ENAB?    Respons: 16191
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable:CURRent:CONDition?`

**Beskrivelse:** Returnerer innholdet i tilstandsregisteret knyttet til statusstrukturen definert i kommandoen. Lesing av tilstandsregisteret er ikke-destruktiv. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767).

**Respons:** `<value>` er et 16-bits heltall i desimalnotasjon.

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Strømkanaler (0, 2, 4, 6, 8, 10) overlast. |
| bit 8 til 13 | Strømkanaler (0, 2, 4, 6, 8, 10) underlast. |

**Eksempel:**

```
STAT:QUES:CURR:COND?    Respons: 2 (strømoverlast på fase 2)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:QUEStionable:CURRent:PTRansition <value>`

**Beskrivelse:** Setter det positive transisjonsfilteret. Å sette en bit i det positive transisjonsfilteret gjør at en 0-til-1-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Strømkanaler (0, 2, 4, 6, 8, 10) overlast, positiv transisjon. |
| bit 8 til 13 | Strømkanaler (0, 2, 4, 6, 8, 10) underlast, positiv transisjon. |

**Eksempel:**

```
STAT:QUES:CURR:PTR 16191
STAT:QUES:CURR:PTR?     Respons: 16191
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable:CURRent:NTRansition <value>`

**Beskrivelse:** Setter det negative transisjonsfilteret. Å sette en bit i det negative transisjonsfilteret gjør at en 0-til-1-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Strømkanaler (0, 2, 4, 6, 8, 10) overlast, negativ transisjon. |
| bit 8 til 13 | Strømkanaler (0, 2, 4, 6, 8, 10) underlast, negativ transisjon. |

**Eksempel:**

```
STAT:QUES:CURR:NTR 16191
STAT:QUES:CURR:NTR?     Respons: 16191
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable:CURRent[:EVENt]?`

**Beskrivelse:** Denne spørringen returnerer innholdet i hendelsesregisteret knyttet til statusstrukturen definert i kommandoen. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767). Merk at lesing av hendelsesregisteret nullstiller det.

**Respons:** `<value>` er et 16-bits heltall i desimalnotasjon.

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Strømkanaler (0, 2, 4, 6, 8, 10) hendelse. |
| bit 8 til 13 | Strømkanaler (0, 2, 4, 6, 8, 10) hendelse. |

**Eksempel:**

```
STAT:QUES:CURR?         Respons: 2 (strømoverlast på fase 2)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:QUEStionable:CURRent:ENABle <value>`

**Beskrivelse:** Setter aktiveringsmasken som tillater at sanne tilstander i hendelsesregisteret rapporteres i sammendragsbiten. Hvis en bit er 1 i enable-registeret og den tilknyttede hendelsesbiten går til sann, inntreffer en positiv transisjon i den tilknyttede sammendragsbiten. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Biter | Betydning |
|---|---|
| bit 0 til 5 | Strømkanaler (0, 2, 4, 6, 8, 10), aktiver overlast-hendelse. |
| bit 8 til 13 | Strømkanaler (0, 2, 4, 6, 8, 10), aktiver underlast-hendelse. |

**Eksempel:**

```
STAT:QUES:CURR:ENAB 16191
STAT:QUES:CURR:ENAB?    Respons: 16191
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable:CONDition?`

**Beskrivelse:** Returnerer innholdet i tilstandsregisteret knyttet til statusstrukturen definert i kommandoen. Lesing av tilstandsregisteret er ikke-destruktiv. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767).

**Respons:** `<value>` er et 16-bits heltall i desimalnotasjon.

| Bit | Betydning |
|---|---|
| bit 0 | Spenningssammendrag questionable. |
| bit 1 | Strømsammendrag questionable. |
| bit 5 | Frekvens questionable. |

**Eksempel:**

```
STAT:QUES:COND?
Respons: 1 (spenningsover-/underlast på en eller annen fase)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:QUEStionable:PTRansition <value>`

**Beskrivelse:** Setter det positive transisjonsfilteret. Å sette en bit i det positive transisjonsfilteret gjør at en 0-til-1-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Bit | Betydning |
|---|---|
| bit 0 | Spenningssammendrag questionable. |
| bit 1 | Strømsammendrag questionable. |
| bit 5 | Frekvens questionable. |

**Eksempel:**

```
STAT:QUES:PTR 35
STAT:QUES:PTR?      Respons: 35
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable:NTRansition <value>`

**Beskrivelse:** Setter det negative transisjonsfilteret. Å sette en bit i det negative transisjonsfilteret gjør at en 0-til-1-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Bit | Betydning |
|---|---|
| bit 0 | Spenningssammendrag questionable. |
| bit 1 | Strømsammendrag questionable. |
| bit 5 | Frekvens questionable. |

**Eksempel:**

```
STAT:QUES:NTR 35
STAT:QUES:NTR?      Respons: 35
```

- **\*RST-tilstand:** 0

#### `STATus:QUEStionable[:EVENt]?`

**Beskrivelse:** Denne spørringen returnerer innholdet i hendelsesregisteret knyttet til statusstrukturen definert i kommandoen. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767). Merk at lesing av hendelsesregisteret nullstiller det.

**Respons:** `<value>` er et 16-bits heltall i desimalnotasjon.

| Bit | Betydning |
|---|---|
| bit 0 | Spenningssammendrag questionable. |
| bit 1 | Strømsammendrag questionable. |
| bit 5 | Frekvens questionable. |

**Eksempel:**

```
STAT:QUES?          Respons: 1 (spenningsoverlast/-underlast på en eller annen fase)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:QUEStionable:ENABle <value>`

**Beskrivelse:** Setter aktiveringsmasken som tillater at sanne tilstander i hendelsesregisteret rapporteres i sammendragsbiten. Hvis en bit er 1 i enable-registeret og den tilknyttede hendelsesbiten går til sann, inntreffer en positiv transisjon i den tilknyttede sammendragsbiten. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Bit | Betydning |
|---|---|
| bit 0 | Spenningssammendrag questionable. |
| bit 1 | Strømsammendrag questionable. |
| bit 5 | Frekvens questionable. |

**Eksempel:**

```
STAT:QUES:ENAB 35
STAT:QUES:ENAB?     Respons: 35
```

- **\*RST-tilstand:** 0

#### `STATus:OPERation:CONDition?`

**Beskrivelse:** Returnerer innholdet i tilstandsregisteret knyttet til statusstrukturen definert i kommandoen. Lesing av tilstandsregisteret er ikke-destruktiv. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767).

**Respons:**

| Bit | Betydning |
|---|---|
| bit 2 | Ranging (bytter måleområde). |
| bit 3 | Sweeping (minneopptak pågår). |
| bit 5 | Venter på trigger. |
| bit 8 | Synkronisert (hvis sync-kilden endres, oppstår en glitch). |
| bit 10 | Midling (midling pågår; ved slutten av hver midlingssyklus oppstår en glitch). |
| bit 12 | Spektrum-CALCulation pågår. |

**Eksempel:**

```
STAT:OPER:COND?     Respons: 1280 (synkronisert og midler)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:OPERation:PTRansition <value>`

**Beskrivelse:** Setter det positive transisjonsfilteret. Å sette en bit i det positive transisjonsfilteret gjør at en 0-til-1-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret.

**Parametre:**

| Bit | Betydning |
|---|---|
| bit 2 | Ranging (bytter måleområde). |
| bit 3 | Sweeping (minneopptak pågår). |
| bit 5 | Venter på trigger. |
| bit 8 | Synkronisert (hvis sync-kilden endres, oppstår en glitch). |
| bit 10 | Midling (midling pågår; ved slutten av hver midlingssyklus oppstår en glitch). |
| bit 12 | Spektrum-CALCulation pågår. |

**Eksempel:**

```
STAT:OPER:PTR 5948
STAT:OPER:PTR?      Respons: 5948
```

- **\*RST-tilstand:** 0

#### `STATus:OPERation:NTRansition <value>`

**Beskrivelse:** Setter det negative transisjonsfilteret. Å sette en bit i det negative transisjonsfilteret gjør at en 1-til-0-overgang i tilhørende bit i det tilknyttede tilstandsregisteret fører til at 1 skrives i tilhørende bit i det tilknyttede hendelsesregisteret.

**Parametre:**

| Bit | Betydning |
|---|---|
| bit 2 | Ranging (bytter måleområde). |
| bit 3 | Sweeping (minneopptak pågår). |
| bit 5 | Venter på trigger. |
| bit 8 | Synkronisert (hvis sync-kilden endres, oppstår en glitch). |
| bit 10 | Midling (midling pågår; ved slutten av hver midlingssyklus oppstår en glitch). |
| bit 12 | Spektrum-CALCulation pågår. |

**Eksempel:**

```
STAT:OPER:NTR 5948
STAT:OPER:NTR?      Respons: 5948
```

- **\*RST-tilstand:** 0

#### `STATus:OPERation[:EVENt]?`

**Beskrivelse:** Denne spørringen returnerer innholdet i hendelsesregisteret knyttet til statusstrukturen definert i kommandoen. Responsen er (NR1 NUMERIC RESPONSE DATA) (område: 0 til og med 32767). Merk at lesing av hendelsesregisteret nullstiller det.

**Respons:** `<value>` er et 16-bits heltall i desimalnotasjon.

| Bit | Betydning |
|---|---|
| bit 2 | Ranging (bytter måleområde). |
| bit 3 | Sweeping (minneopptak pågår). |
| bit 5 | Venter på trigger. |
| bit 8 | Synkronisert (hvis sync-kilden endres, oppstår en glitch). |
| bit 10 | Midling (midling pågår; ved slutten av hver midlingssyklus oppstår en glitch). |
| bit 12 | Spektrum-CALCulation pågår. |

**Eksempel:**

```
STAT:OPER?          Respons: 2 (bytter måleområde)
```

- **\*RST-tilstand:** Har ingen effekt.

#### `STATus:OPERation:ENABle <value>`

**Beskrivelse:** Setter aktiveringsmasken som tillater at sanne tilstander i hendelsesregisteret rapporteres i sammendragsbiten. Hvis en bit er 1 i enable-registeret og den tilknyttede hendelsesbiten går til sann, inntreffer en positiv transisjon i den tilknyttede sammendragsbiten. Kommandoen aksepterer parameterverdier i begge formater i området 0 til og med 65535 (desimalt) uten feil. Spørringsresponsformatet er \<NR1\>.

**Parametre:**

| Bit | Betydning |
|---|---|
| bit 2 | Ranging (bytter måleområde). |
| bit 3 | Sweeping (minneopptak pågår). |
| bit 5 | Venter på trigger. |
| bit 8 | Synkronisert (hvis sync-kilden endres, oppstår en glitch). |
| bit 10 | Midling (midling pågår; ved slutten av hver midlingssyklus oppstår en glitch). |
| bit 12 | Spektrum-CALCulation pågår. |

**Eksempel:**

```
STAT:OPER:ENAB 5948
STAT:OPER:ENAB?     Respons: 5948
```

- **\*RST-tilstand:** 0

## Hurtigreferanse: alle kommandoer

Dette er en komplett oversikt over alle kommandoer i fjernstyrings-API-et, gruppert etter subsystem ("List of Commands Grouped by Subsystems" fra manualen). Tabellene viser kommandosyntaks, gyldige parametere og eventuelle merknader (fastvarekrav, opsjoner o.l.).

Konvensjoner:

- Store bokstaver i kommandonavnet angir SCPI-kortformen (f.eks. `CALCulate` kan skrives `CALC`).
- Deler i hakeparentes `[...]` er valgfrie.
- `|` skiller alternative verdier eller nøkkelord.
- Merknaden **n. i.** betyr *Not Implemented* (ikke implementert).

### Felleskommandoer (IEEE 488.2)

| Kommando | Parameter | Merknad |
|---|---|---|
| `*CLS` | | |
| `*ESE` | 0 to 255 | |
| `*ESR?` | | |
| `*IDN?` | | |
| `*OPC` | | |
| `*OPC?` | | |
| `*OPT?` | | |
| `*RST` | | |
| `*SRE` | 0 to 255 | |
| `*STB?` | | |
| `*WAI` | | |
| `*SAV` | 10 to 24 | |
| `*RCL` | 1, 2, 10 to 24 | |
| `*LRN?` | | |
| `*TRG` | | |

### ABORt

| Kommando | Parameter | Merknad |
|---|---|---|
| `ABORt` | | |

### CALCulate

| Kommando | Parameter | Merknad |
|---|---|---|
| `CALCulate:TRANsform:FREQuency[:STATe]` | ONCE | |
| `CALCulate:TRANsform:FREQuency:MODE` | FFT \| DFT \| STD | STD: FW V1.5 and up |
| `CALCulate:TRANsform:FREQuency:FUNCtion` | `<function list>` | |
| `CALCulate:TRANsform:FREQuency:STARt` | `<frequency>` | for FFT and DFT only |
| `CALCulate:TRANsform:FREQuency:STOP` | `<frequency>` | for FFT and DFT only |
| `CALCulate:TRANsform:FREQuency:CYCLes` | 4 \| 6 \| 8 \| 10 \| 12 | for STD only (FW V1.5 and up) |
| `CALCulate:TRANsform:FREQuency:GROuping` | COMPonent \| HARMonic \| HGRoup \| HSGRoup \| ISGRoup \| SGRoup | for STD only (FW V1.5 and up) |
| `CALC:DATA?` | `[<count>[,<offset>]]` | |
| `CALC:DATA:PREamble?` | | |
| `CALC:DATA:THD?` | | for STD only (FW V1.5 and up) |
| `CALCulate:INTegral[:STATe]` | ON \| OFF | |
| `CALCulate:INTegral:CLEar[:IMMediate]` | | |
| `CALCulate:INTegral:CLEar:AUTO` | ON \| OFF | |
| `CALCulate:INTegral:STARt:SOURce` | CMD \| TIME \| MAN | |
| `CALCulate:INTegral:STARt[:IMMediate]` | | |
| `CALCulate:INTegral:STARt:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `CALCulate:INTegral:STOP:SOURce` | CMD \| TIME \| MAN \| TINTerval | |
| `CALCulate:INTegral:STOP[:IMMediate]` | | |
| `CALCulate:INTegral:STOP:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `CALCulate:INTegral:STOP:TINTerval` | 1.0e-3 to 9.99e+6 | |
| `CALCulate:HARMonic:ORDer` | `<order>` | |
| `CALCulate:POWer:CORRected` | STAR \| DELTa | FW V1.4 and up |
| `CALCulate:POWer:EFFiciency:REFerence` | `<function1>, <function2>` | |

### DISPlay

| Kommando | Parameter | Merknad |
|---|---|---|
| `DISPlay[:WINDow][:STATe]` | ON \| OFF | |
| `DISPlay:USER:FUNCtion` | `<function list>` | |

### FORMat

| Kommando | Parameter | Merknad |
|---|---|---|
| `FORMat[:DATA]` | ASCii \| REAL, [0..8] \| [32 \| 64] | |
| `FORMat[:DATA]:STATus` | ASCii \| INTeger, [8] \| 16 \| 32 | |
| `FORMat:BORDer` | NORMal \| SWAPped | |
| `FORMat:TRANspose` | ON \| OFF | |

### HCOPy

| Kommando | Parameter | Merknad |
|---|---|---|
| `HCOPy:SDUMp:DATA?` | | |

### INITiate

| Kommando | Parameter | Merknad |
|---|---|---|
| `INITiate:CONTinuous` | ON \| OFF | |
| `INITiate[:IMMediate]` | | |
| `INITiate[:IMMediate]:SEQuence1` / `INITiate[:IMMediate]:NAME STARt` | | |
| `INITiate[:IMMediate]:SEQuence2` / `INITiate[:IMMediate]:NAME STOP` | | |

### INPut

| Kommando | Parameter | Merknad |
|---|---|---|
| `INPut[1..12]:COUPling` | AC \| DC | |
| `INPut[1\|2\|3\|4\|5\|6\|7\|8\|9\|10\|11\|12]:GAIN` | 1.0e-7 to 1.0e+7 | |
| `INPut[1..12]:FILTer[:STATe]` | ON \| OFF | |
| `INPut[1..12]:FILTer[:LPASs]:FREQuency?` | | |
| `INPut[1\|2\|3\|4\|5\|6\|7\|8\|9\|10\|11\|12]:SHUNt` | INTernal \| EXTernal | |
| `INPut[21..28]:TYPe` | VOLTage \| FREQuency | Option PI1 |

### OUTPut

| Kommando | Parameter | Merknad |
|---|---|---|
| `OUTPut9[:STATe]` | ON \| OFF | |

### ROUTe

| Kommando | Parameter | Merknad |
|---|---|---|
| `ROUTe:SYSTem` | "3W" \| "2W" | |

### SENSe

| Kommando | Parameter | Merknad |
|---|---|---|
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC\|[:DC]:RANGe[:UPPer]` | 0.3 to 1000.0 V, 0.03 to 10.0 A, 0.03 to 10 V | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC[\|:DC]:SCALe` | 0.9 to 1.0e+7 | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC[\|:DC]:RANGe[:UPPer]:LIST?` | | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC[\|:DC]:RANGe[:UPPer]:AUTO` | ON \| OFF | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6][\|:POWer]:AC[\|:DC]:APERture` | 0.015 to 3600.0 s | |
| `[SENSe:]SWEep:FREQuency?` | | |
| `[SENSe:]FUNCtion[:ON]` | `<function>{,<function>}` | |
| `[SENSe:]FUNCtion[:ON]:ALL` | | |
| `[SENSe:]FUNCtion:OFF:ALL` | | |
| `[SENSe:]FUNCtion:CONCurrent` | ON \| OFF | |
| `[SENSe:]FUNCtion[:ON]:COUNt?` | | |
| `[SENSe:]DATA?` | `[<function>{,<function...>}]` | |
| `[SENSe:]DATA:STATus?` | `[<function>{,<function...>}]` | |
| `[SENSe:]SWEep1\|2:TIME` | `<value>` \| MAX | |
| `[SENSe:]SWEep1\|2:OFFSet:TIME` | `<value>` \| MAX | |
| `[SENSe:]SWEep1\|2:POINTS?` | | |
| `[SENSe:]SWEep1\|2:OFFSet:POINTS?` | | |
| `[SENSe:]SWEep1\|2[:STATe]` | ON \| OFF | |
| `[SENSe:]SWEep1\|2:COUNt` | `<count>` | |
| `[SENSe:]SWEep1\|2:SFACtor` | 1 to 65535 | |
| `[SENSe:]SWEep1\|2:FUNCtion` | `<function list>` | |

### SENSe2 (motor-/prosessgrensesnitt, Option PI1)

| Kommando | Parameter | Merknad |
|---|---|---|
| `SENSe2:TORQue[1..4]:VOLTage:SCALe` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:VOLTage:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:VOLTage:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:TORQue[1..4]:FREQuency:SCALe` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:FREQuency:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:FREQuency:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:SPEed[1..4]:VOLTage:SCALe[:DEFault]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:VOLTage:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:VOLTage:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:SCALe:PULSe` | 1 to 100000 | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:TYPe[1..4]` | MOTor \| GENerator | Option PI1 |
| `SENSe2:POLepairs[1..4]` | 1 to 999 | Option PI1 |
| `SENSe2:REFerence[1..4][:POWer]` | "POWer[1..6][:ACTive]" | Option PI1 |

### SOURce (analoge utganger, Option PI1)

| Kommando | Parameter | Merknad |
|---|---|---|
| `SOURce:VOLTage[1..4]:MODE` | FIXed \| VARiable | Option PI1 |
| `SOURce:VOLTage[1..4][:LEVel][:IMMediate][:AMPLitude]` | -10.3 to 10.3 | Option PI1 |
| `SOURce:VOLTage[1..4]:FEED` | `<function>` | Option PI1 |
| `SOURce:VOLTage[1..4]:GAIN` | -1e6 to 1e6 | Option PI1 |
| `SOURce:VOLTage[1..4]:ZERO` | -1e6 to 1e6 | Option PI1 |

### SYNC

| Kommando | Parameter | Merknad |
|---|---|---|
| `SYNC:STATe` | ON \| OFF | |
| `SYNC:SOURce` | VOLTage[1..6] \| CURRent[1..6] \| EXTernal | |
| `SYNC[:SOURce][\|CURRent[1..6]\|VOLTage[1..6]]:LEVel` | (-150% to 150% of nominal input range) | |
| `SYNC:LEVel:UNIT` | ABSolute \| PCT | |
| `SYNC[:SOURce]\|CURRent[1..6]\|VOLTage[1..6]:SLOPe` | POSitive \| NEGative | SOUR only |
| `SYNC[:SOURce]\|CURRent[1..6]\|VOLTage[1..6]:FILTer:[LPASs[:STATe]]` | ON \| OFF | SOUR only |
| `SYNC[:SOURce]\|CURRent[1..6]\|VOLTage[1..6]:FILTer:[LPASs]:FREQuency` | 100Hz, 1kHz, 10kHz | SOUR only |
| `SYNC:TIMeout` | 0.015 to 3600 s | |

### TIMer

| Kommando | Parameter | Merknad |
|---|---|---|
| `TIMer:RESet` | | |
| `TIMer:RESet:AUTO` | ON \| OFF | n. i. |
| `TIMer:RESet:TIME?` | | |

### TRACe

| Kommando | Parameter | Merknad |
|---|---|---|
| `TRACe[:DATA]:PREamble?` | `<block>` | |
| `TRACe[:DATA]?` | `[<block> [,<count> [,<offset> [,<sparsing>[,opt_level]]]]]` | |
| `TRACe[:DATA]:STATus?` | `[<block> [,<count> [,<offset> [,<sparsing>]]]]` | |
| `TRACe:FREE?` | | |
| `TRACe:CATalog:LENgth?` | | |
| `TRACe:DELete:ALL` | | |

### TRIGger

| Kommando | Parameter | Merknad |
|---|---|---|
| `TRIGger:STARt:SOURce` | BUS \| TIME \| IMMediate \| MANual \| SYNC \| `<function>` | |
| `TRIGger:STARt:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `TRIGger:STARt:LEVel` | `<level>` | |
| `TRIGger:STARt:SLOPe` | POSitive \| NEGative | |
| `TRIGger:STOP:SOURce` | TIME \| IMMediate \| MANual \| `<function>` | |
| `TRIGger:STOP:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `TRIGger:STOP:LEVel` | `<level>` | |
| `TRIGger:STOP:SLOPe` | POSitive \| NEGative | |

### SYSTem

| Kommando | Parameter | Merknad |
|---|---|---|
| `SYSTem:COMMunicate:GPIB[:SELF]:ADDRess` | 1 to 30 | |
| `SYSTem:COMMunicate:SERial:BAUD` | 1200 to 115200 | |
| `SYSTem:DATE` | `<year>,<month>,<day>` | |
| `SYSTem:TIME` | `<hours>,<minutes>,<seconds>` | |
| `SYSTem:ERRor[:NEXT]?` | | |
| `SYSTem:ERRor:ALL?` | | |
| `SYSTem:KLOCk` | ON \| OFF \| REMote | |
| `SYSTem:LANGuage` | "DEFault" \| "D5255S" \| "D5255T" \| "D5255M" | |
| `SYSTem:VERSion?` | | |

### STATus

| Kommando | Parameter | Merknad |
|---|---|---|
| `STATus:QUEStionable:VOLTage:CONDition?` | | |
| `STATus:QUEStionable:VOLTage:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage[:EVENt]?` | | |
| `STATus:QUEStionable:VOLTage:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:CONDition?` | | |
| `STATus:QUEStionable:CURRent:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent[:EVENt]?` | | |
| `STATus:QUEStionable:CURRent:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:CONDition?` | | |
| `STATus:QUEStionable:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable[:EVENt]?` | | |
| `STATus:QUEStionable:ENABle` | 0 to 65535 | |
| `STATus:OPERation:CONDition?` | | |
| `STATus:OPERation:PTRansition` | 0 to 65535 | |
| `STATus:OPERation:NTRansition` | 0 to 65535 | |
| `STATus:OPERation[:EVENt]?` | | |
| `STATus:OPERation:ENABle` | 0 to 65535 | |

*n. i. – Not Implemented (ikke implementert).*

## Statusrapporteringssystemet

### Innledning

Statusrapporteringssystemet lagrer all informasjon om instrumentets nåværende driftstilstand, for eksempel om feil som har oppstått. Informasjonen lagres i statusregistre og i en feilkø (error queue). Både statusregistrene og feilkøen kan spørres via IEC/IEEE-bussen.

Informasjonen er hierarkisk strukturert:

- Det høyeste nivået utgjøres av statusbyten (**STB**, Status Byte) definert i IEEE 488.2, med tilhørende maskeregister **SRE** (Service Request Enable).
- STB mottar informasjon fra det standardiserte **ESR** (Event Status Register), også definert i IEEE 488.2, med tilhørende maskeregister **ESE** (Event Status Enable).
- STB mottar i tillegg informasjon fra registrene **STATus:OPERation** og **STATus:QUEStionable**, som er definert av SCPI og inneholder detaljert informasjon om instrumentet.

Utgangsbufferet (output buffer) inneholder meldingene instrumentet returnerer til kontrolleren. Utgangsbufferet er ikke en del av statusrapporteringssystemet, men bestemmer verdien av MAV-biten i STB-registeret.

### Oppbygning av et SCPI-statusregister

Hvert SCPI-register består av fem deler, hver 16 bit bred, med ulike funksjoner. De enkelte bitene er uavhengige av hverandre. Hver maskinvarestatus er tilordnet et bitnummer som gjelder for alle fem delene. For eksempel er bit 3 i STATus:OPERation-registeret tilordnet maskinvarestatusen "Wait for trigger" i alle fem delene. Bit 15 (den mest signifikante biten) settes til null i alle fem delene, slik at kontrolleren kan behandle registerinnholdet som et positivt heltall.

#### CONDition-delen

CONDition-delen skrives direkte til av maskinvaren eller av sum-biten fra det neste lavere registeret. Innholdet gjenspeiler instrumentets nåværende status. Denne registerdelen kan bare leses, ikke skrives til eller nullstilles. Lesing påvirker ikke innholdet.

#### PTRansition-delen

PTRansition-delen (Positive Transition) fungerer som en flankedetektor. Hvis en bit i CONDition-delen endres fra 0 til 1, avgjør tilstanden til den tilhørende PTR-biten om EVENt-biten settes til 1:

- PTR-bit = 1: EVENt-biten settes.
- PTR-bit = 0: EVENt-biten settes ikke.

Denne delen kan både skrives til og leses. Lesing påvirker ikke innholdet.

#### NTRansition-delen

NTRansition-delen (Negative Transition) fungerer likeledes som en flankedetektor. Hvis en bit i CONDition-delen endres fra 1 til 0, avgjør tilstanden til den tilhørende NTR-biten om EVENt-biten settes til 1:

- NTR-bit = 1: EVENt-biten settes.
- NTR-bit = 0: EVENt-biten settes ikke.

Denne delen kan både skrives til og leses. Lesing påvirker ikke innholdet.

Med disse to flankeregisterdelene kan brukeren definere hvilken statusovergang i CONDition-delen (ingen, 0 til 1, 1 til 0 eller begge) som skal lagres i EVENt-delen.

#### EVENt-delen

EVENt-delen angir om en hendelse har inntruffet siden den sist ble lest; den er "hukommelsen" til CONDition-delen. Den viser kun de hendelsene som er sluppet gjennom av flankefiltrene. EVENt-delen oppdateres kontinuerlig av instrumentet. Denne delen kan bare leses. Ved lesing nullstilles innholdet. I dagligtale omtales EVENt-delen ofte som synonymt med hele registeret.

#### ENABle-delen

ENABle-delen bestemmer om den tilhørende EVENt-biten bidrar til sum-biten (se nedenfor). Hver bit i EVENt-delen AND-es med den tilhørende ENABle-biten (symbol &). Resultatene av alle logiske operasjoner i denne delen føres videre til sum-biten via en OR-funksjon (symbol +):

- ENABle-bit = 0: den tilhørende EVENt-biten bidrar ikke til sum-biten.
- ENABle-bit = 1: hvis den tilhørende EVENt-biten er 1, settes sum-biten også til 1.

Denne delen kan både skrives til og leses. Lesing påvirker ikke innholdet.

#### Sum-biten

Sum-biten dannes, som nevnt over, fra EVENt-delen og ENABle-delen for hvert register. Resultatet føres inn som en bit i CONDition-delen i det neste høyere registeret.

Instrumentet genererer automatisk en sum-bit for hvert register. Dermed er det sikret at en hendelse, for eksempel en PLL som ikke har låst, kan utløse en service request gjennom alle hierarkiske nivåer.

> **Merk:** Service request enable-registeret (SRE) definert i IEEE 488.2 kan betraktes som ENABle-delen av STB når STB er strukturert i samsvar med SCPI. Tilsvarende kan ESE betraktes som ENABle-delen av ESR.

### Oversikt over statusregistrene

Hierarkiet i statusrapporteringsstrukturen (minimumsstrukturen som kreves av SCPI) er som følger:

- **Status Byte (STB)** — øverste nivå. Mottar sum-biter fra:
  - **Standard Event Status Register (ESR)** — bit 0 Operation Complete, bit 1 Not used, bit 2 Query Error, bit 3 Device Dependent Error, bit 4 Execution Error, bit 5 Command Error, bit 6 User Request, bit 7 Power On.
  - **OPERation Status**-registeret — bit 0–1 Reserved, bit 2 RANGing, bit 3 SWEeping, bit 4 MEASuring, bit 5 Waiting for TRIGger Summary, bit 6–7 Reserved, bit 8 SYNChronized, bit 9 Sync available (reserved), bit 10 Averaging, bit 11 Reserved, bit 12 CALCulation, bit 13–14 Reserved, bit 15 NOT USED*.
  - **QUEStionable Status**-registeret — som igjen mottar sum-biter fra underregistrene:
    - **QUEStionable:CURRent** — bit 0–5 INPut1/3/5/7/9/11 overload, bit 6–7 Reserved, bit 8–13 INPut1/3/5/7/9/11 underload, bit 14 Reserved, bit 15 NOT USED*.
    - **QUEStionable:VOLTage** — bit 0–5 INPut2/4/6/8/10/12 overload, bit 6–7 Reserved, bit 8–13 INPut2/4/6/8/10/12 underload, bit 14 Reserved, bit 15 NOT USED*.
  - **Error/Event Queue** (feilkøen) — styrer bit 2 i STB.
  - **Output Buffer** (utgangsbufferet) — styrer MAV-biten i STB.

Bit 6 i STB er RQS/MSS (utløser SRQ på bussen).

\* Bruk av bit 15 er ikke tillatt, siden enkelte kontrollere kan ha problemer med å lese et 16-bits heltall uten fortegn. Verdien av denne biten skal alltid være 0.

### Beskrivelse av statusregistrene

#### Status Byte (STB) og Service Request Enable Register (SRE)

STB er definert i IEEE 488.2. Det gir en grov oversikt over instrumentstatusen ved å samle informasjonen fra de lavere registrene. Det kan sammenlignes med CONDition-delen av et SCPI-register og utgjør det høyeste nivået i SCPI-hierarkiet. En spesiell egenskap er at bit 6 fungerer som sum-bit for de øvrige bitene i statusbyten.

Statusbyten leses med kommandoen `*STB?` eller via serial poll.

STB har et tilhørende SRE. SRE tilsvarer funksjonelt ENABle-delen i SCPI-registrene. Hver bit i STB er tilordnet en bit i SRE. Bit 6 i SRE ignoreres. Hvis en bit er satt i SRE og den tilhørende biten i STB endres fra 0 til 1, genereres en service request (SRQ) på IEC/IEEE-bussen, som utløser et interrupt i kontrolleren (hvis kontrolleren er konfigurert for det) og kan viderebehandles der.

SRE settes med kommandoen `*SRE` og leses med kommandoen `*SRE?`.

**Tabell 2-1. Status Register Bits**

| Bit nr. | Beskrivelse |
|---|---|
| 2 | **Error Queue Not Empty** — Denne biten settes når det gjøres en oppføring i feilkøen. Hvis biten er aktivert via SRE, genererer hver oppføring i feilkøen en service request. En feil kan da gjenkjennes og undersøkes nærmere ved å spørre feilkøen. Spørringen gir en informativ feilmelding. Denne fremgangsmåten anbefales, siden den reduserer problemene ved IEC/IEEE-buss-styring. |
| 3 | **QUEStionable Status sum bit** — Denne biten settes hvis en EVENt-bit er satt i QUEStionable-statusregisteret og den tilhørende ENABle-biten er satt til 1. En satt bit indikerer en tvilsom (questionable) instrumenttilstand som kan undersøkes nærmere ved å spørre QUEStionable-statusregisteret. |
| 4 | **MAV bit (Message AVailable)** — Denne biten settes hvis det finnes en melding i utgangsbufferet som kan leses. Biten kan brukes til automatisk lesing av data fra instrumentet til kontrolleren (se kapittelet med programeksempler, kapittel 6 i manualen). |
| 5 | **ESB bit** — Sum-bit for event status-registeret. Den settes hvis en av bitene i event status-registeret er satt og aktivert i event status enable-registeret. En satt bit indikerer en alvorlig feil som kan undersøkes nærmere ved å spørre event status-registeret. |
| 6 | **MSS bit (Master Status Summary bit)** — Denne biten settes hvis instrumentet utløser en service request. Det skjer når en av de andre bitene i registeret er satt sammen med sin maskebit i service request enable-registeret (SRE). |
| 7 | **OPERation Status Register sum bit** — Denne biten settes hvis en EVENt-bit er satt i OPERation-statusregisteret og den tilhørende ENABle-biten er satt til 1. En satt bit indikerer at instrumentet utfører en handling. Typen handling kan bestemmes ved å spørre OPERation-statusregisteret. |

#### Event Status Register (ESR) og Event Status Enable Register (ESE)

ESR er definert i IEEE 488.2. Det kan sammenlignes med EVENt-delen av et SCPI-register. Event status-registeret leses med kommandoen `*ESR?`. ESE er den tilhørende ENABle-delen. Det settes med kommandoen `*ESE` og leses med kommandoen `*ESE?`.

**Tabell 2-2. Event Status Register Bits**

| Bit nr. | Beskrivelse |
|---|---|
| 0 | **Operation Complete** — Denne biten settes ved mottak av kommandoen `*OPC` når alle foregående kommandoer er utført. |
| 1 | Denne biten brukes ikke. |
| 2 | **Query Error** — Denne biten settes hvis kontrolleren vil lese data fra instrumentet uten å ha sendt en spørring, eller hvis den ikke henter forespurte data og i stedet sender nye instruksjoner til instrumentet. Årsaken er ofte en feilaktig spørring som dermed ikke kan utføres. |
| 3 | **Device-Dependent Error** — Denne biten settes hvis en enhetsavhengig feil oppstår. En feilmelding med et nummer mellom -300 og -399, eller et positivt feilnummer som beskriver feilen nærmere, legges inn i feilkøen (se kapittel 5 i manualen). |
| 4 | **Execution Error** — Denne biten settes hvis en mottatt kommando er syntaktisk korrekt, men ikke kan utføres av andre grunner. En feilmelding med et nummer mellom -200 og -300, som beskriver feilen nærmere, legges inn i feilkøen (se kapittel 5 i manualen). |
| 5 | **Command Error** — Denne biten settes hvis det mottas en kommando som er udefinert eller syntaktisk ukorrekt. En feilmelding med et nummer mellom -100 og -200, som beskriver feilen nærmere, legges inn i feilkøen (se kapittel 5 i manualen). |
| 6 | **User Request** — Denne biten settes når [LOCAL]-tasten trykkes og instrumentet settes i manuell styring. (Manualen angir også at denne biten ikke brukes.) |
| 7 | **Power On (AC supply voltage On)** — Denne biten settes når instrumentet slås på. |

#### STATus:OPERation-registeret

I CONDition-delen inneholder dette registeret informasjon om hvilke handlinger instrumentet er i ferd med å utføre, og i EVENt-delen informasjon om hvilke handlinger instrumentet har utført siden siste lesing. Registeret leses med kommandoene:

```
STATus:OPERation:CONDition?
STATus:OPERation[:EVENt]?
```

**Tabell 2-3. STATus:OPERation Register Bits**

| Bit nr. | Beskrivelse |
|---|---|
| 0 til 1 | Disse bitene brukes ikke. |
| 2 | **RANGing** — Denne biten er satt mens instrumentet skifter måleområde på inngangskanalene i autorange-modus. |
| 3 | **SWEeping** — Når minneopptak (memory recording) pågår, settes denne biten til 1. Under fylling av pretrigger eller mens instrumentet venter på trigger, er biten ikke satt. |
| 4 | Brukes ikke. |
| 5 | **Waiting for TRIGger Summary** — Når minneopptak venter på trigger etter at det er startet med `INITiate[:IMMediate]:SEQuence1` eller `INITiate:CONTinuous:SEQuence1 ON`, er denne biten satt. Den nullstilles når triggeren ankommer. |
| 6 til 7 | Disse bitene brukes ikke. |
| 8 | **SYNChronized** — Denne biten settes når instrumentet er synkronisert til en gyldig SYNC-kilde. Biten settes til 0 når `SYNC:STATe` settes til OFF. |
| 9 | **SYNChronization Available (reserved)** — Denne biten settes hvis det finnes et gyldig synkroniseringssignal på minst én inngangskanal. (Manualen angir også at denne biten ikke brukes.) |
| 10 | **AVERaging** — Denne biten settes når instrumentet behandler sin midlingssyklus (averaging cycle). I free-run-modus settes biten til 0 i et kort tidsrom ved slutten av hver midlingssyklus. |
| 11 | Denne biten brukes ikke. |
| 12 | **CALCulation** — Denne biten settes til 1 hvis beregningen pågår. Når beregningen er fullført, settes biten til 0. |
| 13 til 14 | Disse bitene brukes ikke. |
| 15 | Denne biten er alltid 0. |

#### STATus:QUEStionable-registeret

Dette registeret inneholder informasjon om ubestemte tilstander som kan oppstå hvis enheten brukes utenfor spesifikasjonene. Det kan spørres med kommandoene:

```
STATus:QUEStionable:CONDition?
STATus:QUEStionable[:EVENt]?
```

**Tabell 2-4. STATus:QUEStionable Register Bits**

| Bit nr. | Beskrivelse |
|---|---|
| 0 | QUEStionable:VOLTage Register Summary. |
| 1 | QUEStionable:CURRent Register Summary. |
| 2 til 4 | Disse bitene brukes ikke. |
| 5 | **FREQuency** — Biten settes hvis frekvensmålingen er ugyldig på grunn av dårlig signalkvalitet. |
| 6 til 14 | Disse bitene brukes ikke. |
| 15 | Denne biten er alltid 0. |

#### STATus:QUEStionable:CURRent-registeret

Dette registeret inneholder informasjon om overload-/underload-tilstander som kan oppstå hvis måleområdet på en strøminngangskanal overskrides eller inngangssignalet er for lavt. Det kan spørres med kommandoene:

```
STATus:QUEStionable:CURRent:CONDition?
STATus:QUEStionable:CURRent[:EVENt]?
```

**Tabell 2-5. STATus:QUEStionable:CURRent Register Bits**

| Bit nr. | Beskrivelse |
|---|---|
| 0 | INPut1 OVERrange |
| 1 | INPut3 OVERrange |
| 2 | INPut5 OVERrange |
| 3 | INPut7 OVERrange |
| 4 | INPut9 OVERrange |
| 5 | INPut11 OVERrange |
| 6 til 7 | Disse bitene brukes ikke. |
| 8 | INPut1 UNDERrange |
| 9 | INPut3 UNDERrange |
| 10 | INPut5 UNDERrange |
| 11 | INPut7 UNDERrange |
| 12 | INPut9 UNDERrange |
| 13 | INPut11 UNDERrange |
| 14 | Denne biten brukes ikke. |
| 15 | Denne biten er alltid 0. |

#### STATus:QUEStionable:VOLTage-registeret

Dette registeret inneholder informasjon om overload-/underload-tilstander som kan oppstå hvis måleområdet på en spenningsinngangskanal overskrides eller inngangssignalet er for lavt. Det kan spørres med kommandoene:

```
STATus:QUEStionable:VOLTage:CONDition?
STATus:QUEStionable:VOLTage[:EVENt]?
```

**Tabell 2-6. STATus:QUEStionable:VOLTage Register Bits**

| Bit nr. | Beskrivelse |
|---|---|
| 0 | INPut2 OVERrange |
| 1 | INPut4 OVERrange |
| 2 | INPut6 OVERrange |
| 3 | INPut8 OVERrange |
| 4 | INPut10 OVERrange |
| 5 | INPut12 OVERrange |
| 6 til 7 | Disse bitene brukes ikke. |
| 8 | INPut2 UNDERrange |
| 9 | INPut4 UNDERrange |
| 10 | INPut6 UNDERrange |
| 11 | INPut8 UNDERrange |
| 12 | INPut10 UNDERrange |
| 13 | INPut12 UNDERrange |
| 14 | Denne biten brukes ikke. |
| 15 | Denne biten er alltid 0. |

### Praktisk bruk av statusrapporteringssystemet

For å bruke statusrapporteringssystemet effektivt må informasjonen som ligger der overføres til kontrolleren og viderebehandles der. Det finnes flere metoder, beskrevet nedenfor. Detaljerte programeksempler finnes i kapittel 6 i manualen.

#### Service request — bruk av hierarkistrukturen (kun GPIB)

Under visse omstendigheter kan instrumentet sende en service request (SRQ) til kontrolleren. Vanligvis utløser denne service requesten et interrupt i kontrolleren, som styreprogrammet kan reagere på med passende handlinger. En SRQ utløses alltid når én eller flere av bitene 2, 3, 4, 5 eller 7 i statusbyten er satt og aktivert i SRE. Hver av disse bitene sammenfatter informasjonen fra et underliggende register, feilkøen eller utgangsbufferet. Ved passende innstilling av ENABle-delene i statusregistrene kan man oppnå at vilkårlige biter i et vilkårlig statusregister utløser en SRQ. For å utnytte mulighetene i service request fullt ut, bør alle biter settes til 1 i enable-registrene SRE og ESE.

**Eksempel: bruk av kommandoen `*OPC` for å generere en SRQ.** Mens programmet venter på SRQ, kan det utføre andre oppgaver:

- Sett bit 0 i ESE (Operation Complete)
- Sett bit 5 i SRE (ESB)

Når innstillingene er fullført, genererer instrumentet en SRQ.

SRQ er den eneste muligheten instrumentet har til å bli aktivt på eget initiativ. Ethvert kontrollerprogram bør konfigurere instrumentet slik at en service request utløses ved feilfunksjon, og programmet bør reagere hensiktsmessig på den. Et detaljert eksempel på en service request-rutine finnes i kapittel 6 i manualen.

**Eksempel: indikere slutten på en midlingssyklus med en SRQ via bit 10 i STATus:OPERation-registeret.** Mens programmet venter på SRQ, kan det utføre andre oppgaver:

- Sett bit 7 i SRE (sum-bit for STATus:OPERation-registeret)
- Sett bit 10 i STATus:OPERation:ENABle-registeret (Averaging)
- Sett bit 10 i STATus:OPERation:NTRansition for å sikre at overgangen for averaging-bit 10 fra 1 til 0 (Averaging) også lagres i EVENt-registeret. Kall av `*CLS`-kommandoen setter alle biter i NTRansition og PTRansition til 1, slik at enhver bitendring registreres. Å aktivere enable-biten, i dette tilfellet bit 10, vil normalt være tilstrekkelig.

Når midlingssyklusen er fullført, genererer instrumentet en SRQ.

#### Serial poll (kun GPIB)

Ved serial poll spørres statusbyten til et instrument, akkurat som med kommandoen `*STB?`. Spørringen realiseres imidlertid via grensesnittmeldinger (interface messages) og er raskere. Serial poll-metoden er definert allerede i IEEE 488.1 og var tidligere den eneste standardiserte muligheten for å polle statusbyten på tvers av ulike instrumenter. Metoden fungerer også med instrumenter som ikke følger SCPI eller IEEE 488.2.

VISA-funksjonen for å utføre serial poll er `viReadSTB`. Serial poll brukes hovedsakelig for å få en rask oversikt over tilstanden til flere instrumenter koblet til IEC-bussen (GPIB).

#### Spørring via kommandoer

Hver del av hvert statusregister kan leses med spørringer (queries). De enkelte kommandoene er angitt i den detaljerte beskrivelsen av statusregistrene over. Det som returneres er alltid et tall som representerer bitmønsteret i registeret som spørres. Evaluering av dette tallet gjøres av kontrollerprogrammet.

Spørringer brukes vanligvis etter en SRQ for å få mer detaljert informasjon om årsaken til SRQ-en.

#### Error-queue-spørring

Hver feiltilstand i instrumentet fører til en oppføring i feilkøen. Oppføringene i feilkøen er detaljerte feilmeldinger i klartekst som kan vises i ERROR-menyen via manuell betjening, eller spørres via IEC-bussen med kommandoen:

```
SYSTem:ERRor?
```

Hvert kall av `SYSTem:ERRor?` henter én oppføring fra feilkøen. Når det ikke lenger er lagret noen feilmeldinger, svarer instrumentet med `0, "No error"`.

Feilkøen bør spørres etter hver SRQ i kontrollerprogrammet, siden oppføringene beskriver feilårsaken mer presist enn statusregistrene. Spesielt i testfasen av et kontrollerprogram bør feilkøen spørres regelmessig, siden også feilaktige kommandoer fra kontrolleren til instrumentet registreres der.

### Tilbakestilling av statusrapporteringssystemet

Tabell 2-7 viser de ulike kommandoene og hendelsene som fører til at statusrapporteringssystemet tilbakestilles. Ingen av kommandoene, bortsett fra `*RST` og `SYSTem:PRESet`, påvirker instrumentets funksjonelle innstillinger. Spesielt endrer DCL ikke instrumentinnstillingene.

**Tabell 2-7. Resetting Instrument Functions**

| Effekt | Slå på forsyningsspenning | DCL, SDC (Device Clear, Selected Device Clear) | `*RST` | `*CLS` |
|---|---|---|---|---|
| Nullstill STB, ESR | ja | — | — | ja |
| Nullstill SRE, ESE | ja | — | — | — |
| Nullstill EVENt-delene av registrene | ja | — | — | ja |
| Nullstill ENABle-delene av alle OPERation- og QUEStionable-registre | ja | — | — | — |
| Fyll PTRansition-delene med 1, nullstill NTRansition-delene | ja | — | — | — |
| Tøm feilkøen | ja | — | — | ja |
| Tøm utgangsbufferet | ja | ja | 1) | 1) |
| Nullstill kommandobehandling og inngangsbuffer | ja | ja | — | — |

1) Enhver kommando som står først i en kommandolinje, dvs. umiddelbart etter en `<PROGRAM MESSAGE TERMINATOR>`, tømmer utgangsbufferet.

## Feilmeldinger

### Innledning

Feilmeldinger legges inn i feil-/hendelseskøen (error/event queue) i statusrapporteringssystemet når instrumentet er i fjernstyringsmodus, og kan leses ut med kommandoen `SYSTem:ERRor?`. Instrumentets svarformat på kommandoen er:

```
<error code>, "<error description>;<remote control command concerned>"
```

Angivelsen av fjernstyringskommandoen med semikolon foran er valgfri.

**Eksempel**

Kommandoen `TEST:COMMAND` gir følgende svar på spørringen `SYSTem:ERRor?`:

```
-113,"Undefined header;TEST:COMMAND"
```

Listene nedenfor beskriver feiltekstene som vises på instrumentet. Det skilles mellom feilmeldinger definert av SCPI, som er merket med negative feilkoder, og de enhetsspesifikke feilmeldingene, som bruker positive feilkoder.

I tabellene nedenfor står feilteksten som legges inn i feil-/hendelseskøen, og som kan leses ut med spørringen `SYSTem:ERRor?`, sammen med en kort forklaring av feilårsaken. Venstre kolonne inneholder den tilhørende feilkoden.

Hendelser som genererer kommandofeil (Command Errors) skal ikke generere utførelsesfeil (Execution Errors), enhetsspesifikke feil (Device-Specific Errors) eller spørringsfeil (Query Errors); se de øvrige feildefinisjonene i dette kapittelet.

### Command Error (kommandofeil)

Et `<error/event number>` i området **[-199, -100]** angir at instrumentets parser har oppdaget en IEEE 488.2-syntaksfeil. Enhver feil i denne klassen fører til at kommandofeilbiten (bit 5) i event status-registeret (IEEE 488.2, avsnitt 11.5.1) settes. Én av følgende hendelser har inntruffet:

- Parseren har oppdaget en IEEE 488.2-syntaksfeil, dvs. at en melding fra kontrolleren til enheten bryter med IEEE 488.2-standarden. Mulige brudd inkluderer et dataelement som bryter med enhetens lytteformater, eller hvis type ikke aksepteres av enheten.
- En ukjent header ble mottatt. Ukjente headere omfatter feilaktige enhetsspesifikke headere og feilaktige eller ikke-implementerte IEEE 488.2 common commands.

| Feilkode | Feiltekst | Forklaring |
|---|---|---|
| -100 | **Command error** | Generisk syntaksfeil for enheter som ikke kan oppdage mer spesifikke feil. Koden angir kun at en Command Error som definert i IEEE 488.2, 11.5.1.1.4 har oppstått. |
| -101 | **Invalid character** | Et syntaktisk element inneholder et tegn som er ugyldig for den typen; for eksempel en header som inneholder et og-tegn, `SETUP&`. |
| -102 | **Syntax error** | En ukjent kommando eller datatype ble påtruffet; for eksempel at en streng ble mottatt når enheten ikke aksepterer strenger. |
| -103 | **Invalid separator** | Parseren forventet en separator og traff på et ulovlig tegn; for eksempel at semikolonet ble utelatt etter en programmeldingsenhet, `*SRE 1:INP1:COUP AC`. |
| -104 | **Data type error** | Parseren gjenkjente et dataelement av en annen type enn tillatt; for eksempel at numeriske data eller strengdata var forventet, men blokkdata ble mottatt. |
| -108 | **Parameter not allowed** | Flere parametre enn forventet ble mottatt for headeren; for eksempel aksepterer common-kommandoen `*SRE` bare én parameter, så `*SRE 2,1` er ikke tillatt. |
| -109 | **Missing parameter** | Færre parametre enn påkrevd ble mottatt for headeren; for eksempel krever common-kommandoen `*SRE` én parameter, så `*SRE` alene er ikke tillatt. |
| -110 | **Command header error** | En feil ble oppdaget i headeren. |
| -112 | **Program mnemonic too long** | Headeren inneholder mer enn tolv tegn (se IEEE 488.2, 7.6.1.4.1). |
| -113 | **Undefined header** | Headeren er syntaktisk korrekt, men er udefinert for denne enheten; for eksempel er `*XYZ` ikke definert for noen enhet. |
| -114 | **Header suffix out of range** | Verdien av et numerisk suffiks knyttet til en programmnemonic (se avsnittet om syntaks og stil) gjør headeren ugyldig. |
| -120 | **Numeric data error** | Genereres ved parsing av et dataelement som ser ut til å være numerisk, inkludert de ikke-desimale numeriske typene. For eksempel vil `INP:GAIN 1.0X2` generere denne feilen. |
| -130 | **Suffix error** | Denne feilen, samt feilene -131 til og med -139, genereres ved parsing av et suffiks. |
| -131 | **Invalid suffix** | Suffikset følger ikke syntaksen beskrevet i IEEE 488.2, 7.7.3.2, eller suffikset passer ikke for denne enheten. |
| -134 | **Suffix too long** | Suffikset inneholdt mer enn 12 tegn (se IEEE 488.2, 7.7.3.4). |
| -138 | **Suffix not allowed** | Et suffiks ble påtruffet etter et numerisk element som ikke tillater suffikser. |
| -140 | **Character data error** | Genereres ved parsing av et tegndataelement (character data). For eksempel vil `INP:COUP XYZ` generere denne feilen. |
| -141 | **Invalid character data** | Enten inneholder tegndataelementet et ugyldig tegn, eller så er det mottatte elementet ikke gyldig for headeren. |
| -144 | **Character data too long** | Tegndataelementet inneholder mer enn tolv tegn (se IEEE 488.2, 7.7.1.4). |
| -148 | **Character data not allowed** | Et lovlig tegndataelement ble påtruffet der enheten forbyr det. |
| -150 | **String data error** | Genereres ved parsing av et strengdataelement. For eksempel vil `FUNC "XYZ"` generere denne feilen. |
| -151 | **Invalid string data** | Et strengdataelement var forventet, men var ugyldig av en eller annen grunn (se IEEE 488.2, 7.7.5.2); for eksempel at en END-melding ble mottatt før det avsluttende anførselstegnet. |

### Execution Error (utførelsesfeil)

Et `<error/event number>` i området **[-299, -200]** angir at en feil er oppdaget av instrumentets utførelseskontrollblokk. Enhver feil i denne klassen skal føre til at utførelsesfeilbiten (bit 4) i event status-registeret (IEEE 488.2, avsnitt 11.5.1) settes. Én av følgende hendelser har inntruffet:

- Et `<PROGRAM DATA>`-element etter en header ble vurdert av enheten til å ligge utenfor lovlig inngangsområde, eller er på annen måte uforenlig med enhetens egenskaper.
- En gyldig programmelding kunne ikke utføres korrekt på grunn av en tilstand i enheten.

Utførelsesfeil rapporteres av instrumentet etter at avrunding og evaluering av uttrykk har funnet sted. Avrunding av et numerisk dataelement vil for eksempel ikke bli rapportert som en utførelsesfeil.

| Feilkode | Feiltekst | Forklaring |
|---|---|---|
| -200 | **Execution error** | Generisk feil for enheter som ikke kan oppdage mer spesifikke feil. Koden angir kun at en Execution Error som definert i IEEE 488.2, 11.5.1.1.5 har oppstått. |
| -203 | **Command protected** | Angir at en lovlig passordbeskyttet programkommando eller spørring ikke kunne utføres fordi kommandoen var deaktivert. |
| -212 | **Arm ignored** | Angir at et arming-signal ble mottatt og gjenkjent av enheten, men ble ignorert. Instrumentet genererer denne feilen når ARM mottas uten at minneopptak (memory recording) er konfigurert. |
| -213 | **Init ignored** | Angir at en forespørsel om en måling ble ignorert fordi en annen måling allerede pågikk. |
| -221 | **Settings conflict** | Angir at et lovlig programdataelement ble parset, men ikke kunne utføres på grunn av enhetens nåværende tilstand (se IEEE 488.2, 6.4.5.3 og 11.5.1.1.5). |
| -222 | **Data out of range** | Angir at et lovlig programdataelement ble parset, men ikke kunne utføres fordi den tolkede verdien lå utenfor det lovlige området slik det er definert av enheten (se IEEE 488.2, 11.5.1.1.5). |
| -223 | **Too much data** | Angir at et lovlig programdataelement av typen blokk, uttrykk eller streng ble mottatt med mer data enn enheten kunne håndtere på grunn av minne eller tilsvarende enhetsspesifikke krav. |
| -224 | **Illegal parameter value** | Brukes der en eksakt verdi fra en liste av mulige verdier var forventet. |
| -225 | **Out of memory** | Enheten har ikke nok minne til å utføre den forespurte operasjonen. |
| -230 | **Data corrupt or stale** | Muligens ugyldige data; en ny avlesning er startet, men ikke fullført siden forrige tilgang. |
| -240 | **Hardware error** | Angir at en lovlig programkommando eller spørring ikke kunne utføres på grunn av et maskinvareproblem i enheten. |

### Device-Specific Error (enhetsspesifikk feil)

Et `<error/event number>` i området **[-399, -300]** eller **[1, 32767]** angir at instrumentet har oppdaget en feil, muligens forårsaket av en unormal maskinvare- eller fastvaretilstand. Disse kodene brukes også for feil i selvtest-responser. Enhver feil i denne klassen fører til at biten for enhetsspesifikk feil (bit 3) i event status-registeret (IEEE 488.2, avsnitt 11.5.1) settes.

| Feilkode | Feiltekst | Forklaring |
|---|---|---|
| -300 | **Device-specific error** | Generisk enhetsavhengig feil for enheter som ikke kan oppdage mer spesifikke feil. Koden angir kun at en Device-Dependent Error som definert i IEEE 488.2, 11.5.1.1.6 har oppstått. |
| -310 | **System error** | Angir at en feil som enheten betegner som "system error" har oppstått. Koden er enhetsavhengig. |
| -311 | **Memory error** | Angir en fysisk feil i enhetens minne, for eksempel paritetsfeil. |
| -313 | **Calibration memory lost** | Angir at ikke-flyktige kalibreringsdata som brukes av `*CAL?`-kommandoen har gått tapt. |
| -314 | **Save/recall memory lost** | Angir at de ikke-flyktige dataene lagret med `*SAV?`-kommandoen har gått tapt. |
| -315 | **Configuration memory lost** | Angir at ikke-flyktige konfigurasjonsdata lagret av enheten har gått tapt. |
| -320 | **Storage fault** | Angir at fastvaren oppdaget en feil ved bruk av datalagring. Feilen er ikke en indikasjon på fysisk skade eller svikt i noe masselagringselement. |
| -325 | **Sample factor adjusted** | Angir at anvendelse av gjeldende konfigurasjon førte til at samplingsfaktoren (sample factor) ble justert. |
| -326 | **Recording time too long** | Angir at dataene som ville blitt samlet inn i løpet av den angitte opptakstiden, ikke ville fått plass i tilgjengelig minne. |
| -330 | **Self-test failed** | (Ingen ytterligere forklaring i kilden.) |
| -340 | **Calibration failed** | (Ingen ytterligere forklaring i kilden.) |
| -350 | **Queue overflow** | En spesifikk kode som legges inn i køen i stedet for koden som forårsaket feilen. Koden angir at det ikke er plass i køen, og at en feil oppsto, men ikke ble registrert. |
| -360 | **Communication error** | (Ingen ytterligere forklaring i kilden.) |

### Query Error (spørringsfeil)

Et `<error/event number>` i området **[-499, -400]** angir at instrumentets utgangskø-kontroll (output queue control) har oppdaget et problem med meldingsutvekslingsprotokollen beskrevet i IEEE 488.2, kapittel 6. Enhver feil i denne klassen fører til at spørringsfeilbiten (bit 2) i event status-registeret (IEEE 488.2, avsnitt 11.5.1) settes. Disse feilene tilsvarer protokollfeilene for meldingsutveksling beskrevet i IEEE 488.2, avsnitt 6.5. Én av følgende er tilfelle:

- Det gjøres et forsøk på å lese data fra utgangskøen når ingen utdata verken er til stede eller underveis.
- Data i utgangskøen har gått tapt.
- Hendelser som genererer spørringsfeil skal ikke generere kommandofeil, utførelsesfeil eller enhetsspesifikke feil; se de øvrige feildefinisjonene i dette kapittelet.

| Feilkode | Feiltekst | Forklaring |
|---|---|---|
| -400 | **Query error** | Generisk spørringsfeil for enheter som ikke kan oppdage mer spesifikke feil. Koden angir kun at en Query Error som definert i IEEE 488.2, 11.5.1.1.7 og 6.3 har oppstått. |
| -410 | **Query INTERRUPTED** | Angir at en tilstand som forårsaker en INTERRUPTED Query-feil har oppstått (se IEEE 488.2, 6.3.2.3); for eksempel en spørring etterfulgt av DAB eller GET før et svar var fullstendig sendt. |
| -420 | **Query UNTERMINATED** | Angir at en tilstand som forårsaker en UNTERMINATED Query-feil har oppstått (se IEEE 488.2, 6.3.2.2); for eksempel at enheten ble adressert til å snakke (talk) og en ufullstendig programmelding ble mottatt. |
| -430 | **Query DEADLOCKED** | Angir at en tilstand som forårsaker en DEADLOCKED Query-feil har oppstått (se IEEE 488.2, 6.3.1.7); for eksempel at både inngangsbuffer og utgangsbuffer er fulle og enheten ikke kan fortsette. |
| -440 | **Query UNTERMINATED after indefinite response** | Angir at en spørring ble mottatt i samme programmelding etter at en spørring som ba om et ubestemt (indefinite) svar ble utført (se IEEE 488.2, 6.5.7.5). |

## Programmeringseksempler

Dette kapittelet gjengir eksemplene fra kapittel 6 «Programming Examples» i Remote Control Users Guide. Eksemplene viser hvordan instrumentet programmeres og kan brukes som utgangspunkt for mer komplekse programmeringsoppgaver.

### Introduksjon

I disse eksemplene kan grensesnittet (RS-232 / GPIB / Ethernet) velges ved å sette konstanten `INTFC` til den tilhørende `INTFC_…`-konstanten. Kommunikasjonsparametre (f.eks. seriell port, baudrate, IP-adresse osv.) settes med konstantene `RSRC_NAME` og `RSRC_ATTR_…`.

Programmeringseksemplene er skrevet i ANSI C med VISA-bibliotek implementert i henhold til versjon 2.2 av VISA-spesifikasjonen (www.vxipnp.org), for eksempel National Instruments VISA 2.5 eller nyere. Det er mulig å kommunisere med instrumentet over RS-232- eller Ethernet-grensesnitt kun med grunnleggende operativsystem-API (f.eks. Win32 eller UNIX), altså uten VISA-bibliotek. Det finnes ett slikt eksempel for Ethernet-grensesnittet (med Win32 API) senere i dette kapittelet — se [«U, I, P Measurement over Ethernet Interface without VISA Library»](#u-i-p-measurement-over-ethernet-interface-without-visa-library--rå-tcp-socket), som er det mest relevante hvis du skal lage din egen TCP-klient.

Merk fellesmønsteret i alle eksemplene:

- VISA-ressursnavnet for Ethernet er `"TCPIP::192.168.2.251::23::SOCKET"` — altså en rå TCP-socket mot instrumentets IP-adresse på **port 23**.
- Alle kommandoer sendes som ASCII-tekst avsluttet med linjeskift (`\n`); svar leses tilbake som tekstlinjer.

### Initialize Interface — initialisering av grensesnittet

Grensesnittet må initialiseres før noen kommunikasjon med instrumentet finner sted.

Dette programmet åpner en VISA-sesjon mot instrumentet. Programmet bruker RS-232-, GPIB- eller Ethernet-grensesnitt avhengig av innstillingen av konstanten `INTFC`, og setter I/O-timeout til 10 sekunder. For en egen TCP-klient er det verdt å merke seg ressursnavnet for LAN (`TCPIP::<ip>::23::SOCKET`) og timeout-verdien (10 s), som er et fornuftig utgangspunkt også for en socket-basert klient.

```c
/*
 * INITIALIZE INTERFACE
 *
 * This program will open VISA session to the instrument.
 * The program uses RS-232, GPIB or ethernet interface depending
 * on the setting of 'INTFC' constant and sets the I/O timeout
 * to 10 seconds.
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession  rm,      // Default resource manager session
           vi;      // VISA session

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    puts("OK");

    return 0;

#undef CHECK_STATUS
}
```

### Initialize Instrument — identifisering og reset

Før videre kommunikasjon bør instrumentets identitet verifiseres, og instrumentet bør settes i en kjent (standard) tilstand.

Dette programmet åpner en VISA-sesjon mot instrumentet, leser ID-strengen og resetter instrumentet. SCPI-sekvensen — send `*IDN?`, les svaret, send `*RST` — er direkte gjenbrukbar i en egen TCP-klient som «håndtrykk» ved oppstart: den bekrefter at du snakker med riktig instrument og gir deg en kjent utgangstilstand.

```c
/*
 * IDENTIFY AND RESET THE INSTRUMENT
 *
 * This program will open VISA session to the instrument, read ID string
 * and reset the instrument.
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,         // Default resource manager session
            vi;         // VISA session
ViChar      buffer[512];    // buffer to hold instrument response
ViUInt32    retCnt;         // number of bytes read from the instrument

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

    /* Query the instrument's ID string: */
    viPrintf (vi, "*IDN?\n");
    /* Read the ID string into buffer: */
    viRead (vi, buffer, 256, &retCnt);
    /* Print contens of the buffer on screen: */
    printf (buffer);
    /* Bring the instrument into default state: */
    viPrintf (vi, "*RST\n");

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### Perform Simple Power Measurement — enkel effektmåling

Gyldige signaler bør være tilkoblet instrumentets inngangskanaler, ellers kan måleverdien være ugyldig.

Programmet åpner en VISA-sesjon og utfører en enkel effektmåling, og venter synkront (blokkerer i `viRead` mens det ventes på målingen). Tiden det tar å måle effekten avhenger av spennings- og strømsignalene som er koblet til instrumentet. Standard midlingsintervall (lik måletiden) er 300 ms. Dersom `viRead` går i timeout før den målte effekten returneres, må VISA-timeout økes; bruk funksjonen `viSetAttribute` for å endre timeout-verdien (standard er 10 sek).

Den gjenbrukbare SCPI-sekvensen for din egen klient er: `*RST` → vent på autorange → `*TRG` (trigg en måling) → vent → `DATA? "POW:ACT"` (hent aktiv effekt) → les svarlinjen.

```c
/*
 * MEASURE POWER (wait synchronously)
 *
 * This program will open VISA session to the instrument and perform a simple
 * power measurement. It dwells in viRead function while waiting
 * for measurement.
 *
 * The time it will take to measure the power depends on the voltage and
 * current signals attached to the instrument. Default averaging interval
 * (equals to measurement time) is 300 ms. In the case viRead times out before
 * the measured power is returned, VISA timeout must be increased. Use
 * function viSetAttribute to change the timeout value (default is 10 sec).
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,             // Default resource manager session
            vi;             // VISA session
ViChar      buffer[512];    // buffer to hold instrument response
ViUInt32    retCnt;         // number of bytes read from the instrument

#if WIN32
#include <windows.h>
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}
#endif

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument with GPIB address 5: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

#if 0
    /* enable termination character for read operations: */
    viSetAttribute (vi, VI_ATTR_TERMCHAR, '\n');
    viSetAttribute (vi, VI_ATTR_TERMCHAR_EN, VI_TRUE);
#if INTFC == INTFC_RS232
    viSetAttribute (vi, VI_ATTR_ASRL_END_IN, VI_ASRL_END_TERMCHAR);
#endif
#endif

    viPrintf (vi, "*RST\n");    /* Bring the instrument into a default state */
    Delay (3.0);        /* Wait 3 seconds for autorange to complete */
    viPrintf (vi, "*TRG\n");                /* Trigger a measurement */
    Delay (2.0);                            /* Wait 2 seconds */
    viPrintf (vi, "DATA? \"POW:ACT\"\n");   /* Query the power */
    memset(buffer,0,sizeof(buffer));        /* Clear buffer */
    viRead (vi, buffer, 256, &retCnt);      /* Read value */
    printf(buffer); /* Print the value on the screen */

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### U, I, P Measurement — spennings-, strøm- og effektmåling

Dette eksemplet konfigurerer instrumentet til å måle effekt, spenning og strøm på et trefasesystem 3 x 400V/50Hz og leser målingene.

Tiden det tar å fullføre målingen er 1 s (midlingstid, satt med `APER 1.0`). Spørringen `DATA?` venter ikke på at målingen fullføres — den returnerer de verdiene som er tilgjengelige i øyeblikket. Forsinkelsen på 2 sekunder gir instrumentet nok tid til å fullføre målingen før data leses.

Konfigurasjonssekvensen her (`ROUT:SYST`, `SYNC:SOUR`, `VOLT1:RANG`, `CURR1:RANG:AUTO`, `APER`, `FUNC`, `INIT:CONT ON`, deretter `DATA?`) er nøyaktig den samme som brukes i det VISA-frie Ethernet-eksemplet lenger ned — det er denne SCPI-oppskriften du gjenbruker i din egen TCP-klient.

```c
/*
 * MEASURE U, I, P (wait synchronously)
 *
 * This example will configure the instrument to measure power, voltage
 * and current on three-phase system 3 x 400V/50Hz and reads the measurements.
 *
 * The time it will take to finish the measurement is 1 s (averaging time).
 * The query DATA? does not wait for the measurement to be completed, so it
 * would return whatever values are available at a moment. The delay of 2
 * seconds gives the instrument enough time to finish the measurement before
 * reading data.
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,             // Default resource manager session
            vi;             // VISA session
ViChar      buffer[512];    // buffer to hold instrument response
ViUInt32    retCnt;         // number of bytes read from the instrument

#if WIN32
#include <windows.h>
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}
#endif

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument with GPIB address 5: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

    /* Bring the instrument into a default state: */
    viPrintf (vi, "*RST\n");
    /* Select three wattmeter configuration: */
    viPrintf (vi, "ROUT:SYST \"3W\"\n");
    /* SYNC source = voltage phase 1: */
    viPrintf (vi, "SYNC:SOUR VOLT1\n");
    /* Set voltage range on voltage channel 1 to 300 V: */
    viPrintf (vi, "VOLT1:RANG 300.0\n");
    /* Set current channel 1 to autorange: */
    viPrintf (vi, "CURR1:RANG:AUTO ON\n");
    /* Set averaging time to 1 second: */
    viPrintf (vi, "APER 1.0\n");
    /* Select U, I, P measurement: */
    viPrintf (vi, "FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"\n");
    /* Run continuous measurements: */
    viPrintf (vi, "INIT:CONT ON\n");

    Delay (2.0);        /* Wait 2 seconds */

    viPrintf (vi, "DATA?\n");           /* Query the measurement */
    memset(buffer,0,sizeof(buffer));    /* Clear buffer */
    viRead (vi, buffer, 256, &retCnt);  /* Read values */
    printf (buffer);                    /* Print the value on the screen */

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### Continuous Power Measurement — kontinuerlig effektmåling

Gyldige signaler bør være tilkoblet instrumentets inngangskanaler, ellers kan måleverdien være ugyldig.

Programmet utfører kontinuerlig effektmåling. Bit 10 (verdi `0x400`) i operasjonsstatusregisteret brukes til å detektere slutten på midlingsintervallet — slik sikres det at den nyeste målingen hentes og vises umiddelbart. Standard midlingsintervall er 300 ms; dersom `viRead` går i timeout før målt effekt returneres, må VISA-timeout økes med `viSetAttribute` (standard er 10 sek).

Pollemønsteret er direkte overførbart til en TCP-klient: send `*CLS` (nullstill tidligere hendelse), poll `STAT:OPER?` til bit 10 er satt, og hent så ferske verdier med `DATA? "POW"`. Dette er den anbefalte måten å synkronisere avlesning med instrumentets måletakt på, i stedet for faste ventetider.

```c
/*
 * CONTINUOUS POWER MEASUREMENT
 *
 * This program will open VISA session to the instrument and perform continuous
 * power measurement. Bit 10 of the operating status register is used
 * to determine end of averaging interval. This way it is assured that
 * the newest measurement is immediately fetched and displayed.
 *
 * The time it will take to measure the power depends on the voltage and
 * current signals attached to the instrument. Default averaging interval
 * (equals to measurement time) is 300 ms. In the case viRead times out before
 * the measured power is returned, VISA timeout must be increased.
 * Use function viSetAttribute to change the timeout value (default is 10 sec).
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,                 // Default resource manager session
            vi;                 // VISA session
ViChar      buffer[512];        // buffer to hold instrument response
ViUInt32    retCnt;             // number of bytes read from the instrument
ViUInt16    opStat;

#if WIN32
#include <windows.h>
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}
#endif

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument with GPIB address 5: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

    viPrintf (vi, "*RST\n");    /* Bring the instrument into a default state */
    Delay (3.0);                /* Wait 3 seconds for autorange to complete */
    viPrintf (vi, "*TRG\n");    /* Trigger a measurement */
    Delay (2.0);                /* Wait 2 seconds */

    while (1)
    {
        opStat = 0;
        viPrintf (vi, "*CLS\n");    /* Clear previously detected event */
        do                          /* Run this loop until bit 10 is set */
        {
            /* Query the OPER:STAT register value: */
            viPrintf (vi, "STAT:OPER?\n");
            memset(buffer,0,sizeof(buffer));    /* Clear buffer */
            viRead (vi, buffer, 256, &retCnt);  /* Read value */
            opStat = atoi (buffer);
#if 0
            printf("opStat: 0x%X\n",(unsigned int)opStat);
#endif
        } while ((opStat & 0x400) == 0);

        viPrintf (vi, "DATA? \"POW\"\n");   /* Query the power measurement */
        viRead (vi, buffer, 256, &retCnt);  /* Read value */

        printf (buffer);        /* Print the value on the screen */
    }

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### U, I, P Measurement over Ethernet Interface without VISA Library — rå TCP-socket

**Dette er nøkkeleksemplet for en egen applikasjon som snakker direkte med instrumentet over TCP/IP, uten VISA-bibliotek.** Eksemplet konfigurerer instrumentet til å måle effekt, spenning og strøm på et trefasesystem 3 x 400V/50Hz og leser målingene. Det bruker Win32 Winsock-API, men mønsteret er identisk i alle språk/miljøer med TCP-sockets (Python, C#, Java, Node.js, UNIX sockets osv.).

Tiden det tar å fullføre målingen er 1 s (midlingstid). Spørringen `DATA?` venter ikke på at målingen fullføres, så den returnerer verdiene som er tilgjengelige i øyeblikket; forsinkelsen på 2 sekunder gir instrumentet tid til å fullføre målingen før data leses.

Det som er direkte gjenbrukbart for din egen TCP-klient:

- **Tilkobling:** åpne en TCP-strømsocket (`AF_INET`, `SOCK_STREAM`) og koble til instrumentets IP-adresse (`HOST`, her `192.168.2.251`) på **port 23** (`PORT`).
- **Sending (`socket_puts`):** hver SCPI-kommando sendes som ren ASCII-tekst med `\n` (linjeskift) lagt til på slutten.
- **Mottak (`socket_gets`):** gjør ett `recv()`-kall (inntil 1024 byte) og kutter strengen ved første `\n`; en eventuell `\r` rett før fjernes, og linjen null-termineres. Dette er hele «protokollen» — linjebasert tekst over TCP. Merk at koden ikke leser i løkke: en robust klient bør lese gjentatte ganger til `\n` er mottatt, siden TCP ikke garanterer at hele svarlinjen kommer i ett `recv()`-kall (jf. `sio_gets` i RS-232-eksemplet, som leser tegn for tegn til `\n`).
- **SCPI-sekvensen** i `main()` (`*RST`, `ROUT:SYST "3W"`, `SYNC:SOUR VOLT1`, `VOLT1:RANG 300.0`, `CURR1:RANG:AUTO ON`, `APER 1.0`, `FUNC …`, `INIT:CONT ON`, `DATA?`) kan kopieres uendret.
- Den utkommenterte `*IDN?`-blokken (`#if 0 … #endif`) viser hvordan du verifiserer forbindelsen ved oppstart.

```c
/*
 * MEASURE U, I, P (wait synchronously)
 *
 * This example will configure the instrument to measure power, voltage
 * and current on three-phase system 3 x 400V/50Hz and reads the measurements.
 *
 * The time it will take to finish the measurement is 1 s (averaging time).
 * The query DATA? does not wait for the measurement to be completed, so it
 * would return whatever values are available at a moment. The delay of 2
 * seconds gives the instrument enough time to finish the measurement before
 * reading data.
 */

#if WIN32
#define WIN 1
#endif

#if WIN
#if !defined(_MFC_VER)      /* !MFC (!<afx.h>) */
#include <winsock2.h>
#include <windows.h>
#endif  /* !_MFC_VER */
#endif
#include <stdio.h>

#define HOST    "192.168.2.251"
#define PORT    23

#if WIN
#define SOCKET_HANDLE_FMT_PFX   ""
#define SOCKET_HANDLE_FMT       "u"     /* "%u" - 'typedef u_int SOCKET' */
typedef SOCKET socket_handle_t;
#endif

typedef struct {
    socket_handle_t h;
} *socket_t;

/******************************************************************************/
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}

/******************************************************************************/
void wsa_error(char *func,int error)
{
    fprintf(stderr,"%s() failed, error %d\n",
        func,error == -1 ? WSAGetLastError() : error);
}

/******************************************************************************/
int socket_setup(void)
{
#if WIN

    WSADATA wsaData;
    int wsaerrno;

    /*
     * Initialize Windows Socket DLL
     */
    if ( (wsaerrno = WSAStartup(
        MAKEWORD(1,1),  /* at least version 1.1 */
        &wsaData)) != 0 )
    {
        wsa_error("WSAStartup",wsaerrno);
        return -1;
    }

#endif  /* WIN */

    return 0;   /* OK */
}

/******************************************************************************/
int socket_cleanup(void)
{
#if WIN
    if ( WSACleanup() == SOCKET_ERROR )
        wsa_error("WSACleanup",-1);
#endif

    return 0;   /* OK */
}

/******************************************************************************/
socket_t socket_create(void)
{
    socket_handle_t sh;
    socket_t s;

    sh = socket(AF_INET,SOCK_STREAM,0);
#if WIN
    if ( sh == INVALID_SOCKET )
    {
        wsa_error("socket",-1);
        return NULL;
    }
#endif

    s = calloc(1,sizeof(*s));
    if ( !s )
        return NULL;

    s->h = sh;

    return s;   /* OK */
}

/******************************************************************************/
int socket_connect(socket_t s,struct sockaddr *addr,int addrlen)
{
    int ret = 0;    /* OK */

#if WIN
    if ( connect(s->h,addr,addrlen) == SOCKET_ERROR )
    {
        wsa_error("connect",-1);
        return -1;
    }
#endif

    return 0;   /* OK */
}

/******************************************************************************/
int socket_recv(socket_t s,void *buf,int len,int flags)
{
    register int l;

#if WIN
    l = recv(s->h,buf,len,flags);
    if ( l == SOCKET_ERROR )
    {
        wsa_error("recv",-1);
        return -1;
    }
#endif

    return l;
}

/******************************************************************************/
int socket_send(socket_t s,void *buf,int len,int flags)
{
    register int slen;  /* sent length */

#if WIN
    slen = send(s->h,buf,len,flags);
    if ( slen == SOCKET_ERROR )
    {
        wsa_error("send",-1);
        return -1;
    }
#endif

    return slen;
}

/******************************************************************************/
int socket_puts(socket_t s,char *str)
{
    char buf[1024];

    strcpy(buf,str);
    strcat(buf,"\n");
    if ( socket_send(s,buf,strlen(buf),0) < 0 )
        return -1;

    return 0;
}

/******************************************************************************/
int socket_gets(socket_t s,char *str)
{
    char buf[1024];
    char *p;

    if ( socket_recv(s,buf,sizeof(buf),0) < 0 )
        return -1;

    if ( (p = memchr(buf,'\n',sizeof(buf))) != NULL )
    {
        if ( p > buf && p[-1] == '\r' )
            p--;
        *p = '\0';
    }
    else
        buf[sizeof(buf)-1] = '\0';

    strcpy(str,buf);

    return strlen(str);
}

/******************************************************************************/
int main(int argc,char *argv[])
{
    socket_t s;
    struct sockaddr_in saddr;
    struct sockaddr_in *addr_in = (struct sockaddr_in *)&saddr;
    char buffer[1024];

    /* socket (TCP/IP) API initialization: */
    if ( socket_setup() < 0 )
        return 1;

    /*
     * Connect to the instrument:
     */
    /* set destination IP address and TCP port: */
    memset(addr_in,0,sizeof(struct sockaddr_in));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(PORT);
    addr_in->sin_addr.s_addr = inet_addr(HOST);
    /* create socket: */
    s = socket_create();
    if ( !s )
        return 1;
#if 0
    fprintf(stderr,"socket_connect() ...\n");
#endif
    if ( socket_connect(s,(struct sockaddr *)&saddr,sizeof(saddr)) < 0 )
        return 1;
#if 0
    fprintf(stderr,"socket_connect(): done\n");
#endif

#if 0
    if ( socket_puts(s,"*IDN?") < 0 )
        return 1;
    if ( socket_gets(s,buffer) < 0 )
        return 1;
    puts(buffer);
#endif

    /* Bring the instrument into a default state: */
    socket_puts(s,"*RST");
    /* Select three wattmeter configuration: */
    socket_puts(s,"ROUT:SYST \"3W\"");
    /* SYNC source = voltage phase 1: */
    socket_puts(s,"SYNC:SOUR VOLT1");
    /* Set voltage range on voltage channel 1 to 300 V: */
    socket_puts(s,"VOLT1:RANG 300.0");
    /* Set current channel 1 to autorange: */
    socket_puts(s,"CURR1:RANG:AUTO ON");
    /* Set averaging time to 1 second: */
    socket_puts(s,"APER 1.0");
    /* Select U, I, P measurement: */
    socket_puts(s,"FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"");
    /* Run continuous measurements: */
    socket_puts(s,"INIT:CONT ON");

    Delay(2.0);        /* Wait 2 seconds */

    socket_puts(s,"DATA?");             /* Query the measurement */
    memset(buffer,0,sizeof(buffer));    /* Clear buffer */
    socket_gets(s,buffer);              /* Read values */
    puts(buffer);                       /* Print the value on the screen */

    socket_cleanup();

    return 0;
}
```

#### Oppsummert protokoll for egen TCP-klient

Basert på eksemplet over er alt du trenger for å snakke med instrumentet fra egen kode:

1. Åpne TCP-forbindelse til instrumentets IP-adresse, port 23.
2. Send SCPI-kommandoer som ASCII-linjer avsluttet med `\n`.
3. For spørringer (kommandoer som slutter med `?`): les svaret som én tekstlinje avsluttet med `\n` (eventuelt `\r\n` — strip `\r`).
4. Konfigurer måling én gang (`*RST`, `ROUT:SYST`, `SYNC:SOUR`, områder, `APER`, `FUNC`, `INIT:CONT ON`), og hent deretter verdier med `DATA?` så ofte du ønsker — eventuelt synkronisert mot `STAT:OPER?` bit 10 slik det kontinuerlige eksemplet viser.

### U, I, P Measurement over RS-232 Interface without VISA Library

Dette eksemplet gjør det samme som Ethernet-eksemplet over — konfigurerer instrumentet til å måle effekt, spenning og strøm på et trefasesystem 3 x 400V/50Hz og leser målingene — men over RS-232 med Win32 API (`CreateFile`/`ReadFile`/`WriteFile`) i stedet for sockets. Det er mest relevant som referanse hvis du trenger seriell kommunikasjon; for en TCP-applikasjon er Ethernet-eksemplet over det du skal bruke. Merk at SCPI-sekvensen i `main()` er identisk — bare transportlaget er byttet ut.

Tiden det tar å fullføre målingen er 1 s (midlingstid). Spørringen `DATA?` venter ikke på at målingen fullføres; forsinkelsen på 2 sekunder gir instrumentet tid til å fullføre målingen før data leses.

```c
/*
 * MEASURE U, I, P (wait synchronously)
 *
 * This example will configure the instrument to measure power, voltage
 * and current on three-phase system 3 x 400V/50Hz and reads the measurements.
 *
 * The time it will take to finish the measurement is 1 s (averaging time).
 * The query DATA? does not wait for the measurement to be completed, so it
 * would return whatever values are available at a moment. The delay of 2
 * seconds gives the instrument enough time to finish the measurement before
 * reading data.
 */

#if WIN32
#define WIN 1
#endif

#if WIN
#if !defined(_MFC_VER)      /* !MFC (!<afx.h>) */
#include <windows.h>
#endif  /* !_MFC_VER */
#endif
#include <stdio.h>

/*
 * "\\.\com<num>" in order to support "COM10" and above.
 *
 * More info: MSKB article Q115831:
 *
 *      http://support.microsoft.com/default.aspx?scid=kb;EN-US;q115831
 */
#define SIO_PORT            "\\\\.\\com1"
#define SIO_BAUDRATE        115200

#define SIO_INPUT_BUFSIZE   4096
#define SIO_OUTPUT_BUFSIZE  4096

#define MAX(a,b)    ( (a) > (b) ? (a) : (b) )

#if WIN
/* serial port handle ('CreateFile("COMx:",...)'): */
typedef HANDLE          sio_handle_t;
#endif

typedef unsigned char byte;

typedef struct {
    sio_handle_t handle;

    /*
     * I/O buffer (similar to 'FILE'):
     */
    struct {
        byte *base; /* address of allocated buffer */
        byte *ptr;  /* pointer to first available byte in buffer 'base' */
        int size;   /* size in bytes of allocated buffer 'base' */
        int cnt;    /* current number of bytes available in buffer at 'ptr' */
    } buf;

} sio_t;

/******************************************************************************/
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}

/******************************************************************************/
static void sio_error(char *func)
{
    fprintf(stderr,"%s() failed, error %ld\n",func,GetLastError());
}

/******************************************************************************/
static int sio_open(sio_t *sio,char *device)
{
    COMMTIMEOUTS timeouts;
    sio_handle_t handle;
    DCB dcb;

    memset(sio,0,sizeof(*sio));

    handle = CreateFile(
        device,                         // LPCTSTR lpFileName
        GENERIC_READ | GENERIC_WRITE,   // DWORD dwDesiredAccess
        0,                              // DWORD dwShareMode
        NULL,                           // LPSECURITY_ATTRIBUTES lpSecurityAttributes
        OPEN_EXISTING,                  // DWORD dwCreationDisposition
        0,                              // DWORD dwFlagsAndAttributes
        NULL                            // HANDLE hTemplateFile
    );
    if ( handle == INVALID_HANDLE_VALUE )
    {
        sio_error("CreateFile");
        return -1;
    }
    sio->handle = handle;

    dcb.DCBlength = sizeof(dcb);
    if ( !GetCommState(handle,&dcb) )
    {
        sio_error("GetCommState");
        return -1;
    }

    /*
     * Baud Rate:
     */
    dcb.BaudRate = SIO_BAUDRATE;

    /*
     * Character Size:
     */
    dcb.ByteSize = 8;

    /*
     * Parity:
     */
    dcb.Parity  = NOPARITY;
    dcb.fParity = TRUE;

    /*
     * Stop Bits:
     */
    dcb.StopBits = ONESTOPBIT;

    /*
     * Hand-shake:
     */
    dcb.fRtsControl        = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow       = FALSE;
    dcb.fDtrControl        = DTR_CONTROL_ENABLE;
    dcb.fOutxDsrFlow       = FALSE;
    dcb.fDsrSensitivity    = FALSE;
    dcb.fOutX              = FALSE;
    dcb.fInX               = FALSE;
    dcb.fTXContinueOnXoff  = FALSE;
    dcb.fAbortOnError      = TRUE;
    if ( !SetCommState(handle,&dcb) )
    {
        sio_error("SetCommState");
        return -1;
    }

    /*
     * Read timeout: MAXDWORD, 0, 0
     * - read operation is to return immediately with the characters
     *   that have already been received, even if no characters have
     *   been received (i.e. read operation does not block)
     */
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    /*
     * Write timeout: 0, n
     * - write operation will not block
     */
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    if ( !SetCommTimeouts(handle,&timeouts) )
    {
        sio_error("SetCommTimeouts");
        return -1;
    }

    /*
     * Set up I/O buffers:
     */
    if ( !SetupComm(handle,SIO_INPUT_BUFSIZE,SIO_OUTPUT_BUFSIZE) )
    {
        sio_error("SetupComm");
        return -1;
    }

    sio->buf.size = MAX(SIO_INPUT_BUFSIZE,SIO_OUTPUT_BUFSIZE);
    sio->buf.base = malloc(sio->buf.size);
    if ( !sio->buf.base)
        return -1;
    sio->buf.ptr = sio->buf.base;
    sio->buf.cnt = 0;

    return 0;   /* OK */
}

/******************************************************************************/
static int sio_close(sio_t *sio)
{
    CloseHandle(sio->handle);
    if ( sio->buf.base )
        free(sio->buf.base);
    sio->buf.base = NULL;

    return 0;   /* OK */
}

/******************************************************************************/
static int sio_read(sio_t *sio,byte *buf,int bufsize)
{
    DWORD l;

    if ( !ReadFile(sio->handle,buf,1,&l,NULL) )
    {
        sio_error("ReadFile");
        return -1;
    }

    return l;
}

/******************************************************************************/
static int sio_write(sio_t *sio,byte *buf,int bufsize)
{
    DWORD l, len;

    for(len = 0; len < (DWORD)bufsize; len += l)
    {
        if ( !WriteFile(sio->handle,buf+len,bufsize-len,&l,NULL) )
        {
            sio_error("WriteFile");
            return -1;
        }
    }
    return len;
}

/******************************************************************************/
static int sio_fillbuf(sio_t *sio)
{
    register int l;

    l = sio_read(sio,sio->buf.base,sio->buf.size);
    if ( l <= 0 )
        return l;

#if 0
    fprintf(stderr,"sio_fillbuf(): sio_read(): %d\n",l);
#endif

    sio->buf.cnt = l;
    sio->buf.ptr = sio->buf.base;

    return l;
}

/******************************************************************************/
static int sio_getc(sio_t *sio,int *pchar)
{
    int ret;

    if ( !sio->buf.cnt && (ret = sio_fillbuf(sio)) <= 0 )
        return ret;

    sio->buf.cnt--;
    *pchar = *sio->buf.ptr++;

    return 1;   /* OK */
}

/******************************************************************************/
static int sio_gets(sio_t *sio,char *str)
{
    register int len;
    int c, ret;

    for(len = 0; ; )
    {
        if ( (ret = sio_getc(sio,&c)) < 0 )
            return ret;
        if ( ret == 0 )
            Sleep(10);  /* no data on input, don't hog CPU */
        else
        {
            if ( c == '\r' )
                continue;
            str[len++] = (char)c;
            if ( c == '\n' )
                break;
        }
    }
    str[len] = '\0';    /* terminate string */

    return 0;   /* OK */
}

/******************************************************************************/
static int sio_puts(sio_t *sio,char *str)
{
    char buf[1024];

    strcpy(buf,str);
    strcat(buf,"\n");

    return sio_write(sio,(byte *)buf,strlen(buf));
}

/******************************************************************************/
int main(int argc,char *argv[])
{
    char buffer[1024];
    sio_t sio;

    /*
     * Open connection to the instrument:
     */
    if ( sio_open(&sio,SIO_PORT) < 0 )
        return 1;

#if 0
    if ( sio_puts(&sio,"*IDN?") < 0 )
        return 1;
    if ( sio_gets(&sio,buffer) < 0 )
        return 1;
    puts(buffer);
#endif

#if 1
    /* Bring the instrument into a default state: */
    sio_puts(&sio,"*RST");
#endif
    /* Select three wattmeter configuration: */
    sio_puts(&sio,"ROUT:SYST \"3W\"");
    /* SYNC source = voltage phase 1: */
    sio_puts(&sio,"SYNC:SOUR VOLT1");
    /* Set voltage range on voltage channel 1 to 300 V: */
    sio_puts(&sio,"VOLT1:RANG 300.0");
    /* Set current channel 1 to autorange: */
    sio_puts(&sio,"CURR1:RANG:AUTO ON");
    /* Set averaging time to 1 second: */
    sio_puts(&sio,"APER 1.0");
    /* Select U, I, P measurement: */
    sio_puts(&sio,"FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"");
    /* Run continuous measurements: */
    sio_puts(&sio,"INIT:CONT ON");

    Delay(2.0);        /* Wait 2 seconds */

    /* --- Rekonstruert: kildeutsnittet slutter på side 6-26 --- */
    sio_puts(&sio,"DATA?");             /* Query the measurement */
    memset(buffer,0,sizeof(buffer));    /* Clear buffer */
    sio_gets(&sio,buffer);              /* Read values */
    puts(buffer);                       /* Print the value on the screen */

    sio_close(&sio);

    return 0;
}
```

> **Merk:** Kildeutsnittet fra manualen slutter etter `Delay(2.0);` (side 6-26), midt i slutten av RS-232-eksemplet. Linjene etter kommentaren «Rekonstruert» ovenfor er ikke fra manualen, men er rekonstruert etter samme mønster som Ethernet-eksemplet: spørring med `sio_puts(&sio,"DATA?")`, lesing av svaret med `sio_gets`, utskrift av verdien og lukking av porten med `sio_close`.
