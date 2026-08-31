# Progetto Sistemi Operativi - A.A. 2025/2026
# Battaglia navale multiutente
# Lorenzo Tarantino - Leonardo Rocco Cicalini

all: server client

server:
	@gcc src/server/server.c utils/utils.c protocollo/protocollo.c -o battaglia_server -lpthread
	@gcc src/bot/bot.c utils/utils.c protocollo/protocollo.c -o battaglia_bot
	@echo "==> Server e Bot pronti per l'esecuzione"

client:
	@gcc src/client/client.c utils/utils.c protocollo/protocollo.c gui/gui.c -o battaglia_client
	@echo "==> Client (POSIX) pronto per l'esecuzione"

wclient:
	@gcc src/client/client.c utils/utils.c protocollo/protocollo.c gui/gui.c -o battaglia_client.exe -lws2_32
	@echo "==> Client (WINAPI) pronto per l'esecuzione"

bot:
	@gcc src/bot/bot.c utils/utils.c protocollo/protocollo.c -o battaglia_bot
	@echo "==> Bot pronto per l'esecuzione"

clean:
	@rm -f battaglia_server battaglia_client battaglia_client.exe battaglia_bot _user
	@echo "==> Eseguibili rimossi correttamente"

help:
	@echo "=================================================================="
	@echo "                         ISTRUZIONI D'USO                         "
	@echo "=================================================================="
	@echo "  make all       - Compila server, client e bot per standard POSIX"
	@echo "  make server    - Compila sia il server che il bot"
	@echo "  make client    - Compila solo il client (POSIX)"
	@echo "  make wclient   - Compila solo il client (WINAPI)"
	@echo "  make bot       - Compila solo il bot"
	@echo "  make clean     - Rimuove tutti gli eseguibili compilati"
	@echo "  make help      - Mostra questo elenco di comandi"
	@echo "=================================================================="