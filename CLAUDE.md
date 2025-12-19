# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build the project (outputs chess-ai binary)
make

# The build command compiles:
# gcc -o chess-ai main.c db.c mem.c chess.c -l sqlite3 -I. -lpthread
```

## Dependencies

- SQLite3 (`libsqlite3-dev`)
- pthreads

## Architecture Overview

This is a chess AI that learns from self-play by storing win/loss/draw statistics for each board position and move combination in a SQLite database.

### Core Components

- **main.c** - Entry point and game loop. Handles interactive/batch mode selection, runs games, and coordinates AI move selection based on database statistics.

- **chess.c / chess.h** - Chess engine implementation:
  - Board representation: 64-element array with piece encoding (bit 3 = color: 0=black, 8=white; bits 0-2 = piece type)
  - Legal move generation for all pieces including special moves (castling, en passant, pawn promotion)
  - Move disambiguation for algebraic notation
  - Check/checkmate/stalemate detection
  - Board state serialization for database storage

- **db.c / db.h** - SQLite database layer for storing position statistics in `chess.db`

- **mem.c / mem.h** - Debug memory allocation wrapper that tracks allocations for leak detection

### Database Schema

The `chess_positions` table stores:
- `serial`: Serialized board state (castling rights, en passant, piece positions)
- `move`: Move in algebraic notation
- `wins`, `losses`, `draws`: Statistics for this position/move combination

### Game Modes

- **Interactive**: User enters moves manually; type 'c' for computer move, 'q' to quit
- **Batch**: Runs N self-play games automatically; press any key to interrupt between games

### Key Data Structures

- `board_state_struct`: Complete game state including board, castling rights, en passant position, current player
- `move`: Linked list node for legal moves with position and algebraic notation
- `db_move`: Database query result with win rate calculation

### Legacy Code

`mysql_main.c` is an older version using MySQL instead of SQLite - not part of the current build.
