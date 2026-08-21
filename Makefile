all:
	server client

server:
	gcc src/server/server.c utils/utils.c protocollo/protocollo.c -o battaglia_server -lpthread

client:
	gcc src/client/client.c utils/utils.c protocollo/protocollo.c gui/gui.c -o battaglia_client -lpthread

bot:
	gcc src/bot/bot.c utils/utils.c protocollo/protocollo.c -o bot -lpthread

clean:
	rm -f battaglia_server battaglia_client bot

help:
	@echo "\n
		   Per compilare tutto digitare 'make all'. \n
		   - Per compilare solo il server digitare 'make server'\n
		   - Per compilare solo il client digitare 'make client'\n
		   - Per compilare solo il bot digitare 'make bot'\n"