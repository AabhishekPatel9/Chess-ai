#pragma once
// ============================================================
// search.h — Search engine, TT, evaluation
// ============================================================

#include "board.h"
#include <vector>
#include <chrono>

// ─── Transposition Table ───────────────────────────────────

enum TTFlag : uint8_t { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
    uint64_t key;
    int16_t  score;
    int8_t   depth;
    TTFlag   flag;
    Move     best;
};

struct SearchResult {
    Move best_move;
    int  score;
    int  depth;
    int  nodes;
    int  time_ms;
    int  tt_hits;
    int  tt_stores;
};

class Searcher {
public:
    Searcher(int tt_size_mb = 64);

    SearchResult search(Board& board, int max_depth, int max_time_ms);

private:
    // ─── Transposition Table ────────────────────────────────
    std::vector<TTEntry> tt;
    int tt_mask;
    int tt_hits, tt_stores;
    void tt_store(uint64_t key, int depth, int score, TTFlag flag, const Move& best);
    bool tt_probe(uint64_t key, int depth, int alpha, int beta,
                  int& score, Move& best) const;

    // ─── Search state ───────────────────────────────────────
    int nodes;
    Move killers[MAX_PLY][2];
    int history[2][64][64];

    // ─── Time ───────────────────────────────────────────────
    std::chrono::steady_clock::time_point start_time;
    int max_time;
    bool time_up;
    void check_time();

    // ─── Core search ────────────────────────────────────────
    int root_search(Board& board, int depth, Move& best_move);
    int alphabeta(Board& board, int depth, int alpha, int beta, int ply, bool null_ok);
    int quiescence(Board& board, int alpha, int beta, int ply);

    // ─── Evaluation ─────────────────────────────────────────
    int evaluate(const Board& board) const;

    // ─── Move ordering ──────────────────────────────────────
    void score_moves(const Board& board, Move* moves, int count, int ply,
                     const Move& tt_move, int* scores) const;
    void sort_moves(Move* moves, int* scores, int count, int start) const;
};
