# ♛ Chess — Play vs AI

A browser-based chess game where you play as White against a custom-built C++ AI engine. Features a sleek dark-themed UI, adjustable search depth, and real-time engine statistics.

![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=flat&logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-1572B6?style=flat&logo=css3&logoColor=white)
![JavaScript](https://img.shields.io/badge/JavaScript-F7DF1E?style=flat&logo=javascript&logoColor=black)
![Python](https://img.shields.io/badge/Python-3776AB?style=flat&logo=python&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=cplusplus&logoColor=white)

---
Working link : https://chess-ai-xh6z.onrender.com/

## ✨ Features

- **Custom C++ Chess Engine** — Hand-written engine with alpha-beta pruning, quiescence search, null-move pruning, late move reductions, and transposition tables (64 MB)
- **Adjustable Difficulty** — Search depth slider from 1 (instant) to 20 (deep analysis)
- **Premium Dark UI** — Modern glassmorphism design with smooth animations, move hints, and check highlighting
- **Real-Time Engine Stats** — See evaluation, depth reached, search time, and cache hit rates after every AI move
- **Full Chess Rules** — Castling, en passant, pawn promotion (with UI picker), 50-move rule, and threefold repetition detection
- **Move History** — Scrollable move list in standard algebraic notation
- **Captured Pieces** — Visual display of captured material for both sides
- **Undo Support** — Take back your last move (undoes both your move and the AI's response)
- **Confetti Celebration** — Animated confetti on checkmate

---

## 🏗️ Architecture

```
Chess/
├── index.html              # Single-page UI (start screen, board, modals)
├── css/styles.css          # Premium dark theme with CSS custom properties
├── js/app.js               # Frontend controller (rendering, clicks, API calls)
├── server.py               # FastAPI backend (game state, C++ engine bridge)
├── requirements.txt        # Python dependencies
├── .gitignore
└── cpp_engine/             # Custom chess engine in C++
    ├── types.h             # Core types, Move struct, constants
    ├── board.h / board.cpp # Board representation, move generation, attack detection
    ├── search.h / search.cpp # Search (iterative deepening, alpha-beta), evaluation, TT
    ├── main.cpp            # CLI interface (reads FEN from stdin, outputs best move)
    └── Makefile            # Build configuration (g++, -O3, C++17)
```

### How It Works

1. **Frontend** (`app.js`) renders the board from FEN strings and sends moves to the backend via REST API
2. **Backend** (`server.py`) manages game state using the `python-chess` library for move validation
3. **C++ Engine** runs as a persistent subprocess — the Python server sends FEN positions via stdin and reads the best move from stdout
4. The engine's transposition table persists across moves within a game for faster searches

---

## 🚀 Getting Started

### Prerequisites

- **Python 3.10+**
- **g++ (C++17)** — for compiling the engine (e.g., via [MSYS2](https://www.msys2.org/) on Windows)

### 1. Install Python dependencies

```bash
pip install -r requirements.txt
```

### 2. Compile the C++ engine

```bash
cd cpp_engine
make
```

This produces `chess_engine.exe` in the `cpp_engine/` directory.

### 3. Run the server

```bash
python server.py
```

### 4. Play

Open **http://localhost:8000** in your browser, adjust the search depth, and click **Start Game**.

---

## 🔌 API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/new-game` | Start a new game (`{ ai_color, depth }`) |
| `POST` | `/api/move` | Submit player's move (`{ move, promotion }`) |
| `POST` | `/api/ai-move` | Trigger the AI to think and respond |
| `POST` | `/api/undo` | Undo the last move pair (player + AI) |
| `GET` | `/api/state` | Get the current game state |

---

## 🧠 Engine Techniques

| Technique | Description |
|-----------|-------------|
| Iterative Deepening | Searches progressively deeper, using time guards |
| Alpha-Beta Pruning | Prunes branches that can't improve the result |
| Quiescence Search | Extends search on captures to avoid horizon effects |
| Null-Move Pruning | Skips a turn to detect refutations cheaply |
| Late Move Reductions | Searches unlikely moves at reduced depth |
| Transposition Table | 64 MB hash table to avoid re-searching positions |
| Killer Heuristic | Remembers moves that caused beta cutoffs |
| History Heuristic | Scores quiet moves by past success |
| MVV-LVA Ordering | Captures ordered by Most Valuable Victim – Least Valuable Attacker |
| Aspiration Windows | Narrow alpha-beta window based on previous iteration |
| Piece-Square Tables | Positional evaluation (middlegame + endgame king tables) |
| Pawn Structure | Doubled, isolated, and passed pawn evaluation |
| King Safety | Pawn shield bonus in middlegame |

---

## 📄 License

This project is open-source. Feel free to use, modify, and distribute.
