#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <sqlite3.h>
#include "chess.h"
#include "db.h"
#include "mem.h"

void print_board(unsigned char*);
void print_piece(unsigned char);
void print_legal_moves(struct move*);
void end_game(int winner);
struct db_move *get_moves_from_database(const char *serial);
void update_database(const char *serial, const char *move, int player);

extern struct mem_addresses *g_mem_addresses;
extern struct db_move_history g_move_history;
extern struct board_history g_board_history;
extern sqlite3 *db;

bool g_run_program = true;
bool g_kb_int = false;

void *int_thread_func() {
  char input[10];
  while(g_run_program) {
    scanf("%s", input);
    g_kb_int = true;
  }
}
 
int main (int argc, char **argv) {
	int rc;

	FILE *log = fopen("logfile", "w");
	FILE *draw_log = fopen("draw_log", "a");

	rc = init_db();
	
	if (rc != 0) {
		printf("Could not initialize DB, quitting\n");
		return rc;
	}
	srand(time(NULL));
	struct board_state_struct *board_state = (struct board_state_struct*)debug_malloc(sizeof(struct board_state_struct)); 
	unsigned char board[64][3];
	char move_history[20000];
	char user_input[10];
	char serial[100];

	bool interactive_mode = false;
	int num_games = 1;

	printf("Interactive mode? (y/n) ");
	scanf("%s", user_input);
	if (strcmp(user_input, "y") == 0) {
		interactive_mode = true;
	}
	pthread_t *int_thread = NULL;
	if (!interactive_mode) {
		printf("Number of games: ");
		scanf("%s", user_input);
		num_games = atoi(user_input);    
		int_thread = (pthread_t*)malloc(sizeof(pthread_t));
		if (pthread_create(int_thread, NULL, int_thread_func, NULL)) {
		fprintf(stderr, "Couldn't create interrupt thread\n");
		return 1;
		}
	}
	
	time_t start_time = time(NULL);
	int game_cnt = 0;
	
	for (; game_cnt < num_games; game_cnt++) {
		// only interrupt between games
		if (g_kb_int) {
			break;
		}
		board_state->black_king_castle = true;
		board_state->black_queen_castle = true;
		board_state->white_king_castle = true;
		board_state->white_queen_castle = true;
		board_state->en_passant_position = -1;
		board_state->cur_player = 1;
		memcpy(board_state->board_pieces, default_board, 64);
		int i = 0;
		for (int r = 1; r < 9; r++) {
			for (int f = 0; f < 8; f++) {
				board[i][0] = 97 + f;
				board[i][1] = 48 + r;
				board[i][2] = 0;
				i++;
			}
		}
		memset(move_history, 0, 2000);  
		memset(&g_move_history, 0, sizeof(struct db_move_history));
		memset(&g_board_history, 0, sizeof(struct board_history));
		fprintf(log, "Game %i:\n", game_cnt);

		int turn_cnt = 1, last_player = -1;
		bool doloop = true, endgame = false;  
	//  memset(db_move_history, 0, sizeof(db_move_history));
		while (doloop) {
			struct move *legal_moves = get_all_legal_moves(board_state, board_state->cur_player, true, true);          
			struct db_move *moves_from_db = NULL;
			if (last_player != board_state->cur_player) {
	//      printf("Last char: %c\n", move_history[strlen(move_history) - 1]);
				bool in_check = move_history[strlen(move_history) - 1] == '+' || move_history[strlen(move_history) - 1] == '#'; // check_for_check(board_state, board_state->cur_player == 0 ? 1 : 0);    
	//      if (in_check) printf("Check!\n");
				if (legal_moves == NULL) {
					int winner = board_state->cur_player == 1 ? 0 : 1;
					if (in_check) {
						// strcat(move_history, "#");
						printf("Checkmate\n");
						fprintf(log, "Checkmate\n");
					} else {
						printf("Stalemate\n");
						fprintf(log, "Stalemate\n");
						winner = -1;
					}    
					end_game(winner);
					endgame = true;
				} else if (in_check) {
					//strcat(move_history, "+");
				}
				serialize_game_state(board_state, serial);
				strcpy(g_board_history.boards[g_board_history.count].serial, serial);
				g_board_history.count++;   
				
				// check for insufficient material
				bool insuf_mat = true;
				int bishop_col = 0; // 1 = light, 2 = dark
				int knight_count = 0;
				for (int i = 0; i < 64; i++) {
					if (board_state->board_pieces[i] % 8 == BQ ||
							board_state->board_pieces[i] % 8 == BR ||
							board_state->board_pieces[i] % 8 == BP ) {
						// if a queen, rook, or pawn exists, not stalemate
						insuf_mat = false;
						break;
					}
					if (board_state->board_pieces[i] % 8 == BB) {
						if (bishop_col == 0) {
							bishop_col = is_light_square(i) ? 1 : 2;
						} else if (bishop_col != (is_light_square(i) ? 1 : 2)) {
							insuf_mat = false;
							break;            
						}
					}      
					if (board_state->board_pieces[i] % 8 == BN) {
						knight_count++;
						if (knight_count > 1 || (knight_count > 0 && bishop_col != 0)) {
							insuf_mat = false;
							break;
						}
					}        
				}
				
				if (!endgame && insuf_mat) {
					printf("Draw - Insufficient Material\n");
					fprintf(log, "Draw - Insufficient Material\n");
					//fprintf(draw_log, "%s\nInsufficient Material\n", move_history);
					end_game(-1);
					endgame = true;
				}

				// check for threefold repetition
				if (!endgame) {
					for (int i = 0; i < g_board_history.count; i++) {
						int count = 0;
						bool end = false;
						for (int j = 0; j < g_board_history.count; j++) {
	//	        printf("comparing: %s - %s\n", &(g_board_history.boards[i].serial[1]), &(g_board_history.boards[j].serial[1]));
							if (strcmp(&(g_board_history.boards[i].serial[1]), &(g_board_history.boards[j].serial[1])) == 0) {                    
								count++;
								if (count >= 3) {
									end = true;
									break;
								}
							}        
						}    
	//      printf("count: %i\n", count);
						if (end) {
							printf("Draw - Threefold Repetition\n");
							fprintf(log, "Draw - Threefold Repetition\n");
							//fprintf(draw_log, "%s\nThreefold Repetition\n", move_history);
							end_game(-1);
							endgame = true;
							break;
						}
					}
				}
			}    
			last_player = board_state->cur_player;
			// apparently, some tournaments have a 50 turn maximum game, so let's use that
			// also, it should help keep the database smaller, since most (useless) moves are in the 50-200 turn range
			if (!endgame && turn_cnt > 50) {
	//      doloop = false;
				printf("Draw - 50 Turns\n");
				fprintf(log, "Draw - 50 Turns\n");
				//fprintf(draw_log, "%s\n200 Turns\n", move_history);
				end_game(-1);
				endgame = true;
			}

			
			
	//    printf("%s\n", move_history);
			// fprintf(log, "%s\n", move_history);
			
	/*    for (int h = 0; h < g_move_history.count; h++) {
				printf("%s - %s - %i\n", g_move_history.moves[h].serial, g_move_history.moves[h].move, g_move_history.moves[h].player);
			}*/
			print_board(board_state->board_pieces);
			printf("\n%s player's turn:\n", board_state->cur_player == 1 ? "White" : "Black");
	//    print_legal_moves(legal_moves);

	/*    for (struct move *int s = 0; s < 34; s++) {
				printf("%i ", (int)serial[s]);      
			}*/
			//printf("\n");//game state: %s\n", serial);
			if (endgame) {
				doloop = false;
			//  break;    
			} else {
				if (interactive_mode) {
					printf("Enter move: ");
					scanf("%s", user_input);
				}
	//    int t1 = time(NULL);
	//    while (time(NULL) < t1 + 1) { }
				if (interactive_mode && strcmp(user_input, "q") == 0) {
					doloop = false;
				} else if (!interactive_mode || (interactive_mode && strcmp(user_input, "c") == 0)) {
					fprintf(log, "%s: ", board_state->cur_player == 1 ? "White" : "Black");
					int num_new_moves = 0, move_cnt = 0;
					char move_choice[10];        
					moves_from_db = get_moves_from_database(serial);
	//        printf("got moves from db\n");
	//        for (struct db_move *cur2 = moves_from_db; cur2 != NULL; cur2 = cur2->next) {
	//          fprintf(log, "m: %s has a win rate of %f\n", cur2->move, cur2->win_rate);
	//        }
	//        printf("logged moves\n");

					if (moves_from_db != NULL && moves_from_db->win_rate > .5) {
						// printf("%s has a win rate of %f\n", moves_from_db->move, moves_from_db->win_rate);
						sprintf(move_choice, "%s", moves_from_db->move);          
						fprintf(log, "+++ ");
					} else {
						struct move *new_moves = NULL;
						move_cnt = 0;
						for (struct move *cur = legal_moves; cur != NULL; cur = cur->next) {
							bool keep_looking = true;
							for (struct db_move *cur2 = moves_from_db; keep_looking && cur2 != NULL; cur2 = cur2->next) {
								if (strcmp(cur2->move, cur->text) == 0) {
									keep_looking = false;
								}
							}
							if (keep_looking) {
								struct move *temp = (struct move*)debug_malloc(sizeof(struct move));
								strcpy(temp->text, cur->text);
								temp->next = new_moves;
								new_moves = temp;
								num_new_moves++;
							}
						}
						if (new_moves == NULL && moves_from_db != NULL) {
							sprintf(move_choice, "%s", moves_from_db->move);      
							fprintf(log, "+   ");
						} else {
							int move_index = rand() % num_new_moves;            
							for (struct move *cur = new_moves; cur != NULL; cur = cur->next) {
								if (move_cnt == move_index) {
									sprintf(move_choice, "%s", cur->text);          
									fprintf(log, "++  ");
									break;                    
								}          
								move_cnt++;        
							}
						}
						for (struct move *cur = new_moves; cur != NULL;) {
							struct move *temp = cur->next;
							debug_free(cur);
							cur = temp;
						}
					}
	//        printf("move chosen: %s\n", move_choice);
					fprintf(log, "%s\n", move_choice);
					if (execute_move(move_choice, legal_moves, board_state, board_state->cur_player)) {            
	//          printf("move executed\n");
						if (strlen(move_history) > 0) {
							strcat(move_history, " ");
	//            printf("added a space\n");
						}          
						if (board_state->cur_player == 1) {
							char t[10];
							sprintf(t, "%i. ", turn_cnt);
	//            printf("sprintf turn_cnt\n");
							strcat(move_history, t);
	//            printf("strcat turn_cnt\n");
						}
	//          printf("aboute to run update_databasae\n");
						update_database(serial, move_choice, board_state->cur_player);
	//          printf("g_history updated\n");
						strcat(move_history, move_choice);
	//          printf("history updated\n");
						if (board_state->cur_player == 1) {
							board_state->cur_player = 0;
							turn_cnt++;
						} else {
							board_state->cur_player = 1;
						}
	//          printf("player turn finished\n");
					}       
				} else {
					if (execute_move(user_input, legal_moves, board_state, board_state->cur_player)) {            
						if (strlen(move_history) > 0) {
							strcat(move_history, " ");
						}          
						if (board_state->cur_player == 1) {
							char t[10];
							sprintf(t, "%i. ", turn_cnt);
							strcat(move_history, t);
						}
						update_database(serial, user_input, board_state->cur_player);
						strcat(move_history, user_input);
						if (board_state->cur_player == 1) {
							board_state->cur_player = 0;
							turn_cnt++;
						} else {
							board_state->cur_player = 1;
						}
					}       
				}
			}
			// clean up legal moves
			for (struct move *cur = legal_moves; cur != NULL;) {
				struct move *temp = cur->next;
				debug_free(cur);
				cur = temp;
			}
			// clean up database moves
			for (struct db_move *cur = moves_from_db; cur != NULL;) {
				struct db_move *temp = cur->next;
				debug_free(cur);
				cur = temp;
			}
		}




	}






	time_t elapsed_seconds = time(NULL) - start_time;
	int hours = (int)floor(elapsed_seconds / 3600);
	int minutes = (int)floor(elapsed_seconds % 3600 / 60);
	int seconds = elapsed_seconds % 3600 % 60;
	
	printf("%i game%s completed in %02i:%02i:%02i\n", game_cnt, game_cnt != 1 ? "s" : "", hours, minutes, seconds);
	

	debug_free(board_state);
	fclose(log);
	fclose(draw_log);
	
	log = fopen("mem_log", "w");  
	for (struct mem_addresses *cur = g_mem_addresses, *prev = NULL; cur != NULL; cur = cur->next) {  
		fprintf(log, "%s", cur->text); 
	}
	fclose(log);
	
	g_run_program = false;
	if (int_thread) {
		printf("Type a character and press enter...\n");
		if (pthread_join(*int_thread, NULL)) {
		fprintf(stderr, "Error joining interrupt thread\n");
		return 2;
		}
	}

	return 0;
}

void print_piece(unsigned char piece) {
  unsigned char masked_piece = piece & 7;
  char output = ' ';
  if (masked_piece == 1) {
    output = 'P';
  } else if (masked_piece == 2) {
    output = 'R';
  } else if (masked_piece == 3) {
    output = 'N';
  } else if (masked_piece == 4) {
    output = 'B';
  } else if (masked_piece == 5) {
    output = 'Q';
  } else if (masked_piece == 6) {
    output = 'K';
  }
  printf(" %c ", output);  
}

void print_board(unsigned char *board_pieces) {
  printf("\n");
  bool is_light = true;
  char color[12];
  for (int r = 8; r >= 0; r--) {
    if (r > 0) {
      printf(" %c ", r + 48);
    } else {
      printf("   ");
    }
    for (int f = 0; f < 8; f++) {
      if (r > 0) {
        if (is_light == true) {
          strcpy(color, "88;120;56");
        } else {
          strcpy(color, "120;88;56");
        }
        printf("\x1b[48;2;%sm", color);
        if (board_pieces[f + (r - 1) * 8] < 8) {
          printf("\x1b[38;2;0;0;0m");
        }
        if (board_pieces[f + (r - 1) * 8] == 0) {
          printf("   ");
        } else {
          print_piece(board_pieces[f + (r - 1) * 8]);
        }        
        printf("\x1b[0m");
        is_light = !is_light;
      } else {
        printf("%c  ", f % 8 + 97);
      }
    }
    is_light = !is_light;
    printf("\n");
  }
  printf("\n");  
}

void print_legal_moves(struct move *move_list) {
  printf("Legal moves: ");
  for (struct move *cur = move_list; cur != NULL; cur = cur->next) {
    printf("%s ", cur->text);
  }
  printf("\n");
}

void end_game(int winner) {
  char query[512];
  for (int i = 0; i < g_move_history.count; i++) {
    bool skip = false;
    for (int j = 0; j < i; j++) {
      if (strcmp(g_move_history.moves[i].move, g_move_history.moves[j].move) == 0 &&
          strcmp(g_move_history.moves[i].serial, g_move_history.moves[j].serial) == 0) {
        skip = true;
        break;
      }
    }
    if (skip) {
      continue;
    }
    char win_lose_draw[10];
    if (winner == -1) {
      sprintf(win_lose_draw, "draws");
    } else if (g_move_history.moves[i].player == winner) {
      sprintf(win_lose_draw, "wins");
    } else {
      sprintf(win_lose_draw, "losses");
    }
    sprintf(query, "INSERT INTO chess_positions (serial, move, %s) VALUES ('%s', '%s', 1) "
                   "ON CONFLICT(serial, move) DO UPDATE SET %s = %s + 1",
            win_lose_draw, g_move_history.moves[i].serial, g_move_history.moves[i].move,
            win_lose_draw, win_lose_draw);
    exec_query(query);
  }
}

struct db_move *get_moves_from_database(const char *serial) {
  struct db_move *moves_from_db = NULL, *temp = NULL;
  char query[512];
  sqlite3_stmt *stmt;

  sprintf(query, "SELECT move, CASE WHEN wins + losses + draws > 0 "
                 "THEN (wins + 0.5 * draws) / (wins + losses + draws) ELSE 0 END as win_rate "
                 "FROM chess_positions WHERE serial = '%s' ORDER BY win_rate ASC", serial);

  if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
    return NULL;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    temp = (struct db_move*)debug_malloc(sizeof(struct db_move));
    sprintf(temp->move, "%s", sqlite3_column_text(stmt, 0));
    temp->win_rate = sqlite3_column_double(stmt, 1);
    temp->next = moves_from_db;
    moves_from_db = temp;
  }

  sqlite3_finalize(stmt);
  return moves_from_db;
}

void update_database(const char *serial, const char *move, int player) {
  strcpy(g_move_history.moves[g_move_history.count].serial, serial);
  strcpy(g_move_history.moves[g_move_history.count].move, move);
  g_move_history.moves[g_move_history.count].player = player;
  g_move_history.count++;
}

