.PHONY: clean
web_server: web_server.c web_server.h
	gcc -o web_server web_server.c
clean:
	rm web_server
