# Progetto Sistemi operativi

#### Partecipanti:

* Lorenzo Tarantino
* Leonardo Rocco Cicalini

## **Battaglia navale multiutente⚓️**

## Specifiche da implementare

> Realizzazione di una versione elettronica del famoso gioco "battaglia navale" con un numero di giocatori arbitrario. In questa versione più processi client (residenti in generale su macchine diverse) sono l'interfaccia tra i giocatori e il server (residente in generale su una macchina separata dai client). Un client, una volta abilitato dal server, accetta come input una mossa, la trasmette al server, e riceve la risposta dal server. In questa versione della battaglia navale una mossa consiste oltre alle due coordinate anche nell'identificativo del giocatore contro cui si vuole far fuoco. Il server a sua volta quando riceve una mossa, comunica ai client se qualcuno è stato colpito se uno dei giocatori è il vincitore (o se è stato eliminato), altrimenti abilita il prossimo client a spedire una mossa. La generazione della posizione delle navi per ogni client è lasciata alla discrezione dello studente.
> 

## Compilazione *HOW-TO-USE*

Per procedere con l’esecuzione dei programmi c’è un MakeFile nella directory principale. Per procedere con la compilazione si deve digitare da terminale:

```makefile
	make all        # per per compilazione standard POSIX di server e client
	make winapi     # per compilazione standard WINAPI
```

### Server

Il sorgente del server scritto è pensato per **compilazione solo su sistemi POSIX**. Per poter compilare solo il server digitare a terminale:

```makefile
	make server    # compilazione dell'eseguibile del server
```

Attenzione: qualora si digiti il comando per la compilazione del server, in automatico viene compilato anche il bot, questo perchè come da codice, ci si aspetta di trovare già un eseguibile nel momento del lancio del bot.

### Client

Il client si divide in due release: una per ambienti POSIX, una per ambienti WINAPI. Per poter compilare si deve digitare:

```makefile
    make client      # permette la compilazione dell'eseguibile del client (POSIX)
    make wiclient    # permette la compilazione dell'eseguibile del client (WINAPI)
```

## Bot (Modalità extra)

Durante la realizzazione del codice, abbiamo pensato che fosse interessante chiedere all’utente la modalità di gioco, di default è come da specifica, altrimenti si può optare per l’1v1. Il Bot serve proprio a questo, qualora non si colleghi nessun client, (lobby da 1 client) e decide di andare o in 1v1 oppure in default (TcT). Il Bot è pensato per ambiente POSIX in quanto a lanciarlo sarebbe solo il server quindi non avrebbe avuto senso una traduzione in standard WINAPI.

Per poter compilare il Bot digitare:

```makefile
    make bot        #permette la compilazione dell'eseguibile del bot
```

### Voci extra

Nel Makefile sono presenti istruzioni per la pulizia dell'ambiente e un helper per le informazioni di compilazione:

```makefile
    make clean      # permette la pulizia degli eseguibili
    make help       # permete di visualizzare le istruzioni di compilazione
```

## Documentazione

I file per la documentazione sono disponibili sia nella cartella documentazione (in formato PDF), sia al link documentazione.
