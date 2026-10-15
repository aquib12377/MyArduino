#define ILI9341_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

#define SPI_FREQUENCY      8000000
#define SPI_READ_FREQUENCY 2000000

// Minimal fonts only — removes ~10KB
#define LOAD_GLCD