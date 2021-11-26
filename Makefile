game-ai: main.c
	gcc -o chess-ai main.c -I. `mysql_config --cflags --libs` -lpthread