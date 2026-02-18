#pragma once
// ============================================================
// types.h — Core types, constants, and Move struct
// ============================================================

#include <cstdint>
#include <string>

// ─── Piece types ────────────────────────────────────────────
// Positive = white, negative = black, 0 = empty
enum PieceType { PT_NONE=0, PT_PAWN=1, PT_KNIGHT=2, PT_BISHOP=3, PT_ROOK=4, PT_QUEEN=5, PT_KING=6 };

// Piece constants (signed, white positive, black negative)
constexpr int W_PAWN   =  1, W_KNIGHT =  2, W_BISHOP =  3;
constexpr int W_ROOK   =  4, W_QUEEN  =  5, W_KING   =  6;
constexpr int B_PAWN   = -1, B_KNIGHT = -2, B_BISHOP = -3;
constexpr int B_ROOK   = -4, B_QUEEN  = -5, B_KING   = -6;

constexpr int WHITE_SIDE = 0;
constexpr int BLACK_SIDE = 1;

// ─── Square helpers ─────────────────────────────────────────
// 0=a1, 1=b1, ..., 63=h8
inline int sq_file(int s)           { return s & 7; }
inline int sq_rank(int s)           { return s >> 3; }
inline int make_sq(int f, int r)    { return (r << 3) | f; }
inline int mirror_sq(int s)         { return s ^ 56; }
inline bool sq_valid(int s)         { return s >= 0 && s < 64; }

// ─── Piece helpers ──────────────────────────────────────────
inline int piece_type(int p)  { return p > 0 ? p : -p; }
inline int piece_side(int p)  { return p > 0 ? WHITE_SIDE : BLACK_SIDE; }
inline int piece_sign(int side) { return side == WHITE_SIDE ? 1 : -1; }

// ─── Move struct ────────────────────────────────────────────
constexpr int FL_NONE    = 0;
constexpr int FL_CASTLE  = 1;
constexpr int FL_EP      = 2;
constexpr int FL_DOUBLE  = 4;

struct Move {
    uint8_t from, to;
    int8_t captured;    // Piece on target square before move (0 if none)
    int8_t promotion;   // Promoted piece (signed, 0 if none)
    uint8_t flags;

    Move() : from(0), to(0), captured(0), promotion(0), flags(0) {}
    Move(int f, int t, int cap=0, int promo=0, int fl=0)
        : from(f), to(t), captured(cap), promotion(promo), flags(fl) {}

    bool operator==(const Move& o) const {
        return from == o.from && to == o.to && promotion == o.promotion;
    }
    bool is_null() const { return from == to; }

    std::string uci() const {
        std::string s;
        s += char('a' + sq_file(from));
        s += char('1' + sq_rank(from));
        s += char('a' + sq_file(to));
        s += char('1' + sq_rank(to));
        if (promotion) {
            constexpr char pc[] = " nbrq";
            s += pc[piece_type(promotion)];
        }
        return s;
    }

    static Move from_uci(const std::string& s, const int* board);
};

// ─── Undo info ──────────────────────────────────────────────
struct UndoInfo {
    int castling;
    int ep_square;
    int halfmove;
    uint64_t hash;
};

// ─── Constants ──────────────────────────────────────────────
constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY   = 128;
constexpr int INF_SCORE = 100000;
constexpr int MATE_SCORE = 99000;

// Piece values
constexpr int PIECE_VAL[] = { 0, 100, 320, 330, 500, 900, 20000 };

// ─── Direction tables ───────────────────────────────────────
constexpr int KNIGHT_DIRS[] = { 17, 15, 10, 6, -6, -10, -15, -17 };
constexpr int BISHOP_DIRS[] = {  9,  7, -7, -9 };
constexpr int ROOK_DIRS[]   = {  8,  1, -1, -8 };
constexpr int KING_DIRS[]   = {  1, -1,  8, -8,  9,  7, -7, -9 };
