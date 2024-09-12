//board.h
#ifndef CHESS_H
#define CHESS_H

#include <iostream>
#include <bitset>
#include <cstdint>
#include <chrono>
#include <vector>
#include <array>

using namespace std::chrono;
using namespace std;

#define BOARD_SIZE 8;

enum pieces { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };


// Function declarations
void move_bit(int64_t* val, int oldPos, int newPos);
bool is_capture(int oldPos, int newPos);
bool empty_square(int position);
void print_board(int64_t bitboard);

int64_t generate_diagonal_moves(int currentPosition);
int64_t generate_sideways_moves(int currentPosition);
int64_t generate_knight_moves(int currentPosition);

int64_t isPinned(int currentPosition, pieces pieceType);
void timeFunction(int currentPosition, int64_t (*func)(int));

int64_t castle_condition();
int64_t king_moves(int currentPosition);

#endif // CHESS_H
