game-ai: main.c
	gcc -o chess-ai main.c db.c mem.c chess.c -l sqlite3 -I. -lpthread
