#pragma once
// ============================================================
// board.h — Board state, move generation, attack detection
// ============================================================

#include "types.h"
#include <array>
#include <vector>
#include <string>

class Board {
public:
    int board[64];            // Piece at each square (signed: +white, -black)
    int side;                 // WHITE_SIDE or BLACK_SIDE
    int castling;             // Bits: 1=WK, 2=WQ, 4=BK, 8=BQ
    int ep_square;            // En passant target square (-1 if none)
    int halfmove;             // Half-move clock (for 50-move rule)
    int fullmove;
    uint64_t hash;            // Zobrist hash

    int king_sq[2];           // King square per side

    // ─── Zobrist ────────────────────────────────────────────
    static uint64_t Z_PIECE[13][64];  // [piece_index][square]
    static uint64_t Z_SIDE;
    static uint64_t Z_CASTLE[16];
    static uint64_t Z_EP[8];          // per file
    static bool z_init_done;
    static void init_zobrist();
    static int piece_index(int p) {   // Maps signed piece to 0-12
        if (p > 0) return p;          // 1-6 = white P,N,B,R,Q,K
        if (p < 0) return 6 + (-p);   // 7-12 = black P,N,B,R,Q,K
        return 0;
    }

    // ─── Construction ───────────────────────────────────────
    Board();
    void set_fen(const std::string& fen);
    std::string to_fen() const;

    // ─── Move execution ─────────────────────────────────────
    void make_move(const Move& m, UndoInfo& undo);
    void unmake_move(const Move& m, const UndoInfo& undo);
    void make_null_move(UndoInfo& undo);
    void unmake_null_move(const UndoInfo& undo);

    // ─── Move generation ────────────────────────────────────
    int gen_legal_moves(Move* moves) const;
    int gen_pseudo_moves(Move* moves) const;
    int gen_captures(Move* moves) const;
    bool is_legal(const Move& m);

    // ─── Attack detection ───────────────────────────────────
    bool is_attacked(int sq, int by_side) const;
    bool in_check() const { return is_attacked(king_sq[side], side ^ 1); }

    // ─── Utilities ──────────────────────────────────────────
    void compute_hash();
    bool is_draw() const;
    int count_repetitions() const;

private:
    void gen_pawn_moves(Move* moves, int& count) const;
    void gen_knight_moves(Move* moves, int& count) const;
    void gen_slider_moves(Move* moves, int& count, int piece_t) const;
    void gen_king_moves(Move* moves, int& count) const;

    void gen_pawn_captures(Move* moves, int& count) const;
    void gen_knight_captures(Move* moves, int& count) const;
    void gen_slider_captures(Move* moves, int& count, int piece_t) const;
    void gen_king_captures(Move* moves, int& count) const;

    // Position history for repetition detection
    static constexpr int MAX_HISTORY = 1024;
    uint64_t pos_history[MAX_HISTORY];
    int pos_history_count;
};
