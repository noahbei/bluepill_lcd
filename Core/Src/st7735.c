#include "stm32f1xx_hal.h"
#include "st7735.h"
#include "malloc.h"
#include "string.h"
#include <stdlib.h>

#define DELAY 0x80
#define CUSTOM_DELAY 10
#define END_DELAY 1000
#define PI 3.1415926;

// based on Adafruit ST7735 library for Arduino
static const uint8_t
  init_cmds1[] = {            // Init for 7735R, part 1 (red or green tab)
    15,                       // 15 commands in list:
    ST7735_SWRESET,   DELAY,  //  1: Software reset, 0 args, w/delay
      150,                    //     150 ms delay
    ST7735_SLPOUT ,   DELAY,  //  2: Out of sleep mode, 0 args, w/delay
      255,                    //     500 ms delay
    ST7735_FRMCTR1, 3      ,  //  3: Frame rate ctrl - normal mode, 3 args:
      0x01, 0x2C, 0x2D,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR2, 3      ,  //  4: Frame rate control - idle mode, 3 args:
      0x01, 0x2C, 0x2D,       //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6      ,  //  5: Frame rate ctrl - partial mode, 6 args:
      0x01, 0x2C, 0x2D,       //     Dot inversion mode
      0x01, 0x2C, 0x2D,       //     Line inversion mode
    ST7735_INVCTR , 1      ,  //  6: Display inversion ctrl, 1 arg, no delay:
      0x07,                   //     No inversion
    ST7735_PWCTR1 , 3      ,  //  7: Power control, 3 args, no delay:
      0xA2,
      0x02,                   //     -4.6V
      0x84,                   //     AUTO mode
    ST7735_PWCTR2 , 1      ,  //  8: Power control, 1 arg, no delay:
      0xC5,                   //     VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    ST7735_PWCTR3 , 2      ,  //  9: Power control, 2 args, no delay:
      0x0A,                   //     Opamp current small
      0x00,                   //     Boost frequency
    ST7735_PWCTR4 , 2      ,  // 10: Power control, 2 args, no delay:
      0x8A,                   //     BCLK/2, Opamp current small & Medium low
      0x2A,  
    ST7735_PWCTR5 , 2      ,  // 11: Power control, 2 args, no delay:
      0x8A, 0xEE,
    ST7735_VMCTR1 , 1      ,  // 12: Power control, 1 arg, no delay:
      0x0E,
    ST7735_INVOFF , 0      ,  // 13: Don't invert display, no args, no delay
    ST7735_MADCTL , 1      ,  // 14: Memory access control (directions), 1 arg:
      ST7735_ROTATION,        //     row addr/col addr, bottom to top refresh
    ST7735_COLMOD , 1      ,  // 15: set color mode, 1 arg, no delay:
      0x05 },                 //     16-bit color

#if (defined(ST7735_IS_128X128) || defined(ST7735_IS_160X128))
  init_cmds2[] = {            // Init for 7735R, part 2 (1.44" display)
    2,                        //  2 commands in list:
    ST7735_CASET  , 4      ,  //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x7F,             //     XEND = 127
    ST7735_RASET  , 4      ,  //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x7F },           //     XEND = 127
#endif // ST7735_IS_128X128

#ifdef ST7735_IS_160X80
  init_cmds2[] = {            // Init for 7735S, part 2 (160x80 display)
    3,                        //  3 commands in list:
    ST7735_CASET  , 4      ,  //  1: Column addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x4F,             //     XEND = 79
    ST7735_RASET  , 4      ,  //  2: Row addr set, 4 args, no delay:
      0x00, 0x00,             //     XSTART = 0
      0x00, 0x9F ,            //     XEND = 159
    ST7735_INVON, 0 },        //  3: Invert colors
#endif

  init_cmds3[] = {            // Init for 7735R, part 3 (red or green tab)
    4,                        //  4 commands in list:
    ST7735_GMCTRP1, 16      , //  1: Gamma Adjustments (pos. polarity), 16 args, no delay:
      0x02, 0x1c, 0x07, 0x12,
      0x37, 0x32, 0x29, 0x2d,
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      , //  2: Gamma Adjustments (neg. polarity), 16 args, no delay:
      0x03, 0x1d, 0x07, 0x06,
      0x2E, 0x2C, 0x29, 0x2D,
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    ST7735_NORON  ,    DELAY, //  3: Normal display on, no args, w/delay
      10,                     //     10 ms delay
    ST7735_DISPON ,    DELAY, //  4: Main screen turn on, no args w/delay
      100 };                  //     100 ms delay

static void ST7735_Select() {
    HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_RESET);
}

void ST7735_Unselect() {
    HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_SET);
}

static void ST7735_Reset() {
    HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_SET);
}

static void ST7735_WriteCommand(uint8_t cmd) {
	// sizeof(cmd) or 1
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&ST7735_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
}

static void ST7735_WriteData(uint8_t* buff, size_t buff_size) {
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&ST7735_SPI_PORT, buff, buff_size, HAL_MAX_DELAY);
}

static void ST7735_ExecuteCommandList(const uint8_t *addr) {
    uint8_t numCommands, numArgs;
    uint16_t ms;

    numCommands = *addr++;
    while(numCommands--) {
        uint8_t cmd = *addr++;
        ST7735_WriteCommand(cmd);

        numArgs = *addr++;
        // If high bit set, delay follows args
        ms = numArgs & DELAY;
        numArgs &= ~DELAY;
        if(numArgs) {
            ST7735_WriteData((uint8_t*)addr, numArgs);
            addr += numArgs;
        }

        if(ms) {
            ms = *addr++;
            if(ms == 255) ms = 500;
            HAL_Delay(ms);
        }
    }
}

void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    // column address set
    ST7735_WriteCommand(ST7735_CASET);
    uint8_t data[] = { 0x00, x0 + ST7735_XSTART, 0x00, x1 + ST7735_XSTART };
    ST7735_WriteData(data, sizeof(data));

    // row address set
    ST7735_WriteCommand(ST7735_RASET);
    data[1] = y0 + ST7735_YSTART;
    data[3] = y1 + ST7735_YSTART;
    ST7735_WriteData(data, sizeof(data));

    // write to RAM
    ST7735_WriteCommand(ST7735_RAMWR);
}

void ST7735_Init() {
    ST7735_Select();
    ST7735_Reset();
    ST7735_ExecuteCommandList(init_cmds1);
    ST7735_ExecuteCommandList(init_cmds2);
    ST7735_ExecuteCommandList(init_cmds3);
    ST7735_Unselect();
}

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
        return;

    ST7735_Select();

    ST7735_SetAddressWindow(x, y, x+1, y+1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    ST7735_WriteData(data, sizeof(data));

    ST7735_Unselect();
}

void ST7735_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = -abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;
    int16_t e2;

    while (1) {
        ST7735_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
   uint32_t i, b, j;

   ST7735_SetAddressWindow(x, y, x+font.width-1, y+font.height-1);

   for(i = 0; i < font.height; i++) {
       b = font.data[(ch - 32) * font.height + i];
       for(j = 0; j < font.width; j++) {
           if((b << j) & 0x8000)  {
               uint8_t data[] = { color >> 8, color & 0xFF };
               ST7735_WriteData(data, sizeof(data));
           } else {
               uint8_t data[] = { bgcolor >> 8, bgcolor & 0xFF };
               ST7735_WriteData(data, sizeof(data));
           }
       }
   }
}

void ST7735_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor) {
   ST7735_Select();

   while(*str) {
       if(x + font.width >= ST7735_WIDTH) {
           x = 0;
           y += font.height;
           if(y + font.height >= ST7735_HEIGHT) {
               break;
           }

           if(*str == ' ') {
               // skip spaces in the beginning of the new line
               str++;
               continue;
           }
       }

       ST7735_WriteChar(x, y, *str, font, color, bgcolor);
       x += font.width;
       str++;
   }

   ST7735_Unselect();
}

void ST7735_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // Draw horizontal linesST7735_DrawRectangle
    for (uint16_t i = 0; i < w; i++) {
        ST7735_DrawPixel(x + i, y, color);
        ST7735_DrawPixel(x + i, y + h - 1, color);
    }

    // Draw vertical lines
    for (uint16_t i = 0; i < h; i++) {
        ST7735_DrawPixel(x, y + i, color);
        ST7735_DrawPixel(x + w - 1, y + i, color);
    }
}

void ST7735_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // clipping
    if((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
    if((x + w - 1) >= ST7735_WIDTH) w = ST7735_WIDTH - x;
    if((y + h - 1) >= ST7735_HEIGHT) h = ST7735_HEIGHT - y;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);

    uint8_t data[] = { color >> 8, color & 0xFF };
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
    for(y = h; y > 0; y--) {
        for(x = w; x > 0; x--) {
            HAL_SPI_Transmit(&ST7735_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
        }
    }

    ST7735_Unselect();
}

void ST7735_FillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // clipping
    if((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
    if((x + w - 1) >= ST7735_WIDTH) w = ST7735_WIDTH - x;
    if((y + h - 1) >= ST7735_HEIGHT) h = ST7735_HEIGHT - y;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);

    // Prepare whole line in a single buffer
    uint8_t pixel[] = { color >> 8, color & 0xFF };
    uint8_t *line = malloc(w * sizeof(pixel));
    for(x = 0; x < w; ++x)
    	memcpy(line + x * sizeof(pixel), pixel, sizeof(pixel));

    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
    for(y = h; y > 0; y--)
        HAL_SPI_Transmit(&ST7735_SPI_PORT, line, w * sizeof(pixel), HAL_MAX_DELAY);

    free(line);
    ST7735_Unselect();
}

void ST7735_FillScreen(uint16_t color) {
    ST7735_FillRectangle(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_FillScreenFast(uint16_t color) {
    ST7735_FillRectangleFast(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

//void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
//    if((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
//    if((x + w - 1) >= ST7735_WIDTH) return;
//    if((y + h - 1) >= ST7735_HEIGHT) return;
//
//    ST7735_Select();
//    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);
//    ST7735_WriteData((uint8_t*)data, sizeof(uint16_t)*w*h);
//    ST7735_Unselect();
//}

//static void ConvHL(uint8_t *s, int32_t l)
//{
//	uint8_t v;
//	while (l > 0) {
//		v = *(s+1);
//		*(s+1) = *s;
//		*s = v;
//		s += 2;
//		l -= 2;
//	}
//}

void sendSPI(uint8_t *data, int size)
{
	HAL_SPI_Transmit(&ST7735_SPI_PORT, data, size, HAL_MAX_DELAY);
}

// WIP
void ST7735_DrawBitmap(uint16_t w, uint16_t h, uint8_t *data) {
	//HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
	//HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_RESET);
	ST7735_WriteCommand(ST7735_RAMWR);
	//ConvHL(data, (int32_t)w*h*2);
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
    sendSPI(data, w*h*2);
    //ST7735_Unselect();
}

void ST7735_InvertColors(bool invert) {
    ST7735_Select();
    ST7735_WriteCommand(invert ? ST7735_INVON : ST7735_INVOFF);
    ST7735_Unselect();
}

void ST7735_SetGamma(GammaDef gamma)
{
	ST7735_Select();
	ST7735_WriteCommand(ST7735_GAMSET);
	ST7735_WriteData((uint8_t *) &gamma, sizeof(gamma));
	ST7735_Unselect();
}

void ST7735_Test() {
    // Test filling the screen with different colors
    ST7735_FillScreen(ST7735_BLACK);
    HAL_Delay(CUSTOM_DELAY);
    ST7735_FillScreen(ST7735_RED);
    HAL_Delay(CUSTOM_DELAY);
    ST7735_FillScreen(ST7735_GREEN);
    HAL_Delay(CUSTOM_DELAY);
    ST7735_FillScreen(ST7735_BLUE);
    HAL_Delay(CUSTOM_DELAY);
    ST7735_FillScreen(ST7735_WHITE);
    HAL_Delay(CUSTOM_DELAY);
    ST7735_FillScreen(ST7735_BLACK);
    HAL_Delay(END_DELAY);

    // Test writing a large amount of text to the screen
    ST7735_WriteString(0, 0, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere.", Font_7x10, ST7735_WHITE, ST7735_BLACK);
    HAL_Delay(END_DELAY);

    // Test drawing lines
    testlines(ST7735_YELLOW);
    HAL_Delay(END_DELAY);

    // Test drawing rectangles
    testdrawrects(ST7735_MAGENTA);
    HAL_Delay(END_DELAY);

    // Test drawing pixels
    ST7735_DrawPixel(10, 10, ST7735_RED);
    ST7735_DrawPixel(20, 20, ST7735_GREEN);
    ST7735_DrawPixel(30, 30, ST7735_BLUE);
    HAL_Delay(END_DELAY);

    // Test filling rectangles
    ST7735_FillRectangle(10, 10, 20, 20, ST7735_CYAN);
    HAL_Delay(CUSTOM_DELAY);
    ST7735_FillRectangleFast(40, 40, 30, 30, ST7735_MAGENTA);
    HAL_Delay(CUSTOM_DELAY);
    ST7735_FillRectangleFast(80, 80, 40, 40, ST7735_RED);
    HAL_Delay(END_DELAY);

    // Test writing strings
    ST7735_WriteString(10, 10, "Hello, World!", Font_7x10, ST7735_BLACK, ST7735_WHITE);
    HAL_Delay(END_DELAY);

    // Test drawing images (assuming you have an image array 'image_data')
    // const uint16_t image_data[] = { ... }; // Example image data
    // ST7735_DrawImage(0, 0, 128, 128, image_data);
    // HAL_Delay(1000);

    // Test inverting colors
    ST7735_InvertColors(true);
    HAL_Delay(END_DELAY);
    ST7735_InvertColors(false);
    HAL_Delay(END_DELAY);

    // Test setting gamma
    ST7735_SetGamma(GAMMA_10);
    HAL_Delay(200);
    ST7735_SetGamma(GAMMA_22);
    HAL_Delay(200);
    ST7735_FillScreenFast(ST7735_BLACK);

    ST7735_FillScreenFast(ST7735_BLACK);
	HAL_Delay(CUSTOM_DELAY);
	ST7735_FillScreenFast(ST7735_RED);
	HAL_Delay(CUSTOM_DELAY);
	ST7735_FillScreenFast(ST7735_GREEN);
	HAL_Delay(CUSTOM_DELAY);
	ST7735_FillScreenFast(ST7735_BLUE);
	HAL_Delay(CUSTOM_DELAY);
	ST7735_FillScreenFast(ST7735_WHITE);
	HAL_Delay(CUSTOM_DELAY);
	ST7735_FillScreenFast(ST7735_BLACK);
	HAL_Delay(END_DELAY);
}

void testlines(uint16_t color) {
    ST7735_FillScreenFast(ST7735_BLACK);
    for (int16_t x = 0; x < ST7735_WIDTH; x += 6) {
        ST7735_DrawLine(0, 0, x, ST7735_HEIGHT - 1, color);
        HAL_Delay(CUSTOM_DELAY);
    }
    for (int16_t y = 0; y < ST7735_HEIGHT; y += 6) {
        ST7735_DrawLine(0, 0, ST7735_WIDTH - 1, y, color);
        HAL_Delay(CUSTOM_DELAY);
    }

    ST7735_FillScreenFast(ST7735_BLACK);
    for (int16_t x = 0; x < ST7735_WIDTH; x += 6) {
        ST7735_DrawLine(ST7735_WIDTH - 1, 0, x, ST7735_HEIGHT - 1, color);
        HAL_Delay(CUSTOM_DELAY);
    }
    for (int16_t y = 0; y < ST7735_HEIGHT; y += 6) {
        ST7735_DrawLine(ST7735_WIDTH - 1, 0, 0, y, color);
        HAL_Delay(CUSTOM_DELAY);
    }

    ST7735_FillScreenFast(ST7735_BLACK);
    for (int16_t x = 0; x < ST7735_WIDTH; x += 6) {
        ST7735_DrawLine(0, ST7735_HEIGHT - 1, x, 0, color);
        HAL_Delay(CUSTOM_DELAY);
    }
    for (int16_t y = 0; y < ST7735_HEIGHT; y += 6) {
        ST7735_DrawLine(0, ST7735_HEIGHT - 1, ST7735_WIDTH - 1, y, color);
        HAL_Delay(CUSTOM_DELAY);
    }

    ST7735_FillScreenFast(ST7735_BLACK);
    for (int16_t x = 0; x < ST7735_WIDTH; x += 6) {
        ST7735_DrawLine(ST7735_WIDTH - 1, ST7735_HEIGHT - 1, x, 0, color);
        HAL_Delay(CUSTOM_DELAY);
    }
    for (int16_t y = 0; y < ST7735_HEIGHT; y += 6) {
        ST7735_DrawLine(ST7735_WIDTH - 1, ST7735_HEIGHT - 1, 0, y, color);
        HAL_Delay(CUSTOM_DELAY);
    }
}

void testdrawrects(uint16_t color) {
	ST7735_FillScreenFast(ST7735_BLACK);
	for (int16_t x=0; x < ST7735_WIDTH; x+=6) {
		ST7735_DrawRectangle(ST7735_WIDTH/2 -x/2, ST7735_HEIGHT/2 -x/2 , x, x, color);
	}
}
