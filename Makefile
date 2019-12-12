# build an executable named server from server.c
all:
	gcc -Wall dfc.c -o dfc -g -lcrypto -lssl
	gcc -Wall dfs.c -o dfs -g -lcrypto -lssl

dfc:
	gcc -Wall dfc.c -o dfc -g -lcrypto -lssl

dfs:
	gcc -Wall dfs.c -o dfs -g -lcrypto -lssl

clean: 
	$(RM) dfc dfs
	$(RM) -r *.dSYM
	$(RM) -r Client
	$(RM) -r DFS*
