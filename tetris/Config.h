#pragma once

#include <TFT_eSPI.h>

// ============ Ekran Boyutlari (Landscape) ============
constexpr int SCR_W = 160;
constexpr int SCR_H = 128;

// ============ Genel Sabitler ============
constexpr int TARGET_FPS = 60;
constexpr int FRAME_MS   = (1000 / TARGET_FPS);

// ============ Oyun Alani (Grid) ============
constexpr int CELL       = 6;                    // Hucre boyutu (px)
constexpr int BOARD_COLS = 10;                   // Sutun sayisi
constexpr int BOARD_ROWS = 20;                   // Satir sayisi
constexpr int BOARD_PX_X = 3;                    // Grid sol ust kose X (px)
constexpr int BOARD_PX_Y = 4;                    // Grid sol ust kose Y (px)
constexpr int BOARD_PX_W = BOARD_COLS * CELL;    // 60 px
constexpr int BOARD_PX_H = BOARD_ROWS * CELL;    // 120 px

// ============ Sag Panel Yerlesimi ============
constexpr int PANEL_DIV_X = 68;    // Dikey ayirici cizgi
constexpr int PANEL_X     = 74;    // Panel metin baslangici
constexpr int NEXT_BOX_X  = 74;    // Siradaki parca kutusu
constexpr int NEXT_BOX_Y  = 16;
constexpr int NEXT_BOX_W  = 30;
constexpr int NEXT_BOX_H  = 30;

// ============ Dusme Hizi / Seviye ============
constexpr unsigned long FALL_BASE_MS = 800;   // Seviye 1 dusme araligi
constexpr unsigned long FALL_DEC_MS  = 70;    // Seviye basina azalma
constexpr unsigned long FALL_MIN_MS  = 50;    // Alt sinir
constexpr int MAX_LEVEL       = 10;
constexpr int LINES_PER_LEVEL = 10;           // Her 10 satirda 1 seviye

// ============ Kontrol Zamanlamalari ============
constexpr unsigned long SOFT_DROP_MS  = 45;   // Soft drop adim araligi
constexpr unsigned long DAS_DELAY_MS  = 170;  // Ilk yatay tekrar gecikmesi
constexpr unsigned long DAS_REPEAT_MS = 110;  // Yatay oto-tekrar araligi

// ============ Satir Temizleme Animasyonu ============
constexpr unsigned long LINE_CLEAR_MS       = 320;  // Toplam animasyon suresi
constexpr unsigned long LINE_CLEAR_FLASH_MS = 80;   // Yanip sonme periyodu

// ============ Ekranlar Arasi Kilit ============
constexpr unsigned long GAMEOVER_LOCK_MS = 600;  // GameOver'da yanlis basmayi engelle

// ============ Skorlama ============
constexpr int SCORE_TABLE[5] = { 0, 100, 300, 500, 800 };  // 0-4 satir (seviye carpanli)
constexpr int HARD_DROP_PTS  = 2;                          // Hard drop: hucre basina puan

// ============ Joystick (EMA + Deadzone) ============
constexpr float EMA_ALPHA    = 0.30f;  // EMA yumusatma katsayisi
constexpr int   JOY_DEADZONE = 600;    // Merkez olu bolge (ADC birimi)

// ============ Enum'lar ============
enum GameState { MENU, PLAYING, GAMEOVER, PAUSE };
enum PieceType { PIECE_I, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L };

// ============ Parca Renkleri (RGB565) ============
// Ana renkler — TFT_eSPI sabitleri
constexpr uint16_t COLOR_I = TFT_CYAN;
constexpr uint16_t COLOR_O = TFT_YELLOW;
constexpr uint16_t COLOR_T = TFT_MAGENTA;
constexpr uint16_t COLOR_S = TFT_GREEN;
constexpr uint16_t COLOR_Z = TFT_RED;
constexpr uint16_t COLOR_J = TFT_BLUE;
constexpr uint16_t COLOR_L = TFT_ORANGE;

// Acik tonlar (blok ust-sol kenar isigi — %50 beyaz karisimi)
constexpr uint16_t COLOR_I_LT = 0x7FFF;
constexpr uint16_t COLOR_O_LT = 0xFFEF;
constexpr uint16_t COLOR_T_LT = 0xFBFF;
constexpr uint16_t COLOR_S_LT = 0x7FEF;
constexpr uint16_t COLOR_Z_LT = 0xFBEF;
constexpr uint16_t COLOR_J_LT = 0x7BFF;
constexpr uint16_t COLOR_L_LT = 0xFECF;

// Koyu tonlar (blok alt-sag kenar golgesi + hayalet parca — %50 parlaklik)
constexpr uint16_t COLOR_I_DK = 0x03EF;
constexpr uint16_t COLOR_O_DK = 0x7BE0;
constexpr uint16_t COLOR_T_DK = 0x780F;
constexpr uint16_t COLOR_S_DK = 0x03E0;
constexpr uint16_t COLOR_Z_DK = 0x7800;
constexpr uint16_t COLOR_J_DK = 0x000F;
constexpr uint16_t COLOR_L_DK = 0x7AC0;

// Parca tipine gore indeksli diziler (I,O,T,S,Z,J,L)
constexpr uint16_t PIECE_COLORS[7]    = { COLOR_I,    COLOR_O,    COLOR_T,    COLOR_S,    COLOR_Z,    COLOR_J,    COLOR_L    };
constexpr uint16_t PIECE_COLORS_LT[7] = { COLOR_I_LT, COLOR_O_LT, COLOR_T_LT, COLOR_S_LT, COLOR_Z_LT, COLOR_J_LT, COLOR_L_LT };
constexpr uint16_t PIECE_COLORS_DK[7] = { COLOR_I_DK, COLOR_O_DK, COLOR_T_DK, COLOR_S_DK, COLOR_Z_DK, COLOR_J_DK, COLOR_L_DK };

// ============ Arayuz Renkleri ============
constexpr uint16_t COLOR_GRID     = 0x10A2;      // Grid ic cizgileri (cok koyu gri)
constexpr uint16_t COLOR_FRAME    = 0x4A69;      // Cerceve / ayirici (orta gri)
constexpr uint16_t COLOR_TEXT_DIM = 0x8410;      // Soluk etiket metni (gri)
constexpr uint16_t COLOR_SEL_BOX  = 0x18E3;      // Menu secim kutusu dolgusu
constexpr uint16_t COLOR_PANEL_BG = 0x1082;      // GameOver/Pause panel dolgusu
constexpr uint16_t COLOR_FLASH    = TFT_WHITE;   // Satir temizleme flasi

// Menu basligi harf renkleri (T-E-T-R-I-S)
constexpr uint16_t MENU_TITLE_COLORS[6] = { COLOR_T, COLOR_I, COLOR_O, COLOR_S, COLOR_Z, COLOR_L };
