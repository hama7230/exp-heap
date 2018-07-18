
all:
	gcc -fPIC -shared  -o bin/myhook.so src/myhook.c -ldl -Wall -Wextra

heap:
	g++ -Wall -Wextra src/analysis.cpp
