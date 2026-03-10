/*
 * ═══════════════════════════════════════════════════════════════════════
 *   OLED Robot Face  v3.1  —  35 Modes  (FIXED)
 *   Board   : ESP32
 *   Display : SSD1306  128×64  I2C
 *   Button  : GPIO 13  (INPUT_PULLUP)
 *   SDA     : GPIO 21     SCL : GPIO 22
 * ═══════════════════════════════════════════════════════════════════════
 *  ⚙️  USER CONFIG — এখানে WiFi দাও
 *    Bangladesh = UTC+6 → GMT_OFFSET = 21600
 * ═══════════════════════════════════════════════════════════════════════
 *  🎮  BUTTON CONTROLS
 *    DOUBLE (<350 ms)  → *** ALWAYS Next Mode *** (সব mode এ কাজ করে)
 *    SHORT  (<500 ms)  → In-game action  (Snake:TurnLeft / Pong:Down / Breakout:Right / Space:Right)
 *                         Non-game mode  → Next Mode
 *    LONG   (≥500 ms)  → Primary action  (Start/Stop / TurnRight / Fire / Roll / Restart when dead)
 * ═══════════════════════════════════════════════════════════════════════
 */

// ─── USER CONFIG ──────────────────────────────────────────────────────
#define WIFI_SSID   "Me.."
#define WIFI_PASS   "mehedi113"
#define GMT_OFFSET  21600   // UTC+6 Bangladesh
#define DST_OFFSET  0

// ─── LIBRARIES ────────────────────────────────────────────────────────
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>

// ─── HARDWARE ─────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define BUTTON_PIN     13
#define TOTAL_MODES    35

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ═══════════════════════════════════════════════════════════════════════
//  *** BUTTON ENUM — MUST BE FIRST, BEFORE ALL FUNCTIONS ***
// ═══════════════════════════════════════════════════════════════════════
enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG, BTN_DOUBLE };

// Button state variables
unsigned long btnPressTime   = 0;
unsigned long btnReleaseTime = 0;
bool          btnHeld        = false;
bool          btnWaitDouble  = false;
bool          longFired      = false;
int           lastRawBtn     = HIGH;

BtnEvent readButton() {
  int raw = digitalRead(BUTTON_PIN);
  BtnEvent evt = BTN_NONE;
  unsigned long now = millis();

  if (raw == LOW && lastRawBtn == HIGH) {
    btnPressTime = now; btnHeld = true; longFired = false;
  }
  if (raw == LOW && btnHeld && !longFired && (now - btnPressTime >= 500)) {
    longFired = true; evt = BTN_LONG;
  }
  if (raw == HIGH && lastRawBtn == LOW) {
    btnHeld = false;
    if (!longFired) {
      if (btnWaitDouble && (now - btnReleaseTime < 350)) {
        btnWaitDouble = false; evt = BTN_DOUBLE;
      } else {
        btnWaitDouble = true; btnReleaseTime = now;
      }
    }
  }
  if (btnWaitDouble && (now - btnReleaseTime >= 350) && raw == HIGH) {
    btnWaitDouble = false;
    if (evt == BTN_NONE) evt = BTN_SHORT;
  }
  lastRawBtn = raw;
  return evt;
}

// ═══════════════════════════════════════════════════════════════════════
//  NTP REAL-TIME CLOCK
// ═══════════════════════════════════════════════════════════════════════
bool          ntpReady  = false;
unsigned long clockBase = 0;
const int FB_H = 10, FB_M = 0, FB_S = 0;

void setupWiFiClock() {
  display.clearDisplay();
  display.setTextColor(WHITE); display.setTextSize(1);
  display.setCursor(10, 16); display.print("Connecting WiFi...");
  display.setCursor(10, 28); display.print(WIFI_SSID);
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); tries++;
    display.print(".");
    display.display();
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(GMT_OFFSET, DST_OFFSET, "pool.ntp.org", "time.google.com");
    struct tm ti;
    int syncTries = 0;
    while (!getLocalTime(&ti) && syncTries < 10) { delay(1000); syncTries++; }
    ntpReady = (syncTries < 10);
  }

  display.clearDisplay();
  display.setCursor(20, 25);
  if (ntpReady) { display.print("NTP Synced!"); }
  else          { display.print("WiFi Failed"); display.setCursor(10,38); display.print("Internal clock used"); }
  display.display();
  delay(1200);
}

void getRealTime(int &h, int &m, int &s, int &wd, int &day, int &mon, int &yr) {
  if (ntpReady) {
    struct tm ti;
    if (getLocalTime(&ti)) {
      h=ti.tm_hour; m=ti.tm_min; s=ti.tm_sec;
      wd=ti.tm_wday; day=ti.tm_mday; mon=ti.tm_mon+1; yr=ti.tm_year+1900;
      return;
    }
  }
  long tot = FB_H*3600L + FB_M*60L + FB_S + (long)((millis()-clockBase)/1000UL);
  s=tot%60; m=(tot/60)%60; h=(tot/3600)%24;
  wd=0; day=1; mon=1; yr=2025;
}

void getTime(int &h, int &m, int &s) {
  int wd,d,mo,yr; getRealTime(h,m,s,wd,d,mo,yr);
}

const char* wdNames[]  = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char* monNames[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

// ═══════════════════════════════════════════════════════════════════════
//  GLOBAL ANIMATION STATE
// ═══════════════════════════════════════════════════════════════════════

// Mode index
int mode = 0;

// ── Original animations ──
float bx=64,by=32,bvx=2.5f,bvy=1.8f;
int   loadPct=0;
int   matDrop[16];
float radarAngle=0;
int   blipX=70,blipY=25;
int   battLevel=100;
unsigned long lastDrain=0;

// ── Oscilloscope (defined ONCE here) ──
float oscP1=0, oscP2=0;

// ── Stopwatch ──
bool          swRunning=false;
unsigned long swStarted=0, swElapsed=0, swLap=0;
bool          swHasLap=false;

// ── Countdown ──
unsigned long cdTotal=180000UL, cdRemain=180000UL, cdStart=0;
bool          cdRunning=false, cdDone=false;

// ── Snake ──
#define CELL 4
#define GW   (128/CELL)
#define GH   (64/CELL)
#define MAXLEN 80
struct SnakeCell { int x,y; };
SnakeCell snake[MAXLEN];
int snakeLen=3, sDX=1, sDY=0, snakeScore=0;
SnakeCell food;
bool snakeDead=false;
unsigned long snakeLast=0;

// ── Pong ──
float pgBx=64,pgBy=32,pgVx=2.5f,pgVy=1.5f;
int   pgPadY=24, pgAIY=24, pgScore=0, pgAI=0;
const int PAD_H=16, PAD_W=3;
unsigned long pongLast=0;

// ── Fireworks ──
struct Particle { float x,y,vx,vy; bool alive; int life; };
#define MAX_P 45
Particle parts[MAX_P];
unsigned long fwLast=0;

// ── Starfield ──
struct Star { float x,y,z; };
#define NUM_STARS 55
Star stars[NUM_STARS];

// ── Spectrum ──
int   specBar[16];
float specVel[16];
unsigned long specLast=0;

// ── Dice ──
int  diceVal=1;
bool diceRolling=false;
unsigned long diceRollEnd=0;

// ── Marquee ──
const char MARQUEE_TEXT[] = "  *** Me. Robot *** ESP32 *** BANGLADESH ***  ";
int marqueeX=128;
unsigned long marqueeLast=0;

// ── Pac-Man ──
float pacX=10;
int   pacDir=1, pacMouth=0, pacMouthDir=1;
float ghostX=100;
bool  ghostScared=false;
bool  pacDots[16];

// ── Breakout ──
float brBx=64,brBy=45,brVx=2.0f,brVy=-2.0f;
int   brPadX=50, brScore=0;
bool  brDead=false, brWin=false;
const int BR_ROWS=3, BR_COLS=8;
bool  bricks[BR_ROWS][BR_COLS];

// ── Heart Rate ──
int   hrBuf[128];
int   hrHead=0, hrBPM=72;
unsigned long hrLast=0;

// ── Thermometer ──
float thermo=22.0f, thermoTarget=22.0f;
unsigned long thermoLast=0;

// ── Compass ──
float compassAngle=0, compassTarget=0;
unsigned long compassLast=0;

// ── Plasma ──
float plasmaT=0;

// ── Rain ──
struct Drop { float x,y,spd; bool alive; };
#define MAX_DROPS 20
Drop drops[MAX_DROPS];
unsigned long rainLast=0;

// ── Candle ──
float flameH=20, flameW=10;
unsigned long flameLast=0;

// ── Space Shooter ──
int  spShipX=64, spScore=0, spLives=3, spEDir=1;
bool spGameOver=false;
struct Bullet  { int x,y; bool alive; };
struct Enemy   { int x,y; bool alive; };
#define MAX_BULLETS 5
#define MAX_ENEMIES 8
Bullet  spBullets[MAX_BULLETS];
Enemy   spEnemies[MAX_ENEMIES];
unsigned long spMoveLast=0, spBulletLast=0;

// ═══════════════════════════════════════════════════════════════════════
//  HELPER
// ═══════════════════════════════════════════════════════════════════════
void drawHeart(int cx, int cy, int r) {
  display.fillCircle(cx-r/2, cy-1, r/2+1, WHITE);
  display.fillCircle(cx+r/2, cy-1, r/2+1, WHITE);
  display.fillTriangle(cx-r, cy+1, cx+r, cy+1, cx, cy+r+2, WHITE);
}

// ═══════════════════════════════════════════════════════════════════════
//  INIT HELPERS
// ═══════════════════════════════════════════════════════════════════════
void placeFood() {
  bool ok;
  do {
    ok=true; food={random(1,GW-1),random(1,GH-1)};
    for(int i=0;i<snakeLen;i++) if(snake[i].x==food.x&&snake[i].y==food.y){ok=false;break;}
  } while(!ok);
}
void snakeInit(){ snakeLen=3;sDX=1;sDY=0;snakeDead=false;snakeScore=0;
  snake[0]={8,8};snake[1]={7,8};snake[2]={6,8};placeFood(); }

void pongInit(){ pgBx=64;pgBy=32;pgVx=2.5f;pgVy=1.5f;pgPadY=24;pgAIY=24;pgScore=0;pgAI=0; }

void breakoutInit(){
  brBx=64;brBy=45;brVx=2.0f;brVy=-2.0f;brPadX=50;brScore=0;brDead=false;brWin=false;
  for(int r=0;r<BR_ROWS;r++) for(int c=0;c<BR_COLS;c++) bricks[r][c]=true;
}

void pacInit(){
  pacX=10;pacDir=1;ghostX=100;ghostScared=false;
  for(int i=0;i<16;i++) pacDots[i]=true;
}

void spaceInit(){
  spShipX=64;spScore=0;spLives=3;spEDir=1;spGameOver=false;
  for(int i=0;i<MAX_BULLETS;i++) spBullets[i].alive=false;
  for(int i=0;i<MAX_ENEMIES;i++) spEnemies[i]={10+i*14,8,true};
}

void fireParticle(int cx, int cy){
  for(int i=0;i<MAX_P;i++){
    float a=random(0,628)/100.0f, spd=random(10,40)/10.0f;
    parts[i]={(float)cx,(float)cy,spd*cos(a),spd*sin(a),true,random(8,22)};
  }
}

void starsInit(){
  for(int i=0;i<NUM_STARS;i++)
    stars[i]={(float)random(-640,640),(float)random(-320,320),(float)random(1,128)};
}

void specInit(){ for(int i=0;i<16;i++){specBar[i]=0;specVel[i]=0;} }

void rainInit(){
  for(int i=0;i<MAX_DROPS;i++)
    drops[i]={random(0,128)*1.0f,(float)random(-60,0),random(15,45)/10.0f,true};
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE ACTION HANDLERS  (use BtnEvent — declared above)
// ═══════════════════════════════════════════════════════════════════════
void swAction(BtnEvent e) {
  if(e==BTN_LONG){
    if(swRunning){swElapsed+=millis()-swStarted;swRunning=false;}
    else{swStarted=millis();swRunning=true;}
  }
  // SHORT = Lap while running, Reset while stopped
  if(e==BTN_SHORT){
    if(swRunning){swLap=swElapsed+millis()-swStarted;swHasLap=true;}
    else{swElapsed=0;swHasLap=false;}
  }
}

void cdAction(BtnEvent e) {
  if(e==BTN_LONG){
    if(!cdDone){
      if(cdRunning){cdRemain-=millis()-cdStart;cdRunning=false;}
      else{cdStart=millis();cdRunning=true;cdDone=false;}
    }
  }
  // SHORT = Reset
  if(e==BTN_SHORT){cdRemain=cdTotal;cdRunning=false;cdDone=false;}
}

void snakeAction(BtnEvent e){
  // LONG when dead = restart
  if(e==BTN_LONG && snakeDead){snakeInit();return;}
  if(e==BTN_LONG  && !snakeDead){int nx=sDY,ny=-sDX;sDX=nx;sDY=ny;}   // Turn right
  if(e==BTN_SHORT && !snakeDead){int nx=-sDY,ny=sDX;sDX=nx;sDY=ny;}  // Turn left
}

void pongAction(BtnEvent e){
  if(e==BTN_LONG)  pgPadY=constrain(pgPadY-8,0,48);   // Up
  if(e==BTN_SHORT) pgPadY=constrain(pgPadY+8,0,48);   // Down
}

void diceAction(BtnEvent e){
  if(e==BTN_LONG){diceRolling=true;diceRollEnd=millis()+900;}
}

void breakoutAction(BtnEvent e){
  if(e==BTN_LONG  && (brDead||brWin)){breakoutInit();return;}  // Restart when game over
  if(e==BTN_LONG)  brPadX=constrain(brPadX-8,0,100);   // Paddle left
  if(e==BTN_SHORT) brPadX=constrain(brPadX+8,0,100);   // Paddle right
}

void spaceAction(BtnEvent e){
  if(e==BTN_LONG && spGameOver){spaceInit();return;}   // Restart when game over
  if(spGameOver)return;
  if(e==BTN_LONG){   // Fire
    for(int i=0;i<MAX_BULLETS;i++) if(!spBullets[i].alive){spBullets[i]={spShipX,50,true};break;}
  }
  if(e==BTN_SHORT) spShipX=constrain(spShipX+6,6,122);   // Move right
  // No left move needed — ship auto-centers, use long-hold pattern or add 2nd button
}

// ═══════════════════════════════════════════════════════════════════════
//  DRAW FUNCTIONS — MODES 0–14 (original)
// ═══════════════════════════════════════════════════════════════════════
void normalEyes(){
  display.clearDisplay();
  display.fillCircle(38,32,15,WHITE);display.fillCircle(90,32,15,WHITE);
  display.fillCircle(41,29,6,BLACK); display.fillCircle(93,29,6,BLACK);
  display.fillCircle(43,27,2,WHITE); display.fillCircle(95,27,2,WHITE);
  display.display();
}
void blinkingEyes(){
  display.clearDisplay();
  display.fillCircle(38,32,15,WHITE);display.fillCircle(90,32,15,WHITE);
  display.fillCircle(41,29,6,BLACK); display.fillCircle(93,29,6,BLACK);
  display.fillCircle(43,27,2,WHITE); display.fillCircle(95,27,2,WHITE);
  display.display();delay(1000);
  display.clearDisplay();
  display.fillRoundRect(23,30,30,5,2,WHITE);display.fillRoundRect(75,30,30,5,2,WHITE);
  display.display();delay(120);
}
void happyEyes(){
  display.clearDisplay();
  display.fillCircle(38,24,14,WHITE);display.fillRect(24,10,28,14,BLACK);
  display.fillCircle(90,24,14,WHITE);display.fillRect(76,10,28,14,BLACK);
  for(int dx=-4;dx<=4;dx+=2){display.drawPixel(25+dx,42,WHITE);display.drawPixel(103+dx,42,WHITE);}
  display.display();
}
void digitalClock(){
  int h,m,s,wd,d,mo,yr; getRealTime(h,m,s,wd,d,mo,yr);
  display.clearDisplay();
  display.fillRect(0,0,128,12,WHITE);
  display.setTextColor(BLACK);display.setTextSize(1);
  display.setCursor(22,2);display.print("DIGITAL CLOCK");
  display.fillCircle(120,6,3,ntpReady?BLACK:WHITE); // NTP dot
  display.setTextColor(WHITE);display.setTextSize(2);
  display.setCursor(8,18);
  if(h<10)display.print("0");display.print(h);display.print(":");
  if(m<10)display.print("0");display.print(m);display.print(":");
  if(s<10)display.print("0");display.print(s);
  display.setTextSize(1);display.setCursor(5,40);
  display.print(wdNames[wd]);display.print(" ");
  if(d<10)display.print("0");display.print(d);display.print("-");
  display.print(monNames[mo]);display.print("-");display.print(yr);
  display.setCursor(100,40);display.print(h<12?"AM":"PM");
  display.drawLine(0,52,128,52,WHITE);
  display.setCursor(ntpReady?22:5,56);
  display.display();
}
void analogClock(){
  int h,m,s;getTime(h,m,s);
  display.clearDisplay();
  const int cx=64,cy=32,R=29;
  display.drawCircle(cx,cy,R,WHITE);display.drawCircle(cx,cy,R-1,WHITE);
  for(int i=0;i<12;i++){
    float a=i*30.0f*PI/180.0f-PI/2.0f;
    int r1=(i%3==0)?R-6:R-4;
    display.drawLine(cx+r1*cos(a),cy+r1*sin(a),cx+R*cos(a),cy+R*sin(a),WHITE);
  }
  float ha=((h%12)*30.0f+m*0.5f)*PI/180.0f-PI/2.0f;
  float ma=m*6.0f*PI/180.0f-PI/2.0f;
  float sa=s*6.0f*PI/180.0f-PI/2.0f;
  display.drawLine(cx,cy,cx+13*cos(ha),cy+13*sin(ha),WHITE);
  display.drawLine(cx+1,cy,cx+13*cos(ha)+1,cy+13*sin(ha),WHITE);
  display.drawLine(cx,cy,cx+20*cos(ma),cy+20*sin(ma),WHITE);
  for(int d=0;d<24;d+=2) display.drawPixel(cx+(int)(d*cos(sa)),cy+(int)(d*sin(sa)),WHITE);
  display.fillCircle(cx,cy,3,WHITE);display.fillCircle(cx,cy,1,BLACK);
  display.display();
}
void angryEyes(){
  display.clearDisplay();
  display.fillCircle(38,36,14,WHITE);display.fillCircle(90,36,14,WHITE);
  display.fillCircle(36,34,5,BLACK); display.fillCircle(88,34,5,BLACK);
  display.fillTriangle(24,22,52,17,52,24,WHITE);
  display.fillTriangle(76,17,104,22,76,24,WHITE);
  display.display();
}
void sleepyEyes(){
  display.clearDisplay();
  display.fillCircle(38,38,14,WHITE);display.fillRect(24,22,28,16,BLACK);
  display.fillCircle(90,38,14,WHITE);display.fillRect(76,22,28,16,BLACK);
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(105,15);display.print("z");
  display.setTextSize(2);display.setCursor(108,6);display.print("Z");
  display.display();delay(400);
}
void heartEyes(){ display.clearDisplay();drawHeart(38,28,18);drawHeart(90,28,18);display.display(); }
void bouncingBall(){
  display.clearDisplay();
  bx+=bvx;by+=bvy;
  if(bx<=6||bx>=122)bvx=-bvx;
  if(by<=6||by>=58) bvy=-bvy;
  display.fillCircle((int)bx+2,(int)by+2,6,WHITE);
  display.fillCircle((int)bx,  (int)by,  6,WHITE);
  display.fillCircle((int)bx-2,(int)by-2,2,BLACK);
  display.drawLine((int)(bx-bvx*3),(int)(by-bvy*3),(int)bx,(int)by,WHITE);
  display.display();delay(25);
}
void loadingBar(){
  display.clearDisplay();
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(28,8);display.print("LOADING...");
  display.drawRoundRect(10,28,108,16,4,WHITE);
  display.fillRoundRect(12,30,map(loadPct,0,100,0,104),12,3,WHITE);
  display.setCursor(54,50);display.print(loadPct);display.print("%");
  display.display();
  if(++loadPct>100)loadPct=0;delay(40);
}
void matrixRain(){
  display.fillRect(0,0,128,8,BLACK);
  display.setTextColor(WHITE);display.setTextSize(1);
  for(int col=0;col<16;col++){
    display.setCursor(col*8,matDrop[col]*8);
    display.print((char)random(33,126));
    if(++matDrop[col]>8)matDrop[col]=0;
  }
  display.display();delay(90);
}
void sadEyes(){
  display.clearDisplay();
  display.fillCircle(38,34,14,WHITE);display.fillCircle(90,34,14,WHITE);
  display.fillCircle(38,38,5,BLACK); display.fillCircle(90,38,5,BLACK);
  display.drawLine(24,18,40,24,WHITE);display.drawLine(40,24,52,18,WHITE);
  display.drawLine(76,18,90,24,WHITE);display.drawLine(90,24,104,18,WHITE);
  display.fillCircle(33,50,3,WHITE);display.drawLine(33,48,33,55,WHITE);
  display.fillCircle(85,50,3,WHITE);display.drawLine(85,48,85,55,WHITE);
  display.display();
}
void radarScan(){
  const int cx=64,cy=32,R=28;
  display.clearDisplay();
  display.drawCircle(cx,cy,R,WHITE);display.drawCircle(cx,cy,R/2,WHITE);
  display.drawLine(cx-R,cy,cx+R,cy,WHITE);display.drawLine(cx,cy-R,cx,cy+R,WHITE);
  display.drawLine(cx,cy,(int)(cx+R*cos(radarAngle-PI/2.0f)),(int)(cy+R*sin(radarAngle-PI/2.0f)),WHITE);
  display.fillCircle(blipX,blipY,2,WHITE);
  radarAngle+=0.08f;
  if(radarAngle>=TWO_PI){
    radarAngle=0;
    do{blipX=cx+random(-R+4,R-4);blipY=cy+random(-R+4,R-4);}
    while(sqrt((float)(sq(blipX-cx)+sq(blipY-cy)))>R-3);
  }
  display.display();delay(25);
}
void batteryDisplay(){
  display.clearDisplay();
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(35,3);display.print("BATTERY");
  display.drawRoundRect(14,18,96,32,3,WHITE);
  display.fillRect(110,27,6,14,WHITE);
  int fw=map(battLevel,0,100,0,90);
  if(battLevel>20||(millis()/300)%2) display.fillRoundRect(16,20,fw,28,2,WHITE);
  display.setTextSize(2);
  display.setTextColor(battLevel>40?BLACK:WHITE);
  display.setCursor(38,26);
  if(battLevel<10)display.print(" ");
  display.print(battLevel);display.print("%");
  display.display();
  if(millis()-lastDrain>200){if(--battLevel<0)battLevel=100;lastDrain=millis();}
}
void surprisedEyes(){
  display.clearDisplay();
  display.drawCircle(38,30,17,WHITE);display.drawCircle(38,30,16,WHITE);
  display.fillCircle(38,30,8,WHITE); display.fillCircle(40,28,3,BLACK);
  display.drawCircle(90,30,17,WHITE);display.drawCircle(90,30,16,WHITE);
  display.fillCircle(90,30,8,WHITE); display.fillCircle(92,28,3,BLACK);
  display.drawLine(21,9,55,9,WHITE); display.drawLine(73,9,107,9,WHITE);
  display.drawCircle(64,54,7,WHITE);
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 15 — STOPWATCH
// ═══════════════════════════════════════════════════════════════════════
void swDraw(){
  unsigned long tot=swElapsed+(swRunning?millis()-swStarted:0);
  int cs=(tot%1000)/10,s=(tot/1000)%60,mn=(tot/60000)%60,hr=tot/3600000;
  display.clearDisplay();
  display.fillRect(0,0,128,12,WHITE);
  display.setTextColor(BLACK);display.setTextSize(1);
  display.setCursor(22,2);display.print(swRunning?">> STOPWATCH <<":"|| STOPWATCH ||");
  display.setTextColor(WHITE);display.setTextSize(2);
  display.setCursor(4,16);
  if(hr<10)display.print("0");display.print(hr);display.print(":");
  if(mn<10)display.print("0");display.print(mn);display.print(":");
  if(s <10)display.print("0");display.print(s);
  display.setTextSize(1);display.setCursor(102,20);
  display.print(".");if(cs<10)display.print("0");display.print(cs);
  display.drawRoundRect(2,42,124,8,3,WHITE);
  display.fillRoundRect(3,43,map(cs,0,99,0,122),6,2,WHITE);
  if(swHasLap){
    int ls=(swLap/1000)%60,lm=(swLap/60000)%60;
    display.setCursor(0,53);
    display.print("LAP ");if(lm<10)display.print("0");display.print(lm);
    display.print(":");if(ls<10)display.print("0");display.print(ls);
  }
  display.setCursor(0,53);display.print(swRunning?"L=Stop S=Lap":"L=Go S=Rst  Dbl=Next");
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 16 — COUNTDOWN TIMER
// ═══════════════════════════════════════════════════════════════════════
void cdDraw(){
  if(cdRunning){
    long r=(long)cdRemain-(long)(millis()-cdStart);
    if(r<=0){r=0;cdRunning=false;cdDone=true;} cdRemain=r;cdStart=millis();
  }
  int rs=cdRemain/1000,mm=rs/60,ss=rs%60,pct=map(cdRemain,0,cdTotal,0,100);
  display.clearDisplay();
  display.fillRect(0,0,128,12,WHITE);
  display.setTextColor(BLACK);display.setTextSize(1);
  display.setCursor(20,2);display.print("COUNTDOWN TIMER");
  if(cdDone){
    display.setTextColor(WHITE);display.setTextSize(2);
    if((millis()/400)%2){display.setCursor(10,26);display.print("TIME'S UP!");}
  } else {
    display.setTextColor(WHITE);display.setTextSize(3);
    display.setCursor(14,18);
    if(mm<10)display.print("0");display.print(mm);display.print(":");
    if(ss<10)display.print("0");display.print(ss);
    display.drawRoundRect(2,47,124,10,3,WHITE);
    if(pct>20||(millis()/300)%2) display.fillRoundRect(3,48,map(pct,0,100,0,120),8,2,WHITE);
  }
  display.setTextSize(1);display.setCursor(0,57);display.print("L=Go  S=Reset  Dbl=Next");
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 17 — SNAKE
// ═══════════════════════════════════════════════════════════════════════
void snakeDraw(){
  display.clearDisplay();
  if(!snakeDead&&millis()-snakeLast>120){
    snakeLast=millis();
    for(int i=snakeLen-1;i>0;i--) snake[i]=snake[i-1];
    snake[0].x+=sDX;snake[0].y+=sDY;
    if(snake[0].x<0)snake[0].x=GW-1;if(snake[0].x>=GW)snake[0].x=0;
    if(snake[0].y<0)snake[0].y=GH-1;if(snake[0].y>=GH)snake[0].y=0;
    for(int i=1;i<snakeLen;i++) if(snake[i].x==snake[0].x&&snake[i].y==snake[0].y) snakeDead=true;
    if(snake[0].x==food.x&&snake[0].y==food.y){if(snakeLen<MAXLEN)snakeLen++;snakeScore++;placeFood();}
  }
  for(int i=0;i<snakeLen;i++){
    int px=snake[i].x*CELL,py=snake[i].y*CELL;
    if(i==0) display.fillRect(px,py,CELL,CELL,WHITE);
    else     display.drawRect(px,py,CELL,CELL,WHITE);
  }
  int fx=food.x*CELL,fy=food.y*CELL;
  if((millis()/300)%2) display.fillRect(fx,fy,CELL,CELL,WHITE);
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(0,0);display.print("S:");display.print(snakeScore);
  if(snakeDead){
    display.fillRoundRect(20,22,88,22,4,BLACK);display.drawRoundRect(20,22,88,22,4,WHITE);
    display.setCursor(28,27);display.print("GAME OVER");
    display.setCursor(10,37);display.print("L=Restart Dbl=Next");
  }
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 18 — PONG
// ═══════════════════════════════════════════════════════════════════════
void pongDraw(){
  if(millis()-pongLast<18)return;pongLast=millis();
  if(pgAIY+PAD_H/2<pgBy-1)pgAIY+=2;if(pgAIY+PAD_H/2>pgBy+1)pgAIY-=2;
  pgAIY=constrain(pgAIY,0,48);
  pgBx+=pgVx;pgBy+=pgVy;
  if(pgBy<=2||pgBy>=62)pgVy=-pgVy;
  if(pgBx<=7&&pgBx>=5&&pgBy>=pgPadY&&pgBy<=pgPadY+PAD_H){pgVx=fabs(pgVx)+0.1f;pgVy+=random(-10,10)*0.1f;}
  if(pgBx>=120&&pgBx<=123&&pgBy>=pgAIY&&pgBy<=pgAIY+PAD_H){pgVx=-fabs(pgVx)-0.1f;}
  if(pgBx<0){pgAI++;pgBx=64;pgBy=32;pgVx=2.5f;}
  if(pgBx>128){pgScore++;pgBx=64;pgBy=32;pgVx=-2.5f;}
  pgVx=constrain(pgVx,-5.0f,5.0f);pgVy=constrain(pgVy,-4.0f,4.0f);
  display.clearDisplay();
  for(int y=0;y<64;y+=6) display.drawPixel(64,y,WHITE);
  display.fillRect(4,pgPadY,PAD_W,PAD_H,WHITE);
  display.fillRect(121,pgAIY,PAD_W,PAD_H,WHITE);
  display.fillRect((int)pgBx-1,(int)pgBy-1,3,3,WHITE);
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(44,1);display.print(pgScore);
  display.setCursor(76,1);display.print(pgAI);
  display.setCursor(8,57);display.print("YOU");
  display.setCursor(96,57);display.print("AI");
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 19 — FIREWORKS
// ═══════════════════════════════════════════════════════════════════════
void fireworksDraw(){
  if(millis()-fwLast>1000){fwLast=millis();fireParticle(random(20,108),random(10,50));}
  display.clearDisplay();
  for(int i=0;i<MAX_P;i++){
    if(!parts[i].alive)continue;
    parts[i].x+=parts[i].vx;parts[i].y+=parts[i].vy;
    parts[i].vy+=0.15f;parts[i].life--;
    if(parts[i].life<=0)parts[i].alive=false;
    if(parts[i].x>=0&&parts[i].x<128&&parts[i].y>=0&&parts[i].y<64)
      display.drawPixel((int)parts[i].x,(int)parts[i].y,WHITE);
  }
  display.display();delay(16);
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 20 — STARFIELD WARP
// ═══════════════════════════════════════════════════════════════════════
void starfieldDraw(){
  display.clearDisplay();
  for(int i=0;i<NUM_STARS;i++){
    stars[i].z-=3.0f;
    if(stars[i].z<=0) stars[i]={(float)random(-640,640),(float)random(-320,320),128.0f};
    float sx=stars[i].x/stars[i].z*64+64,sy=stars[i].y/stars[i].z*32+32;
    if(sx<0||sx>=128||sy<0||sy>=64)continue;
    int sz=map((int)stars[i].z,1,128,3,0);
    if(sz<=0) display.drawPixel((int)sx,(int)sy,WHITE);
    else      display.fillRect((int)sx,(int)sy,sz,sz,WHITE);
  }
  display.display();delay(18);
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 21 — SPECTRUM ANALYZER
// ═══════════════════════════════════════════════════════════════════════
void spectrumDraw(){
  if(millis()-specLast>55){ specLast=millis();
    for(int i=0;i<16;i++){
      if(random(0,5)==0)specVel[i]=random(5,15);
      specBar[i]+=(int)specVel[i];specVel[i]*=0.82f;
      if(specBar[i]>50)specBar[i]=50;if(specBar[i]<0)specBar[i]=0;
      if(specBar[i]>0)specBar[i]--;
    }
  }
  display.clearDisplay();
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(25,0);display.print("SPECTRUM");
  for(int i=0;i<16;i++){
    int x=i*8,h=specBar[i],y=63-h;
    display.fillRect(x+1,y,6,h,WHITE);
    if(h>2)display.fillRect(x+1,y-2,6,2,WHITE);
  }
  display.drawLine(0,63,127,63,WHITE);
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 22 — OSCILLOSCOPE  (defined ONCE)
// ═══════════════════════════════════════════════════════════════════════
void oscilloscopeDraw(){
  display.clearDisplay();
  for(int x=0;x<128;x+=16) display.drawLine(x,0,x,63,WHITE);
  display.drawLine(0,32,127,32,WHITE);
  int py=32;
  for(int x=0;x<128;x++){
    int y=32-(int)(22*sin(x*0.08f+oscP1));
    display.drawLine(x-1<0?0:x-1,py,x,y,WHITE);py=y;
  }
  py=32;
  for(int x=0;x<128;x++){
    int y=32-(int)(12*sin(x*0.13f+oscP2));
    if(x%2==0) display.drawPixel(x,y,WHITE);py=y;
  }
  oscP1+=0.15f;oscP2+=0.09f;
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(0,0);display.print("CH1");
  display.setCursor(0,56);display.print("CH2");
  display.display();delay(18);
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 23 — DICE ROLLER
// ═══════════════════════════════════════════════════════════════════════
void drawDiceFace(int x,int y,int s,int val){
  const bool face[6][9]={
    {0,0,0,0,1,0,0,0,0},{1,0,0,0,0,0,0,0,1},{1,0,0,0,1,0,0,0,1},
    {1,0,1,0,0,0,1,0,1},{1,0,1,0,1,0,1,0,1},{1,0,1,1,0,1,1,0,1}};
  int mg=s/5,pip=s/8+1,v=constrain(val,1,6)-1;
  for(int r=0;r<3;r++) for(int c=0;c<3;c++) if(face[v][r*3+c]){
    display.fillCircle(x+mg+c*(s-2*mg)/2, y+mg+r*(s-2*mg)/2, pip, WHITE);
  }
}
void diceDraw(){
  if(diceRolling){if(millis()<diceRollEnd)diceVal=random(1,7);else diceRolling=false;}
  display.clearDisplay();
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(40,2);display.print("DICE");
  int ds=40,dx=(128-ds)/2,dy=(64-ds)/2+4;
  display.drawRoundRect(dx,dy,ds,ds,4,WHITE);
  drawDiceFace(dx,dy,ds,diceVal);
  display.setCursor(56,57);display.print(diceVal);
  display.setCursor(0,57);display.print("L=Roll");
  if(diceRolling){display.setCursor(100,57);display.print("...");}
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 24 — BINARY CLOCK
// ═══════════════════════════════════════════════════════════════════════
void drawBitCol(int x,int val,int bits,const char* lbl){
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(x-1,55);display.print(lbl);
  for(int b=0;b<bits;b++){
    bool on=(val>>(bits-1-b))&1;int y=5+b*11;
    if(on) display.fillCircle(x+3,y+4,4,WHITE);
    else   display.drawCircle(x+3,y+4,4,WHITE);
  }
}
void binaryClockDraw(){
  int h,m,s;getTime(h,m,s);
  display.clearDisplay();
  display.fillRect(0,0,128,10,WHITE);
  display.setTextColor(BLACK);display.setTextSize(1);
  display.setCursor(25,1);display.print("BINARY CLOCK");
  drawBitCol(8,h,4,"H");drawBitCol(30,m/10,3,"M");drawBitCol(50,m%10,4,"m");
  drawBitCol(74,s/10,3,"S");drawBitCol(94,s%10,4,"s");
  display.setTextColor(WHITE);display.setTextSize(1);
  const char* bv[]={"8","4","2","1"};
  for(int b=0;b<4;b++){display.setCursor(114,5+b*11);display.print(bv[b]);}
  display.setCursor(0,57);
  if(h<10)display.print("0");display.print(h);display.print(":");
  if(m<10)display.print("0");display.print(m);display.print(":");
  if(s<10)display.print("0");display.print(s);
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 25 — SCROLLING MARQUEE
// ═══════════════════════════════════════════════════════════════════════
void marqueeDraw(){
  if(millis()-marqueeLast<35)return;marqueeLast=millis();
  display.clearDisplay();
  display.fillRect(0,0,128,12,WHITE);
  display.setTextColor(BLACK);display.setTextSize(1);
  display.setCursor(25,2);display.print("MARQUEE TEXT");
  display.setTextColor(WHITE);display.setTextSize(2);
  display.setCursor(marqueeX,22);display.print(MARQUEE_TEXT);
  display.drawLine(0,54,128,54,WHITE);
  display.setTextSize(1);display.setCursor(35,57);display.print("mode 25/35");
  marqueeX-=2;
  int textW=strlen(MARQUEE_TEXT)*12;
  if(marqueeX<-textW)marqueeX=128;
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 26 — PAC-MAN
// ═══════════════════════════════════════════════════════════════════════
void pacmanDraw(){
  display.clearDisplay();
  display.drawRoundRect(2,15,124,34,5,WHITE);
  for(int i=0;i<16;i++) if(pacDots[i]) display.fillCircle(8+i*7,32,2,WHITE);
  pacMouth+=pacMouthDir*2;
  if(pacMouth>=40)pacMouthDir=-1;
  if(pacMouth<= 0)pacMouthDir= 1;
  int pm=(int)pacX,py=32;
  display.fillCircle(pm,py,9,WHITE);
  if(pacDir==1) display.fillTriangle(pm,py,pm+11,py-pacMouth/8,pm+11,py+pacMouth/8,BLACK);
  else          display.fillTriangle(pm,py,pm-11,py-pacMouth/8,pm-11,py+pacMouth/8,BLACK);
  display.fillCircle(pm+pacDir*3,py-4,2,BLACK);
  int dotIdx=(int)((pacX-8)/7);
  if(dotIdx>=0&&dotIdx<16) pacDots[dotIdx]=false;
  ghostX+=(pacDir==1?0.8f:-0.8f);
  int gx=(int)ghostX,gy=32;
  display.fillRoundRect(gx-7,gy-9,14,14,7,WHITE);
  display.fillRect(gx-7,gy+1,14,6,WHITE);
  for(int i=0;i<3;i++) display.fillTriangle(gx-7+i*5,gy+7,gx-4+i*5,gy+7,gx-5+i*5+3,gy+4,WHITE);
  if(!ghostScared){
    display.fillCircle(gx-3,gy-4,2,BLACK);display.fillCircle(gx+3,gy-4,2,BLACK);
    display.fillCircle(gx-2,gy-4,1,WHITE);display.fillCircle(gx+4,gy-4,1,WHITE);
  } else {
    display.drawLine(gx-5,gy-6,gx-2,gy-3,BLACK);display.drawLine(gx-5,gy-3,gx-2,gy-6,BLACK);
    display.drawLine(gx+2,gy-6,gx+5,gy-3,BLACK);display.drawLine(gx+2,gy-3,gx+5,gy-6,BLACK);
  }
  pacX+=pacDir*1.2f;
  if(pacX>118){pacDir=-1;ghostScared=true;for(int i=0;i<16;i++)pacDots[i]=true;}
  if(pacX< 10){pacDir= 1;ghostScared=false;}
  ghostX=constrain(ghostX,8,120);
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(2,2);display.print("PAC-MAN");
  display.display();delay(30);
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 27 — BREAKOUT
// ═══════════════════════════════════════════════════════════════════════
void breakoutDraw(){
  display.clearDisplay();
  const int BW=14,BH=6,BOFF_X=4,BOFF_Y=8;
  bool anyLeft=false;
  for(int r=0;r<BR_ROWS;r++) for(int c=0;c<BR_COLS;c++){
    if(!bricks[r][c])continue;anyLeft=true;
    int bx2=BOFF_X+c*(BW+2),by2=BOFF_Y+r*(BH+2);
    display.fillRect(bx2,by2,BW,BH,WHITE);
    display.drawRect(bx2,by2,BW,BH,BLACK);
  }
  if(!anyLeft&&!brWin) brWin=true;
  display.fillRoundRect(brPadX,60,28,3,1,WHITE);
  display.fillCircle((int)brBx,(int)brBy,3,WHITE);
  if(!brDead&&!brWin){
    brBx+=brVx;brBy+=brVy;
    if(brBx<3||brBx>125)brVx=-brVx;
    if(brBy<3)brVy=fabs(brVy);
    if(brBy>64)brDead=true;
    if(brBy>=57&&brBy<=61&&brBx>=brPadX&&brBx<=brPadX+28){
      brVy=-fabs(brVy);brVx+=(brBx-(brPadX+14))*0.1f;
    }
    for(int r=0;r<BR_ROWS;r++) for(int c=0;c<BR_COLS;c++){
      if(!bricks[r][c])continue;
      int bx2=BOFF_X+c*(BW+2),by2=BOFF_Y+r*(BH+2);
      if(brBx>=bx2&&brBx<=bx2+BW&&brBy>=by2&&brBy<=by2+BH){
        bricks[r][c]=false;brVy=-brVy;brScore++;
      }
    }
  }
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(0,0);display.print(brScore);
  if(brDead||brWin){
    display.fillRoundRect(20,22,88,20,4,BLACK);display.drawRoundRect(20,22,88,20,4,WHITE);
    display.setCursor(brDead?28:36,27);display.print(brDead?"GAME OVER":"YOU WIN!");
    display.setCursor(4,37);display.print("L=Restart  Dbl=Next");
  }
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 28 — HEART RATE MONITOR
// ═══════════════════════════════════════════════════════════════════════
void heartRateDraw(){
  unsigned long now=millis();
  if(now-hrLast>20){
    hrLast=now;
    long beatMs=60000L/hrBPM,phase=now%beatMs;
    int y=32;
    if(phase<50)          y=32;
    else if(phase<80)     y=32-(int)((phase-50)*1.5f);
    else if(phase<120)    y=32+(int)((phase-80)*0.5f);
    else if(phase<140)    y=32-(int)((phase-120)*8.0f);
    else if(phase<155)    y=32+(int)((140-phase)*5.0f);
    else if(phase<200)    y=32+(int)((phase-155)*0.8f);
    else if(phase<260)    y=32-(int)((phase-200)*0.4f);
    else if(phase<320)    y=32-(int)((320-phase)*0.4f);
    y=constrain(y,5,60);
    hrBuf[hrHead]=y;hrHead=(hrHead+1)%128;
    if(random(0,200)==0) hrBPM=random(60,100);
  }
  display.clearDisplay();
  display.fillRect(0,0,128,11,WHITE);
  display.setTextColor(BLACK);display.setTextSize(1);
  display.setCursor(20,2);display.print("HEART RATE MONITOR");
  display.drawLine(0,32,127,32,WHITE);
  for(int x=1;x<128;x++){
    int i0=(hrHead+x-1)%128,i1=(hrHead+x)%128;
    display.drawLine(x-1,hrBuf[i0],x,hrBuf[i1],WHITE);
  }
  display.setTextColor(WHITE);display.setTextSize(2);
  display.setCursor(80,48);display.print(hrBPM);
  display.setTextSize(1);display.setCursor(80,60);display.print("BPM");
  display.fillCircle(10,54,4,WHITE);display.fillCircle(16,54,4,WHITE);
  display.fillTriangle(6,56,20,56,13,63,WHITE);
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 29 — THERMOMETER
// ═══════════════════════════════════════════════════════════════════════
void thermoDraw(){
  if(millis()-thermoLast>200){
    thermoLast=millis();
    if(random(0,30)==0)thermoTarget=random(150,420)/10.0f;
    if(thermo<thermoTarget)thermo+=0.3f;
    if(thermo>thermoTarget)thermo-=0.3f;
  }
  display.clearDisplay();
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(25,0);display.print("TEMPERATURE");
  display.drawRoundRect(58,6,12,44,5,WHITE);
  display.fillCircle(64,54,7,WHITE);
  int mercH=(int)map((int)(thermo*10),100,450,0,36);
  mercH=constrain(mercH,0,36);
  display.fillRoundRect(60,42-mercH,8,mercH+12,3,WHITE);
  display.fillRoundRect(59,7,10,2,1,BLACK);
  display.setTextSize(2);display.setCursor(2,26);
  display.print((int)thermo);display.print(".");display.print((int)(thermo*10)%10);
  display.setTextSize(1);display.setCursor(2,44);display.print("*C");
  for(int t=10;t<=45;t+=5){
    int y=42-(int)map(t*10,100,450,0,36);
    display.drawLine(70,y,73,y,WHITE);
    if(t%10==0){display.setCursor(75,y-3);display.print(t);}
  }
  if(thermo>35&&(millis()/400)%2){display.setCursor(2,2);display.print("!! HOT !!");}
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 30 — COMPASS
// ═══════════════════════════════════════════════════════════════════════
void compassDraw(){
  if(millis()-compassLast>30){
    compassLast=millis();
    if(random(0,80)==0)compassTarget=random(0,360);
    float diff=compassTarget-compassAngle;
    if(diff>180)diff-=360;if(diff<-180)diff+=360;
    compassAngle+=diff*0.05f;
    if(compassAngle<0)compassAngle+=360;
    if(compassAngle>=360)compassAngle-=360;
  }
  const int cx=64,cy=35,R=26;
  display.clearDisplay();
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(35,0);display.print("COMPASS");
  display.drawCircle(cx,cy,R,WHITE);display.drawCircle(cx,cy,R-1,WHITE);
  for(int deg=0;deg<360;deg+=10){
    float a=deg*PI/180.0f-PI/2.0f;
    int r2=(deg%30==0)?R-5:R-3;
    display.drawLine(cx+(R-1)*cos(a),cy+(R-1)*sin(a),cx+r2*cos(a),cy+r2*sin(a),WHITE);
  }
  const char* cards[]={"N","E","S","W"};
  const float cardAng[]={0,90,180,270};
  for(int i=0;i<4;i++){
    float a=(cardAng[i]+compassAngle)*PI/180.0f-PI/2.0f;
    display.setCursor(cx+(R-9)*cos(a)-2,cy+(R-9)*sin(a)-3);
    display.print(cards[i]);
  }
  float na=compassAngle*PI/180.0f-PI/2.0f;
  display.drawLine(cx,cy,cx+18*cos(na),cy+18*sin(na),WHITE);
  display.drawLine(cx+1,cy,cx+18*cos(na)+1,cy+18*sin(na),WHITE);
  for(int d=0;d<14;d+=3) display.drawPixel(cx-(int)(d*cos(na)),cy-(int)(d*sin(na)),WHITE);
  display.fillCircle(cx,cy,3,WHITE);
  display.setCursor(2,56);display.print((int)compassAngle);display.print("*");
  display.setCursor(59,8);display.print("^N");
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 31 — PLASMA
// ═══════════════════════════════════════════════════════════════════════
void plasmaDraw(){
  display.clearDisplay();
  for(int y=0;y<64;y+=2) for(int x=0;x<128;x+=2){
    float v=sin(x*0.1f+plasmaT)+sin(y*0.15f+plasmaT*0.7f)
           +sin((x+y)*0.08f+plasmaT*1.2f)
           +sin(sqrt((float)(x*x+y*y))*0.07f+plasmaT);
    if(v>0){display.drawPixel(x,y,WHITE);display.drawPixel(x+1,y,WHITE);
            display.drawPixel(x,y+1,WHITE);display.drawPixel(x+1,y+1,WHITE);}
  }
  plasmaT+=0.12f;
  display.display();delay(10);
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 32 — RAIN
// ═══════════════════════════════════════════════════════════════════════
void rainDraw(){
  if(millis()-rainLast<18)return;rainLast=millis();
  display.clearDisplay();
  unsigned long t=millis();
  // Puddle ripples
  for(int r=1;r<=3;r++) display.drawEllipse(64,61,(t/150)%8+r*3,2,WHITE);
  for(int i=0;i<MAX_DROPS;i++){
    if(!drops[i].alive)continue;
    drops[i].y+=drops[i].spd;
    int len=(int)(drops[i].spd*3);
    display.drawLine((int)drops[i].x,(int)drops[i].y,(int)drops[i].x,(int)drops[i].y-len,WHITE);
    if(drops[i].y>60){drops[i].y=-2;drops[i].x=random(0,128);}
  }
  if((t/200)%3==0){int slot=random(0,MAX_DROPS);drops[slot]={random(0,128)*1.0f,-2.0f,random(15,45)/10.0f,true};}
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(40,2);display.print("RAIN");
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 33 — CANDLE
// ═══════════════════════════════════════════════════════════════════════
void candleDraw(){
  if(millis()-flameLast>50){
    flameLast=millis();
    flameH+=(random(0,10)-5)*0.8f;flameW+=(random(0,6)-3)*0.4f;
    flameH=constrain(flameH,12,28);flameW=constrain(flameW,6,14);
  }
  display.clearDisplay();
  const int cx=64,base=56;
  display.fillRoundRect(cx-8,base-20,16,20,2,WHITE);
  display.drawLine(cx,base-20,cx,base-24,WHITE);
  display.fillEllipse(cx,base-24-(int)flameH/2,(int)flameW/2+2,(int)flameH/2+2,WHITE);
  display.fillEllipse(cx,base-24-(int)flameH/2+3,(int)flameW/3,(int)flameH/3,BLACK);
  display.fillCircle(cx,base-22,2,WHITE);
  for(int y=base-18;y<base-2;y+=5) display.drawLine(cx-7,y,cx+7,y,BLACK);
  display.drawLine(cx+5,base-18,cx+5,base-14,WHITE);
  if((millis()/200)%2) for(int r=1;r<=3;r++) display.drawCircle(cx,base-24-(int)flameH/2,r*4,WHITE);
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(35,57);display.print("CANDLE");
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  MODE 34 — SPACE SHOOTER
// ═══════════════════════════════════════════════════════════════════════
void spaceDraw(){
  if(millis()-spMoveLast>300){
    spMoveLast=millis();
    bool hitEdge=false;
    for(int i=0;i<MAX_ENEMIES;i++) if(spEnemies[i].alive){
      spEnemies[i].x+=spEDir*5;
      if(spEnemies[i].x>120||spEnemies[i].x<6)hitEdge=true;
    }
    if(hitEdge){spEDir=-spEDir;for(int i=0;i<MAX_ENEMIES;i++)spEnemies[i].y+=4;}
  }
  if(millis()-spBulletLast>20){spBulletLast=millis();
    for(int i=0;i<MAX_BULLETS;i++) if(spBullets[i].alive){
      spBullets[i].y-=3;if(spBullets[i].y<0)spBullets[i].alive=false;
    }
  }
  for(int b=0;b<MAX_BULLETS;b++) if(spBullets[b].alive)
    for(int e=0;e<MAX_ENEMIES;e++) if(spEnemies[e].alive)
      if(abs(spBullets[b].x-spEnemies[e].x)<6&&abs(spBullets[b].y-spEnemies[e].y)<5){
        spBullets[b].alive=false;spEnemies[e].alive=false;spScore+=10;
      }
  for(int i=0;i<MAX_ENEMIES;i++) if(spEnemies[i].alive&&spEnemies[i].y>52){
    spLives--;if(spLives<=0)spGameOver=true;spEnemies[i].y=8;
  }
  bool anyAlive=false;
  for(int i=0;i<MAX_ENEMIES;i++) if(spEnemies[i].alive)anyAlive=true;
  if(!anyAlive) for(int i=0;i<MAX_ENEMIES;i++) spEnemies[i]={10+i*14,8,true};

  display.clearDisplay();
  for(int i=0;i<10;i++) display.drawPixel((spScore*7+i*13)%128,(i*17+millis()/100)%64,WHITE);
  display.fillTriangle(spShipX,55,spShipX-5,62,spShipX+5,62,WHITE);
  display.fillRect(spShipX-2,57,4,5,BLACK);
  for(int i=0;i<MAX_BULLETS;i++) if(spBullets[i].alive)
    display.fillRect(spBullets[i].x-1,spBullets[i].y,2,5,WHITE);
  for(int i=0;i<MAX_ENEMIES;i++) if(spEnemies[i].alive){
    int ex=spEnemies[i].x,ey=spEnemies[i].y;
    display.fillRoundRect(ex-4,ey,8,5,2,WHITE);
    display.drawLine(ex-5,ey+2,ex-7,ey,WHITE);display.drawLine(ex+5,ey+2,ex+7,ey,WHITE);
    display.drawPixel(ex-2,ey+2,BLACK);display.drawPixel(ex+2,ey+2,BLACK);
  }
  display.setTextColor(WHITE);display.setTextSize(1);
  display.setCursor(0,0);display.print("S:");display.print(spScore);
  display.setCursor(90,0);display.print("HP:");display.print(spLives);
  if(spGameOver){
    display.fillRoundRect(20,24,88,18,4,BLACK);display.drawRoundRect(20,24,88,18,4,WHITE);
    display.setCursor(28,29);display.print("GAME OVER");
    display.setCursor(4,39);display.print("L=Restart  Dbl=Next");
  }
  display.display();
}

// ═══════════════════════════════════════════════════════════════════════
//  RESET
// ═══════════════════════════════════════════════════════════════════════
void resetModeState(){
  loadPct=0;bx=64;by=32;bvx=2.5f;bvy=1.8f;
  battLevel=100;radarAngle=0;oscP1=0;oscP2=0;
  specInit();marqueeX=128;plasmaT=0;
  for(int i=0;i<16;i++) matDrop[i]=random(0,8);
  rainInit();fireParticle(64,32);starsInit();
}

// ═══════════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════════
void setup(){
  pinMode(BUTTON_PIN,INPUT_PULLUP);
  Wire.begin(21,22);
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.clearDisplay();display.display();
  randomSeed(analogRead(0));

  setupWiFiClock();  // NTP sync

  clockBase=millis();
  for(int i=0;i<16;i++) matDrop[i]=random(0,8);
  rainInit();fireParticle(64,32);starsInit();specInit();
  snakeInit();breakoutInit();spaceInit();pacInit();

  display.clearDisplay();
  display.setTextColor(WHITE);display.setTextSize(2);
  display.setCursor(5,8);display.print("Me. ROBOT");
  display.setTextSize(1);display.setCursor(20,32);display.print("v3.1 — 35 Modes");
  display.setCursor(8,46);display.print(" ");
  display.setCursor(ntpReady?18:2,56);
  display.print(ntpReady?"NTP Clock Ready":"Internal Clock");
  display.display();delay(2500);
}

// ═══════════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════════
void loop(){
  BtnEvent evt=readButton();

  // ── DOUBLE = ALWAYS next mode, no exceptions ─────────────
  if(evt==BTN_DOUBLE){
    mode=(mode+1)%TOTAL_MODES;
    resetModeState();
    display.clearDisplay();
    // Show brief mode number overlay
    display.setTextColor(WHITE);display.setTextSize(2);
    display.setCursor(38,24);display.print("M:");display.print(mode);
    display.display();delay(400);
    return;
  }

  // ── SHORT press ───────────────────────────────────────────
  if(evt==BTN_SHORT){
    switch(mode){
      case 15: swAction(evt);       break;  // Stopwatch: Lap / Reset
      case 16: cdAction(evt);       break;  // Countdown: Reset
      case 17: snakeAction(evt);    break;  // Snake: Turn left
      case 18: pongAction(evt);     break;  // Pong: Paddle down
      case 27: breakoutAction(evt); break;  // Breakout: Paddle right
      case 34: spaceAction(evt);    break;  // Space: Move right
      default:
        // Non-game modes: SHORT also advances mode
        mode=(mode+1)%TOTAL_MODES;
        resetModeState();display.clearDisplay();
    }
    return;
  }

  // ── LONG press = primary action ───────────────────────────
  if(evt==BTN_LONG){
    switch(mode){
      case 15: swAction(evt);       break;  // Stopwatch: Start/Stop
      case 16: cdAction(evt);       break;  // Countdown: Start/Stop
      case 17: snakeAction(evt);    break;  // Snake: Turn right / Restart
      case 18: pongAction(evt);     break;  // Pong: Paddle up
      case 23: diceAction(evt);     break;  // Dice: Roll
      case 27: breakoutAction(evt); break;  // Breakout: Paddle left / Restart
      case 34: spaceAction(evt);    break;  // Space: Fire / Restart
      default:
        // Non-interactive modes: LONG = next mode
        mode=(mode+1)%TOTAL_MODES;
        resetModeState();display.clearDisplay();
    }
    return;
  }

  // ── Render ────────────────────────────────────────────────
  switch(mode){
    case  0:normalEyes();break;
    case  1:blinkingEyes();break;
    case  2:happyEyes();break;
    case  3:digitalClock();break;
    case  4:analogClock();break;
    case  5:angryEyes();break;
    case  6:sleepyEyes();break;
    case  7:heartEyes();break;
    case  8:bouncingBall();break;
    case  9:loadingBar();break;
    case 10:matrixRain();break;
    case 11:sadEyes();break;
    case 12:radarScan();break;
    case 13:batteryDisplay();break;
    case 14:surprisedEyes();break;
    case 15:swDraw();break;
    case 16:cdDraw();break;
    case 17:snakeDraw();break;
    case 18:pongDraw();break;
    case 19:fireworksDraw();break;
    case 20:starfieldDraw();break;
    case 21:spectrumDraw();break;
    case 22:oscilloscopeDraw();break;
    case 23:diceDraw();break;
    case 24:binaryClockDraw();break;
    case 25:marqueeDraw();break;
    case 26:pacmanDraw();break;
    case 27:breakoutDraw();break;
    case 28:heartRateDraw();break;
    case 29:thermoDraw();break;
    case 30:compassDraw();break;
    case 31:plasmaDraw();break;
    case 32:rainDraw();break;
    case 33:candleDraw();break;
    case 34:spaceDraw();break;
  }
}
