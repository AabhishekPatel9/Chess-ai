// ============================================================
// Chess App — Pure UI Controller (API-based)
// ============================================================
// All chess logic runs on the Python backend (localhost:8000).
// This file only handles: rendering, clicks, and API calls.
// No sound. No thinking indicator.
// ============================================================

const API = window.location.origin;

class ChessApp {
    constructor() {
        this.selected = null;       // Selected square name ("e2") or null
        this.busy = false;          // True while waiting for API
        this.state = null;          // Latest GameState from server
        this.playerColor = "white"; // Player always white

        // DOM
        this.boardEl = document.getElementById("chessboard");
        this.movesEl = document.getElementById("move-list");
        this.capWhEl = document.getElementById("captured-white");
        this.capBlEl = document.getElementById("captured-black");
        this.statusEl = document.getElementById("game-status");
        this.infoEl = document.getElementById("search-info");
        this.undoBtn = document.getElementById("undo-btn");
        this.newBtn = document.getElementById("new-game-btn");

        this._init();
    }

    _init() {
        // Depth slider — live label update
        const slider = document.getElementById("depth-slider");
        const depthVal = document.getElementById("depth-value");
        if (slider && depthVal) {
            slider.addEventListener("input", () => {
                depthVal.textContent = slider.value;
            });
        }

        // Play button
        const playBtn = document.getElementById("play-btn");
        if (playBtn) {
            playBtn.addEventListener("click", () => {
                const depth = slider ? parseInt(slider.value) : 8;
                document.getElementById("start-screen").classList.add("hidden");
                document.getElementById("game-screen").classList.remove("hidden");
                this.startGame(depth);
            });
        }

        // Undo
        if (this.undoBtn) {
            this.undoBtn.addEventListener("click", () => this.undo());
        }

        // New game (while playing)
        if (this.newBtn) {
            this.newBtn.addEventListener("click", () => this._showStart());
        }
    }

    _showStart() {
        document.getElementById("start-screen").classList.remove("hidden");
        document.getElementById("game-screen").classList.add("hidden");
    }

    // ─── API ────────────────────────────────────────────────────

    async _api(endpoint, method = "GET", body = null) {
        const opts = { method, headers: { "Content-Type": "application/json" } };
        if (body) opts.body = JSON.stringify(body);

        try {
            const res = await fetch(`${API}${endpoint}`, opts);
            if (!res.ok) {
                const err = await res.json().catch(() => ({ detail: "Unknown error" }));
                console.error("API error:", err.detail);
                return null;
            }
            return await res.json();
        } catch (e) {
            console.error("Network error:", e);
            if (this.statusEl) {
                this.statusEl.textContent = "Cannot connect to server";
                this.statusEl.className = "status error";
            }
            return null;
        }
    }

    // ─── Game Flow ──────────────────────────────────────────────

    async startGame(depth) {
        this._setBusy(true);
        const data = await this._api("/api/new-game", "POST", {
            ai_color: "black",
            depth: depth,
        });
        this._setBusy(false);

        if (data) {
            this.state = data;
            this.selected = null;
            this._renderAll();

            // If AI goes first, trigger AI move
            if (data.turn !== this.playerColor && !data.is_game_over) {
                this._doAiMove();
            }
        }
    }

    async makeMove(fromSq, toSq) {
        if (this.busy) return;

        // Build UCI string
        let uci = fromSq + toSq;

        // Check for pawn promotion: white pawn reaching rank 8
        const piece = this._pieceAt(fromSq);
        let promotion = null;
        if (piece && piece.type === "p" && piece.color === "white" && toSq[1] === "8") {
            promotion = await this._showPromotion();
            if (!promotion) return; // Cancelled
        }

        // Step 1: Send player move — board updates INSTANTLY
        this._setBusy(true);
        const moveData = await this._api("/api/move", "POST", {
            move: uci,
            promotion: promotion,
        });

        if (!moveData) {
            this._setBusy(false);
            await this._resync();
            return;
        }

        // Show player's move on the board immediately
        this.state = moveData;
        this.selected = null;
        this._renderAll();

        // Check if game is over after player's move
        if (moveData.is_game_over) {
            this._setBusy(false);
            if (moveData.is_checkmate) this._showConfetti();
            this._showGameOver(moveData.game_result);
            return;
        }

        // Step 2: Ask AI to think (board already shows player's move)
        await this._doAiMove();
    }

    async _doAiMove() {
        // Show "AI is thinking" in the status bar
        if (this.statusEl) {
            this.statusEl.textContent = "AI is thinking...";
            this.statusEl.className = "status";
        }

        this._setBusy(true);
        const aiData = await this._api("/api/ai-move", "POST");
        this._setBusy(false);

        if (aiData) {
            this.state = aiData;
            this._renderAll();

            if (aiData.is_checkmate) this._showConfetti();
            if (aiData.is_game_over) this._showGameOver(aiData.game_result);
        } else {
            await this._resync();
        }
    }

    async undo() {
        if (this.busy) return;
        this._setBusy(true);
        const data = await this._api("/api/undo", "POST");
        this._setBusy(false);

        if (data) {
            this.state = data;
            this.selected = null;
            this._renderAll();
        }
    }

    async _resync() {
        // Re-fetch the actual board state from the server
        const data = await this._api("/api/state");
        if (data) {
            this.state = data;
            this.selected = null;
            this._renderAll();
        }
    }

    _setBusy(busy) {
        this.busy = busy;
    }

    // ─── Board Rendering ────────────────────────────────────────

    _renderAll() {
        this._renderBoard();
        this._renderMoves();
        this._renderCaptures();
        this._renderInfo();
        this._renderStatus();
    }

    _renderBoard() {
        if (!this.state || !this.boardEl) return;
        this.boardEl.innerHTML = "";

        const grid = this._fenToGrid(this.state.fen);

        // Build a set of legal destination squares for the selected piece
        // so we can show dots. We do EXACT matching on legal_moves.
        const legalDests = new Set();
        if (this.selected) {
            for (const m of this.state.legal_moves) {
                // m is a UCI string like "e2e4" or "e7e8q"
                // Check if it starts with our selected square as the FROM
                if (m.substring(0, 2) === this.selected) {
                    legalDests.add(m.substring(2, 4)); // dest square
                }
            }
        }

        for (let row = 0; row < 8; row++) {
            for (let col = 0; col < 8; col++) {
                const file = "abcdefgh"[col];
                const rank = String(8 - row);
                const sq = file + rank;

                const cell = document.createElement("div");
                cell.className = "cell";
                cell.dataset.square = sq;
                cell.classList.add((row + col) % 2 === 0 ? "light" : "dark");

                // Piece
                const piece = grid[row][col];
                if (piece) {
                    const span = document.createElement("span");
                    span.className = `piece piece-${piece.color}`;
                    span.textContent = piece.unicode;
                    cell.appendChild(span);
                }

                // Coordinate labels
                if (col === 0) {
                    const lbl = document.createElement("span");
                    lbl.className = "coord rank-coord";
                    lbl.textContent = rank;
                    cell.appendChild(lbl);
                }
                if (row === 7) {
                    const lbl = document.createElement("span");
                    lbl.className = "coord file-coord";
                    lbl.textContent = file;
                    cell.appendChild(lbl);
                }

                // Selected highlight
                if (this.selected === sq) {
                    cell.classList.add("selected");
                }

                // Last move highlight
                if (this.state.last_move) {
                    if (sq === this.state.last_move.from_sq || sq === this.state.last_move.to_sq) {
                        cell.classList.add("last-move");
                    }
                }

                // Legal destination dots
                if (legalDests.has(sq)) {
                    const dot = document.createElement("span");
                    dot.className = piece ? "capture-hint" : "move-hint";
                    cell.appendChild(dot);
                }

                // Check highlight on king
                if (this.state.is_check && piece && piece.type === "k" &&
                    piece.color === this.state.turn) {
                    cell.classList.add("in-check");
                }

                // Click handler
                cell.addEventListener("click", () => this._onClick(sq));

                this.boardEl.appendChild(cell);
            }
        }
    }

    // ─── Click Logic ────────────────────────────────────────────

    _onClick(sq) {
        if (this.busy || !this.state || this.state.is_game_over) return;

        // Not player's turn
        if (this.state.turn !== this.playerColor) return;

        const piece = this._pieceAt(sq);

        if (this.selected) {
            // Something is already selected

            // Click same square → deselect
            if (sq === this.selected) {
                this.selected = null;
                this._renderBoard();
                return;
            }

            // Click own piece → re-select
            if (piece && piece.color === this.playerColor) {
                this.selected = sq;
                this._renderBoard();
                return;
            }

            // Check if this is a legal move using EXACT substring match
            const from = this.selected;
            const to = sq;
            const isLegal = this.state.legal_moves.some(m =>
                m.substring(0, 2) === from && m.substring(2, 4) === to
            );

            if (isLegal) {
                this.makeMove(from, to);
            } else {
                this.selected = null;
                this._renderBoard();
            }
        } else {
            // Nothing selected — select player's piece
            if (piece && piece.color === this.playerColor) {
                this.selected = sq;
                this._renderBoard();
            }
        }
    }

    // ─── FEN Parsing ────────────────────────────────────────────

    _fenToGrid(fen) {
        const UMAP = {
            K: "♔", Q: "♕", R: "♖", B: "♗", N: "♘", P: "♙",
            k: "♚", q: "♛", r: "♜", b: "♝", n: "♞", p: "♟",
        };

        const rows = fen.split(" ")[0].split("/");
        const grid = [];

        for (let r = 0; r < 8; r++) {
            const row = [];
            for (const ch of rows[r]) {
                if (ch >= "1" && ch <= "8") {
                    for (let i = 0; i < parseInt(ch); i++) row.push(null);
                } else {
                    row.push({
                        unicode: UMAP[ch],
                        color: ch === ch.toUpperCase() ? "white" : "black",
                        type: ch.toLowerCase(),
                    });
                }
            }
            grid.push(row);
        }
        return grid;
    }

    _pieceAt(sq) {
        if (!this.state) return null;
        const col = sq.charCodeAt(0) - 97;
        const row = 8 - parseInt(sq[1]);
        const grid = this._fenToGrid(this.state.fen);
        return grid[row]?.[col] || null;
    }

    // ─── Move History ───────────────────────────────────────────

    _renderMoves() {
        if (!this.movesEl || !this.state) return;
        this.movesEl.innerHTML = "";
        const moves = this.state.move_history;

        for (let i = 0; i < moves.length; i += 2) {
            const num = Math.floor(i / 2) + 1;
            const row = document.createElement("div");
            row.className = "move-row";

            const n = document.createElement("span");
            n.className = "move-number";
            n.textContent = `${num}.`;
            row.appendChild(n);

            const w = document.createElement("span");
            w.className = "move-notation";
            w.textContent = moves[i].notation;
            row.appendChild(w);

            if (i + 1 < moves.length) {
                const b = document.createElement("span");
                b.className = "move-notation";
                b.textContent = moves[i + 1].notation;
                row.appendChild(b);
            }

            this.movesEl.appendChild(row);
        }
        this.movesEl.scrollTop = this.movesEl.scrollHeight;
    }

    // ─── Captured Pieces ────────────────────────────────────────

    _renderCaptures() {
        if (!this.state) return;
        if (this.capWhEl) this.capWhEl.textContent = this.state.captured_white.join(" ");
        if (this.capBlEl) this.capBlEl.textContent = this.state.captured_black.join(" ");
    }

    // ─── Search Info ────────────────────────────────────────────

    _renderInfo() {
        if (!this.infoEl || !this.state) return;
        const info = this.state.ai_search_info;
        if (!info) { this.infoEl.innerHTML = ""; return; }

        // Eval is from player's (White's) perspective:
        // Positive = player advantage, Negative = AI advantage
        const evalPawns = (info.eval / 100).toFixed(1);
        const evalSign = info.eval >= 0 ? "+" : "";
        const evalLabel = info.eval > 50 ? "You're ahead" :
            info.eval < -50 ? "AI is ahead" : "Equal";

        this.infoEl.innerHTML = `
            <span class="info-item">Eval: <strong>${evalSign}${evalPawns}</strong> (${evalLabel})</span>
            <span class="info-item">Depth: <strong>${info.depth}</strong></span>
            <span class="info-item">Time: <strong>${info.time}s</strong></span>
            <span class="info-item">Positions cached: <strong>${(info.tt_stores / 1000).toFixed(0)}k</strong></span>
            <span class="info-item">Cache reused: <strong>${(info.tt_hits / 1000).toFixed(0)}k</strong></span>
        `;
    }

    // ─── Status ─────────────────────────────────────────────────

    _renderStatus() {
        if (!this.statusEl || !this.state) return;

        if (this.state.is_checkmate) {
            const w = this.state.game_result === "white_wins" ? "White" : "Black";
            this.statusEl.textContent = `Checkmate! ${w} wins`;
            this.statusEl.className = "status checkmate";
        } else if (this.state.is_stalemate) {
            this.statusEl.textContent = "Stalemate — Draw";
            this.statusEl.className = "status draw";
        } else if (this.state.is_game_over) {
            this.statusEl.textContent = "Game Over — Draw";
            this.statusEl.className = "status draw";
        } else if (this.state.is_check) {
            const s = this.state.turn === "white" ? "White" : "Black";
            this.statusEl.textContent = `${s} is in check!`;
            this.statusEl.className = "status check";
        } else {
            const s = this.state.turn === "white" ? "White" : "Black";
            this.statusEl.textContent = `${s} to move`;
            this.statusEl.className = "status";
        }
    }

    // ─── Promotion Dialog ───────────────────────────────────────

    _showPromotion() {
        return new Promise(resolve => {
            const modal = document.getElementById("promotion-modal");
            if (!modal) { resolve("q"); return; }

            modal.classList.add("visible");
            const pieces = modal.querySelectorAll(".promo-piece");
            const handlers = [];

            pieces.forEach((el, i) => {
                const h = () => {
                    modal.classList.remove("visible");
                    pieces.forEach((p, j) => p.removeEventListener("click", handlers[j]));
                    resolve(el.dataset.piece);
                };
                handlers[i] = h;
                el.addEventListener("click", h);
            });
        });
    }

    // ─── Game Over ──────────────────────────────────────────────

    _showGameOver(result) {
        const modal = document.getElementById("game-over-modal");
        if (!modal) return;

        const title = modal.querySelector(".modal-title");
        const msg = modal.querySelector(".modal-message");

        if (result === "white_wins") {
            title.textContent = "🎉 You Win!";
            msg.textContent = "Checkmate! White wins.";
        } else if (result === "black_wins") {
            title.textContent = "💀 You Lose";
            msg.textContent = "Checkmate! Black wins.";
        } else {
            title.textContent = "🤝 Draw";
            msg.textContent = "The game ended in a draw.";
        }

        modal.classList.add("visible");

        const btn = modal.querySelector(".play-again-btn");
        if (btn) {
            btn.addEventListener("click", () => {
                modal.classList.remove("visible");
                this._showStart();
            }, { once: true });
        }
    }

    // ─── Confetti ───────────────────────────────────────────────

    _showConfetti() {
        const c = document.getElementById("confetti-container");
        if (!c) return;
        c.innerHTML = "";
        const colors = ["#ffd700", "#ff6b6b", "#48dbfb", "#ff9ff3", "#54a0ff", "#5f27cd"];
        for (let i = 0; i < 80; i++) {
            const p = document.createElement("div");
            p.className = "confetti-particle";
            p.style.left = `${Math.random() * 100}%`;
            p.style.backgroundColor = colors[Math.floor(Math.random() * colors.length)];
            p.style.animationDelay = `${Math.random() * 2}s`;
            p.style.animationDuration = `${2 + Math.random() * 3}s`;
            c.appendChild(p);
        }
        setTimeout(() => { c.innerHTML = ""; }, 5000);
    }
}

// ─── Initialize ─────────────────────────────────────────────────

document.addEventListener("DOMContentLoaded", () => {
    window.chessApp = new ChessApp();
});
