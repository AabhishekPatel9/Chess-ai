// ============================================================
// search.cpp — Search, evaluation, transposition table
// ============================================================

#include "search.h"
#include <algorithm>
#include <cstring>
#include <cmath>

// ============================================================
// Piece-Square Tables (from White's perspective, a8=index 0)
// ============================================================

static const int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int PST_KNIGHT[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

static const int PST_BISHOP[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

static const int PST_ROOK[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

static const int PST_QUEEN[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

static const int PST_KING_MG[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};

static const int PST_KING_EG[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

static const int* PST_TABLE[] = {
    nullptr,     // PT_NONE
    PST_PAWN,    // PT_PAWN
    PST_KNIGHT,  // PT_KNIGHT
    PST_BISHOP,  // PT_BISHOP
    PST_ROOK,    // PT_ROOK
    PST_QUEEN,   // PT_QUEEN
    PST_KING_MG  // PT_KING (middlegame default)
};

// ============================================================
// Evaluation
// ============================================================

static bool is_endgame(const Board& board) {
    int queens = 0, minors = 0;
    for (int sq = 0; sq < 64; sq++) {
        int pt = piece_type(board.board[sq]);
        if (pt == PT_QUEEN) queens++;
        if (pt == PT_KNIGHT || pt == PT_BISHOP) minors++;
    }
    return queens == 0 || (queens <= 2 && minors <= 2);
}

int Searcher::evaluate(const Board& board) const {
    int score = 0;
    int white_material = 0, black_material = 0;
    int white_pawns = 0, black_pawns = 0;
    int white_bishops = 0, black_bishops = 0;
    bool endgame = is_endgame(board);

    // Pawn file tracking for structure eval
    int white_pawn_files[8] = {0};
    int black_pawn_files[8] = {0};

    for (int sq = 0; sq < 64; sq++) {
        int p = board.board[sq];
        if (p == 0) continue;

        int pt = piece_type(p);
        int val = PIECE_VAL[pt];
        const int* pst = (pt == PT_KING && endgame) ? PST_KING_EG : PST_TABLE[pt];

        if (p > 0) { // White
            white_material += val;
            int idx = mirror_sq(sq); // PST from white's perspective
            score += val + (pst ? pst[idx] : 0);
            if (pt == PT_PAWN) { white_pawns++; white_pawn_files[sq_file(sq)]++; }
            if (pt == PT_BISHOP) white_bishops++;
        } else { // Black
            black_material += val;
            int idx = sq; // PST already from correct perspective for black
            score -= val + (pst ? pst[idx] : 0);
            if (pt == PT_PAWN) { black_pawns++; black_pawn_files[sq_file(sq)]++; }
            if (pt == PT_BISHOP) black_bishops++;
        }
    }

    // Bishop pair bonus
    if (white_bishops >= 2) score += 30;
    if (black_bishops >= 2) score -= 30;

    // Pawn structure
    for (int f = 0; f < 8; f++) {
        // Doubled pawns penalty
        if (white_pawn_files[f] > 1) score -= 10 * (white_pawn_files[f] - 1);
        if (black_pawn_files[f] > 1) score += 10 * (black_pawn_files[f] - 1);

        // Isolated pawns penalty
        bool w_adj = (f > 0 && white_pawn_files[f-1]) || (f < 7 && white_pawn_files[f+1]);
        bool b_adj = (f > 0 && black_pawn_files[f-1]) || (f < 7 && black_pawn_files[f+1]);
        if (white_pawn_files[f] && !w_adj) score -= 15;
        if (black_pawn_files[f] && !b_adj) score += 15;
    }

    // Passed pawn bonus
    for (int sq = 0; sq < 64; sq++) {
        int p = board.board[sq];
        if (p == W_PAWN) {
            int f = sq_file(sq), r = sq_rank(sq);
            bool passed = true;
            for (int rr = r + 1; rr < 8 && passed; rr++) {
                for (int ff = std::max(0, f-1); ff <= std::min(7, f+1); ff++) {
                    if (board.board[make_sq(ff, rr)] == B_PAWN) { passed = false; break; }
                }
            }
            if (passed) score += 20 + 10 * r; // More bonus the further advanced
        }
        if (p == B_PAWN) {
            int f = sq_file(sq), r = sq_rank(sq);
            bool passed = true;
            for (int rr = r - 1; rr >= 0 && passed; rr--) {
                for (int ff = std::max(0, f-1); ff <= std::min(7, f+1); ff++) {
                    if (board.board[make_sq(ff, rr)] == W_PAWN) { passed = false; break; }
                }
            }
            if (passed) score -= 20 + 10 * (7 - r);
        }
    }

    // Rook on open/semi-open file
    for (int sq = 0; sq < 64; sq++) {
        int p = board.board[sq];
        if (piece_type(p) != PT_ROOK) continue;
        int f = sq_file(sq);
        if (p > 0) {
            if (!white_pawn_files[f] && !black_pawn_files[f]) score += 20; // Open
            else if (!white_pawn_files[f]) score += 10; // Semi-open
        } else {
            if (!white_pawn_files[f] && !black_pawn_files[f]) score -= 20;
            else if (!black_pawn_files[f]) score -= 10;
        }
    }

    // King safety: pawn shield in middlegame
    if (!endgame) {
        for (int s = 0; s < 2; s++) {
            int ksq = board.king_sq[s];
            int kf = sq_file(ksq), kr = sq_rank(ksq);
            int shield = 0;
            int pawn = (s == WHITE_SIDE) ? W_PAWN : B_PAWN;
            int dir = (s == WHITE_SIDE) ? 1 : -1;

            for (int df = -1; df <= 1; df++) {
                int ff = kf + df;
                if (ff < 0 || ff > 7) continue;
                int sr = kr + dir;
                if (sr >= 0 && sr < 8 && board.board[make_sq(ff, sr)] == pawn) shield++;
                sr = kr + 2 * dir;
                if (sr >= 0 && sr < 8 && board.board[make_sq(ff, sr)] == pawn) shield++;
            }
            if (s == WHITE_SIDE) score += shield * 10;
            else score -= shield * 10;
        }
    }

    // Return from White's perspective
    return score;
}

// ============================================================
// Transposition Table
// ============================================================

Searcher::Searcher(int tt_size_mb) : tt_hits(0), tt_stores(0), nodes(0),
                                     max_time(0), time_up(false) {
    int entries = (tt_size_mb * 1024 * 1024) / sizeof(TTEntry);
    // Round down to power of 2
    int size = 1;
    while (size * 2 <= entries) size *= 2;
    tt.resize(size);
    tt_mask = size - 1;
    memset(history, 0, sizeof(history));
    memset(killers, 0, sizeof(killers));
    for (auto& e : tt) e.key = 0;
}

void Searcher::tt_store(uint64_t key, int depth, int score, TTFlag flag, const Move& best) {
    int idx = (int)(key & tt_mask);
    TTEntry& e = tt[idx];
    // Replace if: new depth >= stored depth, or different position
    if (e.key != key || depth >= e.depth) {
        e.key = key;
        e.score = (int16_t)score;
        e.depth = (int8_t)depth;
        e.flag = flag;
        e.best = best;
        tt_stores++;
    }
}

bool Searcher::tt_probe(uint64_t key, int depth, int alpha, int beta,
                        int& score, Move& best) const {
    int idx = (int)(key & tt_mask);
    const TTEntry& e = tt[idx];
    if (e.key != key) return false;
    best = e.best;

    if (e.depth >= depth) {
        score = e.score;

        if (e.flag == TT_EXACT) { return true; }
        if (e.flag == TT_LOWER && score >= beta)  { return true; }
        if (e.flag == TT_UPPER && score <= alpha) { return true; }
    }
    return false;
}

// ============================================================
// Time Management
// ============================================================

void Searcher::check_time() {
    if (max_time <= 0) return;
    auto now = std::chrono::steady_clock::now();
    int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time).count();
    if (elapsed >= max_time) time_up = true;
}

// ============================================================
// Move Ordering
// ============================================================

void Searcher::score_moves(const Board& board, Move* moves, int count,
                           int ply, const Move& tt_move, int* scores) const {
    for (int i = 0; i < count; i++) {
        const Move& m = moves[i];

        if (m == tt_move) {
            scores[i] = 10000000;
        } else if (m.captured) {
            // MVV-LVA: victim value * 10 - attacker value
            int victim = PIECE_VAL[piece_type(m.captured)];
            int attacker = PIECE_VAL[piece_type(board.board[m.from])];
            scores[i] = 5000000 + victim * 10 - attacker;
        } else if (m.promotion) {
            scores[i] = 4500000 + PIECE_VAL[piece_type(m.promotion)];
        } else if (ply < MAX_PLY && m == killers[ply][0]) {
            scores[i] = 4000000;
        } else if (ply < MAX_PLY && m == killers[ply][1]) {
            scores[i] = 3900000;
        } else {
            int side = piece_side(board.board[m.from]);
            scores[i] = history[side][m.from][m.to];
        }
    }
}

void Searcher::sort_moves(Move* moves, int* scores, int count, int start) const {
    // Selection sort from 'start' — find best remaining and swap to front
    int best_idx = start;
    for (int i = start + 1; i < count; i++) {
        if (scores[i] > scores[best_idx]) best_idx = i;
    }
    if (best_idx != start) {
        std::swap(moves[start], moves[best_idx]);
        std::swap(scores[start], scores[best_idx]);
    }
}

// ============================================================
// Search: Iterative Deepening
// ============================================================

SearchResult Searcher::search(Board& board, int max_depth, int max_time_ms) {
    start_time = std::chrono::steady_clock::now();
    max_time = max_time_ms;
    time_up = false;
    nodes = 0;
    tt_hits = 0;
    tt_stores = 0;
    memset(killers, 0, sizeof(killers));
    memset(history, 0, sizeof(history));

    SearchResult result;
    result.best_move = Move();
    result.score = 0;
    result.depth = 0;

    // Get initial legal moves
    Move legal[MAX_MOVES];
    int n = board.gen_legal_moves(legal);
    if (n == 0) return result;
    result.best_move = legal[0];

    if (max_depth <= 0) max_depth = 100; // unlimited — time controls us

    for (int depth = 1; depth <= max_depth; depth++) {
        Move best;
        int score;

        // Aspiration windows (from depth 5+)
        if (depth >= 5) {
            int delta = 50;
            int alpha = result.score - delta;
            int beta  = result.score + delta;

            score = root_search(board, depth, best);

            // If fell outside window, re-search with full window
            if (time_up) break;
            if (score <= alpha || score >= beta) {
                score = root_search(board, depth, best);
            }
        } else {
            score = root_search(board, depth, best);
        }

        if (time_up && depth > 1) break; // Use previous iteration's result

        if (!best.is_null()) {
            result.best_move = best;
            result.score = score;
            result.depth = depth;
        }

        // If found mate, no point searching deeper
        if (abs(score) > MATE_SCORE - 100) break;

        // Time check: don't start next depth if >50% used
        auto now = std::chrono::steady_clock::now();
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time).count();
        if (max_time_ms > 0 && elapsed > max_time_ms / 2) break;
    }

    auto end = std::chrono::steady_clock::now();
    result.time_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start_time).count();
    result.nodes = nodes;
    result.tt_hits = tt_hits;
    result.tt_stores = tt_stores;
    return result;
}

// ============================================================
// Root Search
// ============================================================

int Searcher::root_search(Board& board, int depth, Move& best_move) {
    Move moves[MAX_MOVES];
    int n = board.gen_legal_moves(moves);
    if (n == 0) {
        best_move = Move();
        return board.in_check() ? -MATE_SCORE : 0;
    }

    int scores[MAX_MOVES];
    Move tt_best;
    int tt_score;
    if (tt_probe(board.hash, 0, -INF_SCORE, INF_SCORE, tt_score, tt_best)) {
        // Just use for ordering, don't trust the score at root
    }
    score_moves(board, moves, n, 0, tt_best, scores);

    int alpha = -INF_SCORE, beta = INF_SCORE;
    int best_score = -INF_SCORE;
    best_move = moves[0];

    for (int i = 0; i < n; i++) {
        sort_moves(moves, scores, n, i);

        UndoInfo undo;
        board.make_move(moves[i], undo);
        int score = -alphabeta(board, depth - 1, -beta, -alpha, 1, true);
        board.unmake_move(moves[i], undo);

        if (time_up) break;

        if (score > best_score) {
            best_score = score;
            best_move = moves[i];
        }
        if (score > alpha) alpha = score;
    }

    tt_store(board.hash, depth, best_score, TT_EXACT, best_move);
    return best_score;
}

// ============================================================
// Alpha-Beta Search
// ============================================================

int Searcher::alphabeta(Board& board, int depth, int alpha, int beta,
                        int ply, bool null_ok) {
    nodes++;
    if ((nodes & 4095) == 0) check_time();
    if (time_up) return 0;

    // Draw detection
    if (board.is_draw()) return 0;

    // TT lookup
    Move tt_best;
    int tt_score;
    bool tt_hit = tt_probe(board.hash, depth, alpha, beta, tt_score, tt_best);
    if (tt_hit && ply > 0) {
        tt_hits++;
        return tt_score;
    }

    // Quiescence at leaf
    if (depth <= 0) return quiescence(board, alpha, beta, ply);

    bool in_check = board.in_check();
    if (in_check) depth++; // Check extension

    // Null-move pruning
    if (null_ok && !in_check && depth >= 3 && !is_endgame(board)) {
        int R = depth >= 6 ? 3 : 2;
        UndoInfo undo;
        board.make_null_move(undo);
        int null_score = -alphabeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
        board.unmake_null_move(undo);
        if (time_up) return 0;
        if (null_score >= beta) return beta;
    }

    // Generate and check legal moves
    Move moves[MAX_MOVES];
    int n = board.gen_legal_moves(moves);

    if (n == 0) {
        return in_check ? -(MATE_SCORE - ply) : 0; // Checkmate or stalemate
    }

    // Move ordering
    int scores[MAX_MOVES];
    score_moves(board, moves, n, ply, tt_best, scores);

    int best_score = -INF_SCORE;
    Move best_move = moves[0];
    TTFlag tt_flag = TT_UPPER;

    for (int i = 0; i < n; i++) {
        sort_moves(moves, scores, n, i);
        const Move& m = moves[i];

        bool is_cap = m.captured != 0;
        bool is_promo = m.promotion != 0;

        UndoInfo undo;
        board.make_move(m, undo);
        bool gives_check = board.in_check();

        int score;

        // Late Move Reductions (LMR)
        if (i >= 3 && depth >= 3 && !in_check && !gives_check &&
            !is_cap && !is_promo) {
            // Reduced search
            int R = 1 + (i >= 6 ? 1 : 0) + (depth >= 6 ? 1 : 0);
            score = -alphabeta(board, depth - 1 - R, -alpha - 1, -alpha, ply + 1, true);

            if (score > alpha) {
                // Re-search at full depth
                score = -alphabeta(board, depth - 1, -beta, -alpha, ply + 1, true);
            }
        } else {
            score = -alphabeta(board, depth - 1, -beta, -alpha, ply + 1, true);
        }

        board.unmake_move(m, undo);
        if (time_up) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = m;
        }

        if (score > alpha) {
            alpha = score;
            tt_flag = TT_EXACT;

            if (score >= beta) {
                tt_flag = TT_LOWER;
                // Killer and history for quiet moves
                if (!is_cap && !is_promo && ply < MAX_PLY) {
                    if (!(m == killers[ply][0])) {
                        killers[ply][1] = killers[ply][0];
                        killers[ply][0] = m;
                    }
                    // Since we unmade the move, board[m.from] has the piece
                    int ms = piece_side(board.board[m.from]);
                    if (ms >= 0 && ms <= 1) {
                        history[ms][m.from][m.to] += depth * depth;
                        if (history[ms][m.from][m.to] > 1000000) {
                            // Aging
                            for (auto& row : history)
                                for (auto& col : row)
                                    for (auto& v : col) v >>= 1;
                        }
                    }
                }
                break;
            }
        }
    }

    tt_store(board.hash, depth, best_score, tt_flag, best_move);
    return best_score;
}

// ============================================================
// Quiescence Search
// ============================================================

int Searcher::quiescence(Board& board, int alpha, int beta, int ply) {
    nodes++;
    if ((nodes & 4095) == 0) check_time();
    if (time_up) return 0;

    int stand_pat = evaluate(board);
    if (board.side == BLACK_SIDE) stand_pat = -stand_pat;

    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    // Delta pruning
    constexpr int BIG_DELTA = 900; // Queen value
    if (stand_pat + BIG_DELTA < alpha) return alpha;

    Move moves[MAX_MOVES];
    int n = board.gen_captures(moves);

    // Score captures by MVV-LVA
    int scores[MAX_MOVES];
    for (int i = 0; i < n; i++) {
        int victim = PIECE_VAL[piece_type(moves[i].captured)];
        int attacker = PIECE_VAL[piece_type(board.board[moves[i].from])];
        scores[i] = victim * 10 - attacker;
    }

    for (int i = 0; i < n; i++) {
        sort_moves(moves, scores, n, i);

        // SEE-like pruning: skip clearly losing captures
        if (scores[i] < -200 && !board.in_check()) continue;

        // Legality check
        UndoInfo undo;
        board.make_move(moves[i], undo);
        if (board.is_attacked(board.king_sq[board.side ^ 1], board.side)) {
            board.unmake_move(moves[i], undo);
            continue; // Illegal
        }

        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmake_move(moves[i], undo);

        if (time_up) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}
