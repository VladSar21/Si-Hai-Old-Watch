// WS2812 (or compatible) LEDs on the back of the display modules.
#define BACKLIGHTS_PIN (32)
#define NUMPIXELS 6

// Buttons, active low, externally pulled up (with actual resistors!)
#define BUTTON_LEFT_PIN (35)
#define BUTTON_MODE_PIN (34)
#define BUTTON_RIGHT_PIN (39)
#define BUTTON_POWER_PIN (36)

// 3-wire to DS1302 RTC
#define DS1302_SCLK (33)
#define DS1302_IO (25)
#define DS1302_CE (26)

// Chip Select shift register, to select the display
#define CSSR_DATA_PIN (4)
#define CSSR_CLOCK_PIN (22)
#define CSSR_LATCH_PIN (21)

#define TFT_ENABLE_PIN (2)

// ST7789 135 x 240 display with no chip select line

#define TFT_WIDTH 135
#define TFT_HEIGHT 240
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_DC 16   // Data Command, aka Register Select or RS
#define TFT_RST 23  // Connect reset to ensure display initialises

#define SPI_FREQUENCY 40000000

typedef struct { // For current Day and Day 1, 2, 3, etc
  int      Dt;
  String   Period;
  String   Icon;
  String   Trend;
  String   Main0;
  String   Forecast0;
  String   Forecast1;
  String   Forecast2;
  String   Description;
  String   Time;
  String   Country;
  float    lat;
  float    lon;
  float    Temperature;
  float    Feelslike;
  float    Humidity;
  float    High;
  float    Low;
  float    Winddir;
  float    Windspeed;
  float    Rainfall;
  float    Snowfall;
  float    Pop;
  float    Pressure;
  int      Cloudcover;
  int      Visibility;
  int      Sunrise;
  int      Sunset;
  int      Timezone;
} Forecast_record_type;