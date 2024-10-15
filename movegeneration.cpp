#include <iostream>
#include <string>
#include <bitset>
#include <cstdint>
#include <chrono>
#include <vector>
#include <set>
#include <map>
#include <array>
#include <cmath>
#include <sstream>
//#include "chess.h"

/*
TODO:
0. DO WAY MORE RESEARCH INTO PREPROCESSING. i basically need to be able to calculate all my sliding move attacks instantly and same w/ the other pieces
a way of doing that is maybe to precalculate them somehow? the problem with my generation is also that i need to get all set bits and that seems to be really slow
1. Implement a move and undo move function that perfectly returns me to the last move's board state
2. Implement a function that generates all possible moves for a given board state
3. Refactor all code to optimize for speed
4. Implement a massive hash table so I don't search similar branches?
5. Magic bitboards?
6. THEN, work on evaluation function
*/

/*
OPTIMIZATION IDEAS:
-refactor everything to work with this
*/

using namespace std::chrono;
using namespace std;

#define BOARD_SIZE 8;

int board[8][8] = {
    {63, 62, 61, 60, 59, 58, 57, 56},
    {55, 54, 53, 52, 51, 50, 49, 48},
    {47, 46, 45, 44, 43, 42, 41, 40},
    {39, 38, 37, 36, 35, 34, 33, 32},
    {31, 30, 29, 28, 27, 26, 25, 24},
    {23, 22, 21, 20, 19, 18, 17, 16},
    {15, 14, 13, 12, 11, 10,  9,  8},
    {7,   6,  5,  4,  3,  2,  1,  0}
};

enum pieces { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum files { A, B, C, D, E, F, G, H };

int64_t universal = 0xffffffffffffffff;
int64_t empty = 0x0000000000000000;

//CHANGE MOVE TO CONTAIN BOARD STATE THINGS SUCH AS CAPTURES OR CASTLES BOOLEANS SO THAT I CAN UNMAKE MOVES FOR SEARCH
class Move {
    public: 
        int before;
        int after;
        //piece that the pawn was promoted to
        int promotion;
        //square the rook moved to
        int castle;
        pieces piece;
        int captured_piece;

        bool whiteKingMoved;
        bool blackKingMoved;
        bool whiteHCastle;
        bool whiteACastle;
        bool blackACastle;
        bool blackHCastle;
        int enPassantSquare;
        Move(int x, int y, int q, int z, pieces p) {
            before = x;
            after = y;
            castle = z;
            promotion = q;
            piece = p;
        }
};

class MoveGeneration {
    public:
        int64_t white_pawns;
        int64_t black_pawns;

        int64_t white_knights;
        int64_t black_knights;

        int64_t white_bishops;
        int64_t black_bishops;

        int64_t white_rooks;
        int64_t black_rooks;

        int64_t white_queens;
        int64_t black_queens;

        int64_t white_king;
        int64_t black_king;

        int64_t* whiteBitboards[6] ={&white_pawns, &white_knights, &white_bishops, &white_rooks, &white_queens, &white_king};
        int64_t* blackBitboards[6] = {&black_pawns, &black_knights, &black_bishops, &black_rooks, &black_queens, &black_king};

        int64_t white_pieces = white_pawns | white_knights | white_bishops | white_rooks | white_queens | white_king;
        int64_t black_pieces = black_pawns | black_knights | black_bishops | black_rooks | black_queens | black_king;

        int64_t nW[64];
        int64_t nE[64];
        int64_t sW[64];
        int64_t sE[64];
        int64_t n[64];
        int64_t s[64];
        int64_t e[64];
        int64_t w[64];
        int64_t knightMoves[64];
        int64_t kingMoves[64];
        int64_t blackPawnLookup[64];
        int64_t whitePawnLookup[64];
        
        bool whiteTurn;
        bool whiteKingMoved;
        bool blackKingMoved;
        bool whiteHCastle;
        bool whiteACastle;
        bool blackACastle;
        bool blackHCastle;
        int enPassantSquare;

        int64_t generate_diagonal_moves(int);
        int64_t generate_sideways_moves(int);
        int64_t generate_knight_moves(int);
        int64_t pawn_moves(int);
        int64_t every_pinned_piece();
        int64_t isPinned(int, pieces);
        int64_t attacked_by(int);
        int64_t castle_condition();
        int64_t king_moves(int);
        int64_t check_checker();
        int count_moves(int);
        void make_move(Move);
        void set_state(Move&);
        void undo_move(Move);
        void set_fen(string);
        void initialize_lookup_table();
        bool empty_square(int);
        vector<Move> bitboard_to_moves(int64_t, int, pieces);
        vector<Move> generate_all_moves();

        MoveGeneration() {
            white_pawns = 0x000000000000ff00;
            black_pawns = 0x00ff000000000000;

            white_knights = 0x000000000000042;
            black_knights = 0x4200000000000000;

            white_bishops =0x000000000000024;
            black_bishops = 0x2400000000000000;

            white_rooks = 0x000000000000081;
            black_rooks = 0x8100000000000000;

            white_queens = 0x0000000000000010;
            black_queens = 0x1000000000000000;

            white_king = 0x0000000000000008;
            black_king = 0x0800000000000000;

            whiteTurn = true;
            whiteKingMoved = false;
            blackKingMoved = false;
            whiteHCastle = true;
            whiteACastle = true;
            blackACastle = true;
            blackHCastle = true;
            enPassantSquare = -1;

            white_pieces = white_pawns | white_knights | white_bishops | white_rooks | white_queens | white_king;
            black_pieces = black_pawns | black_knights | black_bishops | black_rooks | black_queens | black_king;
        }
};

int64_t eastOne (int64_t b) {return (b << 1) & 0xfefefefefefefefe;}
int64_t westOne (int64_t b) {return (b >> 1) & 0x7f7f7f7f7f7f7f7f;}
uint64_t northOne(uint64_t b) { return (b << 8); }
uint64_t southOne(uint64_t b) { return (b >> 8); }

void move_bit(int64_t* val, int oldPos, int newPos) {
    // Clear the bit at oldPos
    *val &= ~(1ULL << oldPos);
    // Set the bit at newPos
    *val |= 1ULL << newPos;
}

/*
bool is_capture (int oldPos, int newPos) {
    int64_t maskOne = 1ULL << oldPos;
    int64_t maskTwo = 1ULL << newPos;
    if (whiteTurn) {
        for (int i = 0; i < sizeof(blackBitboards)/sizeof(blackBitboards[0]); i++) {
            if (maskOne & *blackBitboards[i] || maskTwo & *blackBitboards[i]) {
                return true;
            }
        }
    } else {
        for (int i = 0; i < sizeof(whiteBitboards)/sizeof(whiteBitboards[0]); i++) {
            if (maskOne & *whiteBitboards[i] || maskTwo & *whiteBitboards[i]) {
                return true;
            }
        }
    }

    return false;
}
*/

bool MoveGeneration::empty_square(int position) {
    int64_t mask = 1ULL << position;
    for (int i = 0; i < sizeof(whiteBitboards)/sizeof(whiteBitboards[0]); i++) {
        if (mask & *whiteBitboards[i] || mask & *blackBitboards[i]) {
            return false;
        }
    }
    return true;
}

void print_board(int64_t bitboard) {
    unsigned long i; 
    i = 1UL<<(sizeof(bitboard)*CHAR_BIT-1);
    int count = 1;
    while(i>0){
        if(bitboard&i) {
            cout << 1;
        }
        else {
            cout << 0;
        }

        if (count % 8 == 0) {
            cout << endl;
        }
        count++;
        i >>= 1;
    }
}

void print_all_board(MoveGeneration& game) {
    // Initialize the board with empty squares
    char board[64];
    for (int i = 0; i < 64; ++i) {
        board[i] = '.';
    }

    // Map pieces to characters
    char piece_chars[6] = {'P', 'N', 'B', 'R', 'Q', 'K'};

    // White pieces
    for (int i = 0; i < 6; ++i) {
        uint64_t bitboard = *game.whiteBitboards[i];
        while (bitboard) {
            int pos = __builtin_ctzll(bitboard);
            board[pos] = piece_chars[i];
            bitboard &= bitboard - 1;
        }
    }

    // Black pieces
    for (int i = 0; i < 6; ++i) {
        uint64_t bitboard = *game.blackBitboards[i];
        while (bitboard) {
            int pos = __builtin_ctzll(bitboard);
            board[pos] = tolower(piece_chars[i]);
            bitboard &= bitboard - 1;
        }
    }

    // Print the board
    std::cout << "  +---+---+---+---+---+---+---+---+\n";
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << rank + 1 << " | ";
        for (int file = 7; file >= 0; --file) {  // Reverse file indexing
            int idx = rank * 8 + file;  // Adjust index calculation
            std::cout << board[idx] << " | ";
        }
        std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
    }
    std::cout << "    h   g   f   e   d   c   b   a\n";  // Adjust file labels
}


/*
int board[8][8] = {
    {63, 62, 61, 60, 59, 58, 57, 56},
    {55, 54, 53, 52, 51, 50, 49, 48},
    {47, 46, 45, 44, 43, 42, 41, 40},
    {39, 38, 37, 36, 35, 34, 33, 32},
    {31, 30, 29, 28, 27, 26, 25, 24},
    {23, 22, 21, 20, 19, 18, 17, 16},
    {15, 14, 13, 12, 11, 10,  9,  8},
    {7,   6,  5,  4,  3,  2,  1,  0}
};
*/

void MoveGeneration::initialize_lookup_table(){
    int64_t northWest;
    int64_t northEast;
    int64_t southWest;
    int64_t southEast;
    int64_t north = 0x0101010101010100;
    int64_t south = 0x0080808080808080;
    int64_t east;
    int64_t west;
    //north rays
    for (int i = 0; i < 64; i++) {
        n[i] = north << i;
        w[i] = 2*( (1ULL << (i|7)) - (1ULL << i));
        e[i] = ((1ULL << i) - (1ULL << (i & ~7)));
    }

    //south rays
    for (int i = 63; i >= 0; i--) {
        s[63 - i] = south >> i;
    }

    
    int64_t noea = (0x8040201008040200);
    for (int f=0; f < 8; f++, noea = eastOne(noea)) {
        int64_t norte = noea;
        for (int r8 = 0; r8 < 8*8; r8 += 8, norte <<= 8){
            nW[r8 + f] = norte;
        }
    }

    
    int64_t nowe = 0x0102040810204000;
    for (int f=7; f >= 0; f--, nowe = westOne(nowe)) {
        int64_t nortwe = nowe;
        for (int r8 = 0; r8 < 8*8; r8 += 8, nortwe <<= 8){
            nE[r8 + f] = nortwe;
        }
    }

    
    int64_t sowe = 0x0002040810204080;
    for (int f=7; f >= 0; f--, sowe = eastOne(sowe)) {
        int64_t souwe = sowe;
        for (int r8 = 63; r8 >= 0; r8 -= 8, souwe >>= 8){
            sW[r8 - f] = souwe;
        }
    }

    int64_t soea = 0x0040201008040201;

    for (int f=0; f < 8; f++, soea = westOne(soea)) {
        int64_t souea = soea;
        for (int r8 = 63; r8 >= 0; r8 -= 8, souea >>= 8){
            sE[r8 - f] = souea;
        }
    }

    //handle knight lookup table
    int64_t moves, curr;
    for (int i = 0; i < 64; i++) {
        moves = 0;
        curr = 1ULL << i;

        moves |= northOne(northOne(eastOne(curr)));  // Two north, one east
        moves |= northOne(northOne(westOne(curr)));  // Two north, one west
        moves |= southOne(southOne(eastOne(curr)));  // Two south, one east
        moves |= southOne(southOne(westOne(curr)));  // Two south, one west
        moves |= eastOne(eastOne(northOne(curr)));   // Two east, one north
        moves |= eastOne(eastOne(southOne(curr)));   // Two east, one south
        moves |= westOne(westOne(northOne(curr)));   // Two west, one north
        moves |= westOne(westOne(southOne(curr)));   // Two west, one south

        knightMoves[i] = moves;
    }

    //king lookup table
    for (int i = 0; i < 64; i++) {
        moves = 0;
        curr = 1ULL << i;

        moves |= eastOne(curr);
        moves |= westOne(curr);
        moves |= northOne(curr);
        moves |= southOne(curr);
        moves |= eastOne(northOne(curr));
        moves |= westOne(northOne(curr));
        moves |= eastOne(southOne(curr));
        moves |= westOne(southOne(curr));

        kingMoves[i] = moves;
    }
}

int bitscan_reverse(int64_t x) {
    return 63 - __builtin_clzll(x);
}

//BUGGY
int64_t MoveGeneration::generate_diagonal_moves(int currentPosition) {
    int64_t ans = 0;
    int64_t sameColor;
    int64_t differentColor;
    whiteTurn ? sameColor = white_pieces : sameColor = black_pieces;
    whiteTurn ? differentColor = black_pieces : differentColor = white_pieces;

    //handle nE
    int64_t northEastOccupied = nE[currentPosition] & (black_pieces | white_pieces);
    if (northEastOccupied == 0) {
        ans |= nE[currentPosition];
    } else {
        int index = __builtin_ctzll(northEastOccupied);
        ans |= nE[currentPosition];
        ans &= ~(nE[index]);

        if (sameColor & (1ULL << index)) {
            ans &= ~(1ULL << index);
        }
    }

    //handle nW
    int64_t northWestOccupied = nW[currentPosition] & (black_pieces | white_pieces);
    if (northWestOccupied == 0) {
        ans |= nW[currentPosition];
    } else {
        int index = __builtin_ctzll(northWestOccupied);
        ans |= nW[currentPosition];
        ans &= ~(nW[index]);

        if (sameColor & (1ULL << index)) {
            ans &= ~(1ULL << index);
        }
    }

    //handle sE
    int64_t southEastOccupied = sE[currentPosition] & (black_pieces | white_pieces);
    if (southEastOccupied == 0) {
        ans |= sE[currentPosition];
    } else {
        int index = bitscan_reverse(southEastOccupied);
        ans |= sE[currentPosition];
        ans &= ~(sE[index]);

        if (sameColor & (1ULL << index)) {
            ans &= ~(1ULL << index);
        }
    }

    //handle sW
    int64_t southWestOccupied = sW[currentPosition] & (black_pieces | white_pieces);
    if (southWestOccupied == 0) {
        ans |= sW[currentPosition];
    } else {
        int index = bitscan_reverse(southWestOccupied);
        ans |= sW[currentPosition];
        ans &= ~(sW[index]);

        if (sameColor & (1ULL << index)) {
            ans &= ~(1ULL << index);
        }
    }
    
    return ans;
}

int64_t MoveGeneration::generate_sideways_moves(int currentPosition) {
    int64_t ans = 0;
    int64_t sameColor;
    int64_t differentColor;
    whiteTurn ? sameColor = white_pieces : sameColor = black_pieces;
    whiteTurn ? differentColor = black_pieces : differentColor = white_pieces;

    //handle n
    int64_t northOccupied = n[currentPosition] & (black_pieces | white_pieces);
    if (northOccupied == 0) {
        ans |= n[currentPosition];
    } else {
        int index = __builtin_ctzll(northOccupied);
        ans |= n[currentPosition];
        ans &= ~(n[index]);

        if (sameColor & 1ULL << index) {
            ans &= ~(1ULL << index);
        }
    }

    //handle w
    int64_t westOccupied = w[currentPosition] & (black_pieces | white_pieces);
    if (westOccupied == 0) {
        ans |= w[currentPosition];
    } else {
        int index = __builtin_ctzll(westOccupied);
        ans |= w[currentPosition];
        ans &= ~(w[index]);

        if (sameColor & 1ULL << index) {
            ans &= ~(1ULL << index);
        }
    }

    //handle s
    int64_t southOccupied = s[currentPosition] & (black_pieces | white_pieces);
    if (southOccupied == 0) {
        ans |= s[currentPosition];
    } else {
        int index = bitscan_reverse(southOccupied);
        cout << "INDEX: " << index << endl;
        ans |= s[currentPosition];
        ans &= ~(s[index]);

        if (sameColor & 1ULL << index) {
            ans &= ~(1ULL << index);
        }
    }

    //handle e
    int64_t eastOccupied = e[currentPosition] & (black_pieces | white_pieces);
    if (eastOccupied == 0) {
        ans |= e[currentPosition];
    } else {
        int index = bitscan_reverse(eastOccupied);
        cout << "INDEX: " << index << endl;
        ans |= e[currentPosition];
        ans &= ~(e[index]);

        if (sameColor & 1ULL << index) {
            ans &= ~(1ULL << index);
        }
    }

    return ans;
}

int64_t MoveGeneration::generate_knight_moves(int currentPosition) {
    int64_t knightMovement = knightMoves[currentPosition];
    int64_t sameSidePieces;

    whiteTurn ? sameSidePieces = white_pieces : sameSidePieces = black_pieces;

    knightMovement ^= sameSidePieces & knightMovement;

    return knightMovement;
}

int64_t MoveGeneration::pawn_moves(int currentPosition) {
    int64_t ans = 0;
    int currentCol = currentPosition % 8;

    if (whiteTurn) {
        ans |= 1ULL << (currentPosition + 8);

        if (currentPosition >=8 && currentPosition <= 15 && empty_square(currentPosition+16)) {
            ans |=  1ULL << (currentPosition + 16);
        }

        if (currentCol - 1 >= 0) {
            ans |= black_pieces & 1ULL << (currentPosition + 7);
        }

        if (currentCol + 1 <= 7) {
            ans |= black_pieces & 1ULL << (currentPosition + 9);
        }

        if ((black_pieces | white_pieces) & 1ULL << (currentPosition + 8)) {
            ans &= ~(1ULL << (currentPosition + 8) | 1ULL << (currentPosition + 16));
        }

        if (enPassantSquare == currentPosition + 1) {
            ans |= 1ULL << (currentPosition + 9);
        } else if (enPassantSquare == currentPosition - 1) {
            ans |= 1ULL << (currentPosition + 7);
        }
    } else {
        ans |= 1ULL << (currentPosition - 8);

        if (currentPosition >= 48 && currentPosition <= 55) {
            ans |= 1ULL << (currentPosition - 16);
        }

        if (currentCol - 1 >= 0) {
            ans |= white_pieces & 1ULL << (currentPosition - 9);
        }

        if (currentCol + 1 <= 7) {
            ans |= white_pieces & 1ULL << (currentPosition - 7);
        }

        if ((black_pieces | white_pieces) & 1ULL << (currentPosition - 8)) {
            ans &= ~(1ULL << (currentPosition - 8) | 1ULL << (currentPosition - 16));
        }

        if (enPassantSquare == currentPosition + 1) {
            ans |= 1ULL << (currentPosition - 7);
        } else if (enPassantSquare == currentPosition - 1) {
            ans |= 1ULL << (currentPosition - 9);
        }
    }
    
    return ans;
}

bool multiple_set_bits(int64_t x) {
    return x & (x - 1);
}

int64_t MoveGeneration::every_pinned_piece() {
    int kingLocation;
    int64_t sameSide, opponentSide, queens, rooks, bishops, samePawns, enemyPawns;
    int64_t ans = 0;

    whiteTurn ? kingLocation = __builtin_ffsll(white_king) - 1 : kingLocation = __builtin_ffsll(black_king) - 1;
    whiteTurn ? sameSide = white_pieces : sameSide = black_pieces;
    whiteTurn ? opponentSide = black_pieces : opponentSide = white_pieces;
    whiteTurn ? queens = black_queens : queens = white_queens;
    whiteTurn ? rooks = black_rooks : rooks = white_rooks;
    whiteTurn ? bishops = black_bishops : bishops = white_bishops;
    whiteTurn ? samePawns = white_pawns : samePawns = black_pawns;
    whiteTurn ? enemyPawns = black_pawns : enemyPawns = white_pawns;

    int64_t north, south, east, west, northEast, northWest, southEast, southWest;

    north = n[kingLocation];
    south = s[kingLocation];
    east = e[kingLocation];
    west = w[kingLocation];
    northEast = nE[kingLocation];
    northWest = nW[kingLocation];
    southEast = sE[kingLocation];
    southWest = sW[kingLocation];
    
    int64_t attackers = 0;
    int attacker_index;

    attackers = west & (rooks | queens);
    attacker_index = __builtin_ctzll(attackers);
    if (attackers) {
        int64_t same_ray = (west & e[attacker_index]) & (sameSide | opponentSide);
        same_ray &= ~(1ULL << attacker_index);

        if (!multiple_set_bits(same_ray) && (same_ray & sameSide)) {
            ans |= same_ray;
        }

        //handle en passant pin
        int64_t relevantPawns = samePawns & west;
        
    }

    attackers = east & (rooks | queens);
    attacker_index = __builtin_ctzll(attackers);
    if (attackers) {
        int64_t same_ray = (east & w[attacker_index]) & (sameSide | opponentSide);
        same_ray &= ~(1ULL << attacker_index);

        if (!multiple_set_bits(same_ray) && (same_ray & sameSide)) {
            ans |= same_ray;
        }
    }

    return ans;
}

//MAKE SURE EN PASSANT IS TAKEN INTO ACCOUNT
int64_t MoveGeneration::isPinned(int currentPosition, pieces pieceType) {
    int kingLocation;
    int dr; //+1, -1, 0
    int dc; //+1, -1, 0
    int64_t sameColor;
    int64_t differentColor;

    if (whiteTurn) {
        sameColor = white_pieces;
        differentColor = black_pieces;
        kingLocation = __builtin_ffsll(white_king) - 1;
    } else {
        sameColor = black_pieces;
        differentColor = white_pieces;
        kingLocation = __builtin_ffsll(black_king) - 1;
    }

    int positionRow = currentPosition / 8;
    int positionCol = 7 - currentPosition % 8;
    int kingRow = kingLocation / 8;
    int kingCol = 7 - kingLocation % 8;
    int64_t ray;

    if (abs(kingRow - positionRow) == abs(kingCol - positionCol)) {
        kingRow > positionRow ? dr = -1 : dr = 1;
        kingCol > positionCol ? dc = -1 : dc = 1;
    } else if (kingRow == positionRow) {
        kingCol > positionCol ? dc = -1 : dc = 1;
        dr = 0;
    } else if (kingCol == positionCol) {
        kingRow > positionRow ? dr = -1 : dr = 1;
        dc = 0;
    } else {
        return universal;
    }

    
    if (dr == 0) {
        //horizontal
        dc > 0 ? ray = e[kingLocation] : ray = w[kingLocation];
    } else if (dc == 0) {
        dr > 0 ? ray = n[kingLocation] : ray = s[kingLocation];
    } else {
        //diagonal
        if (dr > 0 && dc > 0) {
            ray = nE[kingLocation];
        } else if (dr > 0 && dc < 0) {
            ray = nW[kingLocation];
        } else if (dr < 0 && dc > 0) {
            ray = sE[kingLocation];
        } else {
            ray = sW[kingLocation];
        }
    }

    //print_board(ray);

    return ray;
}

void timeFunction(int currentPosition, int64_t (*func)()) {
    auto start = high_resolution_clock::now();
    func();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);

    cout << duration.count() << endl;
}

//REFACTOR
int64_t MoveGeneration::attacked_by(int currentPosition) {
    int64_t* differentColor[6];
    int kingLocation;
    int kingShifts[8][2] = {{0, -1}, {0, 1}, {1, 0}, {-1, 0}, {1, 1}, {-1, -1}, {-1, 1}, {1, -1}};
    int curr;
    int row = currentPosition / 8;
    int col = currentPosition % 8;
    whiteTurn ? copy(begin(blackBitboards), end(blackBitboards), begin(differentColor)) : copy(begin(whiteBitboards), end(whiteBitboards), begin(differentColor));
    whiteTurn ? kingLocation = __builtin_ffsll(white_king) - 1 : kingLocation = __builtin_ffsll(black_king) - 1;
    
    int64_t diagonals = generate_diagonal_moves(currentPosition);
    int64_t sideways = generate_sideways_moves(currentPosition);

    return ((diagonals | sideways) & *differentColor[4])
        |   (diagonals & *differentColor[2])
        |   (generate_knight_moves(currentPosition) & *differentColor[1])
        |   (sideways & *differentColor[3])
        |  (pawn_moves(currentPosition) & *differentColor[0])
        |   (kingMoves[currentPosition] & *differentColor[5]);
}

//POTENTIALLY REFACTOR BECAUSE ATTACKED BY IS USED SO MUCH
int64_t MoveGeneration::castle_condition() {
    int64_t ans = 0;

    if (whiteTurn) {
        if (whiteKingMoved) {
            return 0;
        }

        if (!(!whiteHCastle || attacked_by(2) || attacked_by(1))) {;
            ans |= 1ULL << 1;
            
        }

        if (!(!whiteACastle || attacked_by(5) || attacked_by(4))) {
            ans |= 1ULL << 5;
        }
    } else {
        if (blackKingMoved) {
            return 0;
        }

        if (!(!blackHCastle || attacked_by(58) || attacked_by(57))) {
            ans |= 1ULL << 57;
        }

        if (!(!blackACastle || attacked_by(61) || attacked_by(60))) {
            ans |= 1ULL << 61;
        }
    }
    return ans & ~(black_pieces | white_pieces);
}

//REFACTOR
int64_t MoveGeneration::king_moves(int currentPosition) {
    int kingShifts[8][2] = {{0, -1}, {0, 1}, {1, 0}, {-1, 0}, {1, 1}, {-1, -1}, {-1, 1}, {1, -1}};
    int64_t ans = 0;
    int row = currentPosition / 8;
    int col = currentPosition % 8;

    int64_t* sameColor[6];
    int64_t* differentColor[6];

    whiteTurn ? copy(begin(whiteBitboards), end(whiteBitboards), begin(sameColor)) : copy(begin(blackBitboards), end(blackBitboards), begin(sameColor));
    whiteTurn ? copy(begin(blackBitboards), end(blackBitboards), begin(differentColor)) : copy(begin(whiteBitboards), end(whiteBitboards), begin(differentColor));

    //whiteTurn ? sameColor = white_pieces : sameColor = black_pieces;

    for (int i = 0; i < 8; ++i) {
        int newRow = row + kingShifts[i][0];
        int newCol = col + kingShifts[i][1];
        int curr = newRow * 8 + newCol;
        if (newRow < 0 || newRow > 7 || newCol < 0 || newCol > 7 || attacked_by(curr)) {
            continue;
        }
        int64_t bitMask = 1ULL << curr;
        bool occupiedBySameColor = false;
        for (int j = 0; j < 6; ++j) {
            if (*sameColor[j] & bitMask) {
                occupiedBySameColor = true;
                break;
            }
        }
        if (!occupiedBySameColor) {
            ans |= bitMask;
        }
    }

    ans |= castle_condition();

    return ans;
}

//REFACTOR, MAKE MORE LIKE ISPINNED AND HAVE IT DIRECT A RAY FROM THE KING TO THE ATTACKER
int64_t MoveGeneration::check_checker() {
    int kingLocation;
    int dr; //+1, -1, 0
    int dc; //+1, -1, 0
    bool up;
    bool right;
    bool straight;
    bool diagonal;
    int64_t* sameColor[6];
    int64_t* differentColor[6];

    if (whiteTurn) {
        copy(begin(whiteBitboards), end(whiteBitboards), begin(sameColor));
        copy(begin(blackBitboards), end(blackBitboards), begin(differentColor));
        kingLocation = __builtin_ffsll(white_king) - 1;
    } else {
        copy(begin(blackBitboards), end(blackBitboards), begin(sameColor));
        copy(begin(whiteBitboards), end(whiteBitboards), begin(differentColor));
        kingLocation = __builtin_ffsll(black_king) - 1;
    }

    int kingRow = kingLocation / 8;
    int kingCol = 7 - kingLocation % 8;

    int64_t attacker_bitboard = attacked_by(kingLocation);
    int setBits = __builtin_popcountll(attacker_bitboard);

    if (setBits == 0) {
        return universal;
    } else if (setBits >= 2) {
        return 0;
    }

    int attackerLocation = __builtin_ffsll(attacker_bitboard) - 1;
    int positionRow = attackerLocation / 8;
    int positionCol = 7 - attackerLocation % 8;

    if (abs(kingRow - positionRow) == abs(kingCol - positionCol)) {
        kingRow > positionRow ? dr = -1 : dr = 1;
        kingCol > positionCol ? dc = -1 : dc = 1;
    } else if (kingRow == positionRow) {
        kingCol > positionCol ? dc = -1 : dc = 1;
        dr = 0;
    } else if (kingCol == positionCol) {
        kingRow > positionRow ? dr = -1 : dr = 1;
        dc = 0;
    } else {
        return universal;
    }

    int r = kingRow + dr;
    int c = kingCol + dc;
    int curr = r * 8 + (7 - c);
    int64_t ans = 0;
    bool foundSelf = false;

    while (r <= 7 && c <= 7 && r >= 0 && c >= 0) {
        if (r==positionRow && c==positionCol) {
            ans |= 1ULL << curr;
            return ans;
        } else {
            ans |= 1ULL << curr;
        }
        r += dr;
        c += dc;
        curr = r * 8 + (7 - c);
    }
    return universal;
}

//ADD & IN FRONT OF Move SO THAT IT CAN STORE THE GAME STATE
void MoveGeneration::make_move(Move move) {
    if (!(1ULL << 0 & white_rooks)) {
        whiteHCastle = false;
    }
    if (!(1ULL << 7 & white_rooks)) {
        whiteACastle = false;
    }
    if (!(1ULL << 56 & black_rooks)) {
        blackHCastle = false;
    }
    if (!(1ULL << 63 & black_rooks)) {
        blackACastle = false;
    }

    
    if (whiteTurn) {
        if (move.piece == 3) {
            if (move.before == 0) {
                whiteHCastle = false;
            } else if (move.before == 7) {
                whiteACastle = false;
            }
        }

        if (move.piece == 5) {
            whiteKingMoved = true;
        }

        if (move.piece == 0 && move.after - move.before == 16) {
            enPassantSquare = move.after;
        } else {
            enPassantSquare = -1;
        }

        if (move.promotion != -1) {
            *whiteBitboards[move.piece] &= ~(1ULL << move.before);
            *whiteBitboards[move.promotion] |= 1ULL << move.after;
        } else if (move.castle != -1) {
            move_bit(whiteBitboards[move.piece], move.before, move.after);
            if (move.after == 1) {
                move_bit(whiteBitboards[move.castle], 0, 2);
            } else {
                move_bit(whiteBitboards[move.castle], 7, 4);
            }
        } else {
            move_bit(whiteBitboards[move.piece], move.before, move.after);
        }

        for (int i = 0; i < 6; ++i) {
            if (*blackBitboards[i] & (1ULL << move.after)) {
                *blackBitboards[i] &= ~(1ULL << move.after);
                break;
            }
        }

        move_bit(&white_pieces, move.before, move.after);
    } else {
        if (move.piece == 3) {
            if (move.before == 56) {
                blackHCastle = false;
            } else if (move.before == 63) {
                blackACastle = false;
            }
        }

        if (move.piece == 5) {
            blackKingMoved = true;
        }

        if (move.piece == 0 && move.before - move.after == 16) {
            enPassantSquare = move.after;
        } else {
            enPassantSquare = -1;
        }

        if (move.promotion != -1) {
            *blackBitboards[move.piece] &= ~(1ULL << move.before);
            *blackBitboards[move.promotion] |= 1ULL << move.after;
        } else if (move.castle != -1) {
            move_bit(blackBitboards[move.piece], move.before, move.after);
            if (move.after == 57) {
                move_bit(blackBitboards[3], 56, 58);
                blackHCastle = false;
            } else if (move.after == 61) {
                move_bit(blackBitboards[3], 63, 60);
                blackACastle = false;
            }
        } else {
            move_bit(blackBitboards[move.piece], move.before, move.after);
        }

        for (int i = 0; i < 6; ++i) {
            if (*whiteBitboards[i] & (1ULL << move.after)) {
                *whiteBitboards[i] &= ~(1ULL << move.after);
                break;
            }
        }
    }

    whiteTurn = !whiteTurn;
}

void MoveGeneration::set_state(Move& move) {
    move.enPassantSquare = enPassantSquare;
    move.whiteKingMoved = whiteKingMoved;
    move.blackKingMoved = blackKingMoved;
    move.whiteHCastle = whiteHCastle;
    move.whiteACastle = whiteACastle;
    move.blackACastle = blackACastle;
    move.blackHCastle = blackHCastle;
}

void MoveGeneration::undo_move(Move move) {
    //helper info
    /*
        int before;
        int after;
        int promotion;
        int castle;
        pieces piece;
        pieces captured_piece;
    */

    //reload the past board state
    whiteTurn = !whiteTurn;
    enPassantSquare = move.enPassantSquare;
    whiteKingMoved = move.whiteKingMoved;
    blackKingMoved = move.blackKingMoved;
    whiteHCastle = move.whiteHCastle;
    whiteACastle = move.whiteACastle;
    blackACastle = move.blackACastle;
    blackHCastle = move.blackHCastle;

    //return the piece to where it needs to be
    if (whiteTurn) {
        if (move.castle != -1) {
            if (move.castle == 2) {
                move_bit(whiteBitboards[ROOK], 2, 0);
            } else {
                move_bit(whiteBitboards[ROOK], 4, 7);
            }
        }

        //unset if it was a promotion
        if (move.promotion != -1) {
            *whiteBitboards[move.promotion] &= ~(1ULL << move.after);
            *whiteBitboards[move.piece] |= (1ULL << move.before);
        } else {
            move_bit(whiteBitboards[move.piece], move.after, move.before);  
        }
        if (move.captured_piece != -1) {
            *blackBitboards[move.captured_piece] |= 1ULL << move.after;
        }
    } else {
        if (move.castle != -1) {
            if (move.castle == 58) {
                move_bit(blackBitboards[ROOK], 58, 56);
            } else {
                move_bit(blackBitboards[ROOK], 60, 63);
            }
        }

        //unset if it was a promotion
        if (move.promotion != -1) {
            *blackBitboards[move.promotion] &= ~(1ULL << move.after);
            *blackBitboards[move.piece] |= (1ULL << move.before);
        } else {
            move_bit(blackBitboards[move.piece], move.after, move.before);
        }
        if (move.captured_piece != -1) {
            *whiteBitboards[move.captured_piece] |= 1ULL << move.after;
        }
    }
}

//CHANGE FEN TO ALSO PARSE FOR OTHER VARIABLES LIKE CASTLING RIGHTS, EN PASSANT SQUARE, ETC
void MoveGeneration::set_fen(string fen) {
    int curr = 63;
    vector<char> chars = {'p', 'n', 'b', 'r', 'q', 'k', 'P', 'N', 'B', 'R', 'Q', 'K'};
    set<char> characters(chars.begin(), chars.end());
    map<char, pieces> helper;
    helper = {{'p', PAWN}, {'P', PAWN}, {'b', BISHOP}, {'B', BISHOP}, {'n', KNIGHT}, {'N', KNIGHT}, {'r', ROOK}, {'R', ROOK}, {'q', QUEEN}, {'Q', QUEEN}, {'k', KING}, {'K', KING}};
    white_pawns = 0;
    white_bishops = 0;
    white_knights = 0;
    white_rooks = 0;
    white_queens = 0;
    white_king = 0;

    black_pawns = 0;
    black_bishops = 0;
    black_knights = 0;
    black_rooks = 0;
    black_queens = 0;
    black_king = 0;

    vector<string> splitFen;
    istringstream iss(fen); 
    string word;

    while (getline(iss, word, ' ')) { 
        splitFen.push_back(word);
    }

    for (const std::string& w : splitFen) {
        std::cout << w << std::endl;
    }

    for (int i = 0; i < splitFen[0].size(); i++) {
        if (characters.find(splitFen[0][i]) != characters.end()) {
            if (isupper(splitFen[0][i])) {
                *whiteBitboards[helper[splitFen[0][i]]] |= 1ULL << curr;
            } else {
                *blackBitboards[helper[splitFen[0][i]]] |= 1ULL << curr;
            }
            curr--;
        } else if (isdigit(splitFen[0][i])) {
            curr -= ((splitFen[0][i] - '0'));
        }
    }

    if (splitFen[1] == "w") {
        whiteTurn = true;
    } else {
        whiteTurn = false;
    }

    if (splitFen[2] == "-") {
        whiteHCastle = false;
        whiteACastle = false;
        blackHCastle = false;
        blackACastle = false;
    } else {
        for (int i = 0; i < splitFen[2].size(); i++) {
            if (splitFen[2][i] == 'K') {
                whiteHCastle = true;
            } else if (splitFen[2][i] == 'Q') {
                whiteACastle = true;
            } else if (splitFen[2][i] == 'k') {
                blackHCastle = true;
            } else if (splitFen[2][i] == 'q') {
                blackACastle = true;
            }
        }
    }

    if (splitFen[3] != "-") {
        whiteTurn ? enPassantSquare = stoi(splitFen[3])-8 : enPassantSquare = stoi(splitFen[3])+8;
    }    

    white_pieces = white_pawns | white_bishops | white_knights | white_rooks | white_queens | white_king;
    black_pieces = black_pawns | black_bishops | black_knights | black_rooks | black_queens | black_king;
}

vector<Move> MoveGeneration::bitboard_to_moves(int64_t bitboard, int currentLocation, pieces piece) {
    vector<Move> moves;
    


    return moves;
}

vector<Move> MoveGeneration::generate_all_moves() {
    vector<Move> moves;
    
    

    return moves;
}

int MoveGeneration::count_moves(int depth) {
    if (depth == 0) {
        return 1;
    }
    vector<Move> moves = generate_all_moves();
    int count = 0;
    for (Move move : moves) {
        make_move(move);
        count += count_moves(depth - 1);
        undo_move(move);
    }
    return count;
}

/*
int board[8][8] = {
    {63, 62, 61, 60, 59, 58, 57, 56},
    {55, 54, 53, 52, 51, 50, 49, 48},
    {47, 46, 45, 44, 43, 42, 41, 40},
    {39, 38, 37, 36, 35, 34, 33, 32},
    {31, 30, 29, 28, 27, 26, 25, 24},
    {23, 22, 21, 20, 19, 18, 17, 16},
    {15, 14, 13, 12, 11, 10,  9,  8},
    {7,   6,  5,  4,  3,  2,  1,  0}
};
*/

int main() {
    MoveGeneration game;

    game.initialize_lookup_table();
    game.set_fen("8/1N6/4P3/3B4/2p5/5b2/8/7n w - - 0 1");
    print_all_board(game);

    //game.isPinned(29, KNIGHT);

    auto start = high_resolution_clock::now();
    game.generate_sideways_moves(27);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);

    cout << duration.count() << endl;

    print_board(game.generate_diagonal_moves(36));

    return 0;
}