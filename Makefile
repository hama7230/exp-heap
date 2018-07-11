
all:
	gcc -fPIC -shared  -o bin/myhook.so src/myhook.c -ldl 
