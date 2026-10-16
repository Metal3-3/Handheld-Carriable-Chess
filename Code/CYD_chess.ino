// ============================================================
//  ESP32 Satranc Motoru -> CYD (Cheap Yellow Display, ESP32-2432S028)
//  Ekran: ILI9341 320x240 | Dokunmatik: XPT2046
//  Gerekli kutuphane: LovyanGFX (Library Manager'dan kur)
//  Kart: "ESP32 Dev Module"
//
//  Renkler ters/kirmizi-mavi yer degistirmis gorunuyorsa asagidaki
//  CYD_INVERT / CYD_RGB_ORDER / CYD_ST7789 ayarlarini degistir.
// ============================================================
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Preferences.h>

#define CYD_ST7789     0   // 1: ST7789 panelli CYD surumu
#define CYD_INVERT     0   // 1: renkler ters gorunuyorsa
#define CYD_RGB_ORDER  0   // 1: kirmizi/mavi yer degistirmisse
#define SCREEN_ROTATION 1  // 1 veya 3 (ekrani 180 derece cevirir)

#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

#ifdef SET_LOOP_TASK_STACK_SIZE
SET_LOOP_TASK_STACK_SIZE(16*1024);
#endif

struct Btn { int16_t x, y, w, h; };

class LGFX : public lgfx::LGFX_Device {
#if CYD_ST7789
  lgfx::Panel_ST7789  _panel;
#else
  lgfx::Panel_ILI9341 _panel;
#endif
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_XPT2046 _touch;
public:
  LGFX(void) {
    { auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST; cfg.spi_mode = 0;
      cfg.freq_write = 40000000; cfg.freq_read = 16000000;
      cfg.spi_3wire = false; cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14; cfg.pin_mosi = 13; cfg.pin_miso = 12; cfg.pin_dc = 2;
      _bus.config(cfg); _panel.setBus(&_bus); }
    { auto cfg = _panel.config();
      cfg.pin_cs = 15; cfg.pin_rst = -1; cfg.pin_busy = -1;
      cfg.panel_width = 240; cfg.panel_height = 320;
      cfg.offset_x = 0; cfg.offset_y = 0; cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8; cfg.dummy_read_bits = 1;
      cfg.readable = false; cfg.invert = CYD_INVERT; cfg.rgb_order = CYD_RGB_ORDER;
      cfg.dlen_16bit = false; cfg.bus_shared = false;
      _panel.config(cfg); }
    { auto cfg = _light.config();
      cfg.pin_bl = 21; cfg.invert = false; cfg.freq = 44100; cfg.pwm_channel = 7;
      _light.config(cfg); _panel.setLight(&_light); }
    { auto cfg = _touch.config();
      cfg.x_min = 300; cfg.x_max = 3900; cfg.y_min = 200; cfg.y_max = 3700;
      cfg.pin_int = 36; cfg.bus_shared = false; cfg.offset_rotation = 0;
      cfg.spi_host = SPI3_HOST; cfg.freq = 1000000;
      cfg.pin_sclk = 25; cfg.pin_mosi = 32; cfg.pin_miso = 39; cfg.pin_cs = 33;
      _touch.config(cfg); _panel.setTouch(&_touch); }
    setPanel(&_panel);
  }
};

void ui_poll();   // motor arama dongusunden cagrilir (STOP dokunusu + sayac)


// ===================== MOTOR (orijinal) =====================
//ESP32 chess engine 1.0
//Sergey Urusov, ususovsv@gmail.com

const  signed char fp=1; //pawn
const signed char fn=2; //knight
const signed char  fb=3; //bishop
const signed char fr=4; //rook
const signed char fq=5; //queen
const  signed char fk=6; //king
const int fig_weight[]={0,100,320,330,500,900,0};
const  char fig_symb[]="  NBRQK";
const char fig_symb1[]=" pNBRQK";

int countin=0,countall=0;

const  int MAXSTEPS=150;
const int MAXDEPTH=30;

typedef struct {
  signed  char f1,f2; //    
  signed char c1,c2; //     
  signed char check; //  
  signed char type; //  1-  ,2- ,3- ,4-5-6-7-  ,,,
  short weight; // , 
}  step_t;

int level;
int fdepth;

struct position_t {  
  byte  w; //  1,  - 0
  byte wrk, wrq, brk, brq; // 
  byte pp; //   1..63      
  step_t steps[MAXSTEPS+1]; //     
  int n_steps; //   0..MAXSTEPS  
  int  cur_step; //     0..MAXSTEPS  
  step_t best;  
  int check_on_table;    
  short weight_w; //  
  short weight_b; //  
  short weight_s; //  ,    +
};

step_t  steps2[MAXSTEPS]; //    
int n_steps2; //     

struct packed_t {
  int  q[8];
};
packed_t polep;

String fenstr;
const int MAXEPD=5;
int  bestcount=0;
step_t bestmove[MAXEPD]; //   
boolean bestsolved=0;
boolean  zero=0;

position_t pos[MAXDEPTH]; //   


int TRACE=0;
short  pole[64];

unsigned long timelimith=1*60*1000; //    (1 ) 
unsigned long  starttime;

int nullmove=1;
int multipov=0;
int futility=1;
int lazyeval=1;

int  depth=0;
int nulldepth;
int lazy;
boolean endspiel=0;
boolean stats=1;
int  lastbestdepth=0;
step_t lastbeststep;
boolean halt=0;

step_t bufsteps[MAXSTEPS+1];  //   

step_t game_steps[1000]; // 
position_t game_pos; // 
int game_ply;  // 
boolean game_w; // 
short game_pole[64];

const short column[64]={
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7,  8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5,  6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8,
  1, 2, 3, 4, 5, 6, 7, 8  
};
const  short row[64]={
  8, 8, 8, 8, 8, 8, 8, 8,
  7, 7, 7, 7, 7, 7, 7, 7,
  6,  6, 6, 6, 6, 6, 6, 6,
  5, 5, 5, 5, 5, 5, 5, 5,
  4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1,  1  
};
const short diag1[64]={
  1, 2, 3, 4, 5, 6, 7, 8,
  2, 3, 4,  5, 6, 7, 8, 9,
  3, 4, 5, 6, 7, 8, 9,10,
  4, 5, 6, 7, 8, 9,10,11,
  5,  6, 7, 8, 9,10,11,12,
  6, 7, 8, 9,10,11,12,13,
  7, 8, 9,10,11,12,13,14,
  8, 9,10,11,12,13,14,15  
};
const short diag2[64]={
  8, 7, 6, 5, 4,  3, 2, 1,
  9, 8, 7, 6, 5, 4, 3, 2,
 10, 9, 8, 7, 6, 5, 4, 3,
 11,10, 9,  8, 7, 6, 5, 4,
 12,11,10, 9, 8, 7, 6, 5,
 13,12,11,10, 9, 8, 7, 6,
 14,13,12,11,10,  9, 8, 7,
 15,14,13,12,11,10, 9, 8  
};


int poswk=0; //  
int  posbk=0; //  

unsigned long count=0;

int task_start=0;
int task_check=0;
int  task_l;
short task_s;
unsigned long task_time,task_execute;

const int  stat_weightw[7][64]={
  { 0,  0,  0,  0,  0,  0,  0,  0, //pawn
  100,100,100,100,100,100,100,100,  // 50, 50, 50, 50, 50, 50, 50, 50,
   20, 30, 40, 50, 50, 40, 30, 20,  // 10,  10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20,  10, 10,  5,  //  5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0},
  
 {-50,-40,-30,-30,-30,-30,-40,-50, //knife
  -40,-20,  0,  0,  0,  0,-20,-40,
  -30,  0, 10, 15, 15, 10,  0,-30,
  -30,  5, 15, 20, 20,  15,  5,-30,
  -30,  0, 15, 20, 20, 15,  0,-30,
  -30,  5, 10, 15, 15, 10,  5,-30,
  -40,-20,  0,  5,  5,  0,-20,-40,
  -50,-40,-30,-30,-30,-30,-40,-50},  
  
 {-20,-10,-10,-10,-10,-10,-10,-20, //bishop
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  5, 10, 10,  5,  0,-10,
  -10,  5,  5, 10, 10,  5,  5,-10,
  -10,  0, 10, 10, 10, 10,  0,-10,
  -10, 10, 10, 10, 10, 10, 10,-10,
  -10,  5,  0,  0,  0,  0,  5,-10,
  -20,-10,-10,-10,-10,-10,-10,-20},  

  {  0,  0,  0,  0,  0,  0,  0,  0, //rook
    5, 10, 10, 10, 10, 10, 10,  5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0},
  
 {-20,-10,-10,  -5, -5,-10,-10,-20, //queen
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  5,  5,  5,  5,  0,-10,
   -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
  -10,  5,  5,  5,  5,  5,  0,-10,
  -10,  0,  5,  0,  0,  0,  0,-10,
  -20,-10,-10, -5, -5,-10,-10,-20},  

 {-30,-40,-40,-50,-50,-40,-40,-30,  //kingw
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -20,-30,-30,-40,-40,-30,-30,-20,
  -10,-20,-20,-20,-20,-20,-20,-10,
   10, 10,-10,-10,-10,-10, 10, 10,  
   10, 40, 30,  0,  0,  0, 50, 10}, 

  {-50,-40,-30,-20,-20,-30,-40,-50, //king endspiel
  -30,-20,-10,  0,  0,-10,-20,-30,
  -30,-10, 20, 30, 30, 20,-10,-30,
  -30,-10, 30, 40, 40, 30,-10,-30,
  -30,-10,  30, 40, 40, 30,-10,-30,
  -30,-10, 20, 30, 30, 20,-10,-30,
  -30,-30,  0,  0,  0,  0,-30,-30,
  -50,-30,-30,-30,-30,-30,-30,-50}  

};

const  int stat_weightb[7][64]={
  { 0,  0,  0,  0,  0,  0,  0,  0, //pawn
    5,  10, 10,-20,-20, 10, 10,  5,
    5, -5,-10,  0,  0,-10, -5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5,  5, 10, 25, 25, 10,  5,  5,  
   20, 30, 40,  50, 50, 40, 30, 20,  
  100,100,100,100,100,100,100,100,  
    0,  0,  0,  0,  0,  0,  0,  0},
  
 {-50,-40,-30,-30,-30,-30,-40,-50, //knife
  -40,-20,  0,  0,  0,  0,-20,-40,
  -30,  5, 10, 15, 15, 10,  5,-30,
  -30,  0, 15,  20, 20, 15,  0,-30,
  -30,  5, 15, 20, 20, 15,  5,-30, 
  -30,  0, 10, 15,  15, 10,  0,-30,
  -40,-20,  0,  5,  5,  0,-20,-40,
  -50,-40,-30,-30,-30,-30,-40,-50},  
  
 {-20,-10,-10,-10,-10,-10,-10,-20, //bishop
  -10,  5,  0,  0,  0,  0,  5,-10,
  -10, 10, 10, 10, 10, 10, 10,-10,
  -10,  0, 10, 10, 10, 10,  0,-10,
  -10,  5,  5, 10, 10,  5,  5,-10,
  -10,  0,  5, 10, 10,  5,  0,-10,  
  -10,  0,  0,  0,  0,  0,  0,-10,
  -20,-10,-10,-10,-10,-10,-10,-20},  

  {  0,  0,  0,  5,  5,  0,  0,  0, //rook     
   -5,  0,  0,  0,  0,  0,  0,  -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
    5,  10, 10, 10, 10, 10, 10,  5,   
    0,  0,  0,  0,  0,  0,  0,  0},
  
  {-20,-10,-10, -5, -5,-10,-10,-20, //queen 
  -10,  0,  5,  0,  0,  0,  0,-10,
  -10,  5,  5,  5,  5,  5,  0,-10,
    0,  0,  5,  5,  5,  5,  0, -5,
   -5,  0,  5,  5,  5,  5,  0, -5,
  -10,  0,  5,  5,  5,  5,  0,-10,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -20,-10,-10, -5, -5,-10,-10,-20},  
 
 { 10,  40, 30,  0,  0,  0, 50, 10, //kingb
  10, 10,-10,-10,-10,-10, 10, 10, 
  -10,-20,-20,-20,-20,-20,-20,-10,
  -20,-30,-30,-40,-40,-30,-30,-20,  
  -30,-40,-40,-50,-50,-40,-40,-30, 
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30,
  -30,-40,-40,-50,-50,-40,-40,-30},

  {-50,-30,-30,-30,-30,-30,-30,-50,  //kingb endspiel
  -30,-30,  0,  0,  0,  0,-30,-30,
  -30,-10, 20, 30, 30, 20,-10,-30,
  -30,-10, 30, 40, 40, 30,-10,-30,
  -30,-10,  30, 40, 40, 30,-10,-30,
  -30,-10, 20, 30, 30, 20,-10,-30,
  -30,-20,-10,  0,  0,-10,-20,-30,
  -50,-40,-30,-20,-20,-30,-40,-50}   
 
};


//88  - end of raw
//99 - end of all
const byte diag_step[64][17] {
 {9,18,27,36,45,54,63,99},  //0
 {10,19,28,37,46,55,88,8,99}, //1
 {11,20,29,38,47,88,9,16,99}, //2
  {12,21,30,39,88,10,17,24,99}, //3 
 {13,22,31,88,11,18,25,32,99}, //4 
 {14,23,88,12,19,26,33,40,99},  //5 
 {15,88,13,20,27,34,41,48,99}, //6 
 {14,21,28,35,42,49,56,99}, //7 
  {17,26,35,44,53,62,88,1,99}, //8 
 {18,27,36,45,54,63,88,16,88,0,88,2,99}, //9  
 {19,28,37,46,55,88,17,24,88,1,88,3,99}, //10 
 {20,29,38,47,88,18,25,32,88,2,88,4,99},  //11 
 {21,30,39,88,19,26,33,40,88,3,88,5,99}, //12 
 {22,31,88,20,27,34,41,48,88,4,88,6,99},  //13 
 {23,88,21,28,35,42,49,56,88,5,88,7,99}, //14 
 {22,29,36,43,50,57,88,6,99},  //15 
 {25,34,43,52,61,88,9,2,99}, //16 
 {26,35,44,53,62,88,24,88,8,88,10,3,99},  //17 
 {27,36,45,54,63,88,25,32,88,9,0,88,11,4,99}, //18 
 {28,37,46,55,88,26,33,40,88,10,1,88,12,5,99},  //19 
 {29,38,47,88,27,34,41,48,88,11,2,88,13,6,99}, //20 
 {30,39,88,28,35,42,49,56,88,12,3,88,14,7,99},  //21 
 {31,88,29,36,43,50,57,88,13,4,88,15,99}, //22 
 {30,37,44,51,58,88,14,5,99},  //23 
 {33,42,51,60,88,17,10,3,99}, //24 
 {34,43,52,61,88,32,88,16,88,18,11,4,99},  //25 
 {35,44,53,62,88,33,40,88,17,8,88,19,12,5,99}, //26 
 {36,45,54,63,88,34,41,48,88,18,9,0,88,20,13,6,99},  //27 
 {37,46,55,88,35,42,49,56,88,19,10,1,88,21,14,7,99}, //28 
 {38,47,88,36,43,50,57,88,20,11,2,88,22,15,99},  //29 
 {39,88,37,44,51,58,88,21,12,3,88,23,99}, //30 
 {38,45,52,59,88,22,13,4,99},  //31 
 {41,50,59,88,25,18,11,4,99}, //32 
 {42,51,60,88,40,88,24,88,26,19,12,5,99},  //33 
 {43,52,61,88,41,48,88,25,16,88,27,20,13,6,99}, //34 
 {44,53,62,88,42,49,56,88,26,17,8,88,28,21,14,7,99},  //35 
 {45,54,63,88,43,50,57,88,27,18,9,0,88,29,22,15,99}, //36 
 {46,55,88,44,51,58,88,28,19,10,1,88,30,23,99},  //37 
 {47,88,45,52,59,88,29,20,11,2,88,31,99}, //38 
 {46,53,60,88,30,21,12,3,99},  //39 
 {49,58,88,33,26,19,12,5,99}, //40
 {50,59,88,48,88,32,88,34,27,20,13,6,99},  //41 
 {51,60,88,49,56,88,33,24,88,35,28,21,14,7,99}, //42 
 {52,61,88,50,57,88,34,25,16,88,36,29,22,15,99},  //43 
 {53,62,88,51,58,88,35,26,17,8,88,37,30,23,99}, //44 
 {54,63,88,52,59,88,36,27,18,9,0,88,38,31,99},  //45 
 {55,88,53,60,88,37,28,19,10,1,88,39,99}, //46 
 {54,61,88,38,29,20,11,2,99},  //47 
 {57,88,41,34,27,20,13,6,99}, //48 
 {58,88,56,88,40,88,42,35,28,21,14,7,99},  //49 
 {59,88,57,88,41,32,88,43,36,29,22,15,99}, //50
 {60,88,58,88,42,33,24,88,44,37,30,23,99},  //51 
 {61,88,59,88,43,34,25,16,88,45,38,31,99}, //52 
 {62,88,60,88,44,35,26,17,8,88,46,39,99},  //53 
 {63,88,61,88,45,36,27,18,9,0,88,47,99}, //54 
 {62,88,46,37,28,19,10,1,99},  //55 
 {49,42,35,28,21,14,7,99}, //56 
 {48,88,50,43,36,29,22,15,99}, //57  
 {49,40,88,51,44,37,30,23,99}, //58 
 {50,41,32,88,52,45,38,31,99}, //59  
 {51,42,33,24,88,53,46,39,99}, //60   
 {52,43,34,25,16,88,54,47,99}, //61  
 {53,44,35,26,17,8,88,55,99}, //62 
 {54,45,36,27,18,9,0,99} //63  ++++++
};

const  byte stra_step[64][18] {
 {1,2,3,4,5,6,7,88,8,16,24,32,40,48,56,99}, //0
  {2,3,4,5,6,7,88,9,17,25,33,41,49,57,88,0,99}, //1
 {3,4,5,6,7,88,10,18,26,34,42,50,58,88,1,0,99},  //2
 {4,5,6,7,88,11,19,27,35,43,51,59,88,2,1,0,99}, //3
 {5,6,7,88,12,20,28,36,44,52,60,88,3,2,1,0,99},  //4
 {6,7,88,13,21,29,37,45,53,61,88,4,3,2,1,0,99}, //5
 {7,88,14,22,30,38,46,54,62,88,5,4,3,2,1,0,99},  //6
 {15,23,31,39,47,55,63,88,6,5,4,3,2,1,0,99}, //7
 {9,10,11,12,13,14,15,88,16,24,32,40,48,56,88,0,99},  //8
 {10,11,12,13,14,15,88,17,25,33,41,49,57,88,8,88,1,99}, //9
 {11,12,13,14,15,88,18,26,34,42,50,58,88,9,8,88,2,99},  //10
 {12,13,14,15,88,19,27,35,43,51,59,88,10,9,8,88,3,99}, //11
 {13,14,15,88,20,28,36,44,52,60,88,11,10,9,8,88,4,99},  //12
 {14,15,88,21,29,37,45,53,61,88,12,11,10,9,8,88,5,99}, //13
 {15,88,22,30,38,46,54,62,88,13,12,11,10,9,8,88,6,99},  //14
 {23,31,39,47,55,63,88,14,13,12,11,10,9,8,88,7,99}, //15
 {17,18,19,20,21,22,23,88,24,32,40,48,56,88,8,0,99},  //16
 {18,19,20,21,22,23,88,25,33,41,49,57,88,16,88,9,1,99}, //17
 {19,20,21,22,23,88,26,34,42,50,58,88,17,16,88,10,2,99},  //18
 {20,21,22,23,88,27,35,43,51,59,88,18,17,16,88,11,3,99}, //19
 {21,22,23,88,28,36,44,52,60,88,19,18,17,16,88,12,4,99},  //20
 {22,23,88,29,37,45,53,61,88,20,19,18,17,16,88,13,5,99}, //21
 {23,88,30,38,46,54,62,88,21,20,19,18,17,16,88,14,6,99},  //22
 {31,39,47,55,63,88,22,21,20,19,18,17,16,88,15,7,99}, //23
 {25,26,27,28,29,30,31,88,32,40,48,56,88,16,8,0,99},  //24
 {26,27,28,29,30,31,88,33,41,49,57,88,24,88,17,9,1,99}, //25
 {27,28,29,30,31,88,34,42,50,58,88,25,24,88,18,10,2,99},  //26
 {28,29,30,31,88,35,43,51,59,88,26,25,24,88,19,11,3,99}, //27
 {29,30,31,88,36,44,52,60,88,27,26,25,24,88,20,12,4,99},  //28
 {30,31,88,37,45,53,61,88,28,27,26,25,24,88,21,13,5,99}, //29
 {31,88,38,46,54,62,88,29,28,27,26,25,24,88,22,14,6,99},  //30
 {39,47,55,63,88,30,29,28,27,26,25,24,88,23,15,7,99}, //31
 {33,34,35,36,37,38,39,88,40,48,56,88,24,16,8,0,99},  //32
 {34,35,36,37,38,39,88,41,49,57,88,32,88,25,17,9,1,99}, //33
 {35,36,37,38,39,88,42,50,58,88,33,32,88,26,18,10,2,99},  //34
 {36,37,38,39,88,43,51,59,88,34,33,32,88,27,19,11,3,99}, //35
 {37,38,39,88,44,52,60,88,35,34,33,32,88,28,20,12,4,99},  //36
 {38,39,88,45,53,61,88,36,35,34,33,32,88,29,21,13,5,99}, //37
 {39,88,46,54,62,88,37,36,35,34,33,32,88,30,22,14,6,99},  //38
 {47,55,63,88,38,37,36,35,34,33,32,88,31,23,15,7,99}, //39
 {41,42,43,44,45,46,47,88,48,56,88,32,24,16,8,0,99},  //40
 {42,43,44,45,46,47,88,49,57,88,40,88,33,25,17,9,1,99}, //41
 {43,44,45,46,47,88,50,58,88,41,40,88,34,26,18,10,2,99},  //42
 {44,45,46,47,88,51,59,88,42,41,40,88,35,27,19,11,3,99}, //43
 {45,46,47,88,52,60,88,43,42,41,40,88,36,28,20,12,4,99},  //44
 {46,47,88,53,61,88,44,43,42,41,40,88,37,29,21,13,5,99}, //45
 {47,88,54,62,88,45,44,43,42,41,40,88,38,30,22,14,6,99},  //46
 {55,63,88,46,45,44,43,42,41,40,88,39,31,23,15,7,99}, //47
 {49,50,51,52,53,54,55,88,56,88,40,32,24,16,8,0,99},  //48
 {50,51,52,53,54,55,88,57,88,48,88,41,33,25,17,9,1,99}, //49
 {51,52,53,54,55,88,58,88,49,48,88,42,34,26,18,10,2,99},  //50
 {52,53,54,55,88,59,88,50,49,48,88,43,35,27,19,11,3,99}, //51
 {53,54,55,88,60,88,51,50,49,48,88,44,36,28,20,12,4,99},  //52
 {54,55,88,61,88,52,51,50,49,48,88,45,37,29,21,13,5,99}, //53
 {55,88,62,88,53,52,51,50,49,48,88,46,38,30,22,14,6,99},  //54
 {63,88,54,53,52,51,50,49,48,88,47,39,31,23,15,7,99}, //55
 {57,58,59,60,61,62,63,88,48,40,32,24,16,8,0,99},  //56
 {58,59,60,61,62,63,88,56,88,49,41,33,25,17,9,1,99}, //57
 {59,60,61,62,63,88,57,56,88,50,42,34,26,18,10,2,99},  //58
 {60,61,62,63,88,58,57,56,88,51,43,35,27,19,11,3,99}, //59
 {61,62,63,88,59,58,57,56,88,52,44,36,28,20,12,4,99},  //60
 {62,63,88,60,59,58,57,56,88,53,45,37,29,21,13,5,99}, //61
 {63,88,61,60,59,58,57,56,88,54,46,38,30,22,14,6,99},  //62
 {62,61,60,59,58,57,56,88,55,47,39,31,23,15,7,99} //63
};

const  byte knight_step[64][9] {
 {10,17,99}, //0
 {11,18,16,99}, //1
 {12,19,17,8,99},  //2
 {13,20,18,9,99}, //3
 {14,21,19,10,99}, //4
 {15,22,20,11,99}, //5
  {23,21,12,99}, //6
 {22,13,99}, //7
 {2,18,25,99}, //8
 {3,19,26,24,99},  //9
 {4,20,27,25,16,0,99}, //10
 {5,21,28,26,17,1,99}, //11
 {6,22,29,27,18,2,99},  //12
 {7,23,30,28,19,3,99}, //13
 {31,29,20,4,99}, //14
 {30,21,5,99},  //15
 {1,10,26,33,99}, //16
 {2,11,27,34,32,0,99}, //17
 {3,12,28,35,33,24,8,1,99},  //18
 {4,13,29,36,34,25,9,2,99}, //19
 {5,14,30,37,35,26,10,3,99}, //20
  {6,15,31,38,36,27,11,4,99}, //21
 {7,39,37,28,12,5,99}, //22
 {38,29,13,6,99},  //23
 {9,18,34,41,99}, //24
 {10,19,35,42,40,8,99}, //25
 {11,20,36,43,41,32,16,9,99},  //26
 {12,21,37,44,42,33,17,10,99}, //27
 {13,22,38,45,43,34,18,11,99}, //28
  {14,23,39,46,44,35,19,12,99}, //29
 {15,47,45,36,20,13,99}, //30
 {46,37,21,14,99},  //31
 {17,26,42,49,99}, //32
 {18,27,43,50,48,16,99}, //33
 {19,28,44,51,49,40,24,17,99},  //34
 {20,29,45,52,50,41,25,18,99}, //35
 {21,30,46,53,51,42,26,19,99}, //36
  {22,31,47,54,52,43,27,20,99}, //37
 {23,55,53,44,28,21,99}, //38
 {54,45,29,22,99},  //39
 {25,34,50,57,99}, //40
 {26,35,51,58,56,24,99}, //41
 {27,36,52,59,57,48,32,25,99},  //42
 {28,37,53,60,58,49,33,26,99}, //43
 {29,38,54,61,59,50,34,27,99}, //44
  {30,39,55,62,60,51,35,28,99}, //45
 {31,63,61,52,36,29,99}, //46
 {62,53,37,30,99},  //47
 {33,42,58,99}, //48
 {34,43,59,32,99}, //49
 {35,44,60,56,40,33,99},  //50
 {36,45,61,57,41,34,99}, //51
 {37,46,62,58,42,35,99}, //52
 {38,47,63,59,43,36,99},  //53
 {39,60,44,37,99}, //54
 {61,45,38,99}, //55
 {41,50,99}, //56
  {40,42,51,99}, //57
 {43,52,48,41,99}, //58
 {44,53,49,42,99}, //59
 {45,54,50,43,99},  //60
 {46,55,51,44,99}, //61
 {47,52,45,99}, //62
 {53,46,99} //63  +++++
};

const  byte king_step[64][9] {
 {1,9,8,99}, //0
 {2,10,9,8,0,99}, //1
 {3,11,10,9,1,99},  //2
 {4,12,11,10,2,99}, //3
 {5,13,12,11,3,99}, //4
 {6,14,13,12,4,99},  //5
 {7,15,14,13,5,99}, //6
 {15,14,6,99}, //7
 {1,9,17,16,0,99}, //8
  {2,10,18,17,16,8,0,1,99}, //9
 {3,11,19,18,17,9,1,2,99}, //10
 {4,12,20,19,18,10,2,3,99},  //11
 {5,13,21,20,19,11,3,4,99}, //12
 {6,14,22,21,20,12,4,5,99}, //13
  {7,15,23,22,21,13,5,6,99}, //14
 {23,22,14,6,7,99}, //15
 {9,17,25,24,8,99},  //16
 {10,18,26,25,24,16,8,9,99}, //17
 {11,19,27,26,25,17,9,10,99}, //18
  {12,20,28,27,26,18,10,11,99}, //19
 {13,21,29,28,27,19,11,12,99}, //20
 {14,22,30,29,28,20,12,13,99},  //21
 {15,23,31,30,29,21,13,14,99}, //22
 {31,30,22,14,15,99}, //23
 {17,25,33,32,16,99  }, //24
 {18,26,34,33,32,24,16,17,99}, //25
 {19,27,35,34,33,25,17,18,99},  //26
 {20,28,36,35,34,26,18,19,99}, //27
 {21,29,37,36,35,27,19,20,99}, //28
  {22,30,38,37,36,28,20,21,99}, //29
 {23,31,39,38,37,29,21,22,99}, //30
 {39,38,30,22,23,99},  //31
 {25,33,41,40,24,99}, //32
 {26,34,42,41,40,32,24,25,99}, //33
 {27,35,43,42,41,33,25,26,99},  //34
 {28,36,44,43,42,34,26,27,99}, //35
 {29,37,45,44,43,35,27,28,99}, //36
  {30,38,46,45,44,36,28,29,99}, //37
 {31,39,47,46,45,37,29,30,99}, //38
 {47,46,38,30,31,99},  //39
 {33,41,49,48,32,99}, //40
 {34,42,50,49,48,40,32,33,99}, //41
 {35,43,51,50,49,41,33,34,99},  //42
 {36,44,52,51,50,42,34,35,99}, //43
 {37,45,53,52,51,43,35,36,99}, //44
  {38,46,54,53,52,44,36,37,99}, //45
 {39,47,55,54,53,45,37,38,99}, //46
 {55,54,46,38,39,99},  //47
 {41,49,57,56,40,99}, //48
 {42,50,58,57,56,48,40,41,99}, //49
 {43,51,59,58,57,49,41,42,99},  //50
 {44,52,60,59,58,50,42,43,99}, //51
 {45,53,61,60,59,51,43,44,99}, //52
  {46,54,62,61,60,52,44,45,99}, //53
 {47,55,63,62,61,53,45,46,99}, //54
 {63,62,54,46,47,99},  //55
 {49,57,48,99}, //56
 {50,58,56,48,49,99}, //57
 {51,59,57,49,50,99},  //58
 {52,60,58,50,51,99}, //59
 {53,61,59,51,52,99}, //60
 {54,62,60,52,53,99},  //61
 {55,63,61,53,54,99}, //62
 {62,54,55,99} //63  +++++
};

          
//****************************
String fenout(int l) {
 String s="";
  for (int r=0;r<8;r++) {   
  if (r>0) s=s+"/";
  int empty=0;
  for (int  c=0;c<8;c++) {    
    int f=pole[c+r*8];    
    if (f==0) empty++; 
    if (f!=0||c==7)
     if (empty>0) { s=s+String(empty); empty=0;  }    
    switch (f) {
    case fp: s=s+"P"; break;
    case -fp: s=s+"p"; break;
    case fn: s=s+"N"; break;
    case -fn: s=s+"n"; break;
    case fb:  s=s+"B"; break;
    case -fb: s=s+"b"; break;
    case fr: s=s+"R";  break;
    case -fr: s=s+"r"; break;
    case fq: s=s+"Q"; break;
    case -fq: s=s+"q"; break;
    case fk: s=s+"K"; break;
    case -fk:  s=s+"k"; break;    
    }
  }    
 }
 if (pos[l].w==1) s=s+" w ";  else s=s+" b ";
 if (pos[l].wrk+pos[l].wrq+pos[l].brk+pos[l].brq==0) s=s+"-";
  else {
   if (pos[l].wrk) s=s+"K"; 
   if (pos[l].wrq) s=s+"Q"; 
   if (pos[l].brk) s=s+"k"; 
   if (pos[l].brq) s=s+"q";        
 }
  if (pos[l].pp!=0) s=s+" "+str_pole(pos[l].pp); else s=s+" -";   
 return  s;  
}
//****************************
boolean fen(String ss) {
 char  s='x',i=0,j=0;  boolean load=false;   
  for (int i=0;i<64;i++) pole[i]=0;                               
  pos[0].w=1; 
  pos[0].wrk=0; pos[0].wrq=0;  pos[0].brk=0; pos[0].brq=0;
  pos[0].pp=0; 
  pos[0].cur_step=0; pos[0].n_steps=0;
  int spaces=0;      
  for (int c=0; c<ss.length(); c++) {
    s=ss[c];       
    if (i>7) { i=0;  j++; }        
    if (spaces==3&&int(s)>='a'&&int(s)<='h') {      
      c++;
      char s1=ss[c];
      if (int(s1)>='1'&&int(s1)<='8') {
        pos[0].pp=8*(7-(int(s1)-int('1')))+int(s)-int('a');        
      }
    } else {
    int l=j*8+i;  
    switch (s) {      
    case '/': i=0; break;  
    case 'p': pole[l]=-fp; i++; break;
    case  'P': pole[l]=fp; i++; break;
    case 'n': pole[l]=-fn; i++; break;
    case  'N': pole[l]=fn; i++; break;
    case 'b': if (spaces==0) { pole[l]=-fb; i++;  } else
              if (spaces==1) { pos[0].w=0; load=true; }              
              break;    
    case 'B': pole[l]=fb; i++; break;
    case 'r':  pole[l]=-fr; i++; break;
    case 'R': pole[l]=fr; i++; break;
    case 'q':  if (spaces==0) { pole[l]=-fq; i++; } else pos[0].brq=1; 
              break;
    case 'Q': if (spaces==0) { pole[l]=fq;  i++; } else pos[0].wrq=1;
              break;
    case 'k': if (spaces==0) { pole[l]=-fk;  i++; } else pos[0].brk=1; 
              break;
    case 'K': if (spaces==0) { pole[l]=fk;  i++; } else pos[0].wrk=1;
              break;
    case '1': i++; break;
    case '2': i+=2; break;
    case '3': i+=3; break;
    case '4': i+=4; break;
    case '5': i+=5; break;
    case '6': i+=6;  break;
    case '7': i+=7; break;
    case '8': i=0; j++; break;    
    case  ' ': spaces++; break;    
    case 'w': if (spaces==1) { pos[0].w=1; load=true;  } break;        
    }       
    }    
    if (spaces==4) break;    
  }  
  if (!load) Serial.println(F("Error")); else fenstr=ss;      
  return  load;  
}
//****************************
void getbm(int n,String ep) {  
  ep.trim(); 
  if (ep=="0-0") {
    for (int i=0;i<pos[0].n_steps;i++)  
     if (pos[0].steps[i].type==2)   { // 
        bestmove[n]=pos[0].steps[i];  
        return;        
     }
  } else if (ep=="0-0-0") {
    for  (int i=0;i<pos[0].n_steps;i++) 
     if (pos[0].steps[i].type==3)   { // 
        bestmove[n]=pos[0].steps[i]; 
        return;        
     }
  }  else if (ep.indexOf("=")>-1) { // 
    char fi=ep.indexOf(ep.length()-1);  
    int type=7;
    if (fi=='N') type =4; else if (fi=='B') type=5; else if (fi=='R')  type=6; 
    for (int i=0;i<pos[0].n_steps;i++) 
     if (pos[0].steps[i].type==type)   { //
        bestmove[n]=pos[0].steps[i]; 
        return;        
     }
  } else {  
   int posp1=ep.indexOf("+"); 
   int posp2=ep.indexOf("#");  
   if (posp1>=0||posp2>=0) ep=ep.substring(0,ep.length()-1);  
   char co=ep.charAt(ep.length()-2);
   char ro=ep.charAt(ep.length()-1);  
   int c2=8*(7-(int(ro)-int('1')))+int(co)-int('a');  
   char fi=ep.charAt(0);       
  // int found=0;      
   for (int  i=0;i<pos[0].n_steps;i++) {    
     if (pos[0].steps[i].c2==c2) {                
       if (fig_symb1[abs(pos[0].steps[i].f1)]==fi) {                
        if  (ep.length()==3||ep.length()==4&&ep.charAt(1)=='x'){                  
         bestmove[n]=pos[0].steps[i];         
         return;  
        } else if (int(ep.charAt(1))-int('a')==column[pos[0].steps[i].c1]-1)  {           
         bestmove[n]=pos[0].steps[i];                  
         return;            
        }
       } else if (ep.length()==2&&abs(pos[0].steps[i].f1)==fp)  {        
        bestmove[n]=pos[0].steps[i];         
        return;  
       }        
       if (str_step(pos[0].steps[i])==ep) bestmove[n]=pos[0].steps[i];  
        else {
          String st=str_step(pos[0].steps[i]);
          st=st.substring(0,1)+st.substring(2,st.length());         
          if (pos[0].steps[i].f2!=0&&ep.charAt(1)=='x'&&st==ep) 
           bestmove[n]=pos[0].steps[i];
        }
     }    
   }
  }
}  
//****************************
void epd() {
 for (int i=0;i<MAXEPD;i++)  {
   bestmove[i].c1=-1; 
   bestmove[i].c2=-1; 
   bestmove[i].type=-1;  
 }
 String ep;
 int posbm=fenstr.indexOf("bm"); 
 if (posbm>-1) {
  int posp=fenstr.indexOf(";",posbm+2); 
  if (posp==-1) posp=fenstr.length();
  String ep=fenstr.substring(posbm+3,posp);  
  kingpositions();
  generate_steps(0);
  //show_steps(0);
  
  posp=ep.indexOf(" "); 
  if (posp>-1) {
    getbm(0,ep.substring(0,posp));
    ep=ep.substring(posp+1);    
    posp=ep.indexOf(" "); 
    if (posp>-1)  {
     getbm(1,ep.substring(0,posp));
     ep=ep.substring(posp+1);          
     posp=ep.indexOf(" "); 
     if (posp>-1) {
      getbm(2,ep.substring(0,posp));            
      ep=ep.substring(posp+1);          
      posp=ep.indexOf("  "); 
      if (posp>-1) {
       getbm(3,ep.substring(0,posp));            
       ep=ep.substring(posp+1);          
       posp=ep.indexOf(" "); 
       if (posp>-1) {
        getbm(4,ep.substring(0,posp));                    
       } else getbm(4,ep);
      } else getbm(3,ep);
     } else getbm(2,ep);
    } else getbm(1,ep);
  } else getbm(0,ep);
  if (posp==-1) posp=ep.length();
  String bm=ep.substring(0,posp);
 } 
 bestcount=0;
 for (int i=0;i<MAXEPD;i++)  if (bestmove[i].c1!=-1) bestcount++;
 //for (int i=0;i<MAXEPD;i++) if (bestmove[i].c1!=-1)  Serial.print(str_step(bestmove[i])+" ");
 //Serial.println();
} 
//****************************
void  show_position() {    
  for (int i=0;i<8;i++) {
     Serial.println("-----------------------------------------");       
      Serial.print("|");       
      for (int j=0;j<8;j++)    {        
        signed char f=pole[i*8+j];
        if (f>=0) { Serial.print("  "); Serial.print(fig_symb1[f]); Serial.print(" |"); }                     
         else { Serial.print(" -"); Serial.print(fig_symb1[-f]); Serial.print("  |"); }                     
      }   
      Serial.print("  ");              
      Serial.println(8-i);              
  }    
  Serial.println("-----------------------------------------");       
  Serial.println("   a    b    c    d    e    f    g    h");       
  if (pos[0].w==0) Serial.print("Black move"); else Serial.print("White move");     
  //Serial.print("  rok="+String(pos[0].wrk)+" "+String(pos[0].wrq)+"  "+String(pos[0].brk)+" "+String(pos[0].brq));     
  //if (pos[0].pp!=0) Serial.print("  Prohod:"+String(pos[0].pp));             
  Serial.println();
}
//****************************
String  str_pole(int i) {
  return String(char('a'+i%8)+String(8-i/8));  
}
//**********************************  
String str_steps(step_t st) { //  
  String s="";
   if (st.f1==0) return  s;
   if (st.type==2) s="0-0";
    else if (st.type==3) s="0-0-0";
   else  {    
    if (abs(st.f1)>1) s=s+fig_symb[abs(st.f1)];   
    if (abs(st.f1)==2||abs(st.f1)==4)  {
     s=s+str_pole(st.c1);      
     if (st.f2==0) s=s+"-";    
    }
    if (st.f2!=0) {
      if (abs(st.f1)==1) s=String(char('a'+st.c1%8));
      s=s+"x";    
      if (abs(st.f2)>1) s=s+fig_symb[abs(st.f2)];
    }    
    s=s+str_pole(st.c2);     
   } 
   if (st.type>3) s=s+"="+fig_symb[st.type-2];
   if (st.check==1) s=s+"+"; else 
    if (st.check==2) s=s+"#";
   return  s;
}
//********************************** 
String str_step(step_t st) {
  String s="";
   if (st.f1==0) return s;
   if (st.type==2) s="0-0";
    else if (st.type==3) s="0-0-0";
   else  {    
    if (abs(st.f1)>1)  s=s+fig_symb[abs(st.f1)];     
    s=s+str_pole(st.c1);      
    if (st.f2==0)  s=s+"-";      
    if (st.f2!=0) s=s+"x";    
    s=s+str_pole(st.c2);     
   } 
   if (st.type>3) s=s+"="+fig_symb[st.type-2];
   if (st.check==1)  s=s+"+"; else 
   if (st.check==2) s=s+"#";
   return s;
}
//****************************
void  show_steps(int l) {
 Serial.println("Steps="+String(pos[l].n_steps));
 for  (int i=0;i<pos[l].n_steps;i++) { 
  Serial.print(str_step(pos[l].steps[i]));  Serial.print(" "); Serial.print(pos[l].steps[i].weight); Serial.print("    ");
  if ((i+1)%3==0) Serial.println(); 
  }
} 
//****************************  
void movepos(int l, step_t &s) {     
  pos[l+1].wrk=pos[l].wrk; pos[l+1].wrq=pos[l].wrq;  
  pos[l+1].brk=pos[l].brk; pos[l+1].brq=pos[l].brq;
  pos[l+1].pp=0;    
  if (pos[l].w) { // 
   if (pos[l].wrk||pos[l].wrq) {
    if (s.c1==60) {
      pos[l+1].wrk=0; pos[l+1].wrq=0;
    } else if (s.c1==63) pos[l+1].wrk=0;
    else if (s.c1==56) pos[l+1].wrq=0;
   }
   if (s.type==0&&s.f1==fp&&s.c2==s.c1-16)     
    if (column[s.c2]>1&&pole[s.c2-1]==-fp||column[s.c2]<8&&pole[s.c2+1]==-fp)  pos[l+1].pp=s.c1-8;      
   pos[l+1].weight_w=pos[l].weight_w; pos[l+1].weight_b=pos[l].weight_b;
   if (s.f2!=0) pos[l+1].weight_b-=fig_weight[-s.f2];
   if (s.type>3) pos[l+1].weight_w+=fig_weight[s.type-2]-100;       
   if (stats) {
     if (s.f1==fk&&endspiel)  
      pos[l+1].weight_s=pos[l].weight_s+stat_weightw[6][s.c2]-stat_weightw[6][s.c1];
     else  
      pos[l+1].weight_s=pos[l].weight_s+stat_weightw[s.f1-1][s.c2]-stat_weightw[s.f1-1][s.c1];
     if (s.f2!=0) pos[l+1].weight_s+=stat_weightb[-s.f2-1][s.c2];
   }
  }  else { // 
   if (pos[l].brk||pos[l].brq) {
    if (s.c1==4) {
      pos[l+1].brk=0;  pos[l+1].brq=0;
    } else if (s.c1==7) pos[l+1].brk=0;
    else if (s.c1==0)  pos[l+1].brq=0;
   }   
   if (s.type==0&&s.f1==-fp&&s.c2==s.c1+16) 
    if  (column[s.c2]>1&&pole[s.c2-1]==fp||column[s.c2]<8&&pole[s.c2+1]==fp) pos[l+1].pp=s.c1+8;    
   pos[l+1].weight_w=pos[l].weight_w; pos[l+1].weight_b=pos[l].weight_b;
   if (s.f2!=0) pos[l+1].weight_w-=fig_weight[s.f2];
   if (s.type>3) pos[l+1].weight_b+=fig_weight[s.type-2]-100;         
   if (stats) {
     if (s.f1==-fk&&endspiel)  
      pos[l+1].weight_s=pos[l].weight_s-stat_weightb[6][s.c2]+stat_weightb[6][s.c1];
     else
      pos[l+1].weight_s=pos[l].weight_s-stat_weightb[-s.f1-1][s.c2]+stat_weightb[-s.f1-1][s.c1];
     if (s.f2!=0) pos[l+1].weight_s-=stat_weightw[s.f2-1][s.c2];
   }
  }  
  count++; //                     
}
//****************************  
void checks(int l, step_t &s) {
  if (pole[poswk]!=fk) {      
    Serial.println("wight  king lost!");
    for (int i=0;i<64;i++) if (pole[i]==fk) {  poswk=i; break;  }  
    if (pole[poswk]!=fk)  Serial.println("!!w!!"+String(l)+" "+str_step(pos[l-1].steps[pos[l-1].cur_step]));
  } 
  if (pole[posbk]!=-fk) {      
    Serial.println("black king lost!");
    for (int i=0;i<64;i++) if (pole[i]==-fk) {  posbk=i; break; }      
    if  (pole[posbk]!=-fk)  {
      Serial.println("!!b!!"+String(l)+" "+str_step(pos[l].steps[pos[l].cur_step]));
      Serial.print(str_step(pos[l-1].steps[pos[l-1].cur_step])); Serial.println("  "+String(pos[l-1].steps[pos[l-1].cur_step].type));
      Serial.println(str_step(pos[l-2].steps[pos[l-2].cur_step]));
      Serial.println(str_step(pos[l-3].steps[pos[l-3].cur_step]));
      Serial.println(str_step(pos[l-4].steps[pos[l-4].cur_step]));
      Serial.println(str_step(pos[l-5].steps[pos[l-5].cur_step]));
      Serial.println(str_step(pos[l-6].steps[pos[l-6].cur_step]));
     show_position();
     delay(1000000);
    } 
   }  
 if (abs(s.f2)==fk)  { Serial.println("--king eat--"+str_step(s)+" level="+String(l)); 
  if (0&&s.c1>63||s.c1<0||s.c2>63||s.c2<0)  { Serial.println("!!field out!! c1="+String(s.c1)+" c2="+String(s.c2)+" "+str_step(s)+"  level="+String(l));
     Serial.println(str_step(pos[l-1].steps[pos[l-1].cur_step]));  
     Serial.println(str_step(pos[l-2].steps[pos[l-2].cur_step]));
      Serial.println(str_step(pos[l-3].steps[pos[l-3].cur_step]));
      Serial.println(str_step(pos[l-4].steps[pos[l-4].cur_step]));
      Serial.println(str_step(pos[l-5].steps[pos[l-5].cur_step]));
      Serial.println(str_step(pos[l-6].steps[pos[l-6].cur_step]));
      show_position();
     backstep(l-1,pos[l-1].steps[pos[l-1].cur_step]);
      show_position();
      movestep(l-1,pos[l-1].steps[pos[l-1].cur_step]);
      show_position();
      halt=1;
      Serial.println(get_time((millis()-starttime)/1000));
     if (zero) Serial.println("NULL");  
     delay(10000000);
  }  
  if (pos[l].w&s.f1<0||!pos[l].w&s.f1>0) { Serial.println("--opposite!---"+str_step(s)+"  level="+String(l)); 
      show_position();
     delay(1000000);
  } 
  }
}
//****************************  
void movestep(int l, step_t &s) {           
  //checks(l,s);   
  pole[s.c1]=0;
  pole[s.c2]=s.f1;          
  if (pos[l].w) { // 
   if (s.f1==fk) poswk=s.c2; 
   switch (s.type) {   
   case 0:     
    return;
   case 1: //  
    pole[s.c2+8]=0;  
    break;
   case 2: // 
    pole[60]=0;
    pole[61]=fr;
    pole[62]=fk;
    pole[63]=0;   
    poswk=62;    
    break;
   case 3: // 
    pole[60]=0;
    pole[59]=fr;
    pole[58]=fk;
    pole[57]=0;
    pole[56]=0;  
    poswk=58;     
    break;
   default: 
    pole[s.c2]=s.type-2;   
   } 
  } else { //    
   if (s.f1==-fk) posbk=s.c2; 
   switch (s.type)  {   
   case 0:     
    return; 
   case 1: //  
    pole[s.c2-8]=0;
    break;
   case 2: // 
    pole[4]=0;
    pole[5]=-fr;
    pole[6]=-fk;
    pole[7]=0;
    posbk=6;       
    break;
   case 3: // 
    pole[4]=0;
    pole[3]=-fr;
    pole[2]=-fk;
    pole[1]=0;
    pole[0]=0;   
    posbk=2;    
    break;
   default: 
    pole[s.c2]=2-s.type;
   }
  }      
}
//****************************  
void backstep(int l, step_t  &s) {     
  pole[s.c1]=s.f1;
  pole[s.c2]=s.f2;  
  if (pos[l].w) { //  
   if (s.f1==fk) poswk=s.c1; 
   switch (s.type) {
   case 0: return;
   case 1: //  
    pole[s.c2]=0;
    pole[s.c2+8]=-fp;
    break;
   case 2: // 
    pole[60]=fk;
    pole[61]=0;
    pole[62]=0;
    pole[63]=fr;
    poswk=60;    
    break;
   case 3: // 
    pole[60]=fk;
    pole[59]=0;
    pole[58]=0;
    pole[57]=0;
    pole[56]=fr;
    poswk=60;    
    break;
   }
  } else { //  
  if (s.f1==-fk) posbk=s.c1;
   switch  (s.type) {
   case 0: return;
   case 1: //  
    pole[s.c2]=0;
    pole[s.c2-8]=fp;
    break;
   case 2: // 
    pole[4]=-fk;
    pole[5]=0;
    pole[6]=0;
    pole[7]=-fr;
    posbk=4;    
    break;
   case 3: // 
    pole[4]=-fk;
    pole[3]=0;
    pole[2]=0;
    pole[1]=0;
    pole[0]=-fr;
    posbk=4;    
    break;
   }
  }             
}
//****************************  
void add_king2(int l, int i) {
  signed char f1=pole[i];
  signed char  f2;
  int j=0;
  while (king_step[i][j]!=99) {                
    f2=pole[king_step[i][j]];
    if (f2==0||f2<0&&f1>0||f2>0&&f1<0) {
     steps2[n_steps2].type=0;      
     steps2[n_steps2].c1=i;      
     steps2[n_steps2].c2=king_step[i][j];      
     steps2[n_steps2].f1=f1;      
     steps2[n_steps2].f2=f2;           
     n_steps2++;            
    }             
    j++;
  }    
}
//****************************
void  add_king(int l, int i) {
  signed char f1=pole[i];
  signed char f2;
  int  j=0;
  while (king_step[i][j]!=99) {                
    f2=pole[king_step[i][j]];
    if (f2==0||f2<0&&f1>0||f2>0&&f1<0) {
     pos[l].steps[pos[l].n_steps].type=0;      
     pos[l].steps[pos[l].n_steps].c1=i;      
     pos[l].steps[pos[l].n_steps].c2=king_step[i][j];      
     pos[l].steps[pos[l].n_steps].f1=f1;      
     pos[l].steps[pos[l].n_steps].f2=f2;           
     pos[l].n_steps++;            
    }             
    j++;
  }    
}
//****************************
void add_knight(int l, int i)  {
  signed char f1=pole[i];
  signed char f2;
  int j=0;
  while (knight_step[i][j]!=99)  {                
    f2=pole[knight_step[i][j]];
    if (f2==0||f2<0&&f1>0||f2>0&&f1<0)  {
     pos[l].steps[pos[l].n_steps].type=0;      
     pos[l].steps[pos[l].n_steps].c1=i;      
     pos[l].steps[pos[l].n_steps].c2=knight_step[i][j];      
     pos[l].steps[pos[l].n_steps].f1=f1;      
     pos[l].steps[pos[l].n_steps].f2=f2;           
     pos[l].n_steps++;            
    }             
    j++;
  }    
}
//****************************
void  add_stra(int l, int i) {
  signed char f1=pole[i];
  signed char f2=0;
  int j=0;
  while (stra_step[i][j]!=99) {      
    if (stra_step[i][j]==88)  f2=0;   
    //Serial.print(str_pole(i)); Serial.print(" "); Serial.println(stra_step[i][j]);
    else if (f2==0) {      
     f2=pole[stra_step[i][j]];
     if (f2==0||f2<0&&f1>0||f2>0&&f1<0)  {
      pos[l].steps[pos[l].n_steps].type=0;      
      pos[l].steps[pos[l].n_steps].c1=i;      
      pos[l].steps[pos[l].n_steps].c2=stra_step[i][j];      
      pos[l].steps[pos[l].n_steps].f1=f1;      
      pos[l].steps[pos[l].n_steps].f2=f2;           
      //Serial.print(str_pole(i));
      //Serial.print("-"); Serial.println(str_pole(stra_step[i][j]));
      pos[l].n_steps++;            
     }          
    }
    j++;
  }
}
//****************************
void  add_diag(int l, int i) {
  signed char f1=pole[i];
  signed char f2=0;
  int j=0;
  while (diag_step[i][j]!=99) {                
    if (diag_step[i][j]==88)  f2=0; 
    else if (f2==0) {     
     f2=pole[diag_step[i][j]];
     if  (f2==0||f2<0&&f1>0||f2>0&&f1<0) {
      pos[l].steps[pos[l].n_steps].type=0;      
      pos[l].steps[pos[l].n_steps].c1=i;      
      pos[l].steps[pos[l].n_steps].c2=diag_step[i][j];      
      pos[l].steps[pos[l].n_steps].f1=f1;      
      pos[l].steps[pos[l].n_steps].f2=f2;           
      pos[l].n_steps++;            
     }         
    }
    j++;
  }    
}
//****************************
void add_one(int l,  int c1, int c2) {
  steps2[n_steps2].type=0;      
  steps2[n_steps2].c1=c1;      
  steps2[n_steps2].c2=c2;      
  steps2[n_steps2].f1=pole[c1];      
  steps2[n_steps2].f2=pole[c2];           
  n_steps2++;                
}
//****************************
boolean  checkd_w() {
  signed char f2=0;
  int j=0;
  while (diag_step[poswk][j]!=99)  {                
    if (diag_step[poswk][j]==88) f2=0; 
    else if (f2==0)  {     
     f2=pole[diag_step[poswk][j]];
     if (f2==-fb||f2==-fq) return(true);
    }
    j++;
  }
  f2=0;  j=0;
  while (stra_step[poswk][j]!=99) {                     
    if (stra_step[poswk][j]==88) f2=0; 
    else if  (f2==0) {         
     f2=pole[stra_step[poswk][j]];
     if (f2==-fr||f2==-fq)  return(true);   
     if (j==0||stra_step[poswk][j]==88)
      if (f2==-fk)  return(true);    
    }
    j++;
  }  
  return(false);  
}
//****************************
boolean  checkd_b() {  
  signed char f2=0;
  int j=0;
  while (diag_step[posbk][j]!=99)  {                
    if (diag_step[posbk][j]==88) f2=0; 
    else if (f2==0)  {     
     f2=pole[diag_step[posbk][j]];
     if (f2==fb||f2==fq) return(true);    
    }
    j++;
  }
  f2=0;  j=0;
  while (stra_step[posbk][j]!=99)  {                     
    if (stra_step[posbk][j]==88) f2=0; 
    else if  (f2==0) {         
     f2=pole[stra_step[posbk][j]];
     if (f2==fr||f2==fq)  return(true);    
     if (j==0||stra_step[posbk][j]==88)
      if (f2==fk)  return(true);    
    }
    j++;
  }  
  return(false);  
}
//****************************
boolean  check_w() {
  signed char f2=0;
  int j=0;
  if (pole[poswk]!=fk) {        
    for (int i=0;i<64;i++) if (pole[i]==fk) {  poswk=i; break; }      
  }  
  while (diag_step[poswk][j]!=99) {                
    if (diag_step[poswk][j]==88)  f2=0; 
    else if (f2==0) {     
     f2=pole[diag_step[poswk][j]];
     if  (f2==-fb||f2==-fq) return(true);
    }
    j++;
  }
  f2=0;  j=0;
  while (stra_step[poswk][j]!=99) {                     
    if (stra_step[poswk][j]==88)  f2=0; 
    else if (f2==0) {         
     f2=pole[stra_step[poswk][j]];
     if (f2==-fr||f2==-fq) return(true);   
     if (j==0||stra_step[poswk][j]==88)
      if (f2==-fk) return(true);    
    }
    j++;
  }
  j=0;
  while  (knight_step[poswk][j]!=99) {                
    if (pole[knight_step[poswk][j]]==-fn)  return(true);                 
    j++;
  }
  if (row[poswk]<7) {
    if  (column[poswk]>1&&pole[poswk-9]==-fp) return(true);
    if (column[poswk]<8&&pole[poswk-7]==-fp)  return(true);
  }
  j=0;
  while (king_step[poswk][j]!=99) {                
    if (pole[king_step[poswk][j]]==-fk) return(true);                 
    j++;
  }  
  return(false);  
}

//****************************
boolean  check_b() {  
  signed char f2=0;
  int j=0;
  if (pole[posbk]!=-fk) {        
    for (int i=0;i<64;i++) if (pole[i]==-fk) {  posbk=i; break; }      
  }    
  while (diag_step[posbk][j]!=99) {                
    if (diag_step[posbk][j]==88)  f2=0; 
    else if (f2==0) {     
     f2=pole[diag_step[posbk][j]];
     if  (f2==fb||f2==fq) return(true);    
    }
    j++;
  }
  f2=0;  j=0;
  while (stra_step[posbk][j]!=99) {                     
    if (stra_step[posbk][j]==88)  f2=0; 
    else if (f2==0) {         
     f2=pole[stra_step[posbk][j]];
     if (f2==fr||f2==fq) return(true);    
     if (j==0||stra_step[posbk][j]==88)
      if (f2==fk) return(true);    
    }
    j++;
  }
  j=0;
  while  (knight_step[posbk][j]!=99) {                
    if (pole[knight_step[posbk][j]]==fn)  return(true);                 
    j++;
  }    
  if (row[posbk]>2) {
    if (column[posbk]>1&&pole[posbk+7]==fp) return(true);
    if (column[posbk]<8&&pole[posbk+9]==fp)  return(true);
  }
  j=0;
  while (king_step[posbk][j]!=99) {                
    if (pole[king_step[posbk][j]]==fk) return(true);                 
    j++;
  }    
  return(false);  
}
//****************************
void sort_steps(int  l) { //      
step_t buf;  
   for (int i=0;i<pos[l].n_steps-1;i++) { //          
    int maxweight=pos[l].steps[i].weight;
    int maxj=i;
    for (int j=i+1;j<pos[l].n_steps;j++)  {
     if (pos[l].steps[j].weight>maxweight) {
      maxweight=pos[l].steps[j].weight;  maxj=j;
     }      
    }    
    if (maxweight==0&&l>0) return;    
    if (maxj==i) continue;    
    buf=pos[l].steps[i];
    pos[l].steps[i]=pos[l].steps[maxj];
    pos[l].steps[maxj]=buf;    
   }    
}
//****************************
int  evaluate(int l) { //                
  
  if (!stats) {  
    if (pos[l].w)  return pos[l].weight_w-pos[l].weight_b; else return pos[l].weight_b-pos[l].weight_w;                
  } else {    
   if (pos[l].w) return 5000*(pos[l].weight_w-pos[l].weight_b+pos[l].weight_s)/(pos[l].weight_w+pos[l].weight_b+2000);  
   else return 5000*(pos[l].weight_b-pos[l].weight_w-pos[l].weight_s)/(pos[l].weight_w+pos[l].weight_b+2000);        
  }
} 
//****************************
void taskOne( void *  parameter )
{
   task_time=millis();
   int l,j,check,f;
   signed char  f2;
   unsigned long task_tik;
   unsigned long task_count=0;
   do { //  
    if (task_start==2) {      
    // task_tik=micros(); 
     l=task_l;
     if (pole[poswk]!=fk) {      
      for (int i=0;i<64;i++) if (pole[i]==fk)  {  poswk=i; break; }     
     } 
     if (pole[posbk]!=-fk) {      
      for  (int i=0;i<64;i++) if (pole[i]==-fk) {  posbk=i; break; }        
     }
     if (l>0) {
      if (pos[l-1].steps[pos[l-1].cur_step].check==0) {           
       if (pos[l].w) pos[l].check_on_table=check_w(); else pos[l].check_on_table=check_b();        
       pos[l-1].steps[pos[l-1].cur_step].check=pos[l].check_on_table;          
      } else pos[l].check_on_table=1; 
     } else if (pos[0].w)  pos[0].check_on_table=check_w(); else pos[0].check_on_table=check_b();        
     n_steps2=0;
     for (int ii=0;ii<64;ii++) {
      int i;
      if  (pos[l].w) i=ii; else i=63-ii;
      f=pole[i];          
      if (f==0||f<0&&pos[l].w||f>0&&!pos[l].w)  continue;            
      if (f==fp) {
       if (row[i]<7&&pole[i-8]==0)  add_one(l,i,i-8); 
       if (row[i]==2&&pole[i-8]==0&&pole[i-16]==0) add_one(l,i,i-16);  
       if (row[i]==7) {
        if (pole[i-8]==0) {
         add_one(l,i,i-8);  steps2[n_steps2-1].type=4;      
         add_one(l,i,i-8); steps2[n_steps2-1].type=5;      
         add_one(l,i,i-8); steps2[n_steps2-1].type=6;      
         add_one(l,i,i-8);  steps2[n_steps2-1].type=7;      
        } 
        if (column[i]>1&&pole[i-9]<0)  {
         add_one(l,i,i-9); steps2[n_steps2-1].type=4;      
         add_one(l,i,i-9);  steps2[n_steps2-1].type=5;      
         add_one(l,i,i-9); steps2[n_steps2-1].type=6;      
         add_one(l,i,i-9); steps2[n_steps2-1].type=7;              
        }
        if (column[i]<8&&pole[i-7]<0) {
         add_one(l,i,i-7);  steps2[n_steps2-1].type=4;      
         add_one(l,i,i-7); steps2[n_steps2-1].type=5;      
         add_one(l,i,i-7); steps2[n_steps2-1].type=6;      
         add_one(l,i,i-7);  steps2[n_steps2-1].type=7;              
        }     
      } else {
       if (column[i]>1&&pole[i-9]<0) add_one(l,i,i-9); 
       if (column[i]<8&&pole[i-7]<0)  add_one(l,i,i-7);        
      }
    } else if (f==-fp) {
      if (row[i]>2&&pole[i+8]==0)  add_one(l,i,i+8); 
      if (row[i]==7&&pole[i+8]==0&&pole[i+16]==0) add_one(l,i,i+16);  
      if (row[i]==2) {
        if (pole[i+8]==0) {
         add_one(l,i,i+8);  steps2[n_steps2-1].type=4;      
         add_one(l,i,i+8); steps2[n_steps2-1].type=5;      
         add_one(l,i,i+8); steps2[n_steps2-1].type=6;      
         add_one(l,i,i+8);  steps2[n_steps2-1].type=7;      
        } 
        if (column[i]>1&&pole[i+7]>0)  {
         add_one(l,i,i+7); steps2[n_steps2-1].type=4;      
         add_one(l,i,i+7);  steps2[n_steps2-1].type=5;      
         add_one(l,i,i+7); steps2[n_steps2-1].type=6;      
         add_one(l,i,i+7); steps2[n_steps2-1].type=7;              
        }
        if (column[i]<8&&pole[i+9]>0) {
         add_one(l,i,i+9);  steps2[n_steps2-1].type=4;      
         add_one(l,i,i+9); steps2[n_steps2-1].type=5;      
         add_one(l,i,i+9); steps2[n_steps2-1].type=6;      
         add_one(l,i,i+9);  steps2[n_steps2-1].type=7;              
        }     
      } else {
       if (column[i]>1&&pole[i+7]>0) add_one(l,i,i+7); 
       if (column[i]<8&&pole[i+9]>0)  add_one(l,i,i+9);            
      }
     } else if (!endspiel&&abs(f)==fk)  add_king2(l,i);      
    } //    
    if (pos[l].pp!=0&&pole[pos[l].pp]==0)  { //   !!!
     if (pos[l].w==1) {
      if (column[pos[l].pp]>1&&pole[pos[l].pp+7]==fp)  { add_one(l,pos[l].pp+7,pos[l].pp); steps2[n_steps2-1].type=1; steps2[n_steps2-1].f2=-fp;}
      if (column[pos[l].pp]<8&&pole[pos[l].pp+9]==fp) { add_one(l,pos[l].pp+9,pos[l].pp);  steps2[n_steps2-1].type=1; steps2[n_steps2-1].f2=-fp;}
     } else {
      if  (column[pos[l].pp]>1&&pole[pos[l].pp-9]==-fp) { add_one(l,pos[l].pp-9,pos[l].pp);  steps2[n_steps2-1].type=1; steps2[n_steps2-1].f2=fp;}
      if (column[pos[l].pp]<8&&pole[pos[l].pp-7]==-fp)  { add_one(l,pos[l].pp-7,pos[l].pp); steps2[n_steps2-1].type=1; steps2[n_steps2-1].f2=fp;}
     }
    }
  //   task_execute+=micros()-task_tik;
     task_start=0;             
    } //if task_start   
    task_count++;
    if (task_count>7000000)  {
     task_count=0;
     if (Serial.available()) {
      String s=Serial.readString();
      s.trim();     
      s.toUpperCase();   
      if (s=="STOP") { halt=1;  Serial.println("STOPPED"); } //timelimith=millis(); }     
     } else delay(1);     
    }   
  } while (1);   
}
//****************************
int  action(step_t& s) { //  
int j;      
  signed char f2;
  if (s.f2!=0||s.type>0||abs(s.f1)>fr)  return 0; //       
  switch (s.f1) {  
  case fp: 
     if (row[s.c2]>5)  return 1; // 
     if (column[s.c2]>1&&pole[s.c2-9]<-fp||column[s.c2]<8&&pole[s.c2-7]<-fp)  return 1; // 
     return 0;     
  case -fp: 
     if (row[s.c2]<4) return  1; // 
     if (column[s.c2]>1&&pole[s.c2+7]>fp||column[s.c2]<8&&pole[s.c2+9]>fp)  return 1; // 
     return 0;          
  case fn:      
    j=0;
    while  (knight_step[s.c2][j]!=99) {                    
     if (pole[knight_step[s.c2][j]]<-fb)  return 1; //                
     j++;
    }      
    return 0;     
  case -fn:         
    j=0;
    while (knight_step[s.c2][j]!=99) {                    
     if (pole[knight_step[s.c2][j]]>fb) return 1; //               
     j++;
    }      
    return 0;     
  case fb: 
    j=0; f2=0;
    while  (diag_step[s.c2][j]!=99) {                
     if (diag_step[s.c2][j]==88) f2=0;  
     else if (f2==0) {     
       f2=pole[diag_step[s.c2][j]];
       if  (diag1[s.c1]!=diag1[stra_step[s.c2][j]]&&diag2[s.c1]!=diag2[stra_step[s.c2][j]])
        if (f2<-fb) return 1;              
     }
     j++;
    }   
    return 0;
  case -fb: 
    j=0; f2=0;
    while (diag_step[s.c2][j]!=99)  {                
     if (diag_step[s.c2][j]==88) f2=0; 
     else if (f2==0)  {     
       f2=pole[diag_step[s.c2][j]];
       if (diag1[s.c1]!=diag1[stra_step[s.c2][j]]&&diag2[s.c1]!=diag2[stra_step[s.c2][j]])
        if (f2>fb) return 1;              
     }
     j++;
    }       
    return 0;
  case fr: 
    j=0; f2=0;
    while (stra_step[s.c2][j]!=99)  {                
     if (stra_step[s.c2][j]==88) f2=0; 
     else if (f2==0)  {     
       f2=pole[stra_step[s.c2][j]];
       if (row[s.c1]!=row[stra_step[s.c2][j]]&&column[s.c1]!=column[stra_step[s.c2][j]])
        if (f2<-fr) return 1;              
     }
     j++;
    }           
    return 0;
  case -fr: 
    j=0; f2=0;
    while (stra_step[s.c2][j]!=99)  {                
     if (stra_step[s.c2][j]==88) f2=0; 
     else if (f2==0)  {     
       f2=pole[stra_step[s.c2][j]];
       if (row[s.c1]!=row[stra_step[s.c2][j]]&&column[s.c1]!=column[stra_step[s.c2][j]])
        if (f2>fr) return 1;       
     }              
     j++;
    }               
    return 0;  
  } //switch     
  return 0; 
}
//****************************  
void generate_steps(int l) {
 pos[l].cur_step=0; pos[l].n_steps=0; 
  int check;
 signed char f;  
 task_l=l;
 task_start=2; 
  
 for (int  ii=0;ii<64;ii++) {
  int i;
  if (pos[l].w) i=ii; else i=63-ii;
  f=pole[i];    
  if (f==0||f<0&&pos[l].w||f>0&&!pos[l].w) continue;
  switch (abs(f))  {   
   case fn: add_knight(l,i); break;   
   case fb: add_diag(l,i); break;
   case fr: add_stra(l,i); break;   
   case fq: add_stra(l,i); add_diag(l,i);  break;   
   case fk: if (endspiel) add_king(l,i); break;           
  }   
  } //     
  //int in=0;
  while (task_start==2) {  
   delayMicroseconds(0);
   //in=1;
  }  
  //if (in) countin++;
  //countall++;
 if (pos[l].w==1&&!pos[l].check_on_table)  { //   
  if (pos[l].wrk) { //     
   if (pole[60]==fk&&pole[61]==0&&pole[62]==0&&pole[63]==fr)  {
    pole[60]=0; pole[61]=fk; poswk=61; check=check_w(); pole[60]=fk; poswk=60;  pole[61]=0;    
    if (!check) {    
     pos[l].steps[pos[l].n_steps].type=2;      
     pos[l].steps[pos[l].n_steps].c1=60;      
     pos[l].steps[pos[l].n_steps].c2=62;      
     pos[l].steps[pos[l].n_steps].f1=fk;      
     pos[l].steps[pos[l].n_steps].f2=0;           
     pos[l].n_steps++;                      
    } 
   }
  }  
  if (pos[l].wrq) { // 
   if (pole[60]==fk&&pole[59]==0&&pole[58]==0&&pole[57]==0&&pole[56]==fr)  {
    pole[60]=0; pole[59]=fk; poswk=59; check=check_w(); pole[60]=fk; poswk=60;  pole[59]=0;    
    if (!check) {        
     pos[l].steps[pos[l].n_steps].type=3;      
     pos[l].steps[pos[l].n_steps].c1=60;      
     pos[l].steps[pos[l].n_steps].c2=58;      
     pos[l].steps[pos[l].n_steps].f1=fk;      
     pos[l].steps[pos[l].n_steps].f2=0;           
     pos[l].n_steps++;                      
    } 
   }
  }
 } else if (pos[l].w==0&&!pos[l].check_on_table) { //   
  if (pos[l].brk)  { // 
   if (pole[4]==-fk&&pole[5]==0&&pole[6]==0&&pole[7]==-fr) {
    pole[4]=0;  pole[5]=-fk; posbk=5; check=check_b(); pole[4]=-fk; posbk=4; pole[5]=0;   
    if  (!check) {    
     pos[l].steps[pos[l].n_steps].type=2;      
     pos[l].steps[pos[l].n_steps].c1=4;      
     pos[l].steps[pos[l].n_steps].c2=6;      
     pos[l].steps[pos[l].n_steps].f1=-fk;      
     pos[l].steps[pos[l].n_steps].f2=0;           
     pos[l].n_steps++;                      
    } 
   }
  }
  if (pos[l].brq) { //
   if  (pole[4]==-fk&&pole[3]==0&&pole[2]==0&&pole[1]==0&&pole[0]==-fr) {    
    pole[4]=0;  pole[3]=-fk; posbk=3; check=check_b(); pole[4]=-fk; posbk=4; pole[3]=0;   
    if  (!check) {    
     pos[l].steps[pos[l].n_steps].type=3;      
     pos[l].steps[pos[l].n_steps].c1=4;      
     pos[l].steps[pos[l].n_steps].c2=2;      
     pos[l].steps[pos[l].n_steps].f1=-fk;      
     pos[l].steps[pos[l].n_steps].f2=0;           
     pos[l].n_steps++;                      
    }
   }  
  }
 }  
 for (int i=0;i<n_steps2;i++)  {       
  pos[l].steps[pos[l].n_steps]=steps2[i];
  pos[l].n_steps++;
  }
 for (int i=0;i<pos[l].n_steps;i++) {       
  pos[l].steps[i].check=0;
  pos[l].steps[i].weight=abs(pos[l].steps[i].f2);  
  if (pos[l].steps[i].type>3)  pos[l].steps[i].weight+=fig_weight[pos[l].steps[i].type-2];    
  pos[l].steps[i].weight<<=2;
  if (l>0) { //       
       if (pos[l].best.c2==pos[l].steps[i].c2&&pos[l].best.c1==pos[l].steps[i].c1)  pos[l].steps[i].weight+=5; //                         
       if (pos[l].steps[i].c2==pos[l-1].steps[pos[l-1].cur_step].c2)  pos[l].steps[i].weight+=8; //                      
  }
  //if (l<level) {
   //if (action(pos[l].steps[i])) {
   //  pos[l].steps[i].weight=1;
   //  show_position();   
   //    Serial.println("="+str_step(pos[l].steps[i]));      
   //   delay(20000);
   //}
  //}
 

 } //i     
  //Serial.println("TIME  GENER="+String(micros()-sta)); 
 //show_steps(0);
 //delay(100000000); 
  sort_steps(l); 
 //halt=1;
 //show_steps(0);   
 
}
//****************************  
int draw_repeat(int l) {
 if (l<=12||zero) return 0;
 for (int i=0;i<4;i++)  {
  int li=l-i; 
  if (pos[li].steps[pos[li].cur_step].c1!=pos[li-4].steps[pos[li-4].cur_step].c1||
      pos[li].steps[pos[li].cur_step].c1!=pos[li-8].steps[pos[li-8].cur_step].c1||
      pos[li].steps[pos[li].cur_step].c2!=pos[li-4].steps[pos[li-4].cur_step].c2||
      pos[li].steps[pos[li].cur_step].c2!=pos[li-8].steps[pos[li-8].cur_step].c2)  return 0;
 }
 if (TRACE>0) Serial.println("repeat!");
 return 1;
}
//****************************  
int active(step_t& s) { //     
int j;  
  if (s.f2!=0||s.type>3) return  1; //  
  if (abs(s.f2)==fk) return -1; //   ,   ..
  switch (s.f1) {  
  case fp: 
     if (row[s.c2]>5) return 1; // 
     if (column[s.c2]>1&&posbk==s.c2-9||column[s.c2]<8&&posbk==s.c2-7)  return 1; // 
     return -1;     
  case -fp: 
     if (row[s.c2]<4) return  1; // 
     if (column[s.c2]>1&&poswk==s.c2+7||column[s.c2]<8&&posbk==s.c2+9)  return 1; // 
     return -1;          
  case fn:      
    j=0;
    while  (knight_step[s.c2][j]!=99) {                    
     if (pole[knight_step[s.c2][j]]==-fk)  return 1; //             
     j++;
    }      
    return 0;     
  case -fn:         
    j=0;
    while (knight_step[s.c2][j]!=99) {                    
     if (pole[knight_step[s.c2][j]]==fk) return 1; //             
     j++;
    }      
    return 0;     
  case fb: 
    if (diag1[s.c2]!=diag1[posbk]&&diag2[s.c2]!=diag2[posbk])  return -1; //  
    return 0;
  case -fb: 
    if (diag1[s.c2]!=diag1[poswk]&&diag2[s.c2]!=diag2[poswk])  return -1; //    
    return 0;
  case fr: 
    if (row[s.c2]!=row[posbk]&&column[s.c2]!=column[posbk])  return -1; // - 
    return 0;
  case -fr: 
    if (row[s.c2]!=row[poswk]&&column[s.c2]!=column[poswk])  return -1; // - 
    return 0;
  case fq: 
    if (diag1[s.c2]!=diag1[posbk]&&diag2[s.c2]!=diag2[posbk]&&        //    
        row[s.c2]!=row[posbk]&&column[s.c2]!=column[posbk]) return  -1; // - 
    return 0;
  case -fq: 
    if (diag1[s.c2]!=diag1[poswk]&&diag2[s.c2]!=diag2[poswk]&&        //    
        row[s.c2]!=row[poswk]&&column[s.c2]!=column[poswk]) return  -1; // - 
    return 0;
  } //switch     
  return 0; 
}
//****************************  
int quiescence(int l, int alpha, int beta, int depthleft) {     
  if (depthleft<=0)  { 
    if (l>depth) depth=l;
    return evaluate(l);
  }
  int score=-20000;             
  generate_steps(l);  
  if (!pos[l].check_on_table) {   
    int weight=evaluate(l); 
    if (weight >= score) score=weight; 
    if  (score>alpha) alpha=score;    
    if (alpha>=beta) return alpha;                
  }                      
  int check,checked,act; 
  for (int i=0;i<pos[l].n_steps;i++)  {            
   act=1;
   if (!pos[l].check_on_table) {
    act=active(pos[l].steps[i]);
    if (act==-1) continue;    
   }
   movestep(l,pos[l].steps[i]);        
   check=0;   
   if (act==0) {
    if (pos[l].w) check=checkd_b(); else check=checkd_w();       
    pos[l].steps[i].check=check;
     if (!check) { backstep(l,pos[l].steps[i]);  continue; }      
   }   
   if (pos[l].w) checked=check_w(); else checked=check_b();     
   if (checked) { backstep(l,pos[l].steps[i]);  continue; } //  -            
   
   if (check&&depthleft==1&&l<MAXDEPTH-1) depthleft++;     
   
   pos[l].cur_step=i;                       
   
   movepos(l,pos[l].steps[i]);   
   int tmp=-quiescence(l+1,-beta,-alpha,depthleft-1);   
   backstep(l,pos[l].steps[i]);
   if (draw_repeat(l)) tmp=0;    
   if (tmp>score) score=tmp;
   if (score>alpha) {
    alpha=score;      
    pos[l].best=pos[l].steps[i];          
   }
   if (alpha>=beta ) return  alpha;    
  } 
  if (score==-20000) {
    if (pos[l].check_on_table) {
      score=-10000+l; pos[l-1].steps[pos[l-1].cur_step].check=2; 
    }
  }       
  return score;
}
//****************************  
int alphaBeta(int  l, int alpha, int beta, int depthleft) {
 int score=-20000, check, ext, tmp;      
   if (depthleft<=0) {
      int fd=fdepth; //4-6-8         
      if  (pos[l-1].steps[pos[l-1].cur_step].f2!=0) fd+=2;       ; //                 
      return quiescence(l,alpha,beta,fd);                    
   }   
   if  (l>0) generate_steps(l);   
   if (l>=nulldepth&&zero==0&&depthleft>2)      //2
    if (!pos[l].check_on_table&&pos[l-1].steps[pos[l-1].cur_step].f2==0)  {
     zero=1;  
     pos[l+1].wrk=pos[l].wrk; pos[l+1].wrq=pos[l].wrq; 
     pos[l+1].brk=pos[l].brk;  pos[l+1].brq=pos[l].brq;
     pos[l+1].weight_w=pos[l].weight_w; pos[l+1].weight_b=pos[l].weight_b;  pos[l+1].weight_s=pos[l].weight_s;
     pos[l+1].pp=0;    
     pos[l].cur_step=MAXSTEPS;
     pos[l].steps[MAXSTEPS].f2=0;     
     int tmpz=-alphaBeta(l+1,-beta,-beta+1,depthleft-3);   //3
     zero=0;    
     if (tmpz>=beta) return beta;  
    }    
   if (l>4&&!zero&&depthleft<=2&&futility&&!pos[l].check_on_table&&pos[l-1].steps[pos[l-1].cur_step].f2==0)  { //futility pruning
     int weight=evaluate(l); 
     if (weight-200>=beta)  return beta;
   }   
   for (int i=0;i<pos[l].n_steps;i++) {       
    ext=0;
    if (l==0) { 
      depth=depthleft;                      
      if (level<7)  if (pos[0].steps[pos[0].cur_step].check) ext=2; // 
    }
    movestep(l,pos[l].steps[i]);        
    if (pos[l].w) check=check_w(); else check=check_b();        
    if (check) { backstep(l,pos[l].steps[i]);  continue; } //  -      
    pos[l].cur_step=i;
    movepos(l,pos[l].steps[i]);    
    if (TRACE>0) {
     if (l==0) {              
       Serial.print(str_step(pos[0].steps[i])); Serial.print("  ");
       Serial.print(i+1);  Serial.print("/"); Serial.print(pos[0].n_steps);                        
       //if  (pos[0].steps[i].weight<-9000) { Serial.println(F(" checkmate")); continue; }        
     } else if (TRACE>l) {  
       Serial.println();             
       for (int ll=0;ll<l;ll++) Serial.print("      "); 
       Serial.print(String(l+1)+"-  "+str_step(pos[l].steps[i]));       
     }
    } //TRACE         
    if  (l>2&&!lazy&&!zero&&lazyeval&&!pos[l].steps[i].f2&&!pos[0].steps[pos[0].cur_step].check&&evaluate(l+1)+100<=alpha&&
     (pos[l].w&&!check_b()||!pos[l].w&&!check_w())) {
      lazy=1;
      if  (-alphaBeta(l+1,-beta,-alpha,depthleft-3)<=alpha) tmp=alpha; 
       else { lazy=0;  tmp=-alphaBeta(l+1,-beta,-alpha,depthleft-1+ext);   }
      lazy=0;
    }  else tmp=-alphaBeta(l+1,-beta,-alpha,depthleft-1+ext);   
    backstep(l,pos[l].steps[i]);    
    if (draw_repeat(l)) tmp=0;
    if (l==0) ui_poll();
    if (tmp>score) score=tmp;
    pos[l].steps[i].weight=tmp;    
    if (score>alpha) {
      alpha=score;      
      pos[l].best=pos[l].steps[i];                  
      if (l==0&&level>3) //    
       if (print_best(depthleft))  return alpha;
    }
    if (l==0) {         
        if (TRACE>0) Serial.println("        "+String(tmp));            
        //if (alpha==9999||alpha==-5000)  break;                                                           
    } else  if (TRACE>l) {         
        Serial.print(" = "+String(tmp));                
    }             
    if (alpha>=beta) return alpha;                
    if  (halt||(l<3&&millis()-starttime>timelimith))  //   
      return score;                        
   }   
   if (score==-20000) {
    if (pos[l].check_on_table) {
      score=-10000+l;  pos[l-1].steps[pos[l-1].cur_step].check=2; 
    } else score=0;
   }       
   return score;
}
//****************************  
boolean is_draw() {  //   
  boolean draw=false;
  int cn=0,cbw=0,cbb=0,co=0,cb=0,cw=0;
  for  (int i=0;i<64;i++) {  
    if (abs(pole[i])==1) co++;  
    if (abs(pole[i])>3&&abs(pole[i])<6)  co++; // , ,   
    if (abs(pole[i])==6) continue;  // 
    if (abs(pole[i])==2)  cn++; // 
    if (abs(pole[i])==3&&(column[i]+row[i])%2==0) cbb++; //  
    if  (abs(pole[i])==3&&(column[i]+row[i])%2==1) cbw++; //   
    if (pole[i]==3) cw++;  //     
    if (pole[i]==-3) cb++; //     
  }
  if (cn==1&&co+cbb+cbw==0)  draw=true;  // 
  if (cbb+cbw==1&&co+cn==0) draw=true;  // 
  if (co+cn+cbb==0||co+cn+cbw==0)  draw=true; //    
  if (co+cn==0&&cb==1&&cw==1) draw=true; //    
  return  draw;  
}
//****************************
String get_time(long tim) {  
  char sz[10];      
  if (tim>360000) tim=0;      
  sprintf(sz, "%02d:%02d:%02d",  tim/3600,(tim%3600)/60,tim%60);      
  return String(sz);
}
//****************************
boolean  print_best(int dep) {
  if (halt||millis()-starttime>timelimith) return false;
  if (lastbestdepth==dep&&pos[0].best.type==lastbeststep.type&&
   pos[0].best.c1==lastbeststep.c1&&pos[0].best.c2==lastbeststep.c2)  return false;
  boolean ret=false; 
  if (pos[0].best.type==lastbeststep.type&&pos[0].best.c1==lastbeststep.c1&&pos[0].best.c2==lastbeststep.c2)  {   
   for (int i=0;i<MAXEPD;i++) {    
    if (lastbeststep.c2==bestmove[i].c2&&lastbeststep.c1==bestmove[i].c1&&lastbeststep.type==bestmove[i].type)  {
      ret=true; bestsolved=1; break;
    }
   }
  }
  lastbestdepth=dep;
  lastbeststep=pos[0].best;  
  if (pos[0].w==0) Serial.print("1..."); else  Serial.print("1.");
  String st=str_step(pos[0].best);
  Serial.print(st);
  for (int i=0;i<10-st.length();i++) Serial.print(" ");  
  String depf="/"+String(depth+1)+"  ";
  long tim=(millis()-starttime)/1000;
  String wei=String(pos[0].best.weight/100.,2);
  if (pos[0].best.weight>9000) wei="+M"+String((10001-pos[0].best.weight)/2);
  
  Serial.println("("+wei+") Depth: "+String(dep+depf)+get_time(tim)+"  "+String(count/1000)+"kN");  
  return ret;
}         
//****************************
void  kingpositions() {
  for (int i=0;i<64;i++) { //  
    if (pole[i]==fk) poswk=i;  
    if (pole[i]==-fk) posbk=i;
  } 
}  
//****************************  
boolean solve_step() {
int score; 
  boolean solved=0;   
  count=0;
  countin=0; countall=0;    
  zero=0; lazy=0;
  for (int i=1;i<MAXDEPTH;i++)  {
   if (i%2) pos[i].w=!pos[0].w; else pos[i].w=pos[0].w;    
   pos[i].pp=0;
  } 
  starttime=millis();  
  task_execute=0;
  if (is_draw()) { Serial.println("  DRAW!"); return 1; } //  
  lastbestdepth=-1;
  lastbeststep.type=0; lastbeststep.c1=-1;  lastbeststep.c2=-1;     
  bestsolved=0;    
  
  pos[0].weight_b=0; pos[0].weight_w=0;  
  for (int i=0;i<64;i++) { //    
    if (pole[i]<0) {
      pos[0].weight_b+=fig_weight[-pole[i]];      
    } else if (pole[i]>0) { 
      pos[0].weight_w+=fig_weight[pole[i]];  //   8000           
    }        
  }    
  if (pos[0].weight_w+pos[0].weight_b<3500)  endspiel=true; else endspiel=false;    //3500?
  //Serial.println("endspiel="+String(endspiel));
  
  pos[0].weight_s=0; 
  for (int i=0;i<64;i++) { //   
   int f=pole[i];
   if (!f) continue;
   if (abs(f)==fk&&endspiel) {
     if (f<0) 
       pos[0].weight_s-=stat_weightb[6][i];                               
     else 
       pos[0].weight_s+=stat_weightw[6][i];                                 
   } else {                
     if (f<0)  
      pos[0].weight_s-=stat_weightb[-f-1][i];                               
     else 
      pos[0].weight_s+=stat_weightw[f-1][i];                                     
   }      
  }  

  kingpositions(); //  
  
  if (TRACE>0) Serial.println("  start score="+evaluate(0));

  generate_steps(0);     
  
  int legal=0;
  int check;
  int samebest=0;  
  for (int i=0;i<pos[0].n_steps;i++) {     //       
    movestep(0,pos[0].steps[i]);   
    if (pos[0].w) check=check_w();  else check=check_b();           
    pos[0].check_on_table=check;
    if  (!check) legal++;         
    if (!check) pos[0].steps[i].weight=0; else pos[0].steps[i].weight=-30000;
    backstep(0,pos[0].steps[i]);
  }  
  if (legal==0) 
   if (pos[0].check_on_table)  { Serial.println(" CHECKMATE!"); return 1; } // 
    else { Serial.println("  PAT!"); return 1; } // 
    
  sort_steps(0);
  pos[0].n_steps=legal;  //    
  
  int ALPHA=-20000; 
  int BETA=20000; 
  level=2; 
  if  (timelimith>300000) level=4;      
  for (int x=0;x<MAXDEPTH;x++) { 
    pos[x].best.f1=0;  pos[x].best.c2=-1;     
  } //             

  stats=1;
  
  while  (level<=20) {
   if (TRACE>0) {
    Serial.print(F("******* LEVEL=")); Serial.print(level);
    Serial.println();       
   }      
   for (int x=1;x<MAXDEPTH;x++) {  
    pos[x].best.f1=0; pos[x].best.c2=-1;     
   } //    0 
   for (int  i=0;i<pos[0].n_steps;i++) {     //       
    movestep(0,pos[0].steps[i]);   
    if (pos[0].w) pos[0].steps[i].check=check_b(); 
     else pos[0].steps[i].check=check_w();           
    pos[0].steps[i].weight+=evaluate(0)+pos[0].steps[i].check*500;    
    if (pos[0].steps[i].f2!=0) pos[0].steps[i].weight-=pos[0].steps[i].f1;        
    backstep(0,pos[0].steps[i]);
   }  
   pos[0].steps[0].weight+=10000;  // -  
   sort_steps(0);     
   for (int i=0;i<pos[0].n_steps;i++) pos[0].steps[i].weight=-8000;  //            
   if (nullmove) nulldepth=3; else  nulldepth=93;       
   //BETA=10000; ALPHA=9900; 
   //int sec=(millis()-starttime)/1000;   
   fdepth=4;   
   score=alphaBeta(0,ALPHA,BETA,level);   
   unsigned long  tim=millis()-starttime;        
   boolean out=0;
   if (score>=BETA) out=1;
   if (multipov||samebest>2||out) { 
    samebest=0;          
    ALPHA=-20000;     
    BETA=20000;      
   } else {
    ALPHA=score-100;
    BETA=score+100;      
   }          
   if (tim>timelimith*0.2&&!out) { 
    stats=0;         
    ALPHA=score-300;     
    BETA=score+300;     
   }       
   
   sort_steps(0);    //                      
   if (print_best(level)||bestsolved||score>9900)  { solved=1; break; }     
   if (tim>timelimith||halt) break;       
   if  (pos[0].best.type==lastbeststep.type&&pos[0].best.c1==lastbeststep.c1&&pos[0].best.c2==lastbeststep.c2)  
    { samebest++; } else samebest=0;              
   
   level++; 
   
   //Serial.println(level);
   //Serial.println(tim/1000);
  } //while  level    
  //Serial.println(String(countin)+"/"+String(countall));     
  //Serial.println("Task load: "+String(0.1*task_execute/(millis()-starttime))+"%");     
  return solved;
}
//****************************
unsigned int  getpacked() { //    hash  
 int c=0; unsigned int hash=0;
 for (int i=0;i<8;i++)  {
  polep.q[i]=0; 
  for (int j=0;j<8;j++) {      
   polep.q[i]<<=4;   
   polep.q[i]+=pole[c]&B1111;             
   //hash^=polep.q[i];   
   c++;   
  }  
  hash^=polep.q[i];
 }
 //return hash%MAXHASH;
}
//****************************
void  printBits(int myInt){
 unsigned int mask=1;
 mask<<=31; 
 for (int i=0;i<32;i++)  {   
   if(mask  & myInt)
       Serial.print('1');
   else
       Serial.print('0');
   mask=mask>>1;    
 }
 Serial.println();
}
//****************************
void  elo(int numelo=0) { //ELOmeter tests
 String elos[76]={   
  "7k/1K6/8/4q3/8/2B5/2P5/8  w - -", //Bc3xe5+
  "R7/5qpk/1p6/2p1rb1p/2Q5/2P3P1/1P3P1P/7K w - -", //Qc4xf7
  "k7/8/3r4/8/4N3/5K2/5P2/8 w - -", //Ne4xd6
  "8/8/8/5K2/8/4R3/p2k4/8 w -  -", //Re3-a3
  "8/8/2N5/8/8/4k3/p6K/8 w - -", //Nc6-b4
  "8/1q4k1/8/8/8/6K1/8/1R6  w - -", //Rb1xb7+
  "7k/q5pp/1P6/8/8/8/6PP/7K w - -", //b6xa7
  "rr3b1k/2p2p2/b4N1p/q3p3/2P5/8/PP3P2/KN1R2R1  w - -", //Rg1-g8#
  "r3k2r/5ppp/p3p3/1p1p4/1PpP4/2P1P3/P3KPPP/RR6 w - -",  //Ke2-f1
  "8/8/8/1Pk5/8/8/2P3K1/8 w - -", //c2-c4
  "5rk1/2b2ppp/pp3n2/2p1p1B1/4P3/2NP4/PPP2PPP/5RK1  w - -", //Bg5xf6
  "r3q2k/5Q1p/1pb2bp1/5p2/2B5/PP3N2/K1P3P1/8 w - -", //Qf7xf6#
  "8/R7/1p2k3/2p1q1p1/2P1Q3/1P2K1P1/7r/8 w - -", //Ra7-e7+
  "1b3rk1/2q3p1/p7/2p3N1/2p4P/2P3n1/1PQ3P1/4R1K1  w - -", //Qc2-h7#
  "8/8/8/6p1/5kP1/7K/8/8 w - -" //Kh3-g2
  "2b5/ppp1k1pp/2n1p3/1BNp1p2/3P3P/PP2P1P1/1P1K1P2/8  w - -", //Bb5xc6
  "2r4k1/1ppq1pp1/p1n2n1p/8/3P4/1PBQ1N1P/P4PP1/3R2K1 w - -",  //d4-d5
  "r2qk2r/ppp2ppp/1n1p1nb1/8/2PP4/2NB2P1/PP3PP1/R1BQ1RK1 w - -", //Qd1-e2+
  "8/2P5/3K4/5b2/1p6/6k1/8/8 w - -", //Kd6-e5
  "2r3r1/4Nppk/5b2/qppP4/8/1P3P2/P1P3P1/1K1RR3  w - -", //Re1-h1+
  "3R1rk1/p5pp/1p1Q4/5qP1/4Nn2/1P6/P5PP/7K w - -", //Ne4-f6+
  "r2qr2k/6pp/pp1p4/3Pn1N1/8/1P4P1/P2Q3P/R3R1K1 w - -", //Re1xe5
  "r2q1rk1/1ppn1ppp/p2np3/3p4/B2P4/2P1PN2/P1P1QPPP/R4RK1  w - -", //Ba4-b3
  "5q2/1p2k3/p2p4/5bp1/1B6/P2P1P2/2K3P1/1Q6 w - -", //Bb4xd6+
  "1r3rk1/5ppp/8/1pbN3Q/2p1P3/2Bn1P2/1P3qPP/RR5K w - -", //Nd5-f6+
  "3r1r1k/p3bpR1/1p1p1n1p/4pP1q/2N1P3/1P2BPQP/P4K2/6R1  w - -", //Rg7-h7+
  "rr3nk1/6q1/2p1pRP1/3pP1Qp/6p1/2P3P1/N6P/K1R5 w - -",//Rf6-f7
  "1r2r3/p1q2p1k/R5p1/3p3b/7Q/1P4R1/3B1PPP/6K1 w - -", //Qh4xh5+
  "4br2/p1q1p1k1/4Q1p1/1pN2n2/1P1b4/8/P3B1PP/4BR1K  w - -", //Qe6xf5
  "8/p7/8/PP1k4/1K6/8/8/8 w - -", //a5-a6
  "6r1/pkpq3p/6p1/2P5/2NPnP2/1P5R/7K/3Q4  w - -", //c5-c6+
  "r4rk1/2qnppb1/4p1p1/4P1N1/1p1P1P2/1P6/1KP1N3/3R2QR w -  -", //Rh1-h8+
  "1rb1k2r/2p2ppp/2p5/4p3/2P1n1q1/3Q4/PPPB2PP/2KR2NR w - -",  //Qd3-d8+
  "4r1k1/p4p1p/1p3qpB/3b4/1P1R4/P1Q5/5PPP/6K1 w - -", //Rd4-e4
  "r4r1k/1p1bbppp/1qp1pn2/4B3/3P3P/1P1B1QR1/P3NPP1/1K1R4 w - -", //Bd3-c2?
  "3k1r2/4R3/2pB4/pqP2b1p/3P4/2Pp2P1/8/2K1Q3 w - -", //Re7-d7+
  "r3r1k1/4b1pp/3p3P/6P1/2p1b1q1/4B3/PP1Q2B1/K5RR  w - -", //Qd2-d5+
  "rr4k1/R1Q2ppp/4pq2/8/7n/3P4/2P2NPP/5R1K w - -", //Qc7-b7
  "7R/8/8/8/3kp3/8/r7/4K3 w - -", //Rh8-d8+
  "3B2k1/1b3p1p/p5p1/1p5q/3Q4/1P3P2/P1r3PP/3R2K1  w - -", //Qd4-h8+
  "8/8/3p4/2k5/4P3/8/8/1K6 w - -", //Kb1-a2
  "r2qr1k1/1bpn1p1p/1p3bp1/p3p3/P1P5/4PN1P/1PQ1BPPB/R2R2K1  w - -", //e3-e4
  "3rkn2/1Q2b2p/2p1p3/p2q4/n3NP2/2P1K1R1/P2B3P/2N5 w - -",  //Qb7xe7+
  "8/p7/Pp1kB3/1Ppn2K1/2P5/8/8/8 w - -", //Be6xd5
  "r1bqk2r/pp1n1ppp/2p2n2/3p4/P2Pp3/2P1P3/2PN1PPP/R1BQKB1R  w - -", //Bc1-a3
  "r2k3r/pp3pb1/3p3p/1BpP2p1/Q7/P1P2q2/1P3P2/R3R1K1 w - -",  //Bb5-e8
  "8/6P1/5K1r/8/8/8/8/3k4 w - -", //Kf6-f5 !
  "r4r2/2k2qpp/pRbbR3/P2p1P2/P1pP2P1/2B2Q2/2B5/6K1  w - -", //Rb6xc6+
  "rn2k2r/1bQ1qpp1/p3p3/2bp4/P2N3p/2PB4/1P3PPP/R1B1R1K1 w  - -", //Nd4xe6
  "8/8/5k2/8/p7/8/1PK5/8 w - -", //Kc3
  "rr4k1/1q3ppp/1bQRp3/6P1/4P3/P4N2/1P6/K3R3  w - -", //Rd6-d8+
  "r3kb1r/pp4p1/2p1p1p1/4p3/8/2PqPQ1P/PP1N2P1/1R2K2R w -  -", //Rh1-f1
  "2r2rk1/1R3pp1/n3pP1n/q2p4/p2P1NP1/R2Q1P1p/2N4P/6K1 w - -",  //Qd3-g6
  "r3k1nr/pp1nq1p1/1bp1b2p/3pPp2/3P1B2/2PB1N2/PP1NQ1PP/R3K2R w - -",  //Qe2-f1
  "2k4r/p1b2p2/Pp1r1q1p/1Pp1p3/4P1pP/2P1R1P1/5PB1/1RQ3K1 w - -", //c3-c4
  "r1bq1rk1/ppp2pp1/2np1n1p/3Np1NQ/8/1B1P4/PPP2PP1/2KR3R w - -", //Qh5-g6
  "r2q1rk1/pbp1bppp/1p2pn2/6B1/3P4/3B1N2/PPP1QPPP/R4RK1 w - -", //Rf1-d1
  "3kN2b/2p4P/2p2p2/2P2P1K/8/8/8/8  w - -", //Ne8xc7
  "r2qrnk1/pp2bppp/2p1bn2/3p2B1/3P4/2NBPN2/PPQ2PPP/1R3RK1  w - -" //Qc2-b3
  "rnbq1rk1/ppp4p/3p2p1/3Pp2n/2P1Pp1b/2N2P2/PP1QNBPP/2KR1B1R  w - -", //Bf2xh4
  "8/8/5p2/5p2/5P2/3p3B/5k1P/3K4 w - -", //Kd1-d2
  "5rk1/R6p/3pp3/2p1n2r/2q5/2B5/1PQ2PPP/5RK1  w - -", //Ra7-g7+
  "r6k/1p4pp/p2q4/1r3P2/2npQ2P/6P1/R1PB1P2/2K1R3 w - -",  //Ra2xa6
  "5N2/4P3/7k/6r1/8/8/8/4K3 w - -", //Nf8-g6
  "8/8/8/8/3p1B2/4p3/5p2/5K1k  w - -", //Bf4-g3
  "r4r2/pppqbpk1/2n1b2p/4p1p1/3pP3/3P2PP/PPPN1PBK/R2Q1RN1  w - -", //66
  "4r2k/5Qpp/8/2N5/3n2p1/8/2P3PP/4qR1K w - -",
  "rn2k2r/p4pp1/1p2p2p/1P1pPn1P/2qP4/5N2/2PB1PP1/RQ2K2R  w - -",
  "8/8/8/4p1p1/8/5P2/6K1/3k4 w - -", //69
  "r4rk1/pb1qn1pp/1pnNp3/3pPp2/PP1P1P2/5N2/3Q2PP/R3KB1R  w - -",
  "3rr1k1/3nbppp/p1R2n2/qp1Bp1B1/4P3/5N1P/PP3PP1/2RQ2K1 w - -", //71
  "4r1k1/1b4pp/p1p1r3/2Pp1qb1/PP2p3/2N3B1/4RPPP/3RQ1K1 w - -", //???
  "6k1/p1P5/P1r5/2p1K3/8/8/2R5/8  w - -",
  "r4r2/6kp/2pqppp1/pbR5/3P4/4QN2/PP3PPP/2R3K1 w - -",
  "4k2r/1p3ppp/p1n5/2r1p3/4P3/2N5/PP2KPPP/2RR4  w - -", //75
  "2r5/pp2kp2/3q1p1Q/3Pp3/6b1/1B6/P1P3PP/1K3R2 w - -" 
  
  };
}

// ============================================================
//                    CYD ARAYUZ KISMI
// ============================================================
#define SQ 30      // kare boyutu (piksel)
#define PX 240     // yan panel baslangic x

static const uint16_t C_LIGHT  = RGB565(240,217,181);
static const uint16_t C_DARK   = RGB565(181,136,99);
static const uint16_t C_LASTL  = RGB565(205,210,106);
static const uint16_t C_LASTD  = RGB565(170,162,58);
static const uint16_t C_SEL    = RGB565(110,170,90);
static const uint16_t C_CHECK  = RGB565(235,90,75);
static const uint16_t C_DOT    = RGB565(60,110,50);
static const uint16_t C_PANEL  = RGB565(32,32,38);
static const uint16_t C_TXT    = RGB565(235,235,235);
static const uint16_t C_ACCENT = RGB565(255,200,60);
static const uint16_t C_BTN    = RGB565(70,90,130);
static const uint16_t C_STOP   = RGB565(190,60,50);
static const uint16_t C_WP     = RGB565(250,250,250);
static const uint16_t C_BP     = RGB565(40,40,40);
static const uint16_t C_BLACK  = RGB565(0,0,0);

static const char START_FEN[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
static const unsigned long LEVEL_MS[4] = {1000, 3000, 8000, 20000};

static const Btn B_NEW  = {244,  92, 72, 30};
static const Btn B_UNDO = {244, 126, 72, 30};
static const Btn B_LVL  = {244, 160, 72, 30};
static const Btn B_COL  = {244, 194, 72, 30};
static const Btn B_STOP = {244, 150, 72, 64};

LGFX tft;
Preferences prefs;

bool playerWhite = true;
int  gstate = 0;                 // 0 = insan, 1 = motor, 2 = oyun bitti
int  levelIdx = 1;
step_t legalMoves[MAXSTEPS];
int  nLegal = 0;
bool inCheck = false;
int  checkSq = -1;
int  selSq = -1;
bool target[64];
int  lastFrom = -1, lastTo = -1;
String lastTxt = "", stA = "", stB = "";
volatile bool thinking = false;
unsigned long thinkStart = 0, lastPoll = 0, lastTimer = 0;

bool inBtn(int x, int y, const Btn &b) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

void drawCentered(const char *s, int cx, int cy) {
  int w = tft.textWidth(s);
  int h = tft.fontHeight();
  tft.drawString(s, cx - w / 2, cy - h / 2);
}

bool humanTurn() { return (pos[0].w != 0) == playerWhite; }

// ---------------- cizim ----------------
void drawPiece(int x, int y, int f) {
  bool white = f > 0;
  int k = abs(f);
  uint16_t fill = white ? C_WP : C_BP;
  uint16_t edge = white ? C_BP : RGB565(220,220,220);
  uint16_t txt  = white ? C_BP : C_WP;
  tft.fillCircle(x + 15, y + 15, 12, fill);
  tft.drawCircle(x + 15, y + 15, 12, edge);
  tft.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  tft.setTextColor(txt);
  char s[2] = { "?PNBRQK"[k], 0 };
  drawCentered(s, x + 15, y + 15);
}

void drawSquare(int idx) {
  int c = idx % 8, r = idx / 8;
  int sc = playerWhite ? c : 7 - c;
  int sr = playerWhite ? r : 7 - r;
  int x = sc * SQ, y = sr * SQ;
  bool dark = ((r + c) & 1);
  uint16_t col = dark ? C_DARK : C_LIGHT;
  if (idx == lastFrom || idx == lastTo) col = dark ? C_LASTD : C_LASTL;
  if (idx == selSq) col = C_SEL;
  if (inCheck && idx == checkSq) col = C_CHECK;
  tft.fillRect(x, y, SQ, SQ, col);
  int f = pole[idx];
  if (f != 0) drawPiece(x, y, f);
  if (target[idx]) {
    if (f == 0) tft.fillCircle(x + 15, y + 15, 5, C_DOT);
    else { tft.drawCircle(x + 15, y + 15, 14, C_DOT); tft.drawCircle(x + 15, y + 15, 13, C_DOT); }
  }
  tft.setFont(&lgfx::fonts::Font0);
  tft.setTextColor(dark ? C_LIGHT : C_DARK);
  if (sc == 0) { char t[2] = { (char)('0' + 8 - r), 0 }; tft.drawString(t, x + 2, y + 2); }
  if (sr == 7) { char t[2] = { (char)('a' + c), 0 };     tft.drawString(t, x + SQ - 8, y + SQ - 9); }
}

void drawBoard() { for (int i = 0; i < 64; i++) drawSquare(i); }

void drawStatus() {
  tft.fillRect(PX, 0, 80, 90, C_PANEL);
  tft.setFont(&lgfx::fonts::Font2);
  tft.setTextColor(C_ACCENT);
  drawCentered(stA.c_str(), PX + 40, 12);
  tft.setTextColor(C_TXT);
  drawCentered(stB.c_str(), PX + 40, 30);
  if (lastTxt.length()) {
    drawCentered("Son hamle", PX + 40, 56);
    drawCentered(lastTxt.c_str(), PX + 40, 74);
  }
}

void drawBtn(const Btn &b, const char *t, uint16_t bg, uint16_t fg) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 5, bg);
  tft.setTextColor(fg);
  drawCentered(t, b.x + b.w / 2, b.y + b.h / 2);
}

void drawButtons() {
  tft.fillRect(PX, 90, 80, 150, C_PANEL);
  tft.setFont(&lgfx::fonts::Font2);
  drawBtn(B_NEW,  "YENI",    C_BTN, C_TXT);
  drawBtn(B_UNDO, "GERI AL", C_BTN, C_TXT);
  String l = "SEVIYE " + String(levelIdx + 1);
  drawBtn(B_LVL, l.c_str(), C_BTN, C_TXT);
  drawBtn(B_COL, "RENK", playerWhite ? C_WP : C_BP, playerWhite ? C_BP : C_WP);
}

void drawPanel() {
  tft.fillRect(PX, 0, 80, 240, C_PANEL);
  drawStatus();
  drawButtons();
}

void drawStopUI() {
  tft.fillRect(PX, 90, 80, 150, C_PANEL);
  tft.setFont(&lgfx::fonts::Font2);
  tft.fillRoundRect(B_STOP.x, B_STOP.y, B_STOP.w, B_STOP.h, 8, C_STOP);
  tft.setTextColor(C_WP);
  drawCentered("HAMLE", B_STOP.x + B_STOP.w / 2, B_STOP.y + 22);
  drawCentered("VER",   B_STOP.x + B_STOP.w / 2, B_STOP.y + 42);
}

// ---------------- oyun mantigi ----------------
void gen_legal() {
  kingpositions();
  generate_steps(0);
  nLegal = 0;
  for (int i = 0; i < pos[0].n_steps && nLegal < MAXSTEPS; i++) {
    movestep(0, pos[0].steps[i]);
    boolean chk = pos[0].w ? check_w() : check_b();
    backstep(0, pos[0].steps[i]);
    if (!chk) legalMoves[nLegal++] = pos[0].steps[i];
  }
  kingpositions();
  inCheck = pos[0].check_on_table != 0;
  checkSq = pos[0].w ? poswk : posbk;
}

void evaluateState() {
  gen_legal();
  if (nLegal == 0) {
    gstate = 2;
    if (inCheck) { stA = "MAT!"; stB = humanTurn() ? "Kaybettin" : "Kazandin!"; }
    else         { stA = "PAT";  stB = "Berabere"; }
    return;
  }
  if (is_draw()) { gstate = 2; stA = "Berabere"; stB = "yetersiz"; return; }
  gstate = humanTurn() ? 0 : 1;
  if (gstate == 0) { stA = inCheck ? "SAH!" : "Senin"; stB = inCheck ? "Sen oyna" : "siran"; }
  else             { stA = inCheck ? "SAH!" : "Motor"; stB = "dusunuyor"; }
}

String moveText(const step_t &s) {
  if (s.type == 2) return "0-0";
  if (s.type == 3) return "0-0-0";
  String t = str_pole(s.c1) + "-" + str_pole(s.c2);
  if (s.type > 3) t += "=" + String(fig_symb[s.type - 2]);
  return t;
}

void apply_move(step_t s) {
  movestep(0, s);
  movepos(0, s);
  pos[1].w = !pos[0].w;
  pos[0] = pos[1];
  if (game_ply < 999) game_steps[game_ply++] = s;
  lastFrom = s.c1; lastTo = s.c2;
  lastTxt = moveText(s);
}

void clearSelection() {
  selSq = -1;
  for (int i = 0; i < 64; i++) target[i] = false;
}

void newGame() {
  fen(String(START_FEN));
  kingpositions();
  game_ply = 0;
  endspiel = false; stats = 1;
  clearSelection();
  lastFrom = lastTo = -1; lastTxt = "";
  evaluateState();
  drawBoard();
  drawPanel();
}

void rebuild(int n) {
  fen(String(START_FEN));
  kingpositions();
  endspiel = false; stats = 1;
  for (int i = 0; i < n; i++) {
    step_t s = game_steps[i];
    movestep(0, s);
    movepos(0, s);
    pos[1].w = !pos[0].w;
    pos[0] = pos[1];
  }
  game_ply = n;
  if (n > 0) { lastFrom = game_steps[n - 1].c1; lastTo = game_steps[n - 1].c2; lastTxt = moveText(game_steps[n - 1]); }
  else { lastFrom = lastTo = -1; lastTxt = ""; }
}

void undoMove() {
  if (gstate == 1) return;
  int t = game_ply - 1;
  while (t >= 0 && ((t % 2 == 0) != playerWhite)) t--;
  if (t < (playerWhite ? 0 : 1)) return;
  rebuild(t);
  clearSelection();
  evaluateState();
  drawBoard();
  drawPanel();
}

int pickPromotion() {
  const int types[4] = {7, 6, 5, 4};
  const char *names[4] = {"Q", "R", "B", "N"};
  int x, y;
  while (tft.getTouch(&x, &y)) delay(10);
  tft.fillRoundRect(15, 85, 210, 70, 8, C_PANEL);
  tft.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  for (int i = 0; i < 4; i++) {
    int bx = 22 + i * 50;
    tft.fillRoundRect(bx, 97, 44, 46, 6, C_BTN);
    tft.setTextColor(C_TXT);
    drawCentered(names[i], bx + 22, 120);
  }
  while (true) {
    if (tft.getTouch(&x, &y)) {
      for (int i = 0; i < 4; i++) {
        int bx = 22 + i * 50;
        if (x >= bx && x < bx + 44 && y >= 97 && y < 143) {
          while (tft.getTouch(&x, &y)) delay(10);
          return types[i];
        }
      }
    }
    delay(10);
  }
}

void selectSquare(int idx) {
  bool d[64];
  for (int i = 0; i < 64; i++) d[i] = target[i];
  if (selSq >= 0) d[selSq] = true;
  clearSelection();
  if (idx >= 0) {
    selSq = idx;
    for (int i = 0; i < nLegal; i++) if (legalMoves[i].c1 == idx) target[legalMoves[i].c2] = true;
    d[idx] = true;
    for (int i = 0; i < 64; i++) if (target[i]) d[i] = true;
  }
  for (int i = 0; i < 64; i++) if (d[i]) drawSquare(i);
}

bool tryHumanMove(int from, int to) {
  step_t cand[4]; int n = 0;
  for (int i = 0; i < nLegal && n < 4; i++)
    if (legalMoves[i].c1 == from && legalMoves[i].c2 == to) cand[n++] = legalMoves[i];
  if (n == 0) return false;
  step_t s = cand[0];
  if (n > 1) {
    int t = pickPromotion();
    for (int i = 0; i < n; i++) if (cand[i].type == t) s = cand[i];
  }
  clearSelection();
  apply_move(s);
  evaluateState();
  drawBoard();
  drawStatus();
  return true;
}

void engineMove() {
  stA = "Motor"; stB = "dusunuyor";
  drawStatus();
  drawStopUI();
  for (int i = 0; i < MAXEPD; i++) { bestmove[i].c1 = -1; bestmove[i].c2 = -1; bestmove[i].type = -1; }
  pos[0].best.c1 = -1; pos[0].best.c2 = -1; pos[0].best.f1 = 0;
  halt = 0;
  thinkStart = millis(); lastTimer = 0; lastPoll = 0;
  thinking = true;
  solve_step();
  thinking = false;
  halt = 0;
  int idx = -1;
  if (pos[0].best.c2 != -1)
    for (int i = 0; i < pos[0].n_steps; i++)
      if (pos[0].steps[i].c1 == pos[0].best.c1 && pos[0].steps[i].c2 == pos[0].best.c2 &&
          pos[0].steps[i].type == pos[0].best.type) { idx = i; break; }
  if (idx < 0) idx = 0;
  step_t s = pos[0].steps[idx];
  apply_move(s);
  evaluateState();
  drawBoard();
  drawPanel();
}

// motorun kok hamle dongusunden cagrilir
void ui_poll() {
  if (!thinking) return;
  unsigned long now = millis();
  if (now - lastPoll < 100) return;
  lastPoll = now;
  int x, y;
  if (tft.getTouch(&x, &y) && inBtn(x, y, B_STOP)) halt = 1;
  if (now - lastTimer > 500) {
    lastTimer = now;
    tft.fillRect(PX, 92, 80, 54, C_PANEL);
    tft.setFont(&lgfx::fonts::Font2);
    tft.setTextColor(C_TXT);
    String a = "Sure: " + String((now - thinkStart) / 1000) + "s";
    String b = "Der: " + String(level);
    drawCentered(a.c_str(), PX + 40, 104);
    drawCentered(b.c_str(), PX + 40, 124);
  }
}

void handleTouch(int x, int y) {
  if (x >= PX) {
    if (inBtn(x, y, B_NEW))       newGame();
    else if (inBtn(x, y, B_UNDO)) undoMove();
    else if (inBtn(x, y, B_LVL)) {
      levelIdx = (levelIdx + 1) % 4;
      timelimith = LEVEL_MS[levelIdx];
      prefs.putUChar("lvl", levelIdx);
      drawButtons();
    } else if (inBtn(x, y, B_COL)) {
      playerWhite = !playerWhite;
      prefs.putBool("white", playerWhite);
      newGame();
    }
    return;
  }
  if (gstate != 0) return;
  int sc = constrain(x / SQ, 0, 7), sr = constrain(y / SQ, 0, 7);
  int c = playerWhite ? sc : 7 - sc;
  int r = playerWhite ? sr : 7 - sr;
  int idx = r * 8 + c;
  if (selSq >= 0 && target[idx]) {
    int from = selSq;
    if (tryHumanMove(from, idx)) return;
  }
  int f = pole[idx];
  if (f != 0 && ((f > 0) == playerWhite)) {
    if (idx == selSq) selectSquare(-1); else selectSquare(idx);
  } else if (selSq >= 0) selectSquare(-1);
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  prefs.begin("chess", false);
  levelIdx = constrain((int)prefs.getUChar("lvl", 1), 0, 3);
  playerWhite = prefs.getBool("white", true);
  timelimith = LEVEL_MS[levelIdx];

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.setBrightness(255);
  tft.fillScreen(C_BLACK);

  // dokunmatik kalibrasyon (ilk acilista veya acilista ekrana dokunulursa)
  uint16_t cal[8];
  bool needCal = prefs.getBytesLength("cal") != sizeof(cal);
  tft.setFont(&lgfx::fonts::Font2);
  tft.setTextColor(C_TXT);
  drawCentered("ESP32 Satranc", 160, 100);
  drawCentered("Kalibrasyon icin dokun", 160, 130);
  unsigned long t0 = millis();
  int tx, ty;
  while (millis() - t0 < 1500) { if (tft.getTouch(&tx, &ty)) { needCal = true; break; } delay(20); }
  if (needCal) {
    tft.fillScreen(C_BLACK);
    tft.calibrateTouch(cal, C_WP, C_BLACK, 15);
    prefs.putBytes("cal", cal, sizeof(cal));
  } else {
    prefs.getBytes("cal", cal, sizeof(cal));
    tft.setTouchCalibrate(cal);
  }
  tft.fillScreen(C_BLACK);

  xTaskCreate(taskOne, "TaskOne", 10000, NULL, 1, NULL);
  newGame();
}

void loop() {
  if (gstate == 1) { engineMove(); return; }
  int x, y;
  if (tft.getTouch(&x, &y)) {
    handleTouch(x, y);
    while (tft.getTouch(&x, &y)) delay(10);
    delay(30);
  }
  delay(5);
}
