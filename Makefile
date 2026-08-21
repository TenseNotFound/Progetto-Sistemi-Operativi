all: server client

server:
	@gcc src/server/server.c utils/utils.c protocollo/protocollo.c -o battaglia_server -lpthread
	#quando il bot compilerà correttamente va levato il -
	-@gcc src/bot/bot.c utils/utils.c protocollo/protocollo.c -o bot -lpthread
	

client:
	@gcc src/client/client.c utils/utils.c protocollo/protocollo.c gui/gui.c -o battaglia_client -lpthread

bot:
	@gcc src/bot/bot.c utils/utils.c protocollo/protocollo.c -o bot -lpthread

clean:
	rm -f battaglia_server battaglia_client bot

help:
	@echo "Per compilare tutto digitare 'make all'."
	@echo " - Per compilare solo il server digitare 'make server'."
	@echo "	- Per compilare solo il client digitare 'make client'."
	@echo "	- Per compilare solo il bot digitare 'make bot'."