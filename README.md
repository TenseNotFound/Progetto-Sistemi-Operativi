# Progetto-Sistemi-Operativi
## Battaglia navale multiutente⚓️

Realizzazione di una versione elettronica del famoso gioco "battaglia navale"
con un numero di giocatori arbitrario. 
In questa versione più processi client (residenti in generale su macchine diverse) sono l'interfaccia tra i
giocatori e il server (residente in generale su una macchina separata dai client). 
Un client, una volta abilitato dal server, accetta come input una mossa, 
la trasmette al server, e riceve la risposta dal server. 
In questa versione della battaglia navale una mossa consiste oltre alle due coordinate
anche nell'identificativo del giocatore contro cui si vuole far fuoco.
Il server a sua volta quando riceve una mossa, comunica ai client se
qualcuno e' stato colpito se uno dei giocatori e' il vincitore (o se e' stato
eliminato), altrimenti abilita il prossimo client a spedire una mossa.
La generazione della posizione delle navi per ogni client e' lasciata alla
discrezione dello studente.      


Il progetto si divide quindi in due versioni, una per lo standard Posix ed un altra per lo standard WINAPI.
Per comodità di programmazione si è optato per la suddivisione dei due scenari: è stata creato un branch per Posix ed uno per WINAPI.

Il server.c può essere compilato sia su un dispositivo in locale, oppure se si volesse usare solo il client, ci si deve collegare alla VPN (ZeroTierOne) che ospita un server fisico che esegue l'eseguibile (Proxmox, container LCX).
