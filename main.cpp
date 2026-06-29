#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <climits>
#include <cmath>
#include <array>
#include <algorithm>

// ── Constants ────────────────────────────────────────────────
constexpr int ROWS       = 6;
constexpr int COLS       = 7;
constexpr int CELL       = 90;
constexpr int RADIUS     = 36;
constexpr int PANEL      = 160;
constexpr int WIN_W      = COLS * CELL;
constexpr int WIN_H      = ROWS * CELL + PANEL;
constexpr int AI_DEPTH   = 7;
constexpr float DROP_SPD = 18.0f;
constexpr float FPS      = 60.0f;

// ── Colors ───────────────────────────────────────────────────
const sf::Color C_BG(12, 12, 22);
const sf::Color C_BOARD(18, 88, 168);
const sf::Color C_EMPTY(228, 226, 212);
const sf::Color C_HOVER(200, 198, 185);
const sf::Color C_RED(218, 55, 55);
const sf::Color C_YELLOW(238, 192, 38);
const sf::Color C_PANEL(22, 22, 38);
const sf::Color C_BTN1(38, 38, 58);
const sf::Color C_BTN2(50, 50, 75);
const sf::Color C_WHITE(255, 255, 255);
const sf::Color C_MUTED(140, 140, 170);
const sf::Color C_ACCENT(80, 140, 220);
const sf::Color C_POPUP_BG(28, 28, 48, 245); // Dark semi-transparent popup background

// ── Enums ────────────────────────────────────────────────────
enum Player    { NONE = 0, RED = 1, YELLOW = 2 };
enum GameMode  { MENU, PVP, PVC };
enum GameState { PLAYING, WIN, DRAW };

// ── Game Structs ─────────────────────────────────────────────
struct DropAnim {
    bool active = false;
    int col = 0;
    int row = 0;
    Player player = NONE;
    float y = 0.0f;
    float targetY = 0.0f;
};

struct WinFlash {
    bool active = false;
    float timer = 0.0f;
};

class Game {
public:
    std::array<std::array<Player, COLS>, ROWS> grid{};
    Player current = RED;
    GameMode mode = MENU;
    GameState state = PLAYING;
    
    std::array<int, 3> pvpScores{0, 0, 0}; 
    std::array<int, 3> pvcScores{0, 0, 0}; 
    
    std::vector<sf::Vector2i> winCells;
    int hoverCol = -1;
    int moveCount = 0;
    std::array<int, COLS> heights{};

    static constexpr std::array<int, COLS> ORDER = {3, 2, 4, 1, 5, 0, 6};

    Game() { reset(); }

    void reset() {
        for (auto& row : grid) row.fill(NONE);
        heights.fill(0);
        current = RED;
        state = PLAYING;
        winCells.clear();
        hoverCol = -1;
        moveCount = 0;
    }

    int getRow(int col) const {
        return (heights[col] >= ROWS) ? -1 : ROWS - 1 - heights[col];
    }

    bool checkWin(int row, int col, std::vector<sf::Vector2i>& cells) const {
        Player p = grid[row][col];
        if (p == NONE) return false;

        static const std::array<sf::Vector2i, 4> dirs{{{0, 1}, {1, 0}, {1, 1}, {1, -1}}};
        
        for (const auto& d : dirs) {
            std::vector<sf::Vector2i> line{{row, col}};
            
            for (int i = 1; i < 4; ++i) {
                int r = row + d.x * i, c = col + d.y * i;
                if (r >= 0 && r < ROWS && c >= 0 && c < COLS && grid[r][c] == p) line.push_back({r, c});
                else break;
            }
            for (int i = 1; i < 4; ++i) {
                int r = row - d.x * i, c = col - d.y * i;
                if (r >= 0 && r < ROWS && c >= 0 && c < COLS && grid[r][c] == p) line.push_back({r, c});
                else break;
            }
            if (line.size() >= 4) {
                cells = std::move(line);
                return true;
            }
        }
        return false;
    }

    bool isDraw() const { return moveCount == ROWS * COLS; }

    bool drop(int col) {
        int row = getRow(col);
        if (row == -1) return false;

        grid[row][col] = current;
        heights[col]++;
        moveCount++;

        if (checkWin(row, col, winCells)) {
            state = WIN;
            if (mode == PVP) {
                pvpScores[current]++;
            } else {
                pvcScores[current]++;
            }
        } else if (isDraw()) {
            state = DRAW;
        } else {
            current = (current == RED) ? YELLOW : RED;
        }
        return true;
    }

    int scoreWindow(std::array<Player, 4> w, Player p) const {
        Player o = (p == RED) ? YELLOW : RED;
        int pc = 0, ec = 0, oc = 0;
        for (auto x : w) {
            if (x == p) pc++;
            else if (x == NONE) ec++;
            else oc++;
        }
        if (pc == 4) return 1000;
        if (pc == 3 && ec == 1) return 6;
        if (pc == 2 && ec == 2) return 2;
        if (oc == 3 && ec == 1) return -4;
        if (oc == 4) return -1000;
        return 0;
    }

    int evalBoard(Player p) const {
        int s = 0;
        for (int r = 0; r < ROWS; r++) if (grid[r][3] == p) s += 4;

        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c <= COLS - 4; c++) {
                s += scoreWindow({grid[r][c], grid[r][c+1], grid[r][c+2], grid[r][c+3]}, p);
            }
        }
        for (int c = 0; c < COLS; c++) {
            for (int r = 0; r <= ROWS - 4; r++) {
                s += scoreWindow({grid[r][c], grid[r+1][c], grid[r+2][c], grid[r+3][c]}, p);
            }
        }
        for (int r = 0; r <= ROWS - 4; r++) {
            for (int c = 0; c <= COLS - 4; c++) {
                s += scoreWindow({grid[r][c], grid[r+1][c+1], grid[r+2][c+2], grid[r+3][c+3]}, p);
                s += scoreWindow({grid[r+3][c], grid[r+2][c+1], grid[r+1][c+2], grid[r][c+3]}, p);
            }
        }
        return s;
    }

    int minimax(int depth, bool isMax, int alpha, int beta, int lastRow, int lastCol) {
        std::vector<sf::Vector2i> tmp;
        if (lastRow != -1 && checkWin(lastRow, lastCol, tmp)) {
            return isMax ? (INT_MIN + (AI_DEPTH - depth)) : (INT_MAX - (AI_DEPTH - depth));
        }
        if (moveCount == ROWS * COLS) return 0;
        if (depth == 0) return evalBoard(YELLOW) - evalBoard(RED);

        if (isMax) {
            int best = INT_MIN;
            for (int c : ORDER) {
                int r = getRow(c);
                if (r == -1) continue;
                grid[r][c] = YELLOW; heights[c]++; moveCount++;
                best = std::max(best, minimax(depth - 1, false, alpha, beta, r, c));
                grid[r][c] = NONE; heights[c]--; moveCount--;
                alpha = std::max(alpha, best);
                if (beta <= alpha) break;
            }
            return best;
        } else {
            int best = INT_MAX;
            for (int c : ORDER) {
                int r = getRow(c);
                if (r == -1) continue;
                grid[r][c] = RED; heights[c]++; moveCount++;
                best = std::min(best, minimax(depth - 1, true, alpha, beta, r, c));
                grid[r][c] = NONE; heights[c]--; moveCount--;
                beta = std::min(beta, best);
                if (beta <= alpha) break;
            }
            return best;
        }
    }

    int bestMove() {
        for (Player p : {YELLOW, RED}) {
            for (int c : ORDER) {
                int r = getRow(c);
                if (r == -1) continue;
                grid[r][c] = p;
                std::vector<sf::Vector2i> tmp;
                bool w = checkWin(r, c, tmp);
                grid[r][c] = NONE;
                if (w) return c;
            }
        }

        int best = INT_MIN;
        int bestCol = 3;
        int alpha = INT_MIN;
        int beta = INT_MAX;

        for (int c : ORDER) {
            int r = getRow(c);
            if (r == -1) continue;
            grid[r][c] = YELLOW; heights[c]++; moveCount++;
            int sc = minimax(AI_DEPTH - 1, false, alpha, beta, r, c);
            grid[r][c] = NONE; heights[c]--; moveCount--;
            if (sc > best) {
                best = sc;
                bestCol = c;
            }
            alpha = std::max(alpha, best);
        }
        return bestCol;
    }
};

// ── Rendering Helper Engine ──────────────────────────────────
class Renderer {
private:
    sf::CircleShape circle;
    sf::RectangleShape rect;

public:
    void fillCircle(sf::RenderTarget& t, float cx, float cy, float r, sf::Color col) {
        circle.setRadius(r);
        circle.setFillColor(col);
        circle.setPosition(cx - r, cy - r);
        t.draw(circle);
    }

    void fillRect(sf::RenderTarget& t, float x, float y, float w, float h, sf::Color col, float rx = 0) {
        if (rx <= 0) {
            rect.setSize({w, h});
            rect.setFillColor(col);
            rect.setPosition(x, y);
            t.draw(rect);
            return;
        }
        rect.setSize({w, h - 2 * rx}); rect.setFillColor(col); rect.setPosition(x, y + rx); t.draw(rect);
        rect.setSize({w - 2 * rx, rx * 2}); rect.setPosition(x + rx, y); t.draw(rect);
        rect.setPosition(x + rx, y + h - 2 * rx); t.draw(rect);
        
        fillCircle(t, x + rx, y + rx, rx, col);
        fillCircle(t, x + w - rx, y + rx, rx, col);
        fillCircle(t, x + rx, y + h - rx, rx, col);
        fillCircle(t, x + w - rx, y + h - rx, rx, col);
    }

    void drawText(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned sz, float x, float y, sf::Color col, bool centered = false, bool bold = false) {
        sf::Text tx(s, f, sz);
        tx.setFillColor(col);
        if (bold) tx.setStyle(sf::Text::Bold);
        if (centered) {
            auto b = tx.getLocalBounds();
            tx.setPosition(x - b.width / 2.0f, y - b.height / 2.0f - b.top);
        } else tx.setPosition(x, y);
        t.draw(tx);
    }
};

inline bool btnHit(float mx, float my, float x, float y, float w, float h) {
    return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

// ── Main Entry ───────────────────────────────────────────────
int main() {
    sf::ContextSettings settings;
    settings.antialiasingLevel = 4;
    sf::RenderWindow window(sf::VideoMode(WIN_W, WIN_H), "Connect 4", sf::Style::Titlebar | sf::Style::Close, settings);
    window.setFramerateLimit(static_cast<unsigned int>(FPS));

    sf::Font font;
    bool fontOk = font.loadFromFile("C:/Windows/Fonts/segoeui.ttf") || 
                  font.loadFromFile("C:/Windows/Fonts/arial.ttf")   || 
                  font.loadFromFile("C:/Windows/Fonts/calibri.ttf");

    Game game;
    DropAnim anim;
    WinFlash flash;
    Renderer renderer;
    bool aiPending = false;
    sf::Clock aiClock, flashClock, frameClock;
    sf::Vector2f mousePos;

    auto startAI = [&]() { aiPending = true; aiClock.restart(); };

    // Coordinates for the centered game-over popup layout elements
    constexpr float popW = 340.0f;
    constexpr float popH = 220.0f;
    const float popX = (WIN_W - popW) / 2.0f;
    const float popY = PANEL + (ROWS * CELL - popH) / 2.0f;

    while (window.isOpen()) {
        float dt = frameClock.restart().asSeconds();
        sf::Event ev;

        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed) window.close();

            if (ev.type == sf::Event::MouseMoved) {
                mousePos.x = static_cast<float>(ev.mouseMove.x);
                mousePos.y = static_cast<float>(ev.mouseMove.y);
                if (game.mode != MENU) {
                    game.hoverCol = std::max(0, std::min(COLS - 1, ev.mouseMove.x / CELL));
                }
            }

            if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left) {
                float mx = static_cast<float>(ev.mouseButton.x);
                float my = static_cast<float>(ev.mouseButton.y);

                if (game.mode == MENU) {
                    if (btnHit(mx, my, WIN_W / 2.0f - 165, WIN_H / 2.0f - 35, 150, 60)) { game.mode = PVP; game.reset(); }
                    if (btnHit(mx, my, WIN_W / 2.0f + 15, WIN_H / 2.0f - 35, 150, 60)) { game.mode = PVC; game.reset(); }
                } else {
                    // Check if popup context actions are active
                    if (game.state != PLAYING && !anim.active) {
                        // Play Again Button clicked inside the popup
                        if (btnHit(mx, my, popX + 30, popY + 130, 130, 42)) {
                            game.reset(); aiPending = false; flash.active = false;
                        }
                        // Menu Button clicked inside the popup
                        if (btnHit(mx, my, popX + 180, popY + 130, 130, 42)) {
                            game.mode = MENU; game.reset(); aiPending = false; flash.active = false;
                        }
                    } else {
                        // Standard top bar layout home interface button interaction
                        if (btnHit(mx, my, 10, 10, 90, 34) && !anim.active) { 
                            game.mode = MENU; game.reset(); aiPending = false; anim.active = false; 
                        }

                        bool aiTurn = (game.mode == PVC && game.current == YELLOW);
                        if (game.state == PLAYING && !anim.active && !aiTurn) {
                            int col = static_cast<int>(mx) / CELL;
                            if (col >= 0 && col < COLS) {
                                int row = game.getRow(col);
                                if (row != -1) {
                                    anim = {true, col, row, game.current, -static_cast<float>(RADIUS) * 2, static_cast<float>(row * CELL + CELL / 2.0f)};
                                }
                            }
                        }
                    }
                }
            }

            if (ev.type == sf::Event::KeyPressed) {
                if (ev.key.code == sf::Keyboard::R && game.mode != MENU && !anim.active) { game.reset(); aiPending = false; flash.active = false; }
                if (ev.key.code == sf::Keyboard::Escape && game.mode != MENU && !anim.active) { game.mode = MENU; game.reset(); aiPending = false; }
            }
        }

        // ── Physics Updates ──────────────────────────────────
        if (anim.active) {
            anim.y += DROP_SPD * (3.0f + anim.y / 200.0f);
            if (anim.y >= anim.targetY) {
                anim.y = anim.targetY;
                anim.active = false;
                game.drop(anim.col);
                if (game.state == WIN) { flash.active = true; flashClock.restart(); }
                if (game.mode == PVC && game.state == PLAYING && game.current == YELLOW) startAI();
            }
        }

        if (aiPending && !anim.active && game.state == PLAYING && aiClock.getElapsedTime().asSeconds() > 0.35f) {
            aiPending = false;
            int col = game.bestMove();
            int row = game.getRow(col);
            if (row != -1) {
                anim = {true, col, row, YELLOW, -static_cast<float>(RADIUS) * 2, static_cast<float>(row * CELL + CELL / 2.0f)};
            }
        }

        if (flash.active) flash.timer = flashClock.getElapsedTime().asSeconds();

        // ── Render Frame ─────────────────────────────────────
        window.clear(C_BG);

        if (game.mode == MENU) {
            for (int i = 0; i < 7; ++i) {
                renderer.fillCircle(window, static_cast<float>(i * CELL + CELL / 2), static_cast<float>(WIN_H - CELL / 2), RADIUS + 4, sf::Color(30, 30, 50));
            }
            
            renderer.fillRect(window, WIN_W / 2.0f - 165, WIN_H / 2.0f - 35, 150, 60, sf::Color(218, 55, 55), 12);
            renderer.fillRect(window, WIN_W / 2.0f + 15, WIN_H / 2.0f - 35, 150, 60, sf::Color(24, 95, 165), 12);
            
            if (fontOk) {
                renderer.drawText(window, font, "CONNECT", 56, WIN_W / 2.0f - 10, WIN_H / 2.0f - 190, C_WHITE, true, true);
                renderer.drawText(window, font, "4", 56, WIN_W / 2.0f + 82, WIN_H / 2.0f - 190, C_RED, false, true);
                renderer.drawText(window, font, "Choose a mode to play", 16, WIN_W / 2.0f, WIN_H / 2.0f - 120, C_MUTED, true);
                
                renderer.drawText(window, font, "Player vs Player", 14, WIN_W / 2.0f - 90, WIN_H / 2.0f - 5, C_WHITE, true, true);
                renderer.drawText(window, font, "Player vs CPU", 14, WIN_W / 2.0f + 90, WIN_H / 2.0f - 5, C_WHITE, true, true);
                
                renderer.drawText(window, font, "Press R to restart  |  ESC for menu", 13, WIN_W / 2.0f, WIN_H - 30, C_MUTED, true);
            }
            renderer.fillCircle(window, WIN_W / 2.0f - 90, WIN_H / 2.0f - 65, 22, C_RED);
            renderer.fillCircle(window, WIN_W / 2.0f + 90, WIN_H / 2.0f - 65, 22, C_YELLOW);
        } else {
            // HUD Panels
            renderer.fillRect(window, 0, 0, WIN_W, PANEL, C_PANEL);
            renderer.fillRect(window, 10, 10, 90, static_cast<float>(32), C_BTN1, 7);
            if (fontOk) renderer.drawText(window, font, "< Menu", 13, 55, 26, sf::Color(170, 170, 200), true);

            std::string modeStr = (game.mode == PVP) ? "Player vs Player" : "Player vs CPU";
            if (fontOk) renderer.drawText(window, font, modeStr, 13, WIN_W / 2.0f, 16, C_ACCENT, true);

            float sy = 50;
            renderer.fillRect(window, WIN_W / 2.0f - 160, sy - 2, 140, 34, C_BTN1, 8);
            renderer.fillRect(window, WIN_W / 2.0f + 20, sy - 2, 140, 34, C_BTN1, 8);
            renderer.fillCircle(window, WIN_W / 2.0f - 148, sy + 14, 7, C_RED);
            renderer.fillCircle(window, WIN_W / 2.0f + 148, sy + 14, 7, C_YELLOW);

            std::string p1 = (game.mode == PVC) ? "You" : "Red";
            std::string p2 = (game.mode == PVP) ? "Yellow" : "CPU";
            int score1 = (game.mode == PVP) ? game.pvpScores[1] : game.pvcScores[1];
            int score2 = (game.mode == PVP) ? game.pvpScores[2] : game.pvcScores[2];

            if (fontOk) {
                renderer.drawText(window, font, p1 + "  " + std::to_string(score1), 14, WIN_W / 2.0f - 136, sy + 4, C_WHITE);
                renderer.drawText(window, font, "vs", 12, WIN_W / 2.0f - 12, sy + 6, C_MUTED, true);
                renderer.drawText(window, font, p2 + "  " + std::to_string(score2), 14, WIN_W / 2.0f + 32, sy + 4, C_WHITE);
            }

            std::string statusStr; sf::Color statusCol = C_WHITE;
            if (game.state == PLAYING) {
                if (game.mode == PVC && game.current == YELLOW) { statusStr = "CPU is thinking..."; statusCol = C_YELLOW; }
                else if (game.current == RED) { statusStr = (game.mode == PVC) ? "Your turn (Red)" : "Red's turn"; statusCol = C_RED; }
                else { statusStr = "Yellow's turn"; statusCol = C_YELLOW; }
            } else if (game.state == WIN) {
                if (game.mode == PVC) statusStr = (game.current == RED) ? "You win! \U0001F389" : "CPU wins!";
                else statusStr = (game.current == RED) ? "Red wins!" : "Yellow wins!";
                statusCol = (game.current == RED) ? C_RED : C_YELLOW;
            } else { statusStr = "It's a draw!"; statusCol = C_ACCENT; }

            renderer.fillRect(window, WIN_W / 2.0f - 130, 90, 260, 36, C_BTN2, 9);
            if (fontOk) renderer.drawText(window, font, statusStr, 15, WIN_W / 2.0f, 108, statusCol, true);

            // Drop Guide Line Indicator
            if (game.state == PLAYING && game.hoverCol >= 0 && !anim.active && !(game.mode == PVC && game.current == YELLOW)) {
                sf::Color hc = (game.current == RED) ? C_RED : C_YELLOW; hc.a = 200;
                float hx = static_cast<float>(game.hoverCol * CELL + CELL / 2);
                sf::ConvexShape tri(3);
                tri.setPoint(0, {hx - 10, PANEL - 28}); tri.setPoint(1, {hx + 10, PANEL - 28}); tri.setPoint(2, {hx, PANEL - 12});
                tri.setFillColor(hc); window.draw(tri);
            }

            // Game Board Render
            renderer.fillRect(window, 0, static_cast<float>(PANEL), static_cast<float>(WIN_W), static_cast<float>(ROWS * CELL), C_BOARD, 14);

            if (game.state == PLAYING && game.hoverCol >= 0 && !anim.active && !(game.mode == PVC && game.current == YELLOW)) {
                renderer.fillRect(window, static_cast<float>(game.hoverCol * CELL), static_cast<float>(PANEL), static_cast<float>(CELL), static_cast<float>(ROWS * CELL), sf::Color(255, 255, 255, 18));
            }

            bool flashOn = (flash.active && static_cast<int>(flash.timer * 5) % 2 == 0);
            for (int r = 0; r < ROWS; ++r) {
                for (int c = 0; c < COLS; ++c) {
                    float cx = static_cast<float>(c * CELL + CELL / 2);
                    float cy = static_cast<float>(PANEL + r * CELL + CELL / 2);

                    bool isWin = std::any_of(game.winCells.begin(), game.winCells.end(), [r, c](const sf::Vector2i& wc) { return wc.x == r && wc.y == c; });

                    if (isWin && flash.active) {
                        renderer.fillCircle(window, cx, cy, static_cast<float>(RADIUS + 7), flashOn ? C_WHITE : sf::Color(255, 255, 255, 80));
                    }
                    if (anim.active && anim.col == c && anim.row == r) {
                        renderer.fillCircle(window, cx, cy, static_cast<float>(RADIUS), C_EMPTY);
                        continue;
                    }

                    sf::Color fc = C_EMPTY;
                    if (game.grid[r][c] == RED) fc = C_RED;
                    else if (game.grid[r][c] == YELLOW) fc = C_YELLOW;
                    else if (c == game.hoverCol && game.state == PLAYING && !anim.active && !(game.mode == PVC && game.current == YELLOW)) fc = C_HOVER;

                    if (game.state != PLAYING && !isWin && game.grid[r][c] != NONE) {
                        fc.r = static_cast<sf::Uint8>(fc.r * 0.4f); fc.g = static_cast<sf::Uint8>(fc.g * 0.4f); fc.b = static_cast<sf::Uint8>(fc.b * 0.4f);
                    }
                    renderer.fillCircle(window, cx, cy, static_cast<float>(RADIUS), fc);
                }
            }

            if (anim.active) {
                float cx = static_cast<float>(anim.col * CELL + CELL / 2);
                float cy = static_cast<float>(PANEL) + anim.y;
                renderer.fillCircle(window, cx, cy + 4, static_cast<float>(RADIUS), sf::Color(0, 0, 0, 60));
                renderer.fillCircle(window, cx, cy, static_cast<float>(RADIUS), (anim.player == RED) ? C_RED : C_YELLOW);
                renderer.fillCircle(window, cx - 10, cy - 10, 10, sf::Color(255, 255, 255, 50));
            }

            // ── Game Over Pop-up Box Window Overlay ──────────────────────────────
            if (game.state != PLAYING && !anim.active) {
                // Dim structural layout background
                renderer.fillRect(window, 0, static_cast<float>(PANEL), static_cast<float>(WIN_W), static_cast<float>(ROWS * CELL), sf::Color(0, 0, 0, 100));
                
                // Draw pop-up layout pane
                renderer.fillRect(window, popX, popY, popW, popH, C_POPUP_BG, 16);
                renderer.fillRect(window, popX + 10, popY + 10, popW - 20, 6, (game.state == DRAW) ? C_ACCENT : ((game.current == RED) ? C_RED : C_YELLOW), 3);

                std::string headerText;
                sf::Color headerColor = C_WHITE;

                if (game.state == DRAW) {
                    headerText = "IT'S A DRAW!";
                    headerColor = C_ACCENT;
                } else {
                    if (game.mode == PVC) {
                        headerText = (game.current == RED) ? "YOU WIN! \U0001F389" : "CPU WINS! \U0001F916";
                    } else {
                        headerText = (game.current == RED) ? "RED WINS!" : "YELLOW WINS!";
                    }
                    headerColor = (game.current == RED) ? C_RED : C_YELLOW;
                }

                // Dynamic interactive check properties for hover tracking styling updates
                sf::Color replayBtnColor = btnHit(mousePos.x, mousePos.y, popX + 30, popY + 130, 130, 42) ? sf::Color(45, 125, 75) : sf::Color(35, 105, 60);
                sf::Color menuBtnColor = btnHit(mousePos.x, mousePos.y, popX + 180, popY + 130, 130, 42) ? sf::Color(70, 70, 100) : C_BTN1;

                if (fontOk) {
                    renderer.drawText(window, font, "MATCH OVER", 12, WIN_W / 2.0f, popY + 36, C_MUTED, true, true);
                    renderer.drawText(window, font, headerText, 26, WIN_W / 2.0f, popY + 68, headerColor, true, true);
                }

                // Interactive Buttons
                renderer.fillRect(window, popX + 30, popY + 130, 130, 42, replayBtnColor, 8);
                renderer.fillRect(window, popX + 180, popY + 130, 130, 42, menuBtnColor, 8);

                if (fontOk) {
                    renderer.drawText(window, font, "Play Again", 14, popX + 95, popY + 151, C_WHITE, true, true);
                    renderer.drawText(window, font, "Main Menu", 14, popX + 245, popY + 151, C_WHITE, true, true);
                }
            }

            if (fontOk) renderer.drawText(window, font, "R = restart   ESC = menu", 11, static_cast<float>(WIN_W / 2), WIN_H - 16, sf::Color(100, 100, 130), true);
        }
        window.display();
    }
    return 0;
}