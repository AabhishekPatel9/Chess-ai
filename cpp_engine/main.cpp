// ============================================================
// main.cpp — Command-line interface for the chess engine
//
// Protocol (one position per line):
//   Input:  <FEN> | <max_depth> | <movetime_ms>
//   Output: bestmove <uci> depth <d> eval <cp> nodes <n> time <ms> tt_hits <h> tt_stores <s>
//
// Special commands:
//   quit       — exit
//   ping       — respond with "pong"
// ============================================================

#include "search.h"
#include <iostream>
#include <sstream>
#include <string>

int main() {
    Board::init_zobrist();
    Searcher searcher(64); // 64 MB transposition table

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") break;
        if (line == "ping") {
            std::cout << "pong" << std::endl;
            continue;
        }

        // Parse: FEN | max_depth | movetime_ms
        auto sep1 = line.find('|');
        if (sep1 == std::string::npos) continue;

        std::string fen = line.substr(0, sep1);
        // Trim whitespace
        while (!fen.empty() && fen.back() == ' ') fen.pop_back();
        while (!fen.empty() && fen.front() == ' ') fen.erase(fen.begin());

        int max_depth = 0;   // 0 = unlimited (time controls)
        int movetime = 120000; // default 120 seconds safety

        std::string rest = line.substr(sep1 + 1);
        auto sep2 = rest.find('|');
        if (sep2 != std::string::npos) {
            // Both depth and movetime provided
            try { max_depth = std::stoi(rest.substr(0, sep2)); } catch (...) {}
            try { movetime = std::stoi(rest.substr(sep2 + 1)); } catch (...) {}
        } else {
            // Only one value — treat as movetime (backward compat)
            try { movetime = std::stoi(rest); } catch (...) {}
        }

        Board board;
        board.set_fen(fen);

        SearchResult result = searcher.search(board, max_depth, movetime);

        // Output result
        std::cout << "bestmove " << result.best_move.uci()
                  << " depth " << result.depth
                  << " eval " << result.score
                  << " nodes " << result.nodes
                  << " time " << result.time_ms
                  << " tt_hits " << result.tt_hits
                  << " tt_stores " << result.tt_stores
                  << std::endl;
    }

    return 0;
}
