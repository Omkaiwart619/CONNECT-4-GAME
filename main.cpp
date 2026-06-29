#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <climits>
#include <cmath>
#include <array>

// ── Constants ────────────────────────────────────────────────
const int ROWS       = 6;
const int COLS       = 7;
const int CELL       = 90;
const int RADIUS     = 36;
const int PANEL      = 160;
const int WIN_W      = COLS * CELL;
const int WIN_H      = ROWS * CELL + PANEL;
const int AI_DEPTH   = 7;
const float DROP_SPD = 18.f;   // pixels per frame drop speed
const float FPS      = 60.f;

// ── Colours ──────────────────────────────────────────────────
const sf::Color C_BG      (12,  12,  22);
const sf::Color C_BOARD   (18,  88, 168);
const sf::Color C_BOARD2  (14,  70, 140);
const sf::Color C_EMPTY   (228, 226, 212);
const sf::Color C_HOVER   (200, 198, 185);
const sf::Color C_RED     (218,  55,  55);
const sf::Color C_RED2    (180,  35,  35);
const sf::Color C_YELLOW  (238, 192,  38);
const sf::Color C_YELLOW2 (200, 155,  20);
const sf::Color C_PANEL   ( 22,  22,  38);
const sf::Color C_BTN1    ( 38,  38,  58);
const sf::Color C_BTN2    ( 50,  50,  75);
const sf::Color C_WHITE   (255, 255, 255);
const sf::Color C_MUTED   (110, 110, 140);
const sf::Color C_ACCENT  ( 80, 140, 220);

// ── Enums ────────────────────────────────────────────────────
enum Player    { NONE=0, RED=1, YELLOW=2 };
enum GameMode  { MENU, PVP, PVC };
enum GameState { PLAYING, WIN, DRAW };

// ── Drop animation ───────────────────────────────────────────
struct DropAnim {
    bool   active = false;
    int    col    = 0;
    int    row    = 0;       // target row
    Player player = NONE;
    float  y      = 0.f;    // current pixel y (relative to PANEL)
    float  targetY= 0.f;
};

// ── Win flash ────────────────────────────────────────────────
struct WinFlash {
    bool  active  = false;
    float timer   = 0.f;
};

// ── Transposition table entry ────────────────────────────────
struct TTEntry { int score; int depth; };

// ── Game logic ───────────────────────────────────────────────
struct Game {
    Player     grid[ROWS][COLS] = {};
    Player     current  = RED;
    GameMode   mode     = MENU;
    GameState  state    = PLAYING;
    int        scores[3]= {0,0,0};
    std::vector<std::pair<int,int>> winCells;
    int        hoverCol = -1;
    int        moveCount= 0;

    // column heights (for fast drop & eval)
    int heights[COLS] = {};

    void reset() {
        for (int r=0;r<ROWS;r++) for (int c=0;c<COLS;c++) grid[r][c]=NONE;
        for (int c=0;c<COLS;c++) heights[c]=0;
        current=RED; state=PLAYING; winCells.clear(); hoverCol=-1; moveCount=0;
    }

    int getRow(int col) const {
        if (heights[col]>=ROWS) return -1;
        return ROWS-1-heights[col];
    }

    bool checkWin(int row, int col, std::vector<std::pair<int,int>>& cells) const {
        Player p = grid[row][col];
        if (p==NONE) return false;
        const int dirs[4][2]={{0,1},{1,0},{1,1},{1,-1}};
        for (auto& d:dirs) {
            std::vector<std::pair<int,int>> line;
            line.push_back({row,col});
            for (int i=1;i<4;i++){int r=row+d[0]*i,c=col+d[1]*i;if(r>=0&&r<ROWS&&c>=0&&c<COLS&&grid[r][c]==p)line.push_back({r,c});else break;}
            for (int i=1;i<4;i++){int r=row-d[0]*i,c=col-d[1]*i;if(r>=0&&r<ROWS&&c>=0&&c<COLS&&grid[r][c]==p)line.push_back({r,c});else break;}
            if((int)line.size()>=4){cells=line;return true;}
        }
        return false;
    }

    bool isDraw() const { return moveCount==ROWS*COLS; }

    // returns true if drop succeeded
    bool drop(int col) {
        int row=getRow(col);
        if(row==-1) return false;
        grid[row][col]=current;
        heights[col]++;
        moveCount++;
        if(checkWin(row,col,winCells)){state=WIN;scores[current]++;}
        else if(isDraw()){state=DRAW;}
        else{current=(current==RED)?YELLOW:RED;}
        return true;
    }

    // ── AI ────────────────────────────────────────────────────
    static const int ORDER[7];

    int scoreWindow(Player a,Player b,Player c,Player d,Player p) const {
        Player o=(p==RED)?YELLOW:RED;
        int pc=0,ec=0,oc=0;
        Player w[4]={a,b,c,d};
        for(auto x:w){if(x==p)pc++;else if(x==NONE)ec++;else oc++;}
        if(pc==4)return 1000;
        if(pc==3&&ec==1)return 6;
        if(pc==2&&ec==2)return 2;
        if(oc==3&&ec==1)return -4;
        if(oc==4)return -1000;
        return 0;
    }

    int evalBoard(Player p) const {
        int s=0;
        // center column
        for(int r=0;r<ROWS;r++) if(grid[r][3]==p) s+=4;
        // horizontal
        for(int r=0;r<ROWS;r++) for(int c=0;c<=COLS-4;c++) s+=scoreWindow(grid[r][c],grid[r][c+1],grid[r][c+2],grid[r][c+3],p);
        // vertical
        for(int c=0;c<COLS;c++) for(int r=0;r<=ROWS-4;r++) s+=scoreWindow(grid[r][c],grid[r+1][c],grid[r+2][c],grid[r+3][c],p);
        // diag
        for(int r=0;r<=ROWS-4;r++) for(int c=0;c<=COLS-4;c++) s+=scoreWindow(grid[r][c],grid[r+1][c+1],grid[r+2][c+2],grid[r+3][c+3],p);
        for(int r=3;r<ROWS;r++)    for(int c=0;c<=COLS-4;c++) s+=scoreWindow(grid[r][c],grid[r-1][c+1],grid[r-2][c+2],grid[r-3][c+3],p);
        return s;
    }

    bool isTerminalQuick() const {
        // check win for last piece: scan all
        for(int r=0;r<ROWS;r++) for(int c=0;c<COLS;c++) if(grid[r][c]!=NONE){
            std::vector<std::pair<int,int>> tmp; if(checkWin(r,c,tmp)) return true;
        }
        return isDraw();
    }

    int minimax(int depth,bool isMax,int alpha,int beta) {
        if(isTerminalQuick()||depth==0) return evalBoard(YELLOW)-evalBoard(RED);
        if(isMax){
            int best=INT_MIN;
            for(int ci=0;ci<7;ci++){
                int c=ORDER[ci],r=getRow(c);
                if(r==-1) continue;
                grid[r][c]=YELLOW; heights[c]++; moveCount++;
                best=std::max(best,minimax(depth-1,false,alpha,beta));
                grid[r][c]=NONE; heights[c]--; moveCount--;
                alpha=std::max(alpha,best);
                if(beta<=alpha) break;
            }
            return best;
        } else {
            int best=INT_MAX;
            for(int ci=0;ci<7;ci++){
                int c=ORDER[ci],r=getRow(c);
                if(r==-1) continue;
                grid[r][c]=RED; heights[c]++; moveCount++;
                best=std::min(best,minimax(depth-1,true,alpha,beta));
                grid[r][c]=NONE; heights[c]--; moveCount--;
                beta=std::min(beta,best);
                if(beta<=alpha) break;
            }
            return best;
        }
    }

    int bestMove() {
        // immediate win
        for(int ci=0;ci<7;ci++){int c=ORDER[ci],r=getRow(c);if(r==-1)continue;grid[r][c]=YELLOW;heights[c]++;moveCount++;std::vector<std::pair<int,int>>tmp;bool w=checkWin(r,c,tmp);grid[r][c]=NONE;heights[c]--;moveCount--;if(w)return c;}
        // block
        for(int ci=0;ci<7;ci++){int c=ORDER[ci],r=getRow(c);if(r==-1)continue;grid[r][c]=RED;heights[c]++;moveCount++;std::vector<std::pair<int,int>>tmp;bool w=checkWin(r,c,tmp);grid[r][c]=NONE;heights[c]--;moveCount--;if(w)return c;}
        int best=INT_MIN,bestCol=3;
        for(int ci=0;ci<7;ci++){
            int c=ORDER[ci],r=getRow(c);
            if(r==-1) continue;
            grid[r][c]=YELLOW; heights[c]++; moveCount++;
            int sc=minimax(AI_DEPTH,false,INT_MIN,INT_MAX);
            grid[r][c]=NONE; heights[c]--; moveCount--;
            if(sc>best){best=sc;bestCol=c;}
        }
        return bestCol;
    }
};
const int Game::ORDER[7]={3,2,4,1,5,0,6};

// ── Drawing helpers ──────────────────────────────────────────
void fillCircle(sf::RenderTarget& t,float cx,float cy,float r,sf::Color col){
    sf::CircleShape s(r); s.setFillColor(col); s.setPosition(cx-r,cy-r); t.draw(s);
}

void fillRect(sf::RenderTarget& t,float x,float y,float w,float h,sf::Color col,float rx=0){
    if(rx<=0){sf::RectangleShape s({w,h});s.setFillColor(col);s.setPosition(x,y);t.draw(s);return;}
    // rounded rect via 3 rects + 4 circles
    sf::RectangleShape m({w,h-2*rx}); m.setFillColor(col); m.setPosition(x,y+rx); t.draw(m);
    sf::RectangleShape tp({w-2*rx,rx*2}); tp.setFillColor(col); tp.setPosition(x+rx,y); t.draw(tp);
    sf::RectangleShape bt({w-2*rx,rx*2}); bt.setFillColor(col); bt.setPosition(x+rx,y+h-2*rx); t.draw(bt);
    float cxs[]={x+rx,x+w-rx,x+rx,x+w-rx};
    float cys[]={y+rx,y+rx,y+h-rx,y+h-rx};
    for(int i=0;i<4;i++) fillCircle(t,cxs[i],cys[i],rx,col);
}

void drawText(sf::RenderTarget& t,const sf::Font& f,const std::string& s,unsigned sz,float x,float y,sf::Color col,bool centered=false,bool bold=false){
    sf::Text tx(s,f,sz);
    tx.setFillColor(col);
    if(bold) tx.setStyle(sf::Text::Bold);
    if(centered){auto b=tx.getLocalBounds();tx.setPosition(x-b.width/2.f,y-b.height/2.f-b.top);}
    else tx.setPosition(x,y);
    t.draw(tx);
}

// ── Button helper ────────────────────────────────────────────
bool btnHit(float mx,float my,float x,float y,float w,float h){return mx>=x&&mx<=x+w&&my>=y&&my<=y+h;}

// ── Main ─────────────────────────────────────────────────────
int main(){
    sf::RenderWindow window(sf::VideoMode(WIN_W,WIN_H),"Connect 4",sf::Style::Titlebar|sf::Style::Close);
    window.setFramerateLimit((int)FPS);

    sf::Font font;
    bool fontOk=font.loadFromFile("C:/Windows/Fonts/segoeui.ttf");
    if(!fontOk) fontOk=font.loadFromFile("C:/Windows/Fonts/arial.ttf");
    if(!fontOk) fontOk=font.loadFromFile("C:/Windows/Fonts/calibri.ttf");

    Game game;
    DropAnim anim;
    WinFlash flash;
    bool aiPending=false;
    sf::Clock aiClock, flashClock, frameClock;

    // Pre-build cell shapes for rendering (reuse each frame)
    sf::CircleShape cellShape(RADIUS);
    sf::CircleShape ringShape(RADIUS+6);

    auto startAI=[&](){aiPending=true;aiClock.restart();};

    while(window.isOpen()){
        float dt=frameClock.restart().asSeconds();

        // ── Events ───────────────────────────────────────────
        sf::Event ev;
        while(window.pollEvent(ev)){
            if(ev.type==sf::Event::Closed) window.close();

            if(ev.type==sf::Event::MouseMoved && game.mode!=MENU)
                game.hoverCol=std::max(0,std::min(COLS-1,ev.mouseMove.x/CELL));

            if(ev.type==sf::Event::MouseButtonPressed && ev.mouseButton.button==sf::Mouse::Left){
                float mx=(float)ev.mouseButton.x, my=(float)ev.mouseButton.y;

                if(game.mode==MENU){
                    // PvP
                    if(btnHit(mx,my,WIN_W/2-165,WIN_H/2-35,150,60)){game.mode=PVP;game.reset();}
                    // PvC
                    if(btnHit(mx,my,WIN_W/2+15,WIN_H/2-35,150,60)){game.mode=PVC;game.reset();}
                } else {
                    // Menu btn
                    if(btnHit(mx,my,10,10,90,34)&&!anim.active){game.mode=MENU;game.reset();aiPending=false;anim.active=false;}

                    bool aiTurn=(game.mode==PVC&&game.current==YELLOW);
                    if(game.state==PLAYING&&!anim.active&&!aiTurn){
                        int col=(int)mx/CELL;
                        if(col>=0&&col<COLS){
                            int row=game.getRow(col);
                            if(row!=-1){
                                // start drop animation
                                anim.active=true;
                                anim.col=col;
                                anim.row=row;
                                anim.player=game.current;
                                anim.y=-(float)RADIUS*2;
                                anim.targetY=(float)(row*CELL+CELL/2);
                            }
                        }
                    }

                    // New game
                    if(game.state!=PLAYING&&!anim.active){
                        if(btnHit(mx,my,WIN_W/2-90,PANEL-55,180,38)){game.reset();aiPending=false;flash.active=false;}
                    }
                }
            }

            if(ev.type==sf::Event::KeyPressed){
                if(ev.key.code==sf::Keyboard::R&&game.mode!=MENU&&!anim.active){game.reset();aiPending=false;flash.active=false;}
                if(ev.key.code==sf::Keyboard::Escape&&game.mode!=MENU&&!anim.active){game.mode=MENU;game.reset();aiPending=false;}
            }
        }

        // ── Update drop animation ────────────────────────────
        if(anim.active){
            float speed=DROP_SPD*(1.f+anim.y/200.f+2.f); // accelerate
            anim.y+=speed;
            if(anim.y>=anim.targetY){
                anim.y=anim.targetY;
                anim.active=false;
                // commit move
                game.drop(anim.col);
                if(game.state==WIN){flash.active=true;flashClock.restart();}
                if(game.mode==PVC&&game.state==PLAYING&&game.current==YELLOW) startAI();
            }
        }

        // ── AI move ──────────────────────────────────────────
        if(aiPending&&!anim.active&&game.state==PLAYING&&aiClock.getElapsedTime().asSeconds()>0.35f){
            aiPending=false;
            int col=game.bestMove();
            int row=game.getRow(col);
            if(row!=-1){
                anim.active=true; anim.col=col; anim.row=row;
                anim.player=YELLOW; anim.y=-(float)RADIUS*2;
                anim.targetY=(float)(row*CELL+CELL/2);
            }
        }

        // ── Win flash timer ──────────────────────────────────
        if(flash.active) flash.timer=flashClock.getElapsedTime().asSeconds();

        // ── Draw ─────────────────────────────────────────────
        window.clear(C_BG);

        if(game.mode==MENU){
            // decorative discs background
            for(int i=0;i<7;i++){
                fillCircle(window,(float)(i*CELL+CELL/2),(float)(WIN_H-CELL/2),RADIUS+4,sf::Color(30,30,50));
            }

            // title
            if(fontOk){
                drawText(window,font,"CONNECT",56,WIN_W/2.f-10,WIN_H/2.f-190,C_WHITE,true,true);
                drawText(window,font,"4",56,WIN_W/2.f+82,WIN_H/2.f-190,C_RED,false,true);
                drawText(window,font,"Choose a mode to play",16,WIN_W/2.f,WIN_H/2.f-120,C_MUTED,true);
            }

            // disc icons above buttons
            fillCircle(window,WIN_W/2.f-90,WIN_H/2.f-65,22,C_RED);
            fillCircle(window,WIN_W/2.f+90,WIN_H/2.f-65,22,C_YELLOW);

            // PvP button
            fillRect(window,WIN_W/2.f-165,WIN_H/2.f-35,150,60,sf::Color(60,30,30),12);
            fillRect(window,WIN_W/2.f-165,WIN_H/2.f-35,150,60,sf::Color(C_RED.r,C_RED.g,C_RED.b,200),12);
            if(fontOk){
                drawText(window,font,"Player vs Player",14,WIN_W/2.f-90,WIN_H/2.f-10,C_WHITE,true);
            }

            // PvC button
            fillRect(window,WIN_W/2.f+15,WIN_H/2.f-35,150,60,sf::Color(20,50,90),12);
            fillRect(window,WIN_W/2.f+15,WIN_H/2.f-35,150,60,sf::Color(24,95,165,220),12);
            if(fontOk){
                drawText(window,font,"Player vs CPU",14,WIN_W/2.f+90,WIN_H/2.f-10,C_WHITE,true);
            }

            if(fontOk) drawText(window,font,"Press R to restart  |  ESC for menu",13,WIN_W/2.f,WIN_H-30,C_MUTED,true);

        } else {
            // ── Top panel ────────────────────────────────────
            fillRect(window,0,0,WIN_W,PANEL,C_PANEL);

            // Menu button
            fillRect(window,10,10,90,32,C_BTN1,7);
            if(fontOk) drawText(window,font,"< Menu",13,55,26,sf::Color(170,170,200),true);

            // Mode badge
            std::string modeStr=(game.mode==PVP)?"Player vs Player":"Player vs CPU";
            if(fontOk) drawText(window,font,modeStr,13,WIN_W/2.f,16,C_ACCENT,true);

            // Scores
            float sy=50;
            fillRect(window,WIN_W/2.f-160,sy-2,140,34,C_BTN1,8);
            fillRect(window,WIN_W/2.f+20, sy-2,140,34,C_BTN1,8);
            fillCircle(window,WIN_W/2.f-148,sy+14,7,C_RED);
            std::string p1=(game.mode==PVC)?"You":"Red";
            std::string p2=(game.mode==PVP)?"Yellow":"CPU";
            if(fontOk){
                drawText(window,font,p1+"  "+std::to_string(game.scores[1]),14,WIN_W/2.f-136,sy+4,C_WHITE);
                drawText(window,font,"vs",12,WIN_W/2.f-12,sy+6,C_MUTED,true);
                drawText(window,font,p2+"  "+std::to_string(game.scores[2]),14,WIN_W/2.f+32,sy+4,C_WHITE);
            }
            fillCircle(window,WIN_W/2.f+148,sy+14,7,C_YELLOW);

            // Status bar
            std::string statusStr; sf::Color statusCol=C_WHITE;
            if(game.state==PLAYING){
                bool aiTurn=(game.mode==PVC&&game.current==YELLOW);
                if(aiTurn){statusStr="CPU is thinking...";statusCol=C_YELLOW;}
                else if(game.current==RED){statusStr=(game.mode==PVC)?"Your turn  (Red)":"Red's turn";statusCol=C_RED;}
                else{statusStr="Yellow's turn";statusCol=C_YELLOW;}
            } else if(game.state==WIN){
                if(game.mode==PVC) statusStr=(game.current==RED)?"You win! 🎉":"CPU wins!";
                else statusStr=(game.current==RED)?"Red wins!":"Yellow wins!";
                statusCol=(game.current==RED)?C_RED:C_YELLOW;
            } else {statusStr="It's a draw!";statusCol=C_ACCENT;}

            fillRect(window,WIN_W/2.f-130,90,260,36,C_BTN2,9);
            if(fontOk) drawText(window,font,statusStr,15,WIN_W/2.f,108,statusCol,true);

            // New game button
            if(game.state!=PLAYING&&!anim.active){
                fillRect(window,WIN_W/2.f-90,PANEL-54,180,38,sf::Color(35,95,55),9);
                if(fontOk) drawText(window,font,"New Game  (R)",14,WIN_W/2.f,PANEL-35,C_WHITE,true);
            }

            // Hover arrow indicator
            if(game.state==PLAYING&&game.hoverCol>=0&&!anim.active){
                bool aiTurn=(game.mode==PVC&&game.current==YELLOW);
                if(!aiTurn){
                    sf::Color hc=(game.current==RED)?C_RED:C_YELLOW;
                    hc.a=200;
                    float hx=(float)(game.hoverCol*CELL+CELL/2);
                    // triangle arrow
                    sf::ConvexShape tri(3);
                    tri.setPoint(0,{hx-10,PANEL-28});
                    tri.setPoint(1,{hx+10,PANEL-28});
                    tri.setPoint(2,{hx,   PANEL-12});
                    tri.setFillColor(hc);
                    window.draw(tri);
                }
            }

            // ── Board ────────────────────────────────────────
            fillRect(window,0,(float)PANEL,(float)WIN_W,(float)(ROWS*CELL),C_BOARD,14);

            // column hover highlight
            if(game.state==PLAYING&&game.hoverCol>=0&&!anim.active){
                bool aiTurn=(game.mode==PVC&&game.current==YELLOW);
                if(!aiTurn){
                    sf::RectangleShape colHL({(float)CELL,(float)(ROWS*CELL)});
                    colHL.setFillColor(sf::Color(255,255,255,18));
                    colHL.setPosition((float)(game.hoverCol*CELL),(float)PANEL);
                    window.draw(colHL);
                }
            }

            // cells
            bool flashOn=(flash.active&&(int)(flash.timer*5)%2==0);
            for(int r=0;r<ROWS;r++){
                for(int c=0;c<COLS;c++){
                    float cx=(float)(c*CELL+CELL/2);
                    float cy=(float)(PANEL+r*CELL+CELL/2);

                    bool isWin=false;
                    for(auto& wc:game.winCells) if(wc.first==r&&wc.second==c){isWin=true;break;}

                    // win ring flash
                    if(isWin&&flash.active){
                        sf::Color rc=flashOn?C_WHITE:sf::Color(255,255,255,80);
                        fillCircle(window,cx,cy,(float)(RADIUS+7),rc);
                    }

                    // skip cell if animating piece here
                    if(anim.active&&anim.col==c&&anim.row==r){
                        fillCircle(window,cx,cy,(float)RADIUS,C_EMPTY);
                        continue;
                    }

                    sf::Color fc=C_EMPTY;
                    if(game.grid[r][c]==RED) fc=C_RED;
                    else if(game.grid[r][c]==YELLOW) fc=C_YELLOW;
                    else if(c==game.hoverCol&&game.state==PLAYING){
                        bool aiTurn=(game.mode==PVC&&game.current==YELLOW);
                        if(!aiTurn&&!anim.active) fc=C_HOVER;
                    }

                    // dim non-win cells after game over
                    if(game.state!=PLAYING&&!isWin&&game.grid[r][c]!=NONE){
                        fc.r=(sf::Uint8)(fc.r*0.5f);fc.g=(sf::Uint8)(fc.g*0.5f);fc.b=(sf::Uint8)(fc.b*0.5f);
                    }

                    fillCircle(window,cx,cy,(float)RADIUS,fc);
                }
            }

            // draw falling disc animation
            if(anim.active){
                float cx=(float)(anim.col*CELL+CELL/2);
                float cy=(float)PANEL+anim.y;
                sf::Color dc=(anim.player==RED)?C_RED:C_YELLOW;
                // shadow
                fillCircle(window,cx,cy+4,(float)RADIUS,sf::Color(0,0,0,60));
                fillCircle(window,cx,cy,(float)RADIUS,dc);
                // sheen
                fillCircle(window,cx-10,cy-10,10,sf::Color(255,255,255,50));
            }

            // keyboard hint
            if(fontOk) drawText(window,font,"R = restart   ESC = menu",11,(float)(WIN_W/2),WIN_H-16,sf::Color(60,60,80),true);
        }

        window.display();
    }
    return 0;
}