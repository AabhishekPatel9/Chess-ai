// ============================================================
// board.cpp — Board implementation
// ============================================================

#include "board.h"
#include <sstream>
#include <cstring>
#include <cstdlib>

// ─── Zobrist initialization ────────────────────────────────

uint64_t Board::Z_PIECE[13][64];
uint64_t Board::Z_SIDE;
uint64_t Board::Z_CASTLE[16];
uint64_t Board::Z_EP[8];
bool Board::z_init_done = false;

static uint64_t xorshift64(uint64_t& s) {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s;
}

void Board::init_zobrist() {
    if (z_init_done) return;
    uint64_t seed = 0x12345678ABCDEF01ULL;
    for (int p = 0; p < 13; p++)
        for (int s = 0; s < 64; s++)
            Z_PIECE[p][s] = xorshift64(seed);
    Z_SIDE = xorshift64(seed);
    for (int i = 0; i < 16; i++) Z_CASTLE[i] = xorshift64(seed);
    for (int i = 0; i < 8; i++)  Z_EP[i] = xorshift64(seed);
    z_init_done = true;
}

void Board::compute_hash() {
    hash = 0;
    for (int sq = 0; sq < 64; sq++)
        if (board[sq]) hash ^= Z_PIECE[piece_index(board[sq])][sq];
    if (side == BLACK_SIDE) hash ^= Z_SIDE;
    hash ^= Z_CASTLE[castling];
    if (ep_square >= 0) hash ^= Z_EP[sq_file(ep_square)];
}

// ─── Constructor ───────────────────────────────────────────

Board::Board() : side(WHITE_SIDE), castling(0), ep_square(-1),
                 halfmove(0), fullmove(1), hash(0), pos_history_count(0) {
    init_zobrist();
    memset(board, 0, sizeof(board));
    king_sq[0] = king_sq[1] = -1;
    memset(pos_history, 0, sizeof(pos_history));
}

// ─── FEN ───────────────────────────────────────────────────

void Board::set_fen(const std::string& fen) {
    memset(board, 0, sizeof(board));
    pos_history_count = 0;
    king_sq[0] = king_sq[1] = -1;

    std::istringstream ss(fen);
    std::string pieces, turn, castle, ep;
    ss >> pieces >> turn >> castle >> ep >> halfmove >> fullmove;

    int sq = 56; // Start from a8
    for (char c : pieces) {
        if (c == '/') { sq -= 16; continue; }
        if (c >= '1' && c <= '8') { sq += (c - '0'); continue; }
        int p = 0;
        switch (c) {
            case 'P': p =  1; break; case 'N': p =  2; break;
            case 'B': p =  3; break; case 'R': p =  4; break;
            case 'Q': p =  5; break; case 'K': p =  6; break;
            case 'p': p = -1; break; case 'n': p = -2; break;
            case 'b': p = -3; break; case 'r': p = -4; break;
            case 'q': p = -5; break; case 'k': p = -6; break;
        }
        board[sq] = p;
        if (p == W_KING) king_sq[WHITE_SIDE] = sq;
        if (p == B_KING) king_sq[BLACK_SIDE] = sq;
        sq++;
    }

    side = (turn == "b") ? BLACK_SIDE : WHITE_SIDE;

    castling = 0;
    if (castle != "-") {
        for (char c : castle) {
            if (c == 'K') castling |= 1;
            if (c == 'Q') castling |= 2;
            if (c == 'k') castling |= 4;
            if (c == 'q') castling |= 8;
        }
    }

    ep_square = -1;
    if (ep != "-" && ep.size() == 2) {
        ep_square = make_sq(ep[0] - 'a', ep[1] - '1');
    }

    compute_hash();
    pos_history[pos_history_count++] = hash;
}

std::string Board::to_fen() const {
    std::string fen;
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int f = 0; f < 8; f++) {
            int p = board[make_sq(f, r)];
            if (p == 0) { empty++; continue; }
            if (empty) { fen += std::to_string(empty); empty = 0; }
            if (p > 0) fen += "PNBRQK"[p - 1];
            else       fen += "pnbrqk"[(-p) - 1];
        }
        if (empty) fen += std::to_string(empty);
        if (r > 0) fen += '/';
    }
    fen += (side == WHITE_SIDE) ? " w " : " b ";

    std::string cas;
    if (castling & 1) cas += 'K'; if (castling & 2) cas += 'Q';
    if (castling & 4) cas += 'k'; if (castling & 8) cas += 'q';
    fen += cas.empty() ? "-" : cas;

    fen += ' ';
    if (ep_square >= 0) {
        fen += char('a' + sq_file(ep_square));
        fen += char('1' + sq_rank(ep_square));
    } else fen += '-';

    fen += ' ' + std::to_string(halfmove) + ' ' + std::to_string(fullmove);
    return fen;
}

// ─── Make / Unmake Move ────────────────────────────────────

void Board::make_move(const Move& m, UndoInfo& undo) {
    undo.castling = castling;
    undo.ep_square = ep_square;
    undo.halfmove = halfmove;
    undo.hash = hash;

    int piece = board[m.from];
    int pt = piece_type(piece);
    int s = piece_side(piece);

    // Remove piece from source
    hash ^= Z_PIECE[piece_index(piece)][m.from];
    board[m.from] = 0;

    // Remove captured piece
    if (m.captured) {
        int cap_sq = m.to;
        if (m.flags & FL_EP) {
            cap_sq = make_sq(sq_file(m.to), sq_rank(m.from));
            hash ^= Z_PIECE[piece_index(m.captured)][cap_sq];
            board[cap_sq] = 0;
        } else {
            hash ^= Z_PIECE[piece_index(m.captured)][m.to];
        }
    }

    // Place piece (or promoted piece) on destination
    int placed = m.promotion ? m.promotion : piece;
    board[m.to] = placed;
    hash ^= Z_PIECE[piece_index(placed)][m.to];

    // Update king square
    if (pt == PT_KING) {
        king_sq[s] = m.to;
    }

    // Handle castling rook movement
    if (m.flags & FL_CASTLE) {
        int rook_from, rook_to;
        int rook = piece_sign(s) * PT_ROOK;
        if (sq_file(m.to) == 6) { // Kingside
            rook_from = make_sq(7, sq_rank(m.from));
            rook_to   = make_sq(5, sq_rank(m.from));
        } else { // Queenside
            rook_from = make_sq(0, sq_rank(m.from));
            rook_to   = make_sq(3, sq_rank(m.from));
        }
        hash ^= Z_PIECE[piece_index(rook)][rook_from];
        hash ^= Z_PIECE[piece_index(rook)][rook_to];
        board[rook_from] = 0;
        board[rook_to] = rook;
    }

    // Update castling rights
    hash ^= Z_CASTLE[castling];
    // King moved
    if (pt == PT_KING) {
        if (s == WHITE_SIDE) castling &= ~3;  // Remove WK, WQ
        else                 castling &= ~12; // Remove BK, BQ
    }
    // Rook moved or captured
    if (m.from == 0  || m.to == 0)  castling &= ~2;  // a1 = WQ
    if (m.from == 7  || m.to == 7)  castling &= ~1;  // h1 = WK
    if (m.from == 56 || m.to == 56) castling &= ~8;  // a8 = BQ
    if (m.from == 63 || m.to == 63) castling &= ~4;  // h8 = BK
    hash ^= Z_CASTLE[castling];

    // Update en passant
    if (ep_square >= 0) hash ^= Z_EP[sq_file(ep_square)];
    ep_square = -1;
    if ((m.flags & FL_DOUBLE) && pt == PT_PAWN) {
        ep_square = (m.from + m.to) / 2;
        hash ^= Z_EP[sq_file(ep_square)];
    }

    // Halfmove clock
    if (pt == PT_PAWN || m.captured) halfmove = 0;
    else halfmove++;

    // Switch side
    side ^= 1;
    hash ^= Z_SIDE;

    if (side == WHITE_SIDE) fullmove++;

    // Store position for repetition detection
    if (pos_history_count < MAX_HISTORY)
        pos_history[pos_history_count++] = hash;
}

void Board::unmake_move(const Move& m, const UndoInfo& undo) {
    side ^= 1;
    int piece = m.promotion ? (piece_sign(side) * PT_PAWN) : board[m.to];
    int pt = piece_type(piece);

    board[m.to] = 0;
    board[m.from] = piece;

    if (m.captured) {
        if (m.flags & FL_EP) {
            int cap_sq = make_sq(sq_file(m.to), sq_rank(m.from));
            board[cap_sq] = m.captured;
        } else {
            board[m.to] = m.captured;
        }
    }

    // Undo castling rook
    if (m.flags & FL_CASTLE) {
        int rook = piece_sign(side) * PT_ROOK;
        if (sq_file(m.to) == 6) { // Kingside
            board[make_sq(7, sq_rank(m.from))] = rook;
            board[make_sq(5, sq_rank(m.from))] = 0;
        } else { // Queenside
            board[make_sq(0, sq_rank(m.from))] = rook;
            board[make_sq(3, sq_rank(m.from))] = 0;
        }
    }

    if (pt == PT_KING) king_sq[side] = m.from;

    castling = undo.castling;
    ep_square = undo.ep_square;
    halfmove = undo.halfmove;
    hash = undo.hash;
    if (side == BLACK_SIDE) fullmove--;

    if (pos_history_count > 0) pos_history_count--;
}

void Board::make_null_move(UndoInfo& undo) {
    undo.ep_square = ep_square;
    undo.hash = hash;
    if (ep_square >= 0) hash ^= Z_EP[sq_file(ep_square)];
    ep_square = -1;
    side ^= 1;
    hash ^= Z_SIDE;
}

void Board::unmake_null_move(const UndoInfo& undo) {
    side ^= 1;
    ep_square = undo.ep_square;
    hash = undo.hash;
}

// ─── Attack detection ──────────────────────────────────────

bool Board::is_attacked(int sq, int by_side) const {
    int sign = piece_sign(by_side);

    // Pawn attacks
    if (by_side == WHITE_SIDE) {
        if (sq_rank(sq) > 0) {
            if (sq_file(sq) > 0 && board[sq - 9] == W_PAWN) return true;
            if (sq_file(sq) < 7 && board[sq - 7] == W_PAWN) return true;
        }
    } else {
        if (sq_rank(sq) < 7) {
            if (sq_file(sq) > 0 && board[sq + 7] == B_PAWN) return true;
            if (sq_file(sq) < 7 && board[sq + 9] == B_PAWN) return true;
        }
    }

    // Knight attacks
    for (int d : KNIGHT_DIRS) {
        int to = sq + d;
        if (to >= 0 && to < 64 && abs(sq_file(to) - sq_file(sq)) <= 2 &&
            board[to] == sign * PT_KNIGHT)
            return true;
    }

    // King attacks
    for (int d : KING_DIRS) {
        int to = sq + d;
        if (to >= 0 && to < 64 && abs(sq_file(to) - sq_file(sq)) <= 1 &&
            board[to] == sign * PT_KING)
            return true;
    }

    // Sliding attacks (bishop/queen diagonals)
    for (int d : BISHOP_DIRS) {
        for (int to = sq + d; to >= 0 && to < 64; to += d) {
            if (abs(sq_file(to) - sq_file(to - d)) != 1) break; // Wrapped
            int p = board[to];
            if (p == 0) continue;
            if (p == sign * PT_BISHOP || p == sign * PT_QUEEN) return true;
            break; // Blocked
        }
    }

    // Sliding attacks (rook/queen straights)
    for (int d : ROOK_DIRS) {
        for (int to = sq + d; to >= 0 && to < 64; to += d) {
            if (abs(d) == 1 && sq_rank(to) != sq_rank(to - d)) break; // File wrap
            if (abs(d) == 8 && abs(sq_file(to) - sq_file(to - d)) != 0) break;
            int p = board[to];
            if (p == 0) continue;
            if (p == sign * PT_ROOK || p == sign * PT_QUEEN) return true;
            break;
        }
    }

    return false;
}

// ─── Move generation: Legal ────────────────────────────────

int Board::gen_legal_moves(Move* moves) const {
    Move pseudo[MAX_MOVES];
    int n = gen_pseudo_moves(pseudo);
    int legal = 0;

    Board* self = const_cast<Board*>(this);
    for (int i = 0; i < n; i++) {
        UndoInfo undo;
        self->make_move(pseudo[i], undo);
        if (!is_attacked(king_sq[side ^ 1], side)) {
            moves[legal++] = pseudo[i];
        }
        self->unmake_move(pseudo[i], undo);
    }
    return legal;
}

bool Board::is_legal(const Move& m) {
    UndoInfo undo;
    make_move(m, undo);
    bool legal = !is_attacked(king_sq[side ^ 1], side);
    unmake_move(m, undo);
    return legal;
}

// ─── Pseudo-legal move generation ──────────────────────────

int Board::gen_pseudo_moves(Move* moves) const {
    int count = 0;
    gen_pawn_moves(moves, count);
    gen_knight_moves(moves, count);
    gen_slider_moves(moves, count, PT_BISHOP);
    gen_slider_moves(moves, count, PT_ROOK);
    gen_slider_moves(moves, count, PT_QUEEN);
    gen_king_moves(moves, count);
    return count;
}

int Board::gen_captures(Move* moves) const {
    int count = 0;
    gen_pawn_captures(moves, count);
    gen_knight_captures(moves, count);
    gen_slider_captures(moves, count, PT_BISHOP);
    gen_slider_captures(moves, count, PT_ROOK);
    gen_slider_captures(moves, count, PT_QUEEN);
    gen_king_captures(moves, count);
    return count;
}

// ─── Pawn moves ────────────────────────────────────────────

void Board::gen_pawn_moves(Move* moves, int& c) const {
    int sign = piece_sign(side);
    int pawn = sign * PT_PAWN;
    int dir = (side == WHITE_SIDE) ? 8 : -8;
    int start_rank = (side == WHITE_SIDE) ? 1 : 6;
    int promo_rank = (side == WHITE_SIDE) ? 7 : 0;

    for (int sq = 0; sq < 64; sq++) {
        if (board[sq] != pawn) continue;
        int f = sq_file(sq), r = sq_rank(sq);

        // Forward
        int to = sq + dir;
        if (to >= 0 && to < 64 && board[to] == 0) {
            if (sq_rank(to) == promo_rank) {
                moves[c++] = Move(sq, to, 0, sign * PT_QUEEN);
                moves[c++] = Move(sq, to, 0, sign * PT_ROOK);
                moves[c++] = Move(sq, to, 0, sign * PT_BISHOP);
                moves[c++] = Move(sq, to, 0, sign * PT_KNIGHT);
            } else {
                moves[c++] = Move(sq, to);
                // Double push
                if (r == start_rank) {
                    int to2 = sq + 2 * dir;
                    if (board[to2] == 0)
                        moves[c++] = Move(sq, to2, 0, 0, FL_DOUBLE);
                }
            }
        }

        // Captures
        int cap_dirs[2] = { dir - 1, dir + 1 };
        int cap_files[2] = { f - 1, f + 1 };
        for (int i = 0; i < 2; i++) {
            if (cap_files[i] < 0 || cap_files[i] > 7) continue;
            to = sq + cap_dirs[i];
            if (to < 0 || to >= 64) continue;

            if (board[to] != 0 && piece_side(board[to]) != (int)side) {
                if (sq_rank(to) == promo_rank) {
                    moves[c++] = Move(sq, to, board[to], sign * PT_QUEEN);
                    moves[c++] = Move(sq, to, board[to], sign * PT_ROOK);
                    moves[c++] = Move(sq, to, board[to], sign * PT_BISHOP);
                    moves[c++] = Move(sq, to, board[to], sign * PT_KNIGHT);
                } else {
                    moves[c++] = Move(sq, to, board[to]);
                }
            }
            // En passant
            if (to == ep_square) {
                int cap_pawn = -sign * PT_PAWN;
                moves[c++] = Move(sq, to, cap_pawn, 0, FL_EP);
            }
        }
    }
}

void Board::gen_pawn_captures(Move* moves, int& c) const {
    int sign = piece_sign(side);
    int pawn = sign * PT_PAWN;
    int dir = (side == WHITE_SIDE) ? 8 : -8;
    int promo_rank = (side == WHITE_SIDE) ? 7 : 0;

    for (int sq = 0; sq < 64; sq++) {
        if (board[sq] != pawn) continue;
        int f = sq_file(sq);

        // Promotion by pushing (treated as "capture" for quiescence)
        int fwd = sq + dir;
        if (fwd >= 0 && fwd < 64 && board[fwd] == 0 && sq_rank(fwd) == promo_rank) {
            moves[c++] = Move(sq, fwd, 0, sign * PT_QUEEN);
        }

        int cap_dirs[2] = { dir - 1, dir + 1 };
        int cap_files[2] = { f - 1, f + 1 };
        for (int i = 0; i < 2; i++) {
            if (cap_files[i] < 0 || cap_files[i] > 7) continue;
            int to = sq + cap_dirs[i];
            if (to < 0 || to >= 64) continue;

            if (board[to] != 0 && piece_side(board[to]) != (int)side) {
                if (sq_rank(to) == promo_rank) {
                    moves[c++] = Move(sq, to, board[to], sign * PT_QUEEN);
                } else {
                    moves[c++] = Move(sq, to, board[to]);
                }
            }
            if (to == ep_square) {
                moves[c++] = Move(sq, to, -sign * PT_PAWN, 0, FL_EP);
            }
        }
    }
}

// ─── Knight moves ──────────────────────────────────────────

void Board::gen_knight_moves(Move* moves, int& c) const {
    int knight = piece_sign(side) * PT_KNIGHT;
    for (int sq = 0; sq < 64; sq++) {
        if (board[sq] != knight) continue;
        for (int d : KNIGHT_DIRS) {
            int to = sq + d;
            if (to < 0 || to >= 64) continue;
            if (abs(sq_file(to) - sq_file(sq)) > 2) continue;
            int target = board[to];
            if (target == 0) {
                moves[c++] = Move(sq, to);
            } else if (piece_side(target) != (int)side) {
                moves[c++] = Move(sq, to, target);
            }
        }
    }
}

void Board::gen_knight_captures(Move* moves, int& c) const {
    int knight = piece_sign(side) * PT_KNIGHT;
    for (int sq = 0; sq < 64; sq++) {
        if (board[sq] != knight) continue;
        for (int d : KNIGHT_DIRS) {
            int to = sq + d;
            if (to < 0 || to >= 64) continue;
            if (abs(sq_file(to) - sq_file(sq)) > 2) continue;
            int target = board[to];
            if (target != 0 && piece_side(target) != (int)side)
                moves[c++] = Move(sq, to, target);
        }
    }
}

// ─── Sliding piece moves ──────────────────────────────────

void Board::gen_slider_moves(Move* moves, int& c, int piece_t) const {
    int sign = piece_sign(side);
    int piece = sign * piece_t;
    const int* dirs;
    int ndirs;

    if (piece_t == PT_BISHOP)      { dirs = BISHOP_DIRS; ndirs = 4; }
    else if (piece_t == PT_ROOK)   { dirs = ROOK_DIRS;   ndirs = 4; }
    else /* QUEEN */               { dirs = KING_DIRS;   ndirs = 8; }

    for (int sq = 0; sq < 64; sq++) {
        if (board[sq] != piece) continue;
        for (int di = 0; di < ndirs; di++) {
            int d = dirs[di];
            for (int to = sq + d; to >= 0 && to < 64; to += d) {
                // Check for wrapping
                int prev = to - d;
                if (abs(sq_file(to) - sq_file(prev)) > 1) break;

                int target = board[to];
                if (target == 0) {
                    moves[c++] = Move(sq, to);
                } else {
                    if (piece_side(target) != (int)side)
                        moves[c++] = Move(sq, to, target);
                    break;
                }
            }
        }
    }
}

void Board::gen_slider_captures(Move* moves, int& c, int piece_t) const {
    int sign = piece_sign(side);
    int piece = sign * piece_t;
    const int* dirs;
    int ndirs;

    if (piece_t == PT_BISHOP)      { dirs = BISHOP_DIRS; ndirs = 4; }
    else if (piece_t == PT_ROOK)   { dirs = ROOK_DIRS;   ndirs = 4; }
    else                           { dirs = KING_DIRS;   ndirs = 8; }

    for (int sq = 0; sq < 64; sq++) {
        if (board[sq] != piece) continue;
        for (int di = 0; di < ndirs; di++) {
            int d = dirs[di];
            for (int to = sq + d; to >= 0 && to < 64; to += d) {
                int prev = to - d;
                if (abs(sq_file(to) - sq_file(prev)) > 1) break;
                int target = board[to];
                if (target == 0) continue;
                if (piece_side(target) != (int)side)
                    moves[c++] = Move(sq, to, target);
                break;
            }
        }
    }
}

// ─── King moves ────────────────────────────────────────────

void Board::gen_king_moves(Move* moves, int& c) const {
    int sq = king_sq[side];

    for (int d : KING_DIRS) {
        int to = sq + d;
        if (to < 0 || to >= 64) continue;
        if (abs(sq_file(to) - sq_file(sq)) > 1) continue;
        int target = board[to];
        if (target == 0) {
            moves[c++] = Move(sq, to);
        } else if (piece_side(target) != (int)side) {
            moves[c++] = Move(sq, to, target);
        }
    }

    // Castling
    if (!is_attacked(sq, side ^ 1)) {
        if (side == WHITE_SIDE) {
            if ((castling & 1) && board[5] == 0 && board[6] == 0 &&
                !is_attacked(5, BLACK_SIDE) && !is_attacked(6, BLACK_SIDE))
                moves[c++] = Move(4, 6, 0, 0, FL_CASTLE);
            if ((castling & 2) && board[3] == 0 && board[2] == 0 && board[1] == 0 &&
                !is_attacked(3, BLACK_SIDE) && !is_attacked(2, BLACK_SIDE))
                moves[c++] = Move(4, 2, 0, 0, FL_CASTLE);
        } else {
            if ((castling & 4) && board[61] == 0 && board[62] == 0 &&
                !is_attacked(61, WHITE_SIDE) && !is_attacked(62, WHITE_SIDE))
                moves[c++] = Move(60, 62, 0, 0, FL_CASTLE);
            if ((castling & 8) && board[59] == 0 && board[58] == 0 && board[57] == 0 &&
                !is_attacked(59, WHITE_SIDE) && !is_attacked(58, WHITE_SIDE))
                moves[c++] = Move(60, 58, 0, 0, FL_CASTLE);
        }
    }
}

void Board::gen_king_captures(Move* moves, int& c) const {
    int sq = king_sq[side];
    for (int d : KING_DIRS) {
        int to = sq + d;
        if (to < 0 || to >= 64) continue;
        if (abs(sq_file(to) - sq_file(sq)) > 1) continue;
        int target = board[to];
        if (target != 0 && piece_side(target) != (int)side)
            moves[c++] = Move(sq, to, target);
    }
}

// ─── Draw detection ────────────────────────────────────────

int Board::count_repetitions() const {
    int count = 0;
    for (int i = pos_history_count - 3; i >= 0; i -= 2) {
        if (pos_history[i] == hash) count++;
    }
    return count;
}

bool Board::is_draw() const {
    if (halfmove >= 100) return true;
    if (count_repetitions() >= 2) return true; // 3-fold
    return false;
}

// ─── Move::from_uci ────────────────────────────────────────

Move Move::from_uci(const std::string& s, const int* bd) {
    if (s.size() < 4) return Move();
    int from = make_sq(s[0] - 'a', s[1] - '1');
    int to   = make_sq(s[2] - 'a', s[3] - '1');
    int cap  = bd[to];
    int promo = 0;
    int flags = 0;

    int piece = bd[from];
    int pt = piece_type(piece);
    int sign = piece > 0 ? 1 : -1;

    // Promotion
    if (s.size() == 5) {
        switch (s[4]) {
            case 'q': promo = sign * PT_QUEEN;  break;
            case 'r': promo = sign * PT_ROOK;   break;
            case 'b': promo = sign * PT_BISHOP; break;
            case 'n': promo = sign * PT_KNIGHT; break;
        }
    }

    // En passant
    if (pt == PT_PAWN && sq_file(from) != sq_file(to) && cap == 0) {
        flags = FL_EP;
        cap = -sign * PT_PAWN;
    }

    // Double push
    if (pt == PT_PAWN && abs(sq_rank(to) - sq_rank(from)) == 2) {
        flags = FL_DOUBLE;
    }

    // Castling
    if (pt == PT_KING && abs(sq_file(to) - sq_file(from)) == 2) {
        flags = FL_CASTLE;
    }

    return Move(from, to, cap, promo, flags);
}
