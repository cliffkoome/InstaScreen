// TFT_eSPI setup for the InstaScreen wiring (docs/schematic.png):
// ESP32-WROOM-32 + ILI9341 "UNO shield" display on an 8-bit parallel bus.
//
// Arduino IDE: copy this file into libraries/TFT_eSPI/User_Setups/ and in
// libraries/TFT_eSPI/User_Setup_Select.h comment out the default include and add
//   #include <User_Setups/InstaScreen_ILI9341_Parallel.h>
// PlatformIO: not needed; platformio.ini passes the same values as build flags.
#define USER_SETUP_INFO "InstaScreen_ILI9341_Parallel"

#define ILI9341_DRIVER
#define TFT_PARALLEL_8_BIT

// Control lines
#define TFT_CS   27
#define TFT_DC   14  // "LCD_RS" on the shield silkscreen
#define TFT_RST  26
#define TFT_WR   12  // GPIO12 is a strapping pin (MTDI); see README "Hardware"
#define TFT_RD   13

// Data bus. All pins must be < GPIO32: TFT_eSPI writes the bus through a
// single 32-bit GPIO register for speed.
#define TFT_D0   16  // RX2
#define TFT_D1    4
#define TFT_D2   23
#define TFT_D3   22
#define TFT_D4   21
#define TFT_D5   19
#define TFT_D6   18
#define TFT_D7   17  // TX2

#define LOAD_GLCD   // font 1
#define LOAD_FONT2  // font 2
