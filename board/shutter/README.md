# Documentation

⚠️ **WARNING**: need translation.

Viene usata la PLC [KMP PRODINo ESP32 Ethernet v1](https://kmpelectronics.eu/products/prodino-esp32-ethernet-v1/) come server di controllo del movimento del vano.

![KMP PRODINo ESP32 Ethernet v1](https://kmpelectronics.eu/wp-content/uploads/2019/11/ProDinoESP32_E_5.jpg)

## Descrizione cartelle e file

- `data/`. File che andranno nella SPIFFS, tipicamente pagine web.

  - `dashboard.html` e `dashboard.js`. Contengono l'html e il relativo codice javascript per la dashboard di controllo della board (comandi principali e avanzati, panoramica dello stato).

  - `webserial.html` e `webserial.js`. Contengono l'html e il relativo codice javascript per l'interfaccia WebSerial (vedi dopo).

  - `style.css`. Contiene le regole di formattazione comuni alle pagine web.

- `examples/`. Esempi e prove varie. Fa tenerezza.

- `include/`

  - `global_definitions.h`. Contiene le definizione delle variabili globali e le `#define` globali che servono nel codice. Le variabili sono dichiarate come `extern` per poterle usare nel vari file che si trovano nella cartella `src`; sono invece inizializzate nel file `src/main.cpp`. Tutti gli import sono inseriti qui.

- `lib/`

  - `ProDinoESP32`, versione 1.1.3, [repository ufficiale](https://github.com/kmpelectronics/Arduino/tree/master/ProDinoESP32). Libreria per usare le funzionalità della scheda, come relay, optoin, ethernet, ...

  - `WebSerial`, versione 1.0.0, ispirata a [questa](https://github.com/ayushsharma82/WebSerial) ma pesantemente modificata per migliorarne le funzionalità. Emula una interfaccia seriale mediante una websocket per permettere debugging da remoto.

- `src/`

  - `auxiliary_functions.cpp`. Contiene le funzioni ausiliarie, come la connessione al Wi-Fi, il lampeggio del led, ...

  - `main.cpp`. Contiene il core del codice: inizializzazione delle variabili, setup, loop ed eventuali task secondari.

  - `web_server.cpp`. Contiene il web server con tutte le sue chiamate.

## Funzionamento e logica

### Stato del vano e della PLC

Lo stato globale può essere controllato dalla pagina web o con il comando `status`.

La board determina lo stato del vano mediante i sensori di finecorsa e lo stato di accensione dei relè nel seguente ordine:

- Se uno dei due relè è acceso, il vano viene considerato in apertura o in chiusura in base a quale relè sta funzionando. Questa modalità funziona esclusivamente se il vano è in modalità automatica, in quanto in modalità manuale i relè vengono bypassati e non c'è modo di sapere se il vano si stia muovendo o no.

- Se uno dei due finecorsa è chiuso e i relè sono spenti, il vano viene considerato chiuso o aperto in base a quale finecorsa è chiuso.

- Se nessun finecorsa è chiuso e i relè sono spenti, il vano viene considerato parzialmente aperto.

### Apertura e chiusura

L'apertura e la chiusura del vano vengono comandate con i comandi `open` e `close`. Queste richieste hanno il solo compito di accendere i relè di controllo dei motori. Il controllo successivo dello stato del moto viene svolto nel loop. Quando uno dei due sensori di finecorsa (apertura o chiusura) viene raggiunto, la board attende qualche secondo per completare l'apertura/chiusura e poi spegne i relè.

Per fermare in anticipo il movimento, usare il comando `abort`.

Dare un comando di apertura mentre il vano è in chiusura ferma il vano e lo apre (e viceversa).

### Stato di allerta del vano

#### Allerta hardware

Lo stato di allerta hardware prevede il blocco totale del controllo remoto di apertura e chiusura del vano, che permane anche in caso di riavvio della board o caricamento di un nuovo codice in quanto lo stato viene salvato nella EEPROM.

Se la board rileva che i relè rimangono accesi per più tempo del necessario (circa 20 secondi, il tempo di una apertura o chiusura totale), allora ferma tutto ed entra in stato di allerta; in tal caso, infatti, è molto probabile che si siano verificati problemi meccanici al vano (come la rottura di un anello delle catene di trasmissione dei motori) e quindi è vitale fermare i motori; nota che tuttavia condizioni avverse come la presenza di ghiaccio possono rallentare il movimento del vano, causando lo scatto dello stato di allerta (non dovrebbe succedere perché i tempi sono stati calibrati, ma non si sa mai).

Lo stato di allerta viene attivato anche se il vano non disattiva il finecorsa di chiusura mentre si apre (o viceversa) entro qualche secondo.

Lo stato di allerta può essere controllato con il comando `status` e l'unico modo per resettarlo è usare il comando `reset-alert-status`. È essenziale che lo stato di allerta venga resettato _solo dopo attenti controlli sullo stato della meccanica del vano_.

#### Allerta di rete

Se il vano rileva l'assenza di rete internet (sia LAN che WAN) per più di dieci minuti, entra il allerta network: questo stato prevede il blocco totale del controllo remoto di apertura e chiusura del vano, nonché la chiusura immediata del vano nel caso questo sia aperto. La rilevazione di rete viene fatta mediante un ping a `www.google.com`.

Questo stato di allerta è necessario per garantire la sicurezza della strumentazione in cupola nel caso in cui venga a mancare internet nel bel mezzo a una sessione di osservazione remota. Per questo motivo, non è possibile fare l'override di questo comando o resettarne manualmente lo stato.

È comunque possibile muovere il vano ponendolo in modalità manuale e muovendolo con i pulsanti sul quadro. _Attenzione, il controllo dello stato di allerta è continuo: se si apre il vano in manuale e poi si mette l'interruttore in automatico, il vano si chiuderà nuovamente._

Esiste in realtà un trick per eseguire l'override, che necessita di giocare con il restart della board e poi rimuovere l'alimentazione alla blindo sbarra, ma è _estremamente sconsigliato_ e la sua esistenza non deve uscire da questo README.

### Controllo automatico-manuale e input utente

La cupola è controllabile da remoto solamente se l'interruttore automatico-manuale è posizionato su "automatico": in questo caso, i comandi manuali vengono bloccati, permettendo unicamente il controllo remoto. Viceversa, se l'interruttore si trova su "manuale", il controllo remoto con la board viene bloccato, permettendo solo il controllo manuale con i pulsanti. La board monitora lo stato dell'interruttore usando un ingresso ottico.

## Comunicazione con la board

È possibile interagire con la board usando:

- **Pagina web** alla route principale `/`; autenticazione richiesta.

- **Seriale web** alla route `/webserial`; autenticazione richiesta.

- **API** alla route `/api`.

### Descrizione API

Le API sono accessibili mediante richieste http GET del tipo:

```http
/api?json={"cmd":"command"}
```

dove i possibili valori di `command` sono:

- Funzioni shutter-related:

  - `abort`: interrompe il movimento.
  - `close`: comando di chiusura del vano.
  - `open`: comando di apertura del vano.

- Gestione board:

  - `reset-alert-status`: resetta lo stato di allerta del vano a `false`.
  - `reset-EEPROM`: resetta la EEPROM e riavvia la PLC per rendere effettivo il reset.
  - `restart`: riavvia la PLC (graceful restart).
  - `force-restart`: riavvia la PLC (hard restart).
  - `status`: restituisce lo stato del vano.

- Easter egg:

  - `cit1`: una citazione simpatica (tipo: `text/plain`).
  - `cit2`: un'altra citazione simpatica (tipo: `text/plain`).

La risposta (ad eccezione dei casi indicati) sarà in JSON del tipo:

```json
{
  "rsp": "message"
}
```

dove i possibili valori di `message` sono:

- `done` nel caso di richiesta andata a buon fine.
- un messaggio riportante il tipo di errore riscontrato.

La riposta del comando `status` è invece più estesa e si riporta qui un esempio:

```json
{
    "rsp":{
        "firmware-version":"v0.1.1",
        "uptime":"0 days, 0 hours, 17 minutes, 36 seconds",
        "dome-azimuth":90,
        "target-azimuth":90,
        "movement-status":false,
        "in-park":true,
        "finding-park":false,
        "finding-zero":false,
        "relay":{
            "cw-motor":false,
            "ccw-motor":false,
            "switchboard":false
        },
        "optoin":{
            "auto":true,
            "switchboard-status":true,
            "auto-ignition":false,
            "ac-presence":true,
            "manual-cw-button":false,
            "manual-ccw-button":false,
            "manual-ignition":false
        },
        "wifi":{
            "hostname":"CM-dome",
            "mac-address":"C8:2B:96:A0:90:40"
        }
    }
}
```

Da notare che il parametro `shutter-status` indica lo stato del vano, i cui possibili valori sono:

- `-1`: vano parzialmente aperto
- `0`: vano aperto
- `1`: vano chiuso
- `2`: vano in apertura (solo se in automatico)
- `3`: vano in chiusura (solo se in automatico)
