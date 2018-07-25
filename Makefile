
all:
	gcc -fPIC -shared  -o bin/myhook.so src/myhook.c -ldl -Wall -Wextra

heap:
	g++ -std=c++11 -o bin/heap src/analysis.cpp -Wall -Wextra
