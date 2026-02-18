"""
server.py - FastAPI Chess Game Server.
Run: python server.py
Then open http://localhost:8000
"""

import chess
import os
import subprocess
from fastapi import FastAPI, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional

app = FastAPI(title="Chess Engine")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ─── C++ Engine Process ─────────────────────────────────────────────────────

CPP_ENGINE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cpp_engine", "chess_engine.exe")
cpp_process: subprocess.Popen = None

def _start_cpp_engine():
    global cpp_process
    _stop_cpp_engine()
    
    # Add MSYS2 bin to PATH for DLLs
    env = os.environ.copy()
    msys_bin = r"C:\msys64\ucrt64\bin"
    if os.path.exists(msys_bin):
        env["PATH"] = msys_bin + os.pathsep + env["PATH"]

    cpp_process = subprocess.Popen(
        [CPP_ENGINE_PATH],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, bufsize=1,
        env=env
    )
    print(f"C++ engine started (PID {cpp_process.pid})")

def _stop_cpp_engine():
    global cpp_process
    if cpp_process and cpp_process.poll() is None:
        try:
            cpp_process.stdin.write("quit\n")
            cpp_process.stdin.flush()
            cpp_process.wait(timeout=2)
        except Exception:
            cpp_process.kill()
    cpp_process = None

# ─── Game State ─────────────────────────────────────────────────────────────

board: chess.Board = chess.Board()
ai_color: chess.Color = chess.BLACK
search_depth: int = 8
move_history: list[dict] = []

PIECE_UNICODE = {
    (chess.PAWN, chess.WHITE): "♙", (chess.KNIGHT, chess.WHITE): "♘",
    (chess.BISHOP, chess.WHITE): "♗", (chess.ROOK, chess.WHITE): "♖",
    (chess.QUEEN, chess.WHITE): "♕", (chess.KING, chess.WHITE): "♔",
    (chess.PAWN, chess.BLACK): "♟", (chess.KNIGHT, chess.BLACK): "♞",
    (chess.BISHOP, chess.BLACK): "♝", (chess.ROOK, chess.BLACK): "♜",
    (chess.QUEEN, chess.BLACK): "♛", (chess.KING, chess.BLACK): "♚",
}


class NewGameRequest(BaseModel):
    ai_color: str = "black"
    depth: int = 8


class MoveRequest(BaseModel):
    move: str       # UCI like "e2e4"
    promotion: Optional[str] = None  # "q","r","b","n"


class GameState(BaseModel):
    fen: str
    turn: str
    is_check: bool
    is_checkmate: bool
    is_stalemate: bool
    is_game_over: bool
    game_result: Optional[str]
    legal_moves: list[str]
    move_history: list[dict]
    captured_white: list[str]
    captured_black: list[str]
    last_move: Optional[dict]
    ai_search_info: Optional[dict]


# ─── Helpers ─────────────────────────────────────────────────────────────────

def _get_result() -> Optional[str]:
    if not board.is_game_over():
        return None
    r = board.result()
    if r == "1-0":
        return "white_wins"
    elif r == "0-1":
        return "black_wins"
    return "draw"


def _make_move_record(move: chess.Move, color: chess.Color) -> dict:
    """Build a record BEFORE pushing the move (board.san needs the current pos)."""
    san = board.san(move)
    return {
        "notation": san,
        "from_sq": chess.square_name(move.from_square),
        "to_sq": chess.square_name(move.to_square),
        "color": "white" if color == chess.WHITE else "black",
        "uci": move.uci(),
    }


def _captured_pieces() -> tuple[list[str], list[str]]:
    start = chess.Board()
    cap_w, cap_b = [], []
    for pt in [chess.QUEEN, chess.ROOK, chess.BISHOP, chess.KNIGHT, chess.PAWN]:
        diff_w = len(start.pieces(pt, chess.WHITE)) - len(board.pieces(pt, chess.WHITE))
        diff_b = len(start.pieces(pt, chess.BLACK)) - len(board.pieces(pt, chess.BLACK))
        for _ in range(max(0, diff_w)):
            cap_w.append(PIECE_UNICODE[(pt, chess.WHITE)])
        for _ in range(max(0, diff_b)):
            cap_b.append(PIECE_UNICODE[(pt, chess.BLACK)])
    return cap_w, cap_b


def _build_state(ai_info: Optional[dict] = None) -> GameState:
    cap_w, cap_b = _captured_pieces()
    last = None
    if move_history:
        l = move_history[-1]
        last = {"from_sq": l["from_sq"], "to_sq": l["to_sq"]}
    return GameState(
        fen=board.fen(),
        turn="white" if board.turn == chess.WHITE else "black",
        is_check=board.is_check(),
        is_checkmate=board.is_checkmate(),
        is_stalemate=board.is_stalemate(),
        is_game_over=board.is_game_over(),
        game_result=_get_result(),
        legal_moves=[m.uci() for m in board.legal_moves],
        move_history=move_history,
        captured_white=cap_w,
        captured_black=cap_b,
        last_move=last,
        ai_search_info=ai_info,
    )


def _do_ai_move() -> dict:
    """Send position to C++ engine, get best move back."""
    global cpp_process

    if cpp_process is None or cpp_process.poll() is not None:
        _start_cpp_engine()

    fen = board.fen()
    safety_time = 120000  # 120s safety timeout
    info = {"depth": 0, "eval": 0, "time": 0, "nodes": 0, "tt_hits": 0, "tt_stores": 0}

    try:
        cpp_process.stdin.write(f"{fen} | {search_depth} | {safety_time}\n")
        cpp_process.stdin.flush()
        response = cpp_process.stdout.readline().strip()
    except Exception as e:
        print(f"C++ engine error: {e}")
        _start_cpp_engine()
        return info

    # Parse: bestmove e7e5 depth 13 eval -20 nodes 5000000 time 2816 ...
    parts = response.split()
    best_uci = ""
    i = 0
    while i < len(parts):
        k = parts[i]
        if k == "bestmove" and i + 1 < len(parts):
            best_uci = parts[i + 1]; i += 2
        elif k == "depth" and i + 1 < len(parts):
            info["depth"] = int(parts[i + 1]); i += 2
        elif k == "eval" and i + 1 < len(parts):
            info["eval"] = int(parts[i + 1]); i += 2
        elif k == "nodes" and i + 1 < len(parts):
            info["nodes"] = int(parts[i + 1]); i += 2
        elif k == "time" and i + 1 < len(parts):
            info["time"] = round(int(parts[i + 1]) / 1000, 2); i += 2
        elif k == "tt_hits" and i + 1 < len(parts):
            info["tt_hits"] = int(parts[i + 1]); i += 2
        elif k == "tt_stores" and i + 1 < len(parts):
            info["tt_stores"] = int(parts[i + 1]); i += 2
        else:
            i += 1

    if best_uci:
        try:
            move = chess.Move.from_uci(best_uci)
            if move in board.legal_moves:
                rec = _make_move_record(move, board.turn)
                board.push(move)
                move_history.append(rec)
        except Exception as e:
            print(f"Invalid move from C++ engine: {best_uci}: {e}")

    # Eval from C++ engine is from searching side's perspective.
    # Negate if AI is Black so display is from White (player) perspective.
    if ai_color == chess.BLACK:
        info["eval"] = -info["eval"]

    return info


# ─── API ─────────────────────────────────────────────────────────────────────

@app.post("/api/new-game", response_model=GameState)
def new_game(req: NewGameRequest):
    global board, ai_color, search_depth, move_history
    board = chess.Board()
    ai_color = chess.WHITE if req.ai_color == "white" else chess.BLACK
    search_depth = max(1, min(20, req.depth))
    move_history = []

    # Restart C++ engine for fresh transposition table
    _start_cpp_engine()

    state = _build_state()
    if ai_color == chess.WHITE:
        ai_info = _do_ai_move()
        state = _build_state(ai_info)
    return state


@app.post("/api/move", response_model=GameState)
def player_move(req: MoveRequest):
    """Apply the player's move ONLY. Returns immediately (no AI search)."""
    global board
    if board.is_game_over():
        raise HTTPException(400, "Game is already over")

    player_color = chess.WHITE if ai_color == chess.BLACK else chess.BLACK
    if board.turn != player_color:
        raise HTTPException(400, "It's not your turn")

    uci = req.move
    if req.promotion:
        uci = uci[:4] + req.promotion

    try:
        move = chess.Move.from_uci(uci)
    except ValueError:
        raise HTTPException(400, f"Invalid move format: {uci}")

    if move not in board.legal_moves:
        raise HTTPException(400, f"Illegal move: {uci}")

    rec = _make_move_record(move, board.turn)
    board.push(move)
    move_history.append(rec)

    return _build_state()


@app.post("/api/ai-move", response_model=GameState)
def ai_move():
    """Run the AI search and apply its chosen move. Call AFTER /api/move."""
    global board
    if board.is_game_over():
        return _build_state()

    # Only move if it's the AI's turn
    if board.turn != ai_color:
        return _build_state()

    ai_info = _do_ai_move()
    return _build_state(ai_info)


@app.post("/api/undo", response_model=GameState)
def undo():
    global board
    if not move_history:
        raise HTTPException(400, "No moves to undo")

    # Undo 2 moves (player + AI) back to player's turn
    for _ in range(min(2, len(move_history))):
        if board.move_stack:
            board.pop()
            move_history.pop()

    # Make sure it's the player's turn
    player_color = chess.WHITE if ai_color == chess.BLACK else chess.BLACK
    if board.turn != player_color and board.move_stack:
        board.pop()
        if move_history:
            move_history.pop()

    return _build_state()


@app.get("/api/state", response_model=GameState)
def get_state():
    return _build_state()


# ─── Serve Frontend ─────────────────────────────────────────────────────────

@app.get("/")
def serve_index():
    return FileResponse(os.path.join(os.path.dirname(__file__), "index.html"))


app.mount("/css", StaticFiles(directory=os.path.join(os.path.dirname(__file__), "css")), name="css")
app.mount("/js", StaticFiles(directory=os.path.join(os.path.dirname(__file__), "js")), name="js")


if __name__ == "__main__":
    import uvicorn
    print("╔══════════════════════════════════════════╗")
    print("║    Chess Engine — http://localhost:8000   ║")
    print("╚══════════════════════════════════════════╝")
    uvicorn.run(app, host="0.0.0.0", port=8000, log_level="info")
