#define ST7735_DRIVER
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// Mavi PCB'li ST7735'ler için en stabil profil
#define ST7735_BLACKTAB 

// Renklerin (Kırmızı ve Mavi) ters dönmesini (Neon olmasını) donanımsal olarak engeller!
#define TFT_RGB_ORDER TFT_BGR 

// Ekranın sağındaki/altındaki o rastgele çizgi bozukluklarını (kaymayı) düzeltir
#define TFT_BLACKTAB_OFFSET_X 2
#define TFT_BLACKTAB_OFFSET_Y 1

// Senin çalışan orijinal pinlerin ve YENİ MISO PİNİ (42)
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO 42   // <-- EKLENDİ: Ekranın SPI okuması için yeni hat
#define TFT_CS   15
#define TFT_DC   41
#define TFT_RST  -1  

// #define USE_HSPI_PORT  // KALDIRILDI — TFT ve SD ayni SPI'yi paylassin diye
#define SPI_FREQUENCY  80000000 

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_GFXFF