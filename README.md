# Connect 4 — SFML Desktop Game

A modern, highly polished **Connect 4** desktop game built in C++ using the **SFML (Simple and Fast Multimedia Library)** framework. The game features a fluid graphical interface, physics-based piece-drop animations, multi-game score tracking, and an intelligent computer opponent powered by a Minimax AI algorithm with Alpha-Beta pruning.

---

## 🌟 Key Features

* **Dual Game Modes:**
  * **Player vs Player (PvP):** Local multiplayer for two players sharing a screen.
  * **Player vs CPU (PvC):** A challenging single-player mode against an intelligent computer opponent.
* **Intelligent AI Engine:** Powered by a multi-layered evaluation heuristic spanning a depth-7 search tree, hardened with **Alpha-Beta pruning** for rapid decision-making and optimal defense/offense.
* **Dynamic Visual Experience:**
  * **Smooth Drop Physics:** Game discs realistically accelerate down columns rather than snapping into place.
  * **UX Indicators:** Real-time column hover tracking highlights the selected slot with subtle overlays and directional arrows.
  * **Win Conditions:** Winning sequences trigger a striking, high-contrast flashing ring animation while dimming the losing tokens.
* **Integrated HUD Panel:** Displays the active game mode, running session score charts (You vs CPU / Red vs Yellow), and explicit real-time status text bars ("CPU is thinking...", "Your turn", etc.).

---

## 🛠️ Technical Architecture & Highlights

The project is structured cleanly to handle game rules, state configurations, and hardware-accelerated rendering:

### 1. Board & Layout Configurations
The playing environment is mapped mathematically across an industry-standard **6 Rows × 7 Columns** grid. Cell spacings are strictly dimensioned (`90px` pitch with `36px` token radii) ensuring crisp rendering overhead. The global coordinates dynamically stretch to allocate a dedicated rendering canvas alongside a hardware-optimized dashboard (`160px` top panel height).

### 2. State & Structural Data Types
* **`Game` Struct:** Encapsulates the core game logic. It manages a 2D grid matrix (`Player grid[6][7]`), tracks active column capacities using a fast lookup array (`heights[COLS]`) to accelerate position evaluation, monitors total move counts, and houses victory vectors (`winCells`).
* **`DropAnim` Struct:** Manages delta-time physics frames independently from game rules, calculating pixel velocity increments (`DROP_SPD = 18.f`) to drive the token dropping animation down columns.
* **`WinFlash` Struct:** Acts as a specialized rendering clock tracking timestamp intervals to flash the background vectors of a victorious four-in-a-row connection.

### 3. Core Logic & Win Detection
Rather than continuously scanning the entire board frame-by-frame, the program evaluates winning paths localized to the newest token coordinates. The algorithm checks four vectors (Horizontal, Vertical, and both Diagonals) tracking forward and backward steps to catch any consecutive combination equal to or exceeding 4 tokens. 

### 4. AI Engine: Minimax with Alpha-Beta Pruning
The CPU's tactical brain operates via specialized tree-search operations:
* **Move Order Optimization:** The AI evaluates columns from the center outward (`ORDER = {3, 2, 4, 1, 5, 0, 6}`), dramatically improving the efficiency of Alpha-Beta pruning.
* **Heuristic Weight Matrix (`evalBoard`):** Windows of 4 positions are analyzed and assigned priority scores:
    * *4 Connected (CPU Win):* $+1000$ points
    * *3 Connected + Open Space:* $+6$ points
    * *2 Connected + Open Spaces:* $+2$ points
    * *3 Opponent Tokens Blocked:* $-4$ points
    * *4 Opponent Tokens (Opponent Win):* $-1000$ points
* **Immediate Play Safeguards:** Before descending into deep tree branches, the `bestMove()` system checks for immediate terminal scenarios to lock in an instantly available victory or block an opponent's lethal threat.

---

## 🎮 How to Play

1. **Main Menu:** Select **Player vs Player** or **Player vs CPU** via mouse click.
2. **Dropping Tokens:** Move your mouse over a column. A colored arrow and a highlighted tracking bar will guide your cursor. Left-click to release a token into that column.
3. **Winning:** The first player to align 4 tokens horizontally, vertically, or diagonally wins the match.
4. **Hotkeys:**
   * `R` — Instantly resets the current board to start a new match while preserving running session scores.
   * `ESC` — Quits the current match and returns to the Main Menu.

---

## 📦 Prerequisites & Build Requirements

To run this project locally, ensure you have the following configurations set up:
* **C++ Compiler** (supporting C++11 or higher)
* **SFML Library** (Graphics, Window, and System modules configured)
* **OS Fonts:** Uses default system font paths (`Segoe UI`, `Arial`, or `Calibri`).