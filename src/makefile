# make rule primaria con dummy target ‘all’--> non crea alcun file all ma fa un complete build
# che dipende dai target client e server scritti sotto
all: serv dev
# make rule per il client

dev: device.c utils.h
	gcc -std=c99 device.c  -o dev

serv: server.c utils.h
	gcc -std=c99 server.c  -o serv
# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)
clean:
	rm *o dev serv