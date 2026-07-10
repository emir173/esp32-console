#pragma once
// ============================================================
//  dungeon/Config.h — DUNGEON CRAWLER Yapılandırma Merkezi
//
//  Sorumluluk: Tüm constexpr sabitler, RGB565 renk paleti,
//  enum'lar, grid boyutları ve ortak ileri bildirimler.
//  Diğer TÜM başlık dosyaları bu dosyayı include eder.
//  Sihirli sayı YASAK — her sabit burada isimlendirilmiştir.
// ============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>

// ------------------------------------------------------------
//  EKRAN VE GRID
// ------------------------------------------------------------
constexpr int SCR_W  = 160;                              // Ekran genişliği (piksel)
constexpr int SCR_H  = 128;                              // Ekran yüksekliği (piksel)
constexpr int HUD_H  = 10;                               // Üst HUD şeridi yüksekliği
constexpr int TILE_PX = 8;                               // Bir hücrenin piksel boyutu
constexpr int VIEW_COLS = SCR_W / TILE_PX;               // Görünür sütun sayısı (20)
constexpr int VIEW_ROWS = (SCR_H - HUD_H + TILE_PX - 1) / TILE_PX; // Görünür satır (15, son satır kırpılır)

// ------------------------------------------------------------
//  ZAMAN / FPS
// ------------------------------------------------------------
constexpr int   TARGET_FPS = 60;                         // Hedef kare hızı
// Frame limiter eşiği MİKROSANİYE cinsinden: 1000/60=16 ms yuvarlaması
// 62.5 FPS'e ("63") yol açıyordu; 16666 µs tam 60 FPS verir.
constexpr uint32_t FRAME_US = 1000000UL / TARGET_FPS;
constexpr float FRAME_SEC  = 1.0f / TARGET_FPS;          // Bir karenin ideal süresi (sn)
constexpr float DT_CAP     = 0.05f;                      // Lag spike koruması: dt üst sınırı (sn)
constexpr uint32_t FPS_WINDOW_MS = 1000;                 // FPS sayacı ölçüm penceresi

// ------------------------------------------------------------
//  HARİTA
// ------------------------------------------------------------
constexpr int MAP_W = 32;                                // Harita genişliği (tile)
constexpr int MAP_H = 28;                                // Harita yüksekliği (tile)

// Tile türleri
constexpr uint8_t TILE_WALL   = 0;                       // Geçilmez duvar
constexpr uint8_t TILE_FLOOR  = 1;                       // Yürünebilir zemin
constexpr uint8_t TILE_DOOR   = 2;                       // Kapı (geçilebilir)
constexpr uint8_t TILE_STAIRS = 3;                       // Merdiven (kat geçişi)
constexpr uint8_t TILE_CHEST  = 4;                       // Sandık (etkileşimli)
constexpr uint8_t TILE_LOCKED = 5;                       // Kilitli kapı (anahtar ister)
constexpr uint8_t TILE_SWAMP  = 6;                       // Bataklık (mağara biyomu, yavaşlatır)
constexpr uint8_t TILE_FLOWER = 7;                       // Zehirli çiçek (orman biyomu, zehirler)
constexpr uint8_t TILE_LAVA   = 8;                       // Lav (cehennem biyomu, hasar + geri iter)
constexpr uint8_t TILE_PILLAR = 9;                       // Boss arenası sütunu (v3.2, geçilmez)

// Prosedürel üretim parametreleri
constexpr int MIN_ROOMS        = 4;                      // En az oda sayısı
constexpr int MAX_ROOMS        = 6;                      // En çok oda sayısı
constexpr int ROOM_MIN_W       = 3;                      // Oda min genişlik
constexpr int ROOM_MAX_W       = 7;                      // Oda max genişlik
constexpr int ROOM_MIN_H       = 3;                      // Oda min yükseklik
constexpr int ROOM_MAX_H       = 5;                      // Oda max yükseklik
constexpr int ROOM_PLACE_TRIES = 50;                     // Oda yerleştirme deneme sayısı
constexpr int ROOM_MARGIN      = 1;                      // Odalar arası zorunlu boşluk (tile)
constexpr int CHEST_MIN        = 1;                      // Kat başına min sandık
constexpr int CHEST_MAX        = 3;                      // Kat başına max sandık
constexpr int LOCKED_MIN_FLOOR = 2;                      // Kilitli kapı bu kattan itibaren çıkar
constexpr int SPAWN_TRIES      = 30;                     // Nesne/düşman yerleştirme deneme sayısı

// Fog of War
constexpr int FOG_RADIUS = 5;                            // Görüş yarıçapı (Manhattan, tile)
constexpr uint8_t FOG_DARK = 0;                          // Hiç görülmedi (çizilmez)
constexpr uint8_t FOG_SEEN = 1;                          // Keşfedildi ama uzak (soluk)
constexpr uint8_t FOG_VIS  = 2;                          // Şu an görünür (parlak)

// ------------------------------------------------------------
//  OYUNCU
// ------------------------------------------------------------
constexpr int PLAYER_START_HP  = 20;                     // Başlangıç canı
constexpr int PLAYER_START_ATK = 3;                      // Başlangıç saldırısı
constexpr int PLAYER_START_DEF = 1;                      // Başlangıç savunması
constexpr int XP_BASE          = 10;                     // XP formülü taban değeri
constexpr int XP_PER_LVL       = 5;                      // XP formülü seviye çarpanı
constexpr int LVL_HP_BONUS     = 3;                      // Seviye başına maxHp artışı
constexpr int LVL_ATK_BONUS    = 1;                      // Seviye başına atk artışı
constexpr int LVL_DEF_EVERY    = 3;                      // Kaç seviyede bir def artar

// Yönler (player.dir ve hareket için)
constexpr int DIR_UP    = 0;
constexpr int DIR_RIGHT = 1;
constexpr int DIR_DOWN  = 2;
constexpr int DIR_LEFT  = 3;
constexpr int8_t DIR_DX[4] = { 0, 1, 0, -1 };            // Yön → x delta
constexpr int8_t DIR_DY[4] = { -1, 0, 1, 0 };            // Yön → y delta

// Girdi
constexpr int      JOY_DEADZONE   = 500;                 // Joystick ölü bölge (ADC birimi)
constexpr uint32_t MOVE_REPEAT_MS = 150;                 // Sürekli tutuşta tekrar aralığı
constexpr uint32_t MENU_REPEAT_MS = 180;                 // Menü/envanter imleç tekrar aralığı

// ------------------------------------------------------------
//  DÜŞMANLAR
// ------------------------------------------------------------
constexpr int MAX_ENEMIES       = 10;                    // Düşman havuzu boyutu
constexpr int ENEMIES_BASE      = 3;                     // Kat başına taban düşman
constexpr int ENEMIES_PER_FLOOR = 2;                     // Kat başına ek düşman
constexpr int AGGRO_RANGE       = 9;                     // Takip menzili (Manhattan, tile)
constexpr int GOBLIN_RAGE_RANGE = 2;                     // Goblin bu mesafede hızlanır

// Yarasa: hızlı, zayıf, rastgele hareket
constexpr int BAT_HP = 4,  BAT_ATK = 2, BAT_DEF = 0, BAT_SPEED = 15, BAT_XP = 3;
// İskelet: orta, oyuncuya yürür
constexpr int SKEL_HP = 8, SKEL_ATK = 3, SKEL_DEF = 1, SKEL_SPEED = 30, SKEL_XP = 6;
// Goblin: yavaş ama güçlü, yakında öfkelenir
constexpr int GOB_HP = 14, GOB_ATK = 5, GOB_DEF = 2, GOB_SPEED = 45, GOB_XP = 10;

// Tür dağılımı (yüzde, kat numarasına göre kayar)
constexpr int BAT_PCT_BASE   = 50;                       // Kat 0'da yarasa yüzdesi
constexpr int BAT_PCT_DROP   = 5;                        // Kat başına yarasa azalması
constexpr int BAT_PCT_MIN    = 15;                       // Yarasa alt sınırı
constexpr int GOB_PCT_PER_FL = 8;                        // Kat başına goblin artışı
constexpr int GOB_PCT_MAX    = 50;                       // Goblin üst sınırı

// ------------------------------------------------------------
//  EŞYALAR
// ------------------------------------------------------------
constexpr int INV_SLOTS      = 8;                        // Envanter slot sayısı
constexpr int POTION_HEAL    = 8;                        // İksir iyileştirme miktarı
constexpr int MAXHP_BONUS    = 5;                        // Can artışı eşyası bonusu
constexpr int DROP_POTION_PCT = 25;                      // Düşman iksir düşürme şansı (%)
// Sandık içerik ağırlıkları (kümülatif %: iksir 40, güç 20, kalkan 15, anahtar 15, can 10)
constexpr int CHEST_POTION_TH = 40;
constexpr int CHEST_POWER_TH  = 60;
constexpr int CHEST_SHIELD_TH = 75;
constexpr int CHEST_KEY_TH    = 90;
constexpr int PCT_MAX         = 100;                     // Yüzde zar üst sınırı

// ------------------------------------------------------------
//  SAVAŞ VE EFEKTLER
// ------------------------------------------------------------
constexpr int DMG_VAR_MIN = -1;                          // Hasar varyansı alt (dahil)
constexpr int DMG_VAR_MAX = 2;                           // Hasar varyansı üst (hariç) → ±1

constexpr int   MAX_PARTICLES    = 30;                   // Parçacık havuzu boyutu
constexpr float PARTICLE_GRAVITY = 35.0f;                // Parçacık yerçekimi (px/sn²)
constexpr float PART_SPEED_MIN   = 20.0f;                // Parçacık min hız (px/sn)
constexpr float PART_SPEED_MAX   = 70.0f;                // Parçacık max hız (px/sn)
constexpr float PART_LIFE_MIN    = 0.35f;                // Parçacık min ömür (sn)
constexpr float PART_LIFE_MAX    = 0.70f;                // Parçacık max ömür (sn)
constexpr float PART_BIG_LIFE    = 0.30f;                // Bu ömrün üstünde 2x2 çizilir

constexpr int   MAX_POPUPS   = 8;                        // Hasar yazısı havuzu
constexpr float POPUP_LIFE_S = 0.25f;                    // Hasar yazısı ömrü (~15 kare)
constexpr float POPUP_RISE   = 20.0f;                    // Hasar yazısı yükselme hızı (px/sn)

constexpr int   MAX_SLASHES   = 4;                       // Bıçak izi havuzu
constexpr float SLASH_LIFE_S  = 0.06f;                   // Bıçak izi ömrü (~3 kare)

constexpr float SHAKE_ATTACK = 0.5f;                     // Oyuncu saldırısı sarsıntısı
constexpr float SHAKE_KILL   = 1.0f;                     // Düşman öldürme sarsıntısı
constexpr float SHAKE_HURT   = 1.5f;                     // Hasar alma sarsıntısı
constexpr float SHAKE_DEATH  = 4.0f;                     // Ölüm sarsıntısı
constexpr float SHAKE_STOMP  = 1.0f;                     // Golem adım sarsıntısı (v3.3)
constexpr float SHAKE_DECAY  = 0.85f;                    // Üstel sönümlenme çarpanı
constexpr float SHAKE_MIN    = 0.1f;                     // Bu değerin altında sarsıntı biter

// Parçacık adetleri
constexpr int PART_N_HIT   = 4;                          // Vuruş parçacığı
constexpr int PART_N_HURT  = 5;                          // Hasar alma parçacığı
constexpr int PART_N_KILL  = 10;                         // Ölüm patlaması
constexpr int PART_N_ITEM  = 6;                          // Eşya kullanımı
constexpr int PART_N_CHEST = 8;                          // Sandık açılışı

// ------------------------------------------------------------
//  SÜRELER (durum makinesi)
// ------------------------------------------------------------
constexpr uint32_t GAMEOVER_GUARD_MS = 600;              // GameOver girdi koruması
constexpr uint32_t LEVEL_CLEAR_MS    = 2000;             // Kat geçiş ekranı süresi
constexpr uint32_t MSG_DURATION_MS   = 1500;             // Oyun içi mesaj gösterim süresi
constexpr uint32_t STANDALONE_MSG_MS = 800;              // "OS yok" bilgi ekranı süresi

// ------------------------------------------------------------
//  SES SÜRELERİ (ms) — E-OS Ses Paleti v2.0 kurallarına uygun
// ------------------------------------------------------------
constexpr uint32_t SND_ATK_MS      = 25;                 // Oyuncu saldırısı (440 Hz)
constexpr uint32_t SND_HIT_MS      = 30;                 // Düşmana hasar (330 Hz)
constexpr uint32_t SND_KILL_MS     = 40;                 // Düşman öldürme (523 Hz)
constexpr uint32_t SND_HURT_MS     = 80;                 // Oyuncu hasar (196 Hz)
constexpr uint32_t SND_DIE1_MS     = 120;                // Ölüm 1. nota (165 Hz)
constexpr uint32_t SND_DIE_GAP_MS  = 130;                // Ölüm notaları arası bekleme
constexpr uint32_t SND_DIE2_MS     = 100;                // Ölüm 2. nota (196 Hz)
constexpr uint32_t SND_ITEM_MS     = 30;                 // Eşya toplama (659 Hz)
constexpr uint32_t SND_CHEST_MS    = 30;                 // Sandık notaları (523→659 Hz)
constexpr uint32_t SND_CHEST_GAP_MS = 40;                // Sandık notaları arası
constexpr uint32_t SND_LEVELUP_MS  = 40;                 // Seviye atlama (784 Hz, tavan)
constexpr uint32_t SND_UNLOCK_MS   = 40;                 // Kilit açma (587 Hz)
constexpr uint32_t SND_INV_MS      = 25;                 // Envanter aç/kapat (349 Hz)
constexpr uint32_t SND_USE_MS      = 35;                 // Eşya kullanma (523 Hz)
constexpr uint32_t SND_NAV_MS      = 25;                 // Menü gezinme (349 Hz)
constexpr uint32_t SND_START_MS    = 50;                 // Menü başlat (659 Hz)
constexpr uint32_t SND_RESTART_MS  = 50;                 // Restart (587 Hz)
constexpr uint32_t SND_RESUME_MS   = 40;                 // Pause devam (587 Hz)
constexpr uint32_t SND_PAUSE_MS    = 50;                 // Pause açma (392 Hz)
constexpr uint32_t FANFARE_NOTE_MS = 50;                 // Zafer fanfar nota süresi
constexpr uint32_t FANFARE_TOP_MS  = 40;                 // Fanfar tavan notası (784 Hz)
constexpr uint32_t FANFARE_GAP_MS  = 60;                 // Fanfar notaları arası

// ------------------------------------------------------------
//  RENK PALETİ (RGB565)
// ------------------------------------------------------------
// Zemin ve duvar
constexpr uint16_t COL_BG         = 0x0000;              // Siyah arka plan
constexpr uint16_t COL_WALL       = 0x3186;              // Koyu gri taş duvar
constexpr uint16_t COL_WALL_HL    = 0x4A49;              // Duvar vurgusu (3D efekt)
constexpr uint16_t COL_FLOOR      = 0x18E3;              // Çok koyu gri zemin
constexpr uint16_t COL_FLOOR_ALT  = 0x2104;              // Daha açık zemin (dama deseni)
constexpr uint16_t COL_DOOR       = 0x8410;              // Kapı (orta gri)
constexpr uint16_t COL_STAIRS    = 0xFFE0;               // Merdiven (sarı)
constexpr uint16_t COL_CHEST      = 0xFC00;              // Sandık (turuncu)
constexpr uint16_t COL_LOCKED     = 0xC618;              // Kilitli kapı (açık gri)

// Oyuncu
constexpr uint16_t COL_PLAYER     = 0x07FF;              // Cyan
constexpr uint16_t COL_PLAYER_DK  = 0x0410;              // Koyu cyan

// Düşmanlar
constexpr uint16_t COL_BAT        = 0x780F;              // Mor
constexpr uint16_t COL_SKELETON   = 0xC618;              // Açık gri/beyaz
constexpr uint16_t COL_GOBLIN     = 0x03E0;              // Koyu yeşil

// Eşyalar
constexpr uint16_t COL_POTION     = 0x07E0;              // Yeşil (can iksiri)
constexpr uint16_t COL_POWER      = 0xF800;              // Kırmızı (güç taşı)
constexpr uint16_t COL_SHIELD     = 0x001F;              // Mavi (kalkan)
constexpr uint16_t COL_KEY        = 0xFFE0;              // Sarı (anahtar)
constexpr uint16_t COL_MAXHP      = 0xF81F;              // Pembe (can artışı)

// HUD
constexpr uint16_t COL_HUD_LINE   = 0x2104;              // HUD ayırıcı çizgi
constexpr uint16_t COL_HUD_TEXT   = 0xBDF7;              // HUD metin (açık gri)
constexpr uint16_t COL_HP_FULL    = 0x07E0;              // HP barı tam (yeşil)
constexpr uint16_t COL_HP_MID     = 0xFFE0;              // HP barı %50 (sarı)
constexpr uint16_t COL_HP_LOW     = 0xF800;              // HP barı %25 (kırmızı)

// Fog of War
constexpr uint16_t COL_FOG_SEEN   = 0x0841;              // Keşfedilmiş ama uzak (çok soluk)

// Efekt ve panel renkleri
constexpr uint16_t COL_SLASH      = 0xFFFF;              // Bıçak izi (beyaz)
constexpr uint16_t COL_PANEL_BG   = 0x1082;              // Panel arka planı (çok koyu)
constexpr uint16_t COL_PANEL_BRD  = 0x07FF;              // Panel çerçevesi (cyan)
constexpr uint16_t COL_EMPTY_SLOT = 0x630C;              // Boş slot metni (soluk gri)

// HP barı eşikleri
constexpr int HP_MID_PCT = 50;                           // Bu yüzdenin altında sarı
constexpr int HP_LOW_PCT = 25;                           // Bu yüzdenin altında kırmızı

// ------------------------------------------------------------
//  HUD / MİNİMAP YERLEŞİMİ
// ------------------------------------------------------------
constexpr int HUD_TEXT_Y   = 1;                          // HUD metin satırı Y
constexpr int HUD_BAR_X    = 16;                         // HP barı başlangıç X
constexpr int HUD_BAR_Y    = 2;                          // HP barı başlangıç Y
constexpr int HUD_BAR_W    = 36;                         // HP barı genişliği
constexpr int HUD_BAR_H    = 6;                          // HP barı yüksekliği
constexpr int HUD_FLOOR_X  = 56;                         // "K:" kat etiketi X
constexpr int HUD_LVL_X    = 82;                         // "LV:" etiketi X
constexpr int HUD_FPS_X    = 146;                        // FPS göstergesi X

constexpr int MM_W = MAP_W;                              // Minimap genişliği (1 tile = 1 px)
constexpr int MM_H = MAP_H;                              // Minimap yüksekliği
constexpr int MM_X = SCR_W - MM_W - 3;                   // Minimap sol üst X
constexpr int MM_Y = HUD_H + 2;                          // Minimap sol üst Y
constexpr uint16_t COL_MM_BG   = 0x10A2;                 // Minimap arka planı (çok koyu)
constexpr uint16_t COL_MM_WALL = 0x39E7;                 // Minimap duvar (koyu gri)

// Envanter paneli
constexpr int INV_PANEL_X = 16;
constexpr int INV_PANEL_Y = 12;
constexpr int INV_PANEL_W = 128;
constexpr int INV_PANEL_H = 108;

// ------------------------------------------------------------
//  DURUM MAKİNESİ
// ------------------------------------------------------------
enum GameState { MENU, PLAYING, GAMEOVER, PAUSE, LEVEL_CLEAR, INVENTORY,
                 SPELL_MENU, MERCHANT, BOSS_INTRO, BOSS_FIGHT, DIALOGUE };

// ------------------------------------------------------------
//  YARDIMCILAR VE İLERİ BİLDİRİMLER
// ------------------------------------------------------------
// RGB565 rengi yarıya karartır (fog'daki soluk çizim için)
inline uint16_t dimColor(uint16_t c) { return (c >> 1) & 0x7BEF; }

// Hatırlanan (keşfedilmiş ama görüş dışı) alanın SABİT renkleri — v2.0.
// Türetilmiş karartma (dimColor/çeyrek) zaten koyu olan zemin paletinde
// siyaha çöküyordu; sabit gri tonlar keşfedilen haritayı okunur tutar.
constexpr uint16_t COL_MEM_FLOOR = 0x10A2;   // Hatırlanan zemin (koyu gri, düz)
constexpr uint16_t COL_MEM_WALL  = 0x2965;   // Hatırlanan duvar (orta gri, düz)

// dungeon.ino içinde tanımlanır (GameBase osPlaySound sarmalayıcısı)
void playSound(uint16_t freq, uint32_t dur);
// Combat.h içinde tanımlanır (oyun içi kısa mesaj sistemi)
void showMessage(const char *txt);
// Combat.h içinde tanımlanır (başarım kilidi açma + NVS kaydı)
void unlockAchievement(int idx);
// dungeon.ino içinde tanımlanır (non-blocking jingle başlatıcıları)
void startFanfare();
void startBossWinJingle();
// Combat.h içinde tanımlanır (Golem adım sarsıntı + güm sesi — v3.3)
void golemStompFx();

// ============================================================
//  v2.0 GENİŞLETME SABİTLERİ
// ============================================================

// ------------------------------------------------------------
//  BİYOMLAR — kat 5/10/15'te palet + düşman dağılımı değişir
// ------------------------------------------------------------
enum BiomeType : uint8_t { BIOME_DUNGEON = 0, BIOME_CAVE, BIOME_FOREST, BIOME_HELL };

constexpr int BIOME_CAVE_FLOOR   = 5;                    // Mağara bu kattan itibaren
constexpr int BIOME_FOREST_FLOOR = 10;                   // Orman bu kattan itibaren
constexpr int BIOME_HELL_FLOOR   = 15;                   // Cehennem bu kattan itibaren

constexpr BiomeType biomeForFloor(int f) {
    return (f >= BIOME_HELL_FLOOR)   ? BIOME_HELL
         : (f >= BIOME_FOREST_FLOOR) ? BIOME_FOREST
         : (f >= BIOME_CAVE_FLOOR)   ? BIOME_CAVE
         :                             BIOME_DUNGEON;
}

// Biyom renk paletleri (indeks = BiomeType)
constexpr uint16_t BIO_FLOOR[4]     = { 0x18E3, 0x5142, 0x1202, 0x4882 }; // Zemin
constexpr uint16_t BIO_FLOOR_ALT[4] = { 0x2104, 0x69C3, 0x1A83, 0x60C2 }; // Zemin dama
constexpr uint16_t BIO_WALL[4]      = { 0x3186, 0x4101, 0x0981, 0x1841 }; // Duvar
constexpr uint16_t BIO_WALL_HL[4]   = { 0x4A49, 0x7203, 0x2304, 0x4082 }; // Duvar vurgusu

// Özel tile renkleri
constexpr uint16_t COL_SWAMP    = 0x2A85;                // Bataklık (bulanık yeşil)
constexpr uint16_t COL_SWAMP_DK = 0x1963;                // Bataklık koyu leke
constexpr uint16_t COL_LAVA     = 0xFB20;                // Lav (parlak turuncu)
constexpr uint16_t COL_LAVA_DK  = 0xC980;                // Lav taban (koyu turuncu)

// Biyoma göre düşman tür yüzdeleri (kalan yüzde = iskelet)
constexpr int CAVE_BAT_PCT   = 20, CAVE_GOB_PCT   = 20;  // Mağara: iskelet ağırlıklı
constexpr int FOREST_BAT_PCT = 15, FOREST_GOB_PCT = 55;  // Orman: goblin ağırlıklı
constexpr int HELL_BAT_PCT   = 33, HELL_GOB_PCT   = 34;  // Cehennem: karışık

// Biyom özel tile yerleşimi (kat başına adet)
constexpr int SPECIAL_TILES_MIN = 3;
constexpr int SPECIAL_TILES_MAX = 6;

// ------------------------------------------------------------
//  DÜŞMAN KAT ÖLÇEKLEMESİ (v2.1) — oyuncu seviyesi sonsuz
//  büyürken düşmanlar sabitti; geç oyunda tek vuruşluk oluyorlardı.
//  Kat başına taban HP %10 artar, ATK/DEF/XP/altın periyodik artar.
// ------------------------------------------------------------
constexpr int EN_SCALE_HP_PCT     = 10;  // Kat başına taban HP yüzdesi
constexpr int EN_SCALE_ATK_EVERY  = 3;   // Bu kadar katta bir ATK +1
constexpr int EN_SCALE_DEF_EVERY  = 5;   // Bu kadar katta bir DEF +1
constexpr int EN_SCALE_XP_EVERY   = 2;   // Bu kadar katta bir XP +1
constexpr int EN_SCALE_GOLD_EVERY = 4;   // Bu kadar katta bir altın +1

// Boss ölçeklemesi: ilk karşılaşma (kat 3) taban değerde kalır,
// sonraki turlarda (kat-3) üzerinden büyür
constexpr int BOSS_SCALE_HP_DIV    = 15; // HP += taban * (kat-3) / 15
constexpr int BOSS_SCALE_ATK_EVERY = 4;  // (kat-3)/4 kadar ATK
constexpr int BOSS_SCALE_DEF_EVERY = 6;  // (kat-3)/6 kadar DEF

// ------------------------------------------------------------
//  BOSS SAVAŞLARI — her 3 katta bir (3, 6, 9...)
// ------------------------------------------------------------
constexpr int BOSS_EVERY_N_FLOORS = 3;                   // Boss katı periyodu
constexpr int BOSS_SIZE_TILES     = 2;                   // Boss 2x2 tile kaplar (16x16 px)
constexpr int BOSS_ROOM_W = 11, BOSS_ROOM_H = 9;         // Boss odası boyutu

// Boss istatistikleri: HP / ATK / DEF / hız (tick) / XP
// v2.1 denge: ilk değerler oynanamaz sertlikteydi (Ejderha sn'de ~3 hamle,
// 8 ATK) — hızlar yavaşlatıldı, HP/ATK/DEF törpülendi.
constexpr int DRG_HP = 45, DRG_ATK = 6, DRG_DEF = 2, DRG_SPEED = 35, DRG_XP = 35;
constexpr int LCH_HP = 40, LCH_ATK = 5, LCH_DEF = 3, LCH_SPEED = 40, LCH_XP = 30;
constexpr int GLM_HP = 65, GLM_ATK = 5, GLM_DEF = 5, GLM_SPEED = 50, GLM_XP = 40;

constexpr int BOSS_PHASE2_PCT = 60;                      // HP %60 altı → faz 2
constexpr int BOSS_PHASE3_PCT = 30;                      // HP %30 altı → faz 3
constexpr uint32_t BOSS_SPECIAL_MS = 4000;               // Özel saldırı aralığı
constexpr uint32_t TELE_FIRE_MS  = 500;                  // Ateş alanı uyarı süresi
constexpr uint32_t TELE_ROCK_MS  = 600;                  // Kaya yağmuru uyarı süresi
constexpr uint32_t PHASE_FLASH_MS = 200;                 // Faz geçişi beyaz flash
constexpr uint32_t BOSS_INTRO_MS  = 500;                 // BOSS_INTRO ekran süresi
constexpr int LICH_SUMMON_N       = 2;                   // Lich iskelet çağırma adedi
constexpr int LICH_POISON_STACKS  = 3;                   // Lich zehir yığını
constexpr uint32_t GOLEM_STUN_MS  = 600;                 // Golem sersemletme süresi
constexpr int ROCK_RAIN_TILES     = 3;                   // Kaya yağmuru tile adedi

// Boss renkleri (faz 1 taban + faz 2/3 kızarma)
constexpr uint16_t COL_DRAGON  = 0x05E0;                 // Ejderha (yeşil)
constexpr uint16_t COL_LICH    = 0xA11F;                 // Lich (açık mor)
constexpr uint16_t COL_GOLEM   = 0xA514;                 // Golem (nötr taş grisi — v3.2;
                                                         // eski bej mağara zemininde kayboluyordu)
constexpr uint16_t COL_BOSS_P2 = 0xFD20;                 // Faz 2 (turuncu)
constexpr uint16_t COL_BOSS_P3 = 0xF800;                 // Faz 3 (kırmızı)

// Boss HP barı (oyun alanı üstü; minimap x=125'te başlar, bar 120'de biter)
constexpr int BOSSBAR_X = 20, BOSSBAR_Y = HUD_H + 1;
constexpr int BOSSBAR_W = 100, BOSSBAR_H = 4;

// ------------------------------------------------------------
//  BÜYÜ SİSTEMİ — seviye 2'de açılır (BTN_D)
// ------------------------------------------------------------
constexpr int MAX_SPELLS       = 5;
constexpr int SPELLBOOK_LVL    = 2;                      // Büyü kitabı açılma seviyesi
constexpr int MANA_START       = 20;                     // Başlangıç max mana
constexpr int MANA_PER_LVL     = 3;                      // Seviye başına max mana artışı
constexpr int MANA_PER_KILL    = 1;                      // Düşman öldürmede mana kazancı
constexpr int CD_TICKS_PER_S   = TARGET_FPS;             // Cooldown: 60 tick = 1 sn

// Büyü değerleri: mana / etki / cooldown (sn) / seviye gereği
constexpr int FIREBOLT_MANA = 5, FIREBOLT_DMG = 8,  FIREBOLT_CD_S = 2,  FIREBOLT_LVL = 2;
constexpr int FROST_MANA    = 8, FROST_DMG    = 4,  FROST_CD_S    = 4,  FROST_LVL    = 3;
constexpr int HEAL_MANA     = 6, HEAL_AMOUNT  = 10, HEAL_CD_S     = 6,  HEAL_LVL     = 2;
constexpr int TP_MANA       = 4,                    TP_CD_S       = 8,  TP_LVL       = 4;
constexpr int SHIELD_MANA   = 7,                    SHIELD_CD_S   = 10, SHIELD_LVL   = 5;

constexpr uint32_t FROST_SLOW_MS   = 2000;               // Buz yavaşlatma süresi (düşman)
constexpr uint32_t FROST_VISUAL_MS = 200;                // Buz kristali görsel süresi
constexpr uint32_t SHIELD_MS       = 3000;               // Kalkan bağışıklık süresi
constexpr int SPELL_RANGE          = 6;                  // Hedefli büyü menzili (Chebyshev)

// ------------------------------------------------------------
//  DURUM EFEKTLERİ
// ------------------------------------------------------------
constexpr uint32_t STATUS_TICK_MS  = 1000;               // Zehir/durum tick aralığı (1 sn)
constexpr int POISON_TICK_DMG      = 1;                  // Zehir tick hasarı
constexpr int POISON_MAX_STACKS    = 5;                  // Zehir yığını üst sınırı
constexpr int FLOWER_POISON_STACKS = 2;                  // Zehirli çiçek yığını
constexpr uint32_t SWAMP_SLOW_MS   = 2000;               // Bataklık yavaşlatma süresi
constexpr int LAVA_DMG             = 3;                  // Lav teması hasarı
constexpr int PART_N_POISON        = 3;                  // Zehir tick parçacığı

// ------------------------------------------------------------
//  ALTIN VE TÜCCAR
// ------------------------------------------------------------
constexpr int GOLD_BAT = 3, GOLD_SKEL = 5, GOLD_GOB = 8, GOLD_BOSS = 30;
constexpr int CHEST_GOLD_PCT = 30;                       // Sandıktan altın çıkma şansı
constexpr int CHEST_GOLD_MIN = 5, CHEST_GOLD_MAX = 15;   // Sandık altın aralığı

constexpr int MERCHANT_MIN_FLOOR = 2;                    // Tüccar bu kattan itibaren
constexpr int MERCHANT_PCT       = 40;                   // Tüccar çıkma şansı (%)

constexpr int BUY_POTION = 15, BUY_POWER = 30, BUY_SHIELD = 25, BUY_KEY = 20;
constexpr int SELL_POTION = 8, SELL_POWER = 15, SELL_SHIELD = 12, SELL_KEY = 10;
constexpr int TRADE_ROWS = 8;                            // 4 satın al + 4 sat satırı

// Tüccar paneli
constexpr int MER_PANEL_X = 8,  MER_PANEL_Y = 11;
constexpr int MER_PANEL_W = 144, MER_PANEL_H = 112;

// ------------------------------------------------------------
//  BAŞARIMLAR — NVS "ach_dungeon" bitmask
// ------------------------------------------------------------
constexpr int ACH_COUNT = 6;
constexpr const char *ACH_NAMES[ACH_COUNT] = {
    "First Kill",      // İlk düşman öldür
    "Boss Hunter",     // İlk boss öldür
    "Rich",            // 100 altın biriktir
    "Deep Explorer",   // Kat 10'a ulaş
    "Spell Master",    // 20 büyü kullan
    "Untouched",       // Bir katı hasarsız bitir
};
constexpr int ACH_FIRST_KILL = 0, ACH_BOSS = 1, ACH_RICH = 2,
              ACH_DEEP = 3, ACH_MAGE = 4, ACH_UNTOUCHED = 5;
constexpr int ACH_RICH_GOLD  = 100;                      // "Zengin" eşiği
constexpr int ACH_DEEP_FLOOR = 10;                       // "Derin Kaşif" eşiği
constexpr int ACH_MAGE_CASTS = 20;                       // "Büyü Ustası" eşiği
constexpr uint32_t ACH_SHOW_MS = 2000;                   // Bildirim süresi

// ------------------------------------------------------------
//  v2.0 SES SÜRELERİ — E-OS Ses Paleti v2.0 kurallarına uygun
// ------------------------------------------------------------
// Boss sesleri
constexpr uint32_t SND_BOSS_HIT_MS   = 40;   // Boss'a hasar (330 Hz)
constexpr uint32_t SND_BOSS_PHASE_MS = 80;   // Faz değişimi (247 Hz)
constexpr uint32_t SND_BOSS_DEATH_MS = 150;  // Boss ölüm (165 Hz, uzun)
constexpr uint32_t SND_BOSS_INTRO_MS = 100;  // Boss giriş (196 Hz)

// Büyü sesleri
constexpr uint32_t SND_SPELL_FIRE_MS   = 50; // Ateş (220 Hz)
constexpr uint32_t SND_SPELL_FROST_MS  = 60; // Buz (262 Hz)
constexpr uint32_t SND_SPELL_HEAL_MS   = 40; // İyileştir (659 Hz)
constexpr uint32_t SND_SPELL_TP_MS     = 30; // Işınlanma (523 Hz)
constexpr uint32_t SND_SPELL_SHIELD_MS = 40; // Kalkan (392 Hz)

// Ticaret
constexpr uint32_t SND_BUY_MS      = 30;     // Satın al (659 Hz)
constexpr uint32_t SND_SELL_MS     = 30;     // Sat (523 Hz)
constexpr uint32_t SND_NO_MONEY_MS = 80;     // Para yok (165 Hz, engelleme)

// Başarım
constexpr uint32_t SND_ACH_MS = 50;          // Başarım bildirimi (659 Hz)

// ------------------------------------------------------------
//  v2.0 HUD EK YERLEŞİMİ (HP + mana çift barı, altın)
// ------------------------------------------------------------
constexpr int HUD_HP_Y    = 1;               // HP barı Y (yükseklik 4)
constexpr int HUD_HP_H    = 4;
constexpr int HUD_MANA_Y  = 6;               // Mana barı Y (yükseklik 3)
constexpr int HUD_MANA_H  = 3;
constexpr int HUD_GOLD_X  = 114;             // "G:" altın etiketi X (FPS ile çakışmaz)
constexpr uint16_t COL_MANA = 0x34BF;        // Mana barı (açık mavi)

// ============================================================
//  v3.0 GÖRSEL ANİMASYON SABİTLERİ
// ============================================================
// Tile-arası kayma (lerp) animasyonu — grid mantığı değişmez,
// yalnızca ÇİZİM pozisyonu iki tile arasında yumuşak kayar.
constexpr uint32_t MOVE_ANIM_MS  = 120;      // Oyuncu kayma süresi (< MOVE_REPEAT_MS)
constexpr uint32_t ENEMY_ANIM_MS = 130;      // Düşman/boss kayma süresi

constexpr uint32_t HIT_FLASH_MS  = 90;       // Vurulan düşman beyaz flaş süresi
constexpr int      ATTACK_LUNGE_PX = 2;      // Saldırıda öne atılma mesafesi (px)
constexpr uint32_t ATTACK_LUNGE_MS = 60;     // Öne atılma süresi

constexpr uint32_t LEVELUP_FX_MS = 800;      // Seviye atlama halka efekti süresi
constexpr int      LEVELUP_FX_R  = 16;       // Halkanın ulaştığı max yarıçap (px)

constexpr uint16_t COL_STAIRS_GLOW = 0xFFF6; // Merdiven parlama fazı (sarımsı beyaz)

// ============================================================
//  v3.1 GÖRSEL SABİTLERİ (bitmap sprite + gölge + zemin dekoru)
// ============================================================
constexpr int BOSS_SPR_PX    = BOSS_SIZE_TILES * TILE_PX; // Boss bitmap kenarı (16 px)
constexpr int SPR_BODY_ROWS  = 6;    // 8x8 düşman gövde bitmap satırı (bacaklar ayrı çizilir)
constexpr int BOSS_ANIM_SHIFT = 8;   // Boss kare seçimi: millis >> 8 (~2 Hz döngü)
constexpr int BAT_FLAP_SHIFT  = 7;   // Yarasa kanat çırpması: millis >> 7 (~4 Hz)
constexpr int SHADOW_W_SMALL  = 4;   // 8x8 varlık gölge genişliği (px)
constexpr int SHADOW_W_BOSS   = 10;  // Boss gölge genişliği (px)
constexpr int DECOR_1_IN      = 7;   // Ortalama her 7 zemin tile'ından 1'i dekorlu

// ============================================================
//  v3.2 SABİTLERİ (boss AI + düşman hareket/saldırı hissi)
// ============================================================
constexpr int MS_PER_TICK = 1000 / TARGET_FPS;   // 60 Hz tick süresi (16 ms)

// Düşman kayması artık hamle ARALIĞINA eşitlenir (kesik sıçrama yerine
// sürekli yürüyüş); bu değer yalnız alt sınır olarak kalır.
// (ENEMY_ANIM_MS v3.0'dan: 130 ms — boss/lich ışınlanma vb. için taban)

// Düşman/boss saldırı atılımı oyuncu ile aynı sabitleri kullanır
// (ATTACK_LUNGE_PX/MS); atılım gösterim süresi 2x lunge (gidiş+dönüş hissi).
constexpr uint32_t ENEMY_LUNGE_SHOW_MS = ATTACK_LUNGE_MS * 2;

// Lich büyü mermisi (v3.2): hedef tile atış ANINDA kilitlenir,
// mermi varana dek oyuncu kenara çekilirse ıskalıyor (dodge).
constexpr int      MAX_BOLTS    = 2;             // Aynı anda uçan mermi
constexpr uint32_t LICH_BOLT_MS = 450;           // Mermi uçuş süresi

// ============================================================
//  v3.3 SABİTLERİ (Golem yürüyüş hissi + Lich açık arenası)
// ============================================================
// Golem her adımında yer sarsar + alçak "güm" çalar (ağırlık hissi).
// Ses non-blocking (osPlaySound esp_timer ile kendini durdurur).
constexpr uint16_t GLM_STOMP_FREQ = 165;         // Ses bandının tabanı (Hz)
constexpr uint32_t GLM_STOMP_MS   = 15;          // Kısa vuruş süresi
