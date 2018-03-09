.PHONY: default
default:
	rm -f bin/oai-client
	cc -std=c99 -Wall -g -o bin/oai-client src/oai-client.c -lexpat -lcurl
