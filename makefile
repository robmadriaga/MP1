#makefile
all: crsd crc

crsd: crsd.c
	g++ -g -w -std=c++14 -o crsd crsd.c -lpthread -lrt

crc: crc.c 
	g++ -g -w -std=c++14 -o crc crc.c -lpthread -lrt

clean:
	rm -rf *.o crsd crc