all: server client

server:
	@gcc src/server/server.c utils/utils.c protocollo/protocollo.c -o battaglia_server -lpthread
	#quando il bot compilerà correttamente va levato il -
	-@gcc src/bot/bot.c utils/utils.c protocollo/protocollo.c -o bot -lpthread
	

client:
	@gcc src/client/client.c utils/utils.c protocollo/protocollo.c gui/gui.c -o battaglia_client -lpthread

wclient:
	@gcc src/client/client.c utils/utils.c protocollo/protocollo.c gui/gui.c -o battaglia_client.exe -lws2_32 -lpthread

bot:
	@gcc src/bot/bot.c utils/utils.c protocollo/protocollo.c -o bot -lpthread

clean:
	rm -f battaglia_server battaglia_client battaglia_client.exe bot

help:
	@echo "=================================================================="
	@echo "                         ISTRUZIONI D'USO                         "
	@echo "=================================================================="
	@echo "  make all       - Compila tutto il progetto (client, server, bot)"
	@echo "  make server    - Compila sia il server che il bot"
	@echo "  make client    - Compila solo il client (POSIX)"
	@echo "  make wclient   - Compila solo il client (WINAPI)"
	@echo "  make bot       - Compila solo il bot"
	@echo "  make clean     - Rimuove tutti gli eseguibili compilati"
	@echo "=================================================================="