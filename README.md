# Progetto Sistemi operativi

#### Partecipanti:

* Lorenzo Tarantino
* Leonardo Rocco Cicalini

## **Battaglia navale multiutente**

# Specifiche da implementare

Realizzazione di una versione elettronica del famoso gioco "battaglia navale" con un numero di giocatori arbitrario. In questa versione più processi client (residenti in generale su macchine diverse) sono l'interfaccia tra i giocatori e il server (residente in generale su una macchina separata dai client). Un client, una volta abilitato dal server, accetta come input una mossa, la trasmette al server, e riceve la risposta dal server. In questa versione della battaglia navale una mossa consiste oltre alle due coordinate anche nell'identificativo del giocatore contro cui si vuole far fuoco. Il server a sua volta quando riceve una mossa, comunica ai client se qualcuno è stato colpito se uno dei giocatori è il vincitore (o se è stato eliminato), altrimenti abilita il prossimo client a spedire una mossa. La generazione della posizione delle navi per ogni client è lasciata alla discrezione dello studente.


## Compilazione *Istruzioni d'uso*

Per procedere con l’esecuzione dei programmi c’è un MakeFile nella directory principale. Per procedere con la compilazione si deve digitare da terminale:

```bash
make all        # per compilazione standard POSIX di server e client
```

### Server

Il sorgente del server scritto è pensato per **compilazione solo su sistemi POSIX**. Per poter compilare solo il server digitare a terminale:

```bash
make server    # compilazione dell'eseguibile del server
```
Successivamente digitare:
```bash
./battaglia_server
```

Attenzione: qualora si digiti il comando per la compilazione del server, in automatico verrà compilato anche il bot, questo perchè come da codice, ci si aspetta di trovare già un eseguibile nel momento del lancio del bot.

### Client

Il client si divide in due release: una per ambienti POSIX, una per ambienti WINAPI. Per poter compilare si deve digitare:
>**Per ambiente WINAPI**: sviluppo e test sono stati effettuati su shell MSYS2 (MinGW64) tramite compilatore gcc!
```bash
make client      # permette la compilazione dell'eseguibile del client (POSIX)
make wclient     # permette la compilazione dell'eseguibile del client (WINAPI)
```
Per poter avviare il client, come per il server digitare:
```bash
./battaglia_client
```
Oppure se in ambiente WINAPI digitare:
```bash
./battaglia_client.exe
```
## Bot (Modalità extra)

Durante la realizzazione del codice, abbiamo pensato che fosse interessante chiedere all’utente la modalità di gioco, di default è come da specifica, altrimenti si può optare per l’1v1. Il Bot serve proprio a questo, qualora non si colleghi nessun client, (lobby da 1 client) e decide di andare o in 1v1 oppure in default (TcT). Il Bot è pensato per ambiente POSIX in quanto a lanciarlo sarebbe solo il server quindi non avrebbe avuto senso una traduzione in standard WINAPI.

Per poter compilare il Bot digitare:

```bash
make bot        #permette la compilazione dell'eseguibile del bot
```

### Voci extra

Nel Makefile sono presenti istruzioni per la pulizia dell'ambiente e un helper per le informazioni di compilazione:

```bash
make clean      # permette la pulizia degli eseguibili
make help       # permete di visualizzare le istruzioni di compilazione
```

## Documentazione

Il file per la documentazione è disponibile sia nella cartella documentazione (in formato PDF), sia a questo [link](https://docs.google.com/document/d/1d3KcssjBUIB8lWFCOOmsgyalhz3b7z_a-urk2xLyrgk/edit?usp=sharing).