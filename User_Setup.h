#define ST7735_DRIVER
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

#define ST7735_BLACKTAB

#define TFT_RGB_ORDER TFT_BGR

#define TFT_BLACKTAB_OFFSET_X 2
#define TFT_BLACKTAB_OFFSET_Y 1

#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO 42
#define TFT_CS   15
#define TFT_DC   41
#define TFT_RST  -1

#define USE_HSPI_PORT
#define SPI_FREQUENCY  80000000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_GFXFF
