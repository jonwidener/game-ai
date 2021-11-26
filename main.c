#include <pthread.h>
#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#define EE 0
#define BP 1
#define BR 2
#define BN 3
#define BB 4
#define BQ 5
#define BK 6
#define WP 9
#define WR 10
#define WN 11
#define WB 12
#define WQ 13
#define WK 14

// database create statement
/*
CREATE TABLE `chess_positions` (
  `serial` char(70) NOT NULL,
  `move` varchar(7) NOT NULL,
  `wins` int(11) NOT NULL DEFAULT '0',
  `losses` int(11) NOT NULL DEFAULT '0',
  `draws` int(11) NOT NULL DEFAULT '0',
  UNIQUE KEY `serial_move` (`serial`,`move`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
*/


#define debug_malloc(X) my_debug_malloc( X, __FILE__, __LINE__, __FUNCTION__)
#define debug_free(X) my_debug_free( X )

struct mem_addresses {
  void *ptr;
  char text[100];
  struct mem_addresses *next;
} *g_mem_addresses = NULL;


void* my_debug_malloc(size_t size, const char *file, int line, const char *func)
{
  void *p = malloc(size);
    
  //printf("Allocated = %s, %i, %s, %p[%li]\n", file, line, func, p, size);

  //Link List functionality goes in here
  struct mem_addresses *new_address = (struct mem_addresses*)malloc(sizeof(struct mem_addresses));
  new_address->ptr = p;
  sprintf(new_address->text, "Allocated = %s, %i, %s, %p[%li]\n", file, line, func, p, size);
  new_address->next = g_mem_addresses;
  g_mem_addresses = new_address;    

  return p;
}

void my_debug_free(void *ptr) {  
  if (g_mem_addresses != NULL) {
    for (struct mem_addresses *cur = g_mem_addresses, *prev = NULL; cur != NULL; cur = cur->next) {  
      if (cur->ptr == ptr) {
        if (prev == NULL) {
          g_mem_addresses = cur->next;
        } else if (prev != NULL) {
          prev->next = cur->next;
        }      
        free(cur->ptr);
        free(cur);
        break;
      }
      prev = cur;
    }
  }
}




struct board_state_struct {
  bool black_king_castle, black_queen_castle;
  bool white_king_castle, white_queen_castle;
  int en_passant_position, cur_player;
  unsigned char board_pieces[64];
};

struct move {
  char text[10];
  int old_position, new_position;
  struct move* next;
};

struct db_move {
  int player;
  char serial[100], move[10]; 
  double win_rate;
  struct db_move *next;
};

struct db_move_history {
  int count;
  struct db_move moves[400];  
} g_move_history;

struct board_serial {
  char serial[100];
};

struct board_history {
  int count;
  struct board_serial boards[400];
} g_board_history;

static unsigned char default_board[] = {
  WR, WN, WB, WQ, WK, WB, WN, WR,
  WP, WP, WP, WP, WP, WP, WP, WP,
  EE, EE, EE, EE, EE, EE, EE, EE,   
  EE, EE, EE, EE, EE, EE, EE, EE, 
  EE, EE, EE, EE, EE, EE, EE, EE, 
  EE, EE, EE, EE, EE, EE, EE, EE, 
  BP, BP, BP, BP, BP, BP, BP, BP, 
  BR, BN, BB, BQ, BK, BB, BN, BR
};

/*
static unsigned char default_board[] = {
  EE, EE, EE, EE, BK, EE, EE, EE,
  EE, EE, EE, EE, EE, EE, EE, EE,
  EE, EE, EE, EE, EE, EE, EE, EE,   
  EE, EE, EE, WB, BB, EE, EE, BB, 
  EE, EE, EE, EE, WK, EE, EE, EE, 
  EE, EE, EE, EE, EE, EE, EE, EE, 
  EE, EE, EE, EE, EE, EE, EE, EE, 
  EE, EE, EE, EE, EE, EE, EE, EE
};*/



struct move *get_all_legal_moves(struct board_state_struct*, int, bool, bool);
struct move *get_legal_moves(struct move*, struct board_state_struct*, int, int);
struct move *get_pawn_legal_moves(struct move*, struct board_state_struct*, int, int);
struct move *get_rook_legal_moves(struct move*, struct board_state_struct*, int, int);
struct move *get_knight_legal_moves(struct move*, struct board_state_struct*, int, int);
struct move *get_bishop_legal_moves(struct move*, struct board_state_struct*, int, int);
struct move *get_queen_legal_moves(struct move*, struct board_state_struct*, int, int);
struct move *get_king_legal_moves(struct move*, struct board_state_struct*, int, int);

int string_to_position(char*);
void position_to_string(char*, int);
void print_board(unsigned char*);
void print_piece(unsigned char);
void print_legal_moves(struct move*);
bool execute_move(const char*, struct move*, struct board_state_struct*, int);
struct move *create_move(struct move*, char*, int, int, unsigned char);
struct move *disambiguate_moves(struct move*);
bool create_standard_move(struct move**, int, int, int, unsigned char, unsigned char);
bool check_for_check(struct board_state_struct*, int);
void serialize_game_state(struct board_state_struct*, char*);
void update_database(MYSQL*, const char*, const char*, int);
void end_game(MYSQL*, int);
struct db_move *get_moves_from_database(MYSQL*, const char*);
bool is_light_square(int);

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
  MYSQL *conn;
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  char *server = "127.0.0.1";
  char *user = "pi";
  char *password = "raspberry";
  char *database = "game_ai";
  
  FILE *log = fopen("logfile", "w");
  FILE *draw_log = fopen("draw_log", "a");
  
  conn = mysql_init(NULL);

  /* Connect to database */
  if (!mysql_real_connect(conn, server,
         user, password, database, 0, NULL, 0)) {
    fprintf(stderr, "Connection failed: %s\n", mysql_error(conn));
    exit(1);
  }

  /* send SQL query */
/*  if (mysql_query(conn, "show tables")) {
    fprintf(stderr, "%s\n", mysql_error(conn));
    exit(1);
  }

  res = mysql_use_result(conn);
*/
  /* output table name */
  /* printf("MySQL Tables in mysql database:\n");
  while ((row = mysql_fetch_row(res)) != NULL) {
    printf("%s \n", row[0]);
  }*/

  
  srand(time(NULL));
  struct board_state_struct *board_state = (struct board_state_struct*)debug_malloc(sizeof(struct board_state_struct)); 
  unsigned char board[64][3];
  char move_history[20000];
  char user_input[10];
  char serial[100];
//  struct db_move db_move_history[400];

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
  
  
  /*                                                                                                    */
  /*************************************** Big Loop *****************************************************/
  /*                                                                                                    */
  for (; game_cnt < num_games; game_cnt++) {
  
  // only interrupt between games
  if (g_kb_int) {
    break;
  }
  
  
  // build board
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
        end_game(conn, winner);
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
        end_game(conn, -1);
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
            end_game(conn, -1);
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
      end_game(conn, -1);
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
        moves_from_db = get_moves_from_database(conn, serial);
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
          update_database(conn, serial, move_choice, board_state->cur_player);
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
          update_database(conn, serial, user_input, board_state->cur_player);
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
  
  /*                                                                                                    */
  /*************************************** End Big Loop *************************************************/
  /*                                                                                                    */
  }
  
  time_t elapsed_seconds = time(NULL) - start_time;
  int hours = (int)floor(elapsed_seconds / 3600);
  int minutes = (int)floor(elapsed_seconds % 3600 / 60);
  int seconds = elapsed_seconds % 3600 % 60;
  
  printf("%i game%s completed in %02i:%02i:%02i\n", game_cnt, game_cnt != 1 ? "s" : "", hours, minutes, seconds);
  

  debug_free(board_state);
  mysql_close(conn);
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

struct db_move *get_moves_from_database(MYSQL *conn, const char *serial) {
  struct db_move *moves_from_db = NULL, *temp = NULL;
  char query[255];
  sprintf(query, "select move, if (wins + losses + draws > 0, ((wins + .5 * draws ) / (wins + losses + draws)), 0) as win_rate from chess_positions where serial = '%s' order by win_rate asc", serial);
  if (mysql_query(conn, query)) {
    printf("%s\n", query);
    fprintf(stderr, "%s\n", mysql_error(conn));
    exit(1);
  }

  //printf("serial: %s\n", serial);
  MYSQL_RES *res = mysql_use_result(conn);
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res)) != NULL) {
    temp = (struct db_move*)debug_malloc(sizeof(struct db_move));
    sprintf(temp->move, "%s", row[0]);
        temp->win_rate = atof(row[1]);
    temp->next = moves_from_db;
    moves_from_db = temp;   
  }
  mysql_free_result(res);
  
  return moves_from_db;
}

void end_game(MYSQL *conn, int winner) {
  struct db_move *temp = NULL;
  char query[255];
  for (int i = 0; i < g_move_history.count; i++) {
    bool skip = false;
    for (int j = 0; j < g_move_history.count; j++) {
      if (g_move_history.moves[i].move == g_move_history.moves[j].move &&
          g_move_history.moves[i].serial == g_move_history.moves[j].serial &&
          i != j) {
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
    sprintf(query, "insert into chess_positions (serial, move, %s) values ('%s', '%s', 1) on duplicate key update %s = %s + 1", win_lose_draw, g_move_history.moves[i].serial, g_move_history.moves[i].move, win_lose_draw, win_lose_draw);
    // printf("%s\n", query);
    if (mysql_query(conn, query)) {
      printf("%s\n", query);
      fprintf(stderr, "%s\n", mysql_error(conn));
      exit(1);
    }
  }
    
  sprintf(query, "update total_game_count set total_games = total_games + 1 where game_name = 'Chess'");
  // printf("%s\n", query);
  if (mysql_query(conn, query)) {
    printf("%s\n", query);
    fprintf(stderr, "%s\n", mysql_error(conn));
    exit(1);
  }
}

void update_database(MYSQL *conn, const char* serial, const char *move, int player) {
/*  char query[255];
  sprintf(query, "select serial from chess_positions where serial = '%s' and move = '%s'", serial, move);
  // printf("%s\n", query);
  if (mysql_query(conn, query)) {
    printf("%s\n", query);
    fprintf(stderr, "%s\n", mysql_error(conn));
    exit(1);
  }

  MYSQL_RES *res = mysql_store_result(conn);
  MYSQL_ROW row;
  printf("%s - ", query);
  printf("results: %i\n", mysql_num_rows(res));
  if (mysql_num_rows(res) == 0) {
    mysql_free_result(res);
    sprintf(query, "insert into chess_positions set serial = '%s', move = '%s'", serial, move);
    if (mysql_query(conn, query)) {
      printf("%s\n", query);
      fprintf(stderr, "%s\n", mysql_error(conn));
      exit(1);
    }
  } else {
    mysql_free_result(res);
  }
  */
  //struct db_move *hist = (struct db_move*)malloc(sizeof(struct db_move));
  //hist->serial = (char*)malloc(100);
  //hist->move = (char*)malloc(10);
  strcpy(g_move_history.moves[g_move_history.count].serial, serial);
  strcpy(g_move_history.moves[g_move_history.count].move, move);
  g_move_history.moves[g_move_history.count].player = player;
//  hist->next = *move_history;
  // printf("%s - %s - %i\n", g_move_history.moves[g_move_history.count].serial, g_move_history.moves[g_move_history.count].move, g_move_history.moves[g_move_history.count].player);
  g_move_history.count++;
  //*move_history = hist;
}

void serialize_game_state(struct board_state_struct *board_state, char *serial_string) {
  unsigned char serial[34];
  memset(serial, 0, 34);  
  int count = 0;

  bool first_half = true;
  unsigned char temp = 0;
  for (int i = 0; i < 64; i++) {  
    if (first_half) {
      temp = board_state->board_pieces[i];
    } else {
      temp += board_state->board_pieces[i] * 16;
      serial[(int)floor(i / 2) + 2] = temp;
      temp = 0;      
    }    
    first_half = !first_half;
  }
  char serial_char[3];
  memset(serial_string, 0, 100);
  for (int s = 0; s < 32; s++) {    
    sprintf(serial_char, "%02X", serial[s + 2]);    
//    printf("%s ", serial_char);
    strcat(serial_string, serial_char); 
  }
//  printf("\n%i - %s\n", strlen(serial_string), serial_string);
  
  for (int i = 0; i < g_move_history.count; i++) {
//    printf("original: %s\n", g_move_history.moves[i].serial);
//    printf("compare to: %s\n", &(g_move_history.moves[i].serial[5]));
    if (strcmp(&(g_move_history.moves[i].serial[5]), serial_string) == 0) {
      count++;
      if (count > 2) {
        break;
      }
    }        
  }    
//  printf("position occurrences: %i\n", count);
  
  // byte 0 = 0     1     2     3     4     5     6     7
  // byte 0 = BKC - BQC - WKC - WQC - CUR - 0  -  0  -  0
  serial[0] += board_state->black_king_castle ? 1 << 0: 0;
  serial[0] += board_state->black_queen_castle ? 1 << 1 : 0; 
  serial[0] += board_state->white_king_castle ? 1 << 2 : 0;
  serial[0] += board_state->white_queen_castle ? 1 << 3 : 0;
  serial[0] += board_state->cur_player == 1 ? 1 << 4 : 0;
  serial[0] += 0 == 1 ? 1 << 5 : 0;
  serial[0] += 0 == 1 ? 1 << 6 : 0;
  serial[0] += 0 == 1 ? 1 << 7 : 0;
  serial[1] = (unsigned char)board_state->en_passant_position;
  
  unsigned char *temp_str;

  sprintf(serial_char, "%02X", serial[1]);
//  printf("%u - %s\n", serial[1], serial_char);

  temp_str = strdup(serial_string);
  strcpy(serial_string, serial_char); 
  strcat(serial_string, temp_str);  
  free(temp_str);

  sprintf(serial_char, "%02X", serial[0]);
//  printf("%u - %s\n", serial[0], serial_char);
  temp_str = strdup(serial_string);
  strcpy(serial_string, serial_char); 
  strcat(serial_string, temp_str);  
  free(temp_str);

  sprintf(serial_char, "%i", count);
  temp_str = strdup(serial_string);
  strcpy(serial_string, serial_char); 
  strcat(serial_string, temp_str);  
  free(temp_str);
//  printf("%i - %s\n", strlen(serial_string), serial_string);
}


void position_to_string(char *result, int position) {
  sprintf(result, "%c%c", position % 8 + 97, (int)floor(position / 8) + 48 + 1);   
}

int string_to_position(char *position) {
  return position[0] - 97 + (8 * (position[1] - 48 - 1));
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

bool check_for_check(struct board_state_struct *board_state, int player) {
  int king_position = -1;
  for (int i = 0; i < 64; i++) {
    if (board_state->board_pieces[i] == (player == 1 ? BK : WK)) {
      king_position = i;
      char pos[3];
      position_to_string(pos, i);
    }
  }

  bool found = false;
  struct move *moves = get_all_legal_moves(board_state, player, false, false);      
  for (struct move *cur = moves; cur != NULL;) {    
//    if (board_state->board_pieces[string_to_position("h5")] == WQ) {
//      printf("%s\n", cur->text);
//    }
    if (cur->new_position == king_position) {
      found = true;      
    }
    struct move *temp = cur->next;
    debug_free(cur);
    cur = temp;
  }
  
  return found;    
}

bool execute_move(const char *move_choice, struct move *legal_moves, struct board_state_struct *board_state, int player) {
  for (struct move *cur = legal_moves; cur != NULL; cur = cur->next) {
    if (strcmp(move_choice, cur->text) == 0) {
      // this is a legal move
      // reset en passant position
      int en_passant_position = board_state->en_passant_position;
      board_state->en_passant_position = -1;
      // move piece to its new position
      board_state->board_pieces[cur->new_position] = board_state->board_pieces[cur->old_position];
      // empty the old position
      board_state->board_pieces[cur->old_position] = EE;    
      // check if this was a pawn
      if ((int)(board_state->board_pieces[cur->new_position] & 7) == 1) {
        int dir = 8 * (-1 + 2 * player);
        // check if this piece moved to the en passant position
        if (en_passant_position == cur->new_position) {
          // this was an en passant, remove the captured pawn
          board_state->board_pieces[cur->new_position - dir] = EE;
        }
        // check if this pawn moved 2 ranks
        if (abs((int)floor(cur->new_position / 8) - (int)floor(cur->old_position / 8)) > 1) {
          // an en passant may be possible, record the position
          board_state->en_passant_position = cur->old_position + dir;
          //printf("En Passant Position: %s\n", position_to_string(board_state->en_passant_position));  
        }
        // check for pawn promotion
        char check_promotion = move_choice[strlen(move_choice) - 1];
        if (check_promotion == '+' || check_promotion == '#') {
          check_promotion = move_choice[strlen(move_choice) - 2];
        }
        if (check_promotion == 'Q') {
          board_state->board_pieces[cur->new_position] = (8 * player) + BQ;
        } else if (check_promotion == 'B') {
          board_state->board_pieces[cur->new_position] = (8 * player) + BB;
        } else if (check_promotion == 'N') {
          board_state->board_pieces[cur->new_position] = (8 * player) + BN;
        } else if (check_promotion == 'R') {
          board_state->board_pieces[cur->new_position] = (8 * player) + BR;
        }
      }
      // check if this was a king
      if ((int)(board_state->board_pieces[cur->new_position] & 7) == 6) {
        // check for kingside castle
        if (strcmp(cur->text, "0-0") == 0) {
          // need to move rook also
          if (player == 0) {
            board_state->board_pieces[63] = EE;
            board_state->board_pieces[61] = BR;
          } else if (player == 1) {            
            board_state->board_pieces[7] = EE;
            board_state->board_pieces[5] = WR;
          }
        }
        // check for queenside castle
        else if (strcmp(cur->text, "0-0-0") == 0) {
          // need to move rook also
          if (player == 0) {
            board_state->board_pieces[56] = EE;
            board_state->board_pieces[59] = BR;
          } else if (player == 1) {
            board_state->board_pieces[0] = EE;
            board_state->board_pieces[3] = WR;
          }
        }
        // king moved, can't castle after this
        if (player == 0) {
          board_state->black_king_castle = false;
          board_state->black_queen_castle = false;
        } else if (player == 1) {
          board_state->white_king_castle = false;
          board_state->white_queen_castle = false;
        }
      }
      // check if this was a rook
      if ((int)(board_state->board_pieces[cur->new_position] & 7) == 2) {
        // a rook moved, that side can't castle after this
        if (player == 0) {
          if (cur->old_position == 63) {
            board_state->black_king_castle = false;
          } else if (cur->old_position == 56) {
            board_state->black_queen_castle = false;
          }
        } else if (player == 1) {
          if (cur->old_position == 7) {
            board_state->white_king_castle = false;
          } else if (cur->old_position == 0) {
            board_state->white_queen_castle = false;
          }
        }
      }

      return true;  
    }
  }
  printf("invalid move - %s\n", move_choice);
  return false;
}

struct move *create_move(struct move *move_list, char *text, int new_position, int old_position, unsigned char piece) {
  struct move *new_move = (struct move*)debug_malloc(sizeof(struct move));
  strcpy(new_move->text, text);
  new_move->old_position = old_position;
  new_move->new_position = new_position;
  new_move->next = move_list;
  
  return new_move;
}

struct move *disambiguate_moves(struct move* move_list) {
  for (struct move *cur = move_list; cur != NULL; cur = cur->next) {
    // Skip pieces that can't be ambiguous: K, P
    if (cur->text[0] == 'K' || cur->text[0] >= 97) {
      continue;
    }
    for (struct move *checker = move_list; checker != NULL; checker = checker->next) {
      if (strcmp(cur->text, checker->text) == 0 && cur->old_position != checker->old_position) {
        char cur_pos[3], checker_pos[3], cur_dis, checker_dis;        
        position_to_string(cur_pos, cur->old_position);
        position_to_string(checker_pos, checker->old_position);
        // check if on different files
        if (cur_pos[0] != checker_pos[0]) {
          cur_dis = cur_pos[0];
          checker_dis = checker_pos[0];
        }
        // check if on different ranks
        else if (cur_pos[1] != checker_pos[1]) {
          cur_dis = cur_pos[1];
          checker_dis = checker_pos[1];
        }
        
        // disambiguate
        char new_text[10];// = (char*)malloc(strlen(cur->text) + 1);
        sprintf(new_text, "%c%c%s", cur->text[0], cur_dis, &cur->text[1]);        
        // cur->text = realloc(cur->text, sizeof(new_text));
        strcpy(cur->text, new_text);
        sprintf(new_text, "%c%c%s", checker->text[0], checker_dis, &checker->text[1]);
        // checker->text = realloc(checker->text, sizeof(new_text));
        strcpy(checker->text, new_text);        
        //free(new_text);
      }
    }
  }
  
  return move_list;
}

struct move *get_all_legal_moves(struct board_state_struct *board_state, int player, bool full_legal, bool do_check) {
  struct move *potential_moves = NULL, *legal_moves = NULL; 
  for (int i = 0; i < 64; i++) {
    potential_moves = get_legal_moves(potential_moves, board_state, player, i);
  }
  potential_moves = disambiguate_moves(potential_moves);
  if (full_legal) {
    // loop through potential moves
    // create test board = copy of board_state
    // execute move on test board
    // on test board:
    // find player's king position
    // find opponent's potential moves
    // loop through opponent's potential moves
    // if any move would land a piece on king's square, 
    // then the player's move is illegal    
    // if no move would land a piece on king's square,
    // then add the potential move to legal_moves
    int opponent = player == 1 ? 0 : 1;
    for (struct move *cur = potential_moves; cur != NULL;) {
      int king_positions[3]; // most of the time, only 1 will be used, extras are for castling
      for (int i = 0; i < 3; i++) {
        king_positions[i] = -1;
      }
      struct move *opponent_moves = NULL;
      bool legal_move = true;
      struct board_state_struct *test_board = (struct board_state_struct*)debug_malloc(sizeof(struct board_state_struct));
      memcpy(test_board, board_state, sizeof(struct board_state_struct));
      if (execute_move(cur->text, potential_moves, test_board, player)) {
        for (int i = 0; i < 64; i++) {
          if (test_board->board_pieces[i] == (player == 1 ? WK : BK)) {
            king_positions[0] = i;
          }
        }
        if (strcmp(cur->text, "0-0") == 0) {
          king_positions[1] = king_positions[0] - 1;
          king_positions[2] = king_positions[0] - 2;
        } else if (strcmp(cur->text, "0-0-0") == 0) {
          king_positions[1] = king_positions[0] + 1;
          king_positions[2] = king_positions[0] + 2;
        }        
//        printf("%s - king pos: %s %s %s\n", cur->text, position_to_string(king_positions[0]), king_positions[1] >= 0 ? position_to_string(king_positions[1]) : "None", king_positions[2] >= 0 ? position_to_string(king_positions[2]) : "None");//test_board->board_pieces);
        struct move *opponent_moves = NULL;
        for (int i = 0; i < 64; i++) {
          opponent_moves = get_legal_moves(opponent_moves, test_board, opponent, i);
        }
        for (struct move *opp_cur = opponent_moves; opp_cur != NULL;) {
          if (legal_move) {
            if (opp_cur->new_position == king_positions[0] || opp_cur->new_position == king_positions[1] || opp_cur->new_position == king_positions[2]) {
//            printf("%s is not legal\n", cur->text);
              legal_move = false;
            }
          }
          struct move *temp = opp_cur->next;
          debug_free(opp_cur);
          opp_cur = temp;
        }
      }
      struct move *temp = cur->next;        
      if (legal_move) {
        cur->next = legal_moves;
        legal_moves = cur;
        cur = temp;
        potential_moves = temp;
      } else {
        if (cur != NULL) {
          debug_free(cur);
        }
        cur = temp;
        potential_moves = temp;
      }      
      debug_free(test_board);
    }
  } else {
    legal_moves = potential_moves;
  }
  
  // leaving this here in case I make a breakthrough at some point, but I'm

  if (do_check) {  
    for (struct move *cur = legal_moves; cur != NULL; cur = cur->next) {
//      printf("Will %s cause check? ", cur->text);
      struct board_state_struct *test_board = (struct board_state_struct*)debug_malloc(sizeof(struct board_state_struct));
      memcpy(test_board, board_state, sizeof(struct board_state_struct));
      if (execute_move(cur->text, legal_moves, test_board, player)) {
        if (check_for_check(test_board, player)) {
//          printf("Check!\n");
          struct move *moves = get_all_legal_moves(test_board, player == 1 ? 0 : 1, true, false);
          if (moves == NULL) {
            strcat(cur->text, "#");
          } else {
            strcat(cur->text, "+");
            for (struct move *del = moves; del != NULL;) {
              struct move *temp = del->next;
              debug_free(del);
              del = temp;
            }
          }
        }
      }
      debug_free(test_board);
    }
  }      
  
  return legal_moves;
}

struct move *get_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position) {
  unsigned char piece = board_state->board_pieces[position];  
  if (piece == 0) {
    // no piece in this position
    return move_list;
  } else if ((int)(piece & 8) != (int)(player * 8)) {
    // not current player's piece
    return move_list;
  }
  
  //printf("Current piece: %i\n", piece);
  //printf("Current position: %c%c\n", position % 8 + 97, (int)floor(position / 8) + 48 + 1);   

  unsigned char masked_piece = piece & 7;
  if (masked_piece == 1) {
    move_list = get_pawn_legal_moves(move_list, board_state, player, position);
    //output = 'P';
  } else if (masked_piece == 2) {
    move_list = get_rook_legal_moves(move_list, board_state, player, position);
    //output = 'R';
  } else if (masked_piece == 3) {
    move_list = get_knight_legal_moves(move_list, board_state, player, position);
    //output = 'N';
  } else if (masked_piece == 4) {
    move_list = get_bishop_legal_moves(move_list, board_state, player, position);
    //output = 'B';
  } else if (masked_piece == 5) {
    move_list = get_queen_legal_moves(move_list, board_state, player, position);
    //output = 'Q';
  } else if (masked_piece == 6) {
    move_list = get_king_legal_moves(move_list, board_state, player, position);
    //output = 'K';
  }  
  
  return move_list;
}

struct move *get_pawn_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position) {
  int dir = 8 * (-1 + 2 * player); // -1 for player 0, 1 for player 1, multiplied by 8 for rank change
  // check if pawn is on second/seventh rank
  bool can_move_two = false;
  if ((player == 1 && (int)floor(position / 8) == 1) || 
      (player == 0 && (int)floor(position / 8) == 6)) {
    can_move_two = true;
  }
  
  int new_position = 0;
  // if position directly ahead is empty, can move
  new_position = position + dir;
  if (new_position >= 0 && new_position < 64) { // if this is false, something went wrong
    if (board_state->board_pieces[new_position] == 0) {
      if ((int)floor(new_position / 8) == 7 || (int)floor(new_position / 8) == 0) {
        char promotion_pieces[] = { 'R', 'N', 'B', 'Q' };
        for (int p = 0; p < 4; p++) {          
          char text[10];// = (char*)malloc(4);
          position_to_string(text, new_position);
          sprintf(text, "%s=%c", text, promotion_pieces[p]);
          move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
        }
      } else {
        char text[10];// = (char*)malloc(3);
        position_to_string(text, new_position);
        move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
      }      
    } else {
      can_move_two = false;
    }
    // check for first move 2 space move
    if (can_move_two) {
      new_position = position + 2 * dir;
      if (board_state->board_pieces[new_position] == 0) {
        char text[10];// = (char*)malloc(3);
        position_to_string(text, new_position);
        move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
      }  
    }
    // check for captures
    // check left
    if (position % 8 > 0) { // ie. file 2
      new_position = position + dir - 1;
      if (board_state->board_pieces[new_position] != 0 && (int)(board_state->board_pieces[new_position] & 8) != (int)(player * 8) ||
          board_state->en_passant_position == new_position) {
        if ((int)floor(new_position / 8) == 7 || (int)floor(new_position / 8) == 0) {
          char promotion_pieces[] = { 'R', 'N', 'B', 'Q' };
          for (int p = 0; p < 4; p++) {          
            char text[10];// = (char*)malloc(6); 
            char text2[10];// = (char*)malloc(3);
            position_to_string(text, position);
            position_to_string(text2, new_position);
            sprintf(text, "%cx%s=%c", text[0], text2, promotion_pieces[p]);
            move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
          }
        } else {
          char text[10];// = (char*)malloc(5);
          char text2[10];// = (char*)malloc(3);
          position_to_string(text, position);
          position_to_string(text2, new_position);            
          sprintf(text, "%cx%s", text[0], text2);
          move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
        }      
      }
    }
    // check right
    if (position % 8 < 7) { // ie. file 6
      new_position = position + dir + 1;
      if (board_state->board_pieces[new_position] != 0 && (int)(board_state->board_pieces[new_position] & 8) != (int)(player * 8) ||
          board_state->en_passant_position == new_position) {
        if ((int)floor(new_position / 8) == 7 || (int)floor(new_position / 8) == 0) {
          char promotion_pieces[] = { 'R', 'N', 'B', 'Q' };
          for (int p = 0; p < 4; p++) {          
            char text[10];// = (char*)malloc(6); 
            char text2[10];// = (char*)malloc(3);
            position_to_string(text, position);
            position_to_string(text2, new_position);            
            sprintf(text, "%cx%s%c", text[0], text2, promotion_pieces[p]);
            move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
          }
        } else {
          char text[10];// = (char*)malloc(5);
          char text2[10];// = (char*)malloc(3);
          position_to_string(text, position);
          position_to_string(text2, new_position);            
          sprintf(text, "%cx%s", text[0], text2);
          move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
        }      
      }
    }    
  }
  
  
  return move_list;
}

bool create_standard_move(struct move **move_list, int player, int new_position, int old_position, unsigned char my_piece, unsigned char target_piece) {
  char piece_notation;
  unsigned char masked_piece = my_piece & 7;
  if (masked_piece == 1) {
    piece_notation = 'P';
  } else if (masked_piece == 2) {
    piece_notation = 'R';
  } else if (masked_piece == 3) {
    piece_notation = 'N';
  } else if (masked_piece == 4) {
    piece_notation = 'B';
  } else if (masked_piece == 5) {
    piece_notation = 'Q';
  } else if (masked_piece == 6) {
    piece_notation = 'K';
  }  
  if (target_piece == EE) {
    char text[10];// = (char*)malloc(4); 
    position_to_string(&text[1], new_position);
    text[0] = piece_notation;
    *move_list = create_move(*move_list, text, new_position, old_position, my_piece);      
    return true;
  } else if ((int)(target_piece & 8) == (int)(player * 8)) {
    return false;    
  } else {
    char text[10];// = (char*)malloc(5); 
    position_to_string(&text[2], new_position);
    text[0] = piece_notation;
    text[1] = 'x';
    *move_list = create_move(*move_list, text, new_position, old_position, my_piece);      
    return false;
  }  
}

struct move *get_rook_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position) {   
  // Starting at position, loop by -1, by +1, by -8, by +8
  // if empty, add move, continue
  // if occupied by friendly, break
  // if occupied by opponent, add capture move, break  
  for (int p = position - 1; p >= (int)floor(position / 8) * 8; p--) {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p])) {
      break;
    }
  }
  for (int p = position + 1; p < ((int)floor(position / 8) + 1) * 8; p++) {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p])) {
      break;
    }
  }
  for (int p = position - 8; p >= 0; p -= 8) {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p])) {
      break;
    }
  }  
  for (int p = position + 8; p < 64; p += 8) {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p])) {
      break;
    }
  }

  return move_list;
}

struct move *get_bishop_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position) {   
  // Starting at position, loop by -9, by -7, by +9, by +7
  // if empty, add move, continue
  // if occupied by friendly, break
  // if occupied by opponent, add capture move, break  
  int rank_moves[] = { -2, -1, 1, 2, -2, -1, 1, 2 };
  int file_moves[] = { -1, -2, -2, -1, 1, 2, 2, 1 };
                     
  int rank = (int)floor(position / 8);
  int file = position % 8;  
  int new_rank, new_file, new_position;
  // down left
  for (new_rank = rank - 1, new_file = file - 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank--, new_file--) {  
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position])) {
      break;
    }
  }
  // down right
  for (new_rank = rank - 1, new_file = file + 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank--, new_file++) {  
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position])) {
      break;
    }
  }
  // up left
  for (new_rank = rank + 1, new_file = file - 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank++, new_file--) {  
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position])) {
      break;
    }
  }
  // up right
  for (new_rank = rank + 1, new_file = file + 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank++, new_file++) {  
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position])) {
      break;
    }
  }
  
  return move_list;
}

struct move *get_queen_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position) {
  move_list = get_rook_legal_moves(move_list, board_state, player, position);
  move_list = get_bishop_legal_moves(move_list, board_state, player, position);
  
  return move_list;
}

struct move *get_knight_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position) {
  int rank_moves[] = { -2, -1, 1, 2, -2, -1, 1, 2 };
  int file_moves[] = { -1, -2, -2, -1, 1, 2, 2, 1 };
                     
  int rank = (int)floor(position / 8);
  int file = position % 8;  
  int new_rank, new_file, new_position;
  for (int i = 0; i < 8; i++) {  
    new_rank = rank + rank_moves[i];
    new_file = file + file_moves[i];
    if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
      new_position = new_file + new_rank * 8;
      create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]);
    }
  }
  
  return move_list;
}


struct move *get_king_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position) {   
  int rank_moves[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
  int file_moves[] = { -1, 0, 1, -1, 1, -1, 0, 1};
  int rank = (int)floor(position / 8);
  int file = position % 8;  
  int new_rank, new_file, new_position;
  for (int i = 0; i < 8; i++) {  
    new_rank = rank + rank_moves[i];
    new_file = file + file_moves[i];
    if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8) {
      new_position = new_file + new_rank * 8;
      create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]);  
    }
  }
  
  if (player == 0) {
    if (board_state->black_king_castle && board_state->board_pieces[61] == EE && board_state->board_pieces[62] == EE) {      
      char text[10];// = (char*)malloc(4);
      strcpy(text, "0-0\0");
      move_list = create_move(move_list, text, 62, position, board_state->board_pieces[position]);      
    }
    if (board_state->black_queen_castle && board_state->board_pieces[59] == EE && board_state->board_pieces[58] == EE && board_state->board_pieces[57] == EE) {    
      char text[10];// = (char*)malloc(6);
      strcpy(text, "0-0-0\0");
      move_list = create_move(move_list, text, 58, position, board_state->board_pieces[position]);      
    }
  } else if (player == 1) {
    if (board_state->white_king_castle && board_state->board_pieces[5] == EE && board_state->board_pieces[6] == EE) {      
      char text[10];// = (char*)malloc(4);
      strcpy(text, "0-0\0");
      move_list = create_move(move_list, text, 6, position, board_state->board_pieces[position]);      
    } 
    if (board_state->white_queen_castle && board_state->board_pieces[3] == EE && board_state->board_pieces[2] == EE && board_state->board_pieces[1] == EE) {    
      char text[10];// = (char*)malloc(6);
      strcpy(text, "0-0-0\0");
      move_list = create_move(move_list, text, 2, position, board_state->board_pieces[position]);      
    }
  }
  
  return move_list;
}

bool is_light_square(int position) {
  bool even_rank_index = (int)floor(position / 8) % 2 == 0;
  bool even_file_index = position % 8 % 2 == 0;

  return even_rank_index != even_file_index;
}