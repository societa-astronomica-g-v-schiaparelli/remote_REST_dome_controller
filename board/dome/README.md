# Documentation

⚠️ **WARNING**: need translation.

Viene usata la PLC [KMP ProDino ESP32 Ethernet v1](https://kmpelectronics.eu/products/prodino-esp32-ethernet-v1/) come server di controllo del movimento della cupola.

![KMP PRODINo ESP32 Ethernet v1](https://kmpelectronics.eu/wp-content/uploads/2019/11/ProDinoESP32_E_5.jpg)

## Descrizoine hardware

Il ProDino acquistato è dotato di porta eth, inizialmente pensata come mezzo di comunicazione primaria, dopo estese prove e stress test è risultato che il WiFi è più stabile e responsivo. Alla scheda ProDino è stata aggiunta una scheda custom per avere alti 4 ingressi optoisolati, fatta da Marco, che si collega ai pin digitali esposti dalla scheda.

In oltre, la scheda comunica via seriale 485 con una scheda, sempre sviluppata da Marco, per la lettura dell'encoder e il suono del cicalino pre-rotazione (di seguito i comandi).

## Descrizione cartelle e file

- `data/`. File che andranno nella SPIFFS, tipicamente pagine web.

  - `dashboard.html` e `dashboard.js`. Contengono l'html e il relativo codice javascript per la dashboard di controllo della board (comandi principali e avanzati, panoramica dello stato).

  - `webserial.html` e `webserial.js`. Contengono l'html e il relativo codice javascript per l'interfaccia WebSerial (vedi dopo).

  - `style.css`. Contiene le regole di formattazione comuni alle pagine web.

- `examples/`. Contiene esempi e prove varie.

- `include/`

  - `global_definitions.h`. Contiene le definizione delle variabili globali e le `#define` che servono nel codice. Le variabili sono dichiarate come `extern` per poterle usare nel vari file che si trovano nella cartella `src`; sono invece inizializzate nel file `src/main.cpp`.

- `lib/`

  - `CustomOptoIn`, versione 0.1.0. Libreria per leggere gli ingressi ottici custom.

  - `ProDinoESP32`, versione 1.1.0, [repository ufficiale](https://github.com/kmpelectronics/Arduino/tree/master/ProDinoESP32). Libreria per usare le funzionalità della scheda, come relay, optoin, ethernet, ...

  - `WebSerial`, versione 1.0.0, ispirata a [questa](https://github.com/ayushsharma82/WebSerial) ma pesantemente modificata per migliorarne le funzionalità. Emula una interfaccia seriale mediante una websocket per permettere debugging da remoto.

- `src/`

  - `auxiliary_functions.cpp`. Contiene le funzioni ausiliarie, come la connessione al Wi-Fi, il lampeggio del led, ...

  - `main.cpp`. Contiene il core del codice: inizializzazione delle variabili, setup, loop ed eventuali task secondari.

  - `web_server.cpp`. Contiene il web server con tutte le sue chiamate.

## Tipo di comando

La board mette a disposizione:

- Una pagina web accessibile all'indirizzo `http://board-ip-address/`.

- Delle API attraverso cui comandare la board all'indirizzo `http://board-ip-address/api`. Per interagire con le API, è necessario fornire un json che specifichi il comando formattato come segue: `{"cmd": "command"}` dove, al posto di `command`, si mette il nome del comando di interesse.

- Una seriale web in sola lettura per i log all'indirizzo `http://board-ip-address/webserial`.

## Funzionamento

### Stato della dome e della PLC

Lo stato globale può essere controllato dalla pagina web o con il comando `status`.

### Rotazione

La rotazione è gestita:

- In modalità automatica dai metodi `startSlewing(int az_target)` e `stopSlewing()`,

- In modalità manuale da loop che controlla la pressione dei pulsanti, richiamando ogni 100ms la posizione della cupola con il metodo `domePosition()` e `stopSlewing()`.

### Comunicazione RS-485

PLC con comunicazione seriale, i parametri di comunicazione sono ProDino Master - Encoder Slave, con un bound rate di 19200.

Nota: 1 giro della cupola sono 84.6 giri del pignone. L'encoder è letto a 200 passi giro, quindi un giro completo della cupola sono 16920 (`0x4218`) passi encoder. 16920 / 360 gradi = 47 passi encoder per grado di rotazione.

Comandi 485:

- `0x42` "B": attiva cicalino, si spegne con qualunque altro comando.

- `0x52` "R": richiesta di posizione della cupola, in risposta due byte hex con posizione in gradi della cupola. `0x00B4` -> 180dec -> SUD

- `0x57` "W": forzatura posizione cupola. Il comando è seguito da due byte hex della posizione.

- `0x5A` "Z": richiesta di manovra di Zero, il comando deve essere seguito dalla rotazione della cupola verso lo switch di zero, raggiunto il quale viene ritrasmesso il comando e i due byte hex della posizione.

- `0x44` "D": ripristina la configurazione di default.

- `0x23` "#": disabilita switch di zero, solo per calibrazione.

- `0xC0`  Comando scrittura dati di configurazione totale 9 byte hex:
  - byte 1: comando `0xC0`;
  - byte 2: passi encoder per grado di rotazione della cupola;
  - byte 3 e 4: due byte passi encoder per un giro della cupola;
  - byte 5 e 6: due byte posizione switch di zero "1" in gradi;
  - byte 7 e 8: due byte posizione switch di zero "2" in gradi;
  - byte 9: checksum `(Neg(byte 1+byte2+...+byte8)+1)`.
  - VALORE CORRETTO ATTUALMENTE DELLA CONFIGURAZIONE: `[47, 65, 251, 0, 248, 0, 37]`

- `0xC1` Richiesta lettura dati di configurazione, risposta 8 byte hex (7 dati + checksum, vedi sopra).

La PLC è dotata di tre led:

- LED 1: senso di rotazione.
- LED 2: DATA RX.
- LED 3: Zero switch / all'avvio:
  - 1 lampeggio -> load default config
  - 2 lampeggi -> load eeprom config.

### Controllo automatico-manuale e input utente

Il controllo AUT/MAN viene eseguito direttamente dal loop, quando impostato, prende la totale precedenza su qualsiasi richiesta fatta dalla parte di gestione automatica, in quel caso, il web server risponderà con `{"rsp":"Error: dome in manual mode"}`.
Gli input che l'utente puù dare in modalità manuale sono:

- `AUT/MAN`, selettore della modalità manuale-automatica
- `Accensione quadro elettrico`, pulsante di accensione del quadro comandi che da corrente al motore, alla blindo sbarra e al ProDino del vano
- `Movimentazione oraria`, pulsante di movimentazione oraria
- `Movimentazione anti oraria`, pulsante di movimentazione anti oraria

Tutti i pulsanti sono collegati agli ingressi opto isolati e gestiti dalla funzione `buttonPressed(button, seconds)` che si occupa di controllare se il pulsante è stato premuto, attivare la sirena e ritornare `true` quando il pulsante è stato premuto continuativamente per l'ammontare di secondi specificato come parametro.
### Armo quadri

L'armo quadro elettrico viene gestito dal loop, se la board è in modalità manuale, che controlla se il pulsante "Armo quadro" viene premuto per 2 secondi e, in quel caso, richiama il metodo `switchboardIgnition()` per dare corrente al quadro; in caso di utilizzo automatico il quadro può essere acceso con la richiesta `{"cmd" : "ignite-elettrical-panel"}`, che richiama la medesima funzione.

### Stati di allerta
#### Mancanza di corrente
Sebbene l'intera apparecchiatura di cupola (compresa la blindo sbarra, i motori di rotazione e quelli di apertura/chiusura del vano) sono posti sotto UPS, per evitare di lasciare la cupola aperta e in uno stato incosistente, uno degli ingressi opto isolati della board è stato assegnato al controllo del flusso di corrente a monte del UPS, se questo viene meno, nel `loop` viene triggerata una funzione per comunicare comunicare al vano la chiusura, avvenuta quella richiesta si fa partire la funzione `shutDowun()` per il salvataggio delle variabili.
#### Risveglio della board con errore
DA FARE

## Elenco richieste di rete supportate

Di seguito sono riportate le richieste di rete implementate sulla PLC; tutte le richieste sono di tipo GET.

### Pagine web

- `http://board-ip-address/`: homepage con il riassunto dello stato del vano.

- `http://board-ip-address/webserial`: interfaccia seriale via web (sola lettura).

### API

Per interagire con le API, è necessario fornire un json che specifichi il comando formattato come segue:

```json
{
  "cmd": "command"
}
```

dove, al posto di `command`, si mette il nome del comando di interesse. La risposta sarà del tipo:

```json
{
  "rsp": "response"
}
```

Se il comando è andato a buon fine, la risposta è `done`. Un esempio completo di una richiesta andata a buon fine è il seguente:

```text
HTTP GET REQUEST
  - Full URL: http://board-ip-address/api?json={"cmd":"park"}

RESPONSE
  - Code: 200
  - Type: application/json
  - Body: {"rsp":"done"}
```

#### Comandi

- `abort`: interrompe il movimento (risposta: 200, tipo: `application/json`).

- `cit`: una citazione simpatica (risposta: 200, tipo: `text/plain`).

- `cit2`: un'altra citazione simpatica (risposta: 200, tipo: `text/plain`).

- `find-zeros`: trova gli zeri dell'encoder (risposta: 200, tipo: `application/json`).

- `force-restart`: riavvia la PLC anche se in manuale (hard restart) (risposta: 200, tipo: `application/json`).

- `ignite-elettrical-panel`: accende il quadro elettrico per rotazione e vano (risposta: 200, tipo: `application/json`).

- `park`: parcheggia la cupola (risposta: 200, tipo: `application/json`).

- `reset-EEPROM`: resetta la EEPROM e riavvia la PLC per rendere effettivo il reset (risposta: 200, tipo: `application/json`).

- `restart`: riavvia la PLC se in automatico (graceful restart) (risposta: 200, tipo: `application/json`).

- `set-log-off`: disabilita il log su seriale (risposta: 200, tipo: `application/json`).

- `set-log-on`: abilita il log su seriale (risposta: 200, tipo: `application/json`).

- `slew-to-az`: muovi la cupola all'azimuth specificato, richiede anche la chiave `az-target` (risposta: 200, tipo: `application/json`).

- `turn-off`: spegni i quadri (risposta: 200, tipo: `application/json`).

- `status`: restituisce lo stato del vano (risposta: 200, tipo: `application/json`).

#### Note sul comando status

La riposta del comando `status` è molto estesa e si riporta qui un esempio:

```json
{
  "rsp": {
    "board-name": "CM-dome-shutter",
    "uptime": "0 days, 0 hours, 2 minutes, 33 seconds",
    "shutter-status": 1,
    "movement-status": false,
    "alert-status": false,
    "log": {
      "serial-speed": 115200,
      "web-serial": "/webserial"
    },
    "relay": {
      "opening-motor": false,
      "closing-motor": false
    },
    "optoin": {
      "auto": true,
      "closed-sensor": true,
      "opened-sensor": false
    },
    "internet": {
      "wifi": {
        "status": true,
        "power-saving": 0,
        "hostname": "CM-dome-shutter",
        "mac-address": "24:6F:28:43:70:4C",
        "ssid": "cdf",
        "connection-status": true,
        "ip-address": "192.168.41.85"
      },
      "ethernet": {
        "status": false
      }
    }
  }
}
```

Varie note sulla risposta:

- Il parametro `wifi -> power-saving` _deve_ essere 0: questo indica che la modalità di risparmio energetico è disattivata e che quindi la board ha le migliori prestazioni di rete possibili.

## Note sul codice

Nella funzione `startSlewing(int az_target)` è stato inserito il calcolo della direzione compatto, frutto del codice:

```
if(az_target < current_position){
  if(delta_az > 180)
    direction = CW_DIRECTION;
  else
    direction = CCW_DIRECTION;
} else {
  if(delta_az > 180)
    direction = CCW_DIRECTION;
  else
    direction = CW_DIRECTION;
}
```

Da questo si può notare che la direction è uguale a CW_DIRECTION (quando az_target < current_position) e (delta_az > 180) sono uguali, mentre è CCW_DIRECTION quando sono diversi; quindi il codice può essere semplificato in:
```
direction = (az_target < current_position) == (delta_position >180) ? CW_DIRECTION : CCW_DIRECTION;
```
