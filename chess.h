#ifndef CHESS_H_
#define CHESS_H_
#include <stdbool.h>

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
};

struct board_serial {
  char serial[100];
};

struct board_history {
  int count;
  struct board_serial boards[400];
};

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
bool execute_move(const char*, struct move*, struct board_state_struct*, int);
struct move *create_move(struct move*, char*, int, int, unsigned char);
struct move *disambiguate_moves(struct move*);
bool create_standard_move(struct move**, int, int, int, unsigned char, unsigned char);
bool check_for_check(struct board_state_struct*, int);
void serialize_game_state(struct board_state_struct*, char*);
bool is_light_square(int);

#endif
