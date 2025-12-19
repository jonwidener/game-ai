#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "chess.h"
#include "mem.h"

struct db_move_history g_move_history;
struct board_history g_board_history;

void serialize_game_state(struct board_state_struct *board_state, char *serial_string)
{
  unsigned char serial[34];
  memset(serial, 0, 34);
  int count = 0;

  bool first_half = true;
  unsigned char temp = 0;
  for (int i = 0; i < 64; i++)
  {
    if (first_half)
    {
      temp = board_state->board_pieces[i];
    }
    else
    {
      temp += board_state->board_pieces[i] * 16;
      serial[(int)floor(i / 2) + 2] = temp;
      temp = 0;
    }
    first_half = !first_half;
  }
  char serial_char[3];
  memset(serial_string, 0, 100);
  for (int s = 0; s < 32; s++)
  {
    sprintf(serial_char, "%02X", serial[s + 2]);
    //    printf("%s ", serial_char);
    strcat(serial_string, serial_char);
  }
  //  printf("\n%i - %s\n", strlen(serial_string), serial_string);

  for (int i = 0; i < g_move_history.count; i++)
  {
    //    printf("original: %s\n", g_move_history.moves[i].serial);
    //    printf("compare to: %s\n", &(g_move_history.moves[i].serial[5]));
    if (strcmp(&(g_move_history.moves[i].serial[5]), serial_string) == 0)
    {
      count++;
      if (count > 2)
      {
        break;
      }
    }
  }
  //  printf("position occurrences: %i\n", count);

  // byte 0 = 0     1     2     3     4     5     6     7
  // byte 0 = BKC - BQC - WKC - WQC - CUR - 0  -  0  -  0
  serial[0] += board_state->black_king_castle ? 1 << 0 : 0;
  serial[0] += board_state->black_queen_castle ? 1 << 1 : 0;
  serial[0] += board_state->white_king_castle ? 1 << 2 : 0;
  serial[0] += board_state->white_queen_castle ? 1 << 3 : 0;
  serial[0] += board_state->cur_player == 1 ? 1 << 4 : 0;
  serial[0] += count == 2 ? 1 << 5 : 0;
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

void position_to_string(char *result, int position)
{
  sprintf(result, "%c%c", position % 8 + 97, (int)floor(position / 8) + 48 + 1);
}

int string_to_position(char *position)
{
  return position[0] - 97 + (8 * (position[1] - 48 - 1));
}

bool check_for_check(struct board_state_struct *board_state, int player)
{
  int king_position = -1;
  for (int i = 0; i < 64; i++)
  {
    if (board_state->board_pieces[i] == (player == 1 ? BK : WK))
    {
      king_position = i;
      char pos[3];
      position_to_string(pos, i);
    }
  }

  bool found = false;
  struct move *moves = get_all_legal_moves(board_state, player, false, false);
  for (struct move *cur = moves; cur != NULL;)
  {
    //    if (board_state->board_pieces[string_to_position("h5")] == WQ) {
    //      printf("%s\n", cur->text);
    //    }
    if (cur->new_position == king_position)
    {
      found = true;
    }
    struct move *temp = cur->next;
    debug_free(cur);
    cur = temp;
  }

  return found;
}

bool execute_move(const char *move_choice, struct move *legal_moves, struct board_state_struct *board_state, int player)
{
  for (struct move *cur = legal_moves; cur != NULL; cur = cur->next)
  {
    if (strcmp(move_choice, cur->text) == 0)
    {
      // this is a legal move
      // reset en passant position
      int en_passant_position = board_state->en_passant_position;
      board_state->en_passant_position = -1;
      // move piece to its new position
      board_state->board_pieces[cur->new_position] = board_state->board_pieces[cur->old_position];
      // empty the old position
      board_state->board_pieces[cur->old_position] = EE;
      // check if this was a pawn
      if ((int)(board_state->board_pieces[cur->new_position] & 7) == 1)
      {
        int dir = 8 * (-1 + 2 * player);
        // check if this piece moved to the en passant position
        if (en_passant_position == cur->new_position)
        {
          // this was an en passant, remove the captured pawn
          board_state->board_pieces[cur->new_position - dir] = EE;
        }
        // check if this pawn moved 2 ranks
        if (abs((int)floor(cur->new_position / 8) - (int)floor(cur->old_position / 8)) > 1)
        {
          // an en passant may be possible, record the position
          board_state->en_passant_position = cur->old_position + dir;
          // printf("En Passant Position: %s\n", position_to_string(board_state->en_passant_position));
        }
        // check for pawn promotion
        char check_promotion = move_choice[strlen(move_choice) - 1];
        if (check_promotion == '+' || check_promotion == '#')
        {
          check_promotion = move_choice[strlen(move_choice) - 2];
        }
        if (check_promotion == 'Q')
        {
          board_state->board_pieces[cur->new_position] = (8 * player) + BQ;
        }
        else if (check_promotion == 'B')
        {
          board_state->board_pieces[cur->new_position] = (8 * player) + BB;
        }
        else if (check_promotion == 'N')
        {
          board_state->board_pieces[cur->new_position] = (8 * player) + BN;
        }
        else if (check_promotion == 'R')
        {
          board_state->board_pieces[cur->new_position] = (8 * player) + BR;
        }
      }
      // check if this was a king
      if ((int)(board_state->board_pieces[cur->new_position] & 7) == 6)
      {
        // check for kingside castle
        if (strcmp(cur->text, "0-0") == 0)
        {
          // need to move rook also
          if (player == 0)
          {
            board_state->board_pieces[63] = EE;
            board_state->board_pieces[61] = BR;
          }
          else if (player == 1)
          {
            board_state->board_pieces[7] = EE;
            board_state->board_pieces[5] = WR;
          }
        }
        // check for queenside castle
        else if (strcmp(cur->text, "0-0-0") == 0)
        {
          // need to move rook also
          if (player == 0)
          {
            board_state->board_pieces[56] = EE;
            board_state->board_pieces[59] = BR;
          }
          else if (player == 1)
          {
            board_state->board_pieces[0] = EE;
            board_state->board_pieces[3] = WR;
          }
        }
        // king moved, can't castle after this
        if (player == 0)
        {
          board_state->black_king_castle = false;
          board_state->black_queen_castle = false;
        }
        else if (player == 1)
        {
          board_state->white_king_castle = false;
          board_state->white_queen_castle = false;
        }
      }
      // check if this was a rook
      if ((int)(board_state->board_pieces[cur->new_position] & 7) == 2)
      {
        // a rook moved, that side can't castle after this
        if (player == 0)
        {
          if (cur->old_position == 63)
          {
            board_state->black_king_castle = false;
          }
          else if (cur->old_position == 56)
          {
            board_state->black_queen_castle = false;
          }
        }
        else if (player == 1)
        {
          if (cur->old_position == 7)
          {
            board_state->white_king_castle = false;
          }
          else if (cur->old_position == 0)
          {
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

struct move *create_move(struct move *move_list, char *text, int new_position, int old_position, unsigned char piece)
{
  struct move *new_move = (struct move *)debug_malloc(sizeof(struct move));
  strcpy(new_move->text, text);
  new_move->old_position = old_position;
  new_move->new_position = new_position;
  new_move->next = move_list;

  return new_move;
}

struct move *disambiguate_moves(struct move *move_list)
{
  for (struct move *cur = move_list; cur != NULL; cur = cur->next)
  {
    // Skip pieces that can't be ambiguous: K, P
    if (cur->text[0] == 'K' || cur->text[0] >= 97)
    {
      continue;
    }
    for (struct move *checker = move_list; checker != NULL; checker = checker->next)
    {
      if (strcmp(cur->text, checker->text) == 0 && cur->old_position != checker->old_position)
      {
        char cur_pos[3], checker_pos[3], cur_dis, checker_dis;
        position_to_string(cur_pos, cur->old_position);
        position_to_string(checker_pos, checker->old_position);
        // check if on different files
        if (cur_pos[0] != checker_pos[0])
        {
          cur_dis = cur_pos[0];
          checker_dis = checker_pos[0];
        }
        // check if on different ranks
        else if (cur_pos[1] != checker_pos[1])
        {
          cur_dis = cur_pos[1];
          checker_dis = checker_pos[1];
        }

        // disambiguate
        char new_text[12]; // = (char*)malloc(strlen(cur->text) + 1);
        sprintf(new_text, "%c%c%s", cur->text[0], cur_dis, &cur->text[1]);
        // cur->text = realloc(cur->text, sizeof(new_text));
        strcpy(cur->text, new_text);
        sprintf(new_text, "%c%c%s", checker->text[0], checker_dis, &checker->text[1]);
        // checker->text = realloc(checker->text, sizeof(new_text));
        strcpy(checker->text, new_text);
        // free(new_text);
      }
    }
  }

  return move_list;
}

struct move *get_all_legal_moves(struct board_state_struct *board_state, int player, bool full_legal, bool do_check)
{
  struct move *potential_moves = NULL, *legal_moves = NULL;
  for (int i = 0; i < 64; i++)
  {
    potential_moves = get_legal_moves(potential_moves, board_state, player, i);
  }
  potential_moves = disambiguate_moves(potential_moves);
  if (full_legal)
  {
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
    for (struct move *cur = potential_moves; cur != NULL;)
    {
      int king_positions[3]; // most of the time, only 1 will be used, extras are for castling
      for (int i = 0; i < 3; i++)
      {
        king_positions[i] = -1;
      }
      struct move *opponent_moves = NULL;
      bool legal_move = true;
      struct board_state_struct *test_board = (struct board_state_struct *)debug_malloc(sizeof(struct board_state_struct));
      memcpy(test_board, board_state, sizeof(struct board_state_struct));
      if (execute_move(cur->text, potential_moves, test_board, player))
      {
        for (int i = 0; i < 64; i++)
        {
          if (test_board->board_pieces[i] == (player == 1 ? WK : BK))
          {
            king_positions[0] = i;
          }
        }
        if (strcmp(cur->text, "0-0") == 0)
        {
          king_positions[1] = king_positions[0] - 1;
          king_positions[2] = king_positions[0] - 2;
        }
        else if (strcmp(cur->text, "0-0-0") == 0)
        {
          king_positions[1] = king_positions[0] + 1;
          king_positions[2] = king_positions[0] + 2;
        }
        //        printf("%s - king pos: %s %s %s\n", cur->text, position_to_string(king_positions[0]), king_positions[1] >= 0 ? position_to_string(king_positions[1]) : "None", king_positions[2] >= 0 ? position_to_string(king_positions[2]) : "None");//test_board->board_pieces);
        struct move *opponent_moves = NULL;
        for (int i = 0; i < 64; i++)
        {
          opponent_moves = get_legal_moves(opponent_moves, test_board, opponent, i);
        }
        for (struct move *opp_cur = opponent_moves; opp_cur != NULL;)
        {
          if (legal_move)
          {
            if (opp_cur->new_position == king_positions[0] || opp_cur->new_position == king_positions[1] || opp_cur->new_position == king_positions[2])
            {
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
      if (legal_move)
      {
        cur->next = legal_moves;
        legal_moves = cur;
        cur = temp;
        potential_moves = temp;
      }
      else
      {
        if (cur != NULL)
        {
          debug_free(cur);
        }
        cur = temp;
        potential_moves = temp;
      }
      debug_free(test_board);
    }
  }
  else
  {
    legal_moves = potential_moves;
  }

  // leaving this here in case I make a breakthrough at some point, but I'm

  if (do_check)
  {
    for (struct move *cur = legal_moves; cur != NULL; cur = cur->next)
    {
      //      printf("Will %s cause check? ", cur->text);
      struct board_state_struct *test_board = (struct board_state_struct *)debug_malloc(sizeof(struct board_state_struct));
      memcpy(test_board, board_state, sizeof(struct board_state_struct));
      if (execute_move(cur->text, legal_moves, test_board, player))
      {
        if (check_for_check(test_board, player))
        {
          //          printf("Check!\n");
          struct move *moves = get_all_legal_moves(test_board, player == 1 ? 0 : 1, true, false);
          if (moves == NULL)
          {
            strcat(cur->text, "#");
          }
          else
          {
            strcat(cur->text, "+");
            for (struct move *del = moves; del != NULL;)
            {
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

struct move *get_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position)
{
  unsigned char piece = board_state->board_pieces[position];
  if (piece == 0)
  {
    // no piece in this position
    return move_list;
  }
  else if ((int)(piece & 8) != (int)(player * 8))
  {
    // not current player's piece
    return move_list;
  }

  // printf("Current piece: %i\n", piece);
  // printf("Current position: %c%c\n", position % 8 + 97, (int)floor(position / 8) + 48 + 1);

  unsigned char masked_piece = piece & 7;
  if (masked_piece == 1)
  {
    move_list = get_pawn_legal_moves(move_list, board_state, player, position);
    // output = 'P';
  }
  else if (masked_piece == 2)
  {
    move_list = get_rook_legal_moves(move_list, board_state, player, position);
    // output = 'R';
  }
  else if (masked_piece == 3)
  {
    move_list = get_knight_legal_moves(move_list, board_state, player, position);
    // output = 'N';
  }
  else if (masked_piece == 4)
  {
    move_list = get_bishop_legal_moves(move_list, board_state, player, position);
    // output = 'B';
  }
  else if (masked_piece == 5)
  {
    move_list = get_queen_legal_moves(move_list, board_state, player, position);
    // output = 'Q';
  }
  else if (masked_piece == 6)
  {
    move_list = get_king_legal_moves(move_list, board_state, player, position);
    // output = 'K';
  }

  return move_list;
}

struct move *get_pawn_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position)
{
  int dir = 8 * (-1 + 2 * player); // -1 for player 0, 1 for player 1, multiplied by 8 for rank change
  // check if pawn is on second/seventh rank
  bool can_move_two = false;
  if ((player == 1 && (int)floor(position / 8) == 1) ||
      (player == 0 && (int)floor(position / 8) == 6))
  {
    can_move_two = true;
  }

  int new_position = 0;
  // if position directly ahead is empty, can move
  new_position = position + dir;
  if (new_position >= 0 && new_position < 64)
  { // if this is false, something went wrong
    if (board_state->board_pieces[new_position] == 0)
    {
      if ((int)floor(new_position / 8) == 7 || (int)floor(new_position / 8) == 0)
      {
        char promotion_pieces[] = {'R', 'N', 'B', 'Q'};
        for (int p = 0; p < 4; p++)
        {
          char pos_str[3];
          char text[10]; // = (char*)malloc(4);
          position_to_string(pos_str, new_position);
          sprintf(text, "%s=%c", pos_str, promotion_pieces[p]);
          move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
        }
      }
      else
      {
        char text[10]; // = (char*)malloc(3);
        position_to_string(text, new_position);
        move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
      }
    }
    else
    {
      can_move_two = false;
    }
    // check for first move 2 space move
    if (can_move_two)
    {
      new_position = position + 2 * dir;
      if (board_state->board_pieces[new_position] == 0)
      {
        char text[10]; // = (char*)malloc(3);
        position_to_string(text, new_position);
        move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
      }
    }
    // check for captures
    // check left
    if (position % 8 > 0)
    { // ie. file 2
      new_position = position + dir - 1;
      if (board_state->board_pieces[new_position] != 0 && (int)(board_state->board_pieces[new_position] & 8) != (int)(player * 8) ||
          board_state->en_passant_position == new_position)
      {
        if ((int)floor(new_position / 8) == 7 || (int)floor(new_position / 8) == 0)
        {
          char promotion_pieces[] = {'R', 'N', 'B', 'Q'};
          for (int p = 0; p < 4; p++)
          {
            char pos_str[3], pos_str2[3];
            char text[10]; // = (char*)malloc(6);
            position_to_string(pos_str, position);
            position_to_string(pos_str2, new_position);
            sprintf(text, "%cx%s=%c", pos_str[0], pos_str2, promotion_pieces[p]);
            move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
          }
        }
        else
        {
          char pos_str[3], pos_str2[3];
          char text[10]; // = (char*)malloc(5);
          position_to_string(pos_str, position);
          position_to_string(pos_str2, new_position);
          sprintf(text, "%cx%s", pos_str[0], pos_str2);
          move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
        }
      }
    }
    // check right
    if (position % 8 < 7)
    { // ie. file 6
      new_position = position + dir + 1;
      if (board_state->board_pieces[new_position] != 0 && (int)(board_state->board_pieces[new_position] & 8) != (int)(player * 8) ||
          board_state->en_passant_position == new_position)
      {
        if ((int)floor(new_position / 8) == 7 || (int)floor(new_position / 8) == 0)
        {
          char promotion_pieces[] = {'R', 'N', 'B', 'Q'};
          for (int p = 0; p < 4; p++)
          {
            char pos_str[3], pos_str2[3];
            char text[10]; // = (char*)malloc(6);
            position_to_string(pos_str, position);
            position_to_string(pos_str2, new_position);
            sprintf(text, "%cx%s%c", pos_str[0], pos_str2, promotion_pieces[p]);
            move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
          }
        }
        else
        {
          char pos_str[3], pos_str2[3];
          char text[10]; // = (char*)malloc(5);
          position_to_string(pos_str, position);
          position_to_string(pos_str2, new_position);
          sprintf(text, "%cx%s", pos_str[0], pos_str2);
          move_list = create_move(move_list, text, new_position, position, board_state->board_pieces[position]);
        }
      }
    }
  }

  return move_list;
}

bool create_standard_move(struct move **move_list, int player, int new_position, int old_position, unsigned char my_piece, unsigned char target_piece)
{
  char piece_notation;
  unsigned char masked_piece = my_piece & 7;
  if (masked_piece == 1)
  {
    piece_notation = 'P';
  }
  else if (masked_piece == 2)
  {
    piece_notation = 'R';
  }
  else if (masked_piece == 3)
  {
    piece_notation = 'N';
  }
  else if (masked_piece == 4)
  {
    piece_notation = 'B';
  }
  else if (masked_piece == 5)
  {
    piece_notation = 'Q';
  }
  else if (masked_piece == 6)
  {
    piece_notation = 'K';
  }
  if (target_piece == EE)
  {
    char text[10]; // = (char*)malloc(4);
    position_to_string(&text[1], new_position);
    text[0] = piece_notation;
    *move_list = create_move(*move_list, text, new_position, old_position, my_piece);
    return true;
  }
  else if ((int)(target_piece & 8) == (int)(player * 8))
  {
    return false;
  }
  else
  {
    char text[10]; // = (char*)malloc(5);
    position_to_string(&text[2], new_position);
    text[0] = piece_notation;
    text[1] = 'x';
    *move_list = create_move(*move_list, text, new_position, old_position, my_piece);
    return false;
  }
}

struct move *get_rook_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position)
{
  // Starting at position, loop by -1, by +1, by -8, by +8
  // if empty, add move, continue
  // if occupied by friendly, break
  // if occupied by opponent, add capture move, break
  for (int p = position - 1; p >= (int)floor(position / 8) * 8; p--)
  {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p]))
    {
      break;
    }
  }
  for (int p = position + 1; p < ((int)floor(position / 8) + 1) * 8; p++)
  {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p]))
    {
      break;
    }
  }
  for (int p = position - 8; p >= 0; p -= 8)
  {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p]))
    {
      break;
    }
  }
  for (int p = position + 8; p < 64; p += 8)
  {
    if (!create_standard_move(&move_list, player, p, position, board_state->board_pieces[position], board_state->board_pieces[p]))
    {
      break;
    }
  }

  return move_list;
}

struct move *get_bishop_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position)
{
  // Starting at position, loop by -9, by -7, by +9, by +7
  // if empty, add move, continue
  // if occupied by friendly, break
  // if occupied by opponent, add capture move, break
  int rank_moves[] = {-2, -1, 1, 2, -2, -1, 1, 2};
  int file_moves[] = {-1, -2, -2, -1, 1, 2, 2, 1};

  int rank = (int)floor(position / 8);
  int file = position % 8;
  int new_rank, new_file, new_position;
  // down left
  for (new_rank = rank - 1, new_file = file - 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank--, new_file--)
  {
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]))
    {
      break;
    }
  }
  // down right
  for (new_rank = rank - 1, new_file = file + 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank--, new_file++)
  {
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]))
    {
      break;
    }
  }
  // up left
  for (new_rank = rank + 1, new_file = file - 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank++, new_file--)
  {
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]))
    {
      break;
    }
  }
  // up right
  for (new_rank = rank + 1, new_file = file + 1; new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8; new_rank++, new_file++)
  {
    new_position = new_file + new_rank * 8;
    if (!create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]))
    {
      break;
    }
  }

  return move_list;
}

struct move *get_queen_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position)
{
  move_list = get_rook_legal_moves(move_list, board_state, player, position);
  move_list = get_bishop_legal_moves(move_list, board_state, player, position);

  return move_list;
}

struct move *get_knight_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position)
{
  int rank_moves[] = {-2, -1, 1, 2, -2, -1, 1, 2};
  int file_moves[] = {-1, -2, -2, -1, 1, 2, 2, 1};

  int rank = (int)floor(position / 8);
  int file = position % 8;
  int new_rank, new_file, new_position;
  for (int i = 0; i < 8; i++)
  {
    new_rank = rank + rank_moves[i];
    new_file = file + file_moves[i];
    if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8)
    {
      new_position = new_file + new_rank * 8;
      create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]);
    }
  }

  return move_list;
}

struct move *get_king_legal_moves(struct move *move_list, struct board_state_struct *board_state, int player, int position)
{
  int rank_moves[] = {-1, -1, -1, 0, 0, 1, 1, 1};
  int file_moves[] = {-1, 0, 1, -1, 1, -1, 0, 1};
  int rank = (int)floor(position / 8);
  int file = position % 8;
  int new_rank, new_file, new_position;
  for (int i = 0; i < 8; i++)
  {
    new_rank = rank + rank_moves[i];
    new_file = file + file_moves[i];
    if (new_rank >= 0 && new_rank < 8 && new_file >= 0 && new_file < 8)
    {
      new_position = new_file + new_rank * 8;
      create_standard_move(&move_list, player, new_position, position, board_state->board_pieces[position], board_state->board_pieces[new_position]);
    }
  }

  if (player == 0)
  {
    if (board_state->black_king_castle && board_state->board_pieces[61] == EE && board_state->board_pieces[62] == EE)
    {
      char text[10]; // = (char*)malloc(4);
      strcpy(text, "0-0\0");
      move_list = create_move(move_list, text, 62, position, board_state->board_pieces[position]);
    }
    if (board_state->black_queen_castle && board_state->board_pieces[59] == EE && board_state->board_pieces[58] == EE && board_state->board_pieces[57] == EE)
    {
      char text[10]; // = (char*)malloc(6);
      strcpy(text, "0-0-0\0");
      move_list = create_move(move_list, text, 58, position, board_state->board_pieces[position]);
    }
  }
  else if (player == 1)
  {
    if (board_state->white_king_castle && board_state->board_pieces[5] == EE && board_state->board_pieces[6] == EE)
    {
      char text[10]; // = (char*)malloc(4);
      strcpy(text, "0-0\0");
      move_list = create_move(move_list, text, 6, position, board_state->board_pieces[position]);
    }
    if (board_state->white_queen_castle && board_state->board_pieces[3] == EE && board_state->board_pieces[2] == EE && board_state->board_pieces[1] == EE)
    {
      char text[10]; // = (char*)malloc(6);
      strcpy(text, "0-0-0\0");
      move_list = create_move(move_list, text, 2, position, board_state->board_pieces[position]);
    }
  }

  return move_list;
}

bool is_light_square(int position)
{
  bool even_rank_index = (int)floor(position / 8) % 2 == 0;
  bool even_file_index = position % 8 % 2 == 0;

  return even_rank_index != even_file_index;
}
