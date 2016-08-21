all:
	g++  -pthread server-mt.c -o server
	gcc -w -pthread multi-client.c  -o multi-client

clean:
	rm multi-client server
