/*
 * SmartMatrix Library - Main Header File for SmartMatrix Class
 *
 * Copyright (c) 2014 Louis Beaudoin (Pixelmatix)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SmartMatrix_h
#define SmartMatrix_h

#include "Arduino.h"
#include <stdint.h>
#include "ringbuffer.h"

// include one of the MatrixHardware_*.h files here:
#include "MatrixHardware_KitV1_16x32.h"
//#include "MatrixHardware_KitV1_32x32.h"
//#include "MatrixHardware_KitV1_128x32.h"


class SmartMatrix;
class TextScroller;


// scroll text

typedef enum ScrollMode {
    wrapForward,
    bounceForward,
    bounceReverse,
    stopped,
    off,
    wrapForwardFromLeft,
} ScrollMode;

// TextScroller callback events
enum class eScrollerEvent
{
	None = 0,				// No event occurred.
	Stopped,				// Scroller stopped because numScrolls reached zero.
	FIFOAvailable,			// One or more FIFO slots are available
	FIFOEmpty				// All FIFO slots are available and scroller has stopped.
};

typedef void (*scroller_cb)(TextScroller *scroller, eScrollerEvent event);




// font
#include "MatrixFontCommon.h"
extern const bitmap_font apple3x5;
extern const bitmap_font apple5x7;
extern const bitmap_font apple6x10;
extern const bitmap_font apple8x13;
extern const bitmap_font gohufont6x11;
extern const bitmap_font gohufont6x11b;

typedef enum fontChoices {
    font3x5,
    font5x7,
    font6x10,
    font8x13,
    gohufont11,
    gohufont11b
} fontChoices;


// color
typedef struct rgb24
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;


	rgb24(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0)
		: red(red), green(green), blue(blue)
	{}


	// assign
	rgb24& operator=(const rgb24 &rv)
	{
		red		= rv.red;
		green	= rv.green;
		blue	= rv.blue;

		return *this;
	}

	// compare
	bool operator==(const rgb24 &rv)
	{
		return (red   == rv.red)   &&
			   (green == rv.green) &&
			   (blue  == rv.blue);
	}
	bool operator!=(const rgb24 &rv)
	{
		return !(*this == rv);
	}
}  __attribute__ ((aligned(1), packed)) rgb24;


#if COLOR_DEPTH_RGB > 24
#define color_chan_t uint16_t
#else
#define color_chan_t uint8_t
#endif

typedef enum colorCorrectionModes {
    ccNone,
    cc24,
    cc12,
    cc48
} colorCorrectionModes;


// config
typedef enum rotationDegrees {
    rotation0,
    rotation90,
    rotation180,
    rotation270
} rotationDegrees;

typedef struct screen_config {
    rotationDegrees rotation;
    uint16_t localWidth;
    uint16_t localHeight;
} screen_config;

#define SMART_MATRIX_CAN_TRIPLE_BUFFER 1


// text scroller class
class TextScroller
{
public:

	//char text[textLayerMaxStringLength];
	unsigned char		framesPerScroll			= 4;
	unsigned char		frameCurrent			= 0;
	bool				hasForeground			= false;
	bool				ringEnabled				= false;
	char				ringDelimiter			= '\n';
	int					textLen					= 0;
	int					fontTopOffset			= 1;
	int					fontLeftOffset			= 1;
	int					scrollCounter			= 0;
	int					scrollMin;
	int					scrollMax;
	int					scrollPosition;
	struct
	{
		int				x0, y0;											// top-left of drawing rectangle
		int				x1, y1;											// bottom-right of drawing rectangle
	}					bounds;
	scroller_cb			cbEvents				= NULL;					// 

	const bitmap_font	*scrollFont				= &apple5x7;
	SmartMatrix			*matrix					= NULL;

	rgb24				textColor				= { 0xff, 0xff, 0xff };
	ScrollMode			scrollMode				= bounceForward;

	RingBuffer			text;


	void setup(bool start);


public:
	TextScroller(SmartMatrix *matrix);


    // scroll text
	void scrollText(const char *text, int numScrolls);
	void setScrollMode(ScrollMode mode);
	void setScrollSpeed(unsigned char pixels_per_second);
	void setScrollFont(fontChoices newFont);
	void setScrollColor(const rgb24 &newColor)				{ textColor = newColor; }
	void setScrollOffsetFromTop(int offset);
	void setScrollStartOffsetFromLeft(int offset);
	void stopScrollText(void);
	void setScrollBoundary(int x0, int y0, int x1, int y1)
		{ bounds = {x0, y0, x1, y1}; }
	void setRingBuffer(uint8_t *buffer, size_t size);
	size_t appendRing(const char *text);
	bool getRingStatus(size_t &used, size_t &room);

	// returns positive number indicating number of loops left if running
	// returns  0 if stopped
	// returns -1 if continuously scrolling
	int  getScrollStatus(void) const						{ return scrollCounter; }
	void setEventCallback(scroller_cb func)					{ cbEvents = func; }


	bool updateScrolling();
	bool drawFramebuffer(size_t id);
};


// matrix display class
class SmartMatrix {
public:
    SmartMatrix(void);
    void begin(void);

    // drawing functions
    void swapBuffers(bool copy = true);
    void drawPixel(int16_t x, int16_t y, const rgb24& color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const rgb24& color);
    void drawFastVLine(int16_t x, int16_t y0, int16_t y1, const rgb24& color);
    void drawFastHLine(int16_t x0, int16_t x1, int16_t y, const rgb24& color);
    void drawCircle(int16_t x0, int16_t y0, uint16_t radius, const rgb24& color);
    void fillCircle(int16_t x0, int16_t y0, uint16_t radius, const rgb24& outlineColor, const rgb24& fillColor);
    void fillCircle(int16_t x0, int16_t y0, uint16_t radius, const rgb24& color);
    void drawEllipse(int16_t x0, int16_t y0, uint16_t radiusX, uint16_t radiusY, const rgb24& color);
    void drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, const rgb24& color);
    void fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, const rgb24& fillColor);
    void fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
      const rgb24& outlineColor, const rgb24& fillColor);
    void drawRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const rgb24& color);
    void fillRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const rgb24& color);
    void fillRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const rgb24& outlineColor, const rgb24& fillColor);
    void drawRoundRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t radius, const rgb24& outlineColor);
    void fillRoundRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t radius, const rgb24& fillColor);
    void fillRoundRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t radius,
      const rgb24& outlineColor, const rgb24& fillColor);
    void fillScreen(const rgb24& color);
    void drawChar(int16_t x, int16_t y, const rgb24& charColor, char character);
    void drawString(int16_t x, int16_t y, const rgb24& charColor, const char text[]);
    void drawString(int16_t x, int16_t y, const rgb24& charColor, const rgb24& backColor, const char text[]);
    void drawMonoBitmap(int16_t x, int16_t y, uint8_t width, uint8_t height, const rgb24& bitmapColor, const uint8_t *bitmap);
    const rgb24 readPixel(int16_t x, int16_t y) const;
    rgb24 *backBuffer(void);
    void setBackBuffer(rgb24 *newBuffer);
    rgb24 *getRealBackBuffer(void);

    // scroll text (backwards compatibility)
    void scrollText(const char inputtext[], int numScrolls)	{ scrollers[0].scrollText(inputtext, numScrolls); }
    void setScrollMode(ScrollMode mode)						{ scrollers[0].setScrollMode(mode); }
    void setScrollSpeed(unsigned char pixels_per_second)	{ scrollers[0].setScrollSpeed(pixels_per_second); }
    void setScrollFont(fontChoices newFont)					{ scrollers[0].setScrollFont(newFont); }
    void setScrollColor(const rgb24 & newColor)				{ scrollers[0].setScrollColor(newColor); }
#define setScrollOffsetFromEdge setScrollOffsetFromTop // backwards compatibility
    void setScrollOffsetFromTop(int offset)					{ scrollers[0].setScrollOffsetFromTop(offset); }
    void setScrollStartOffsetFromLeft(int offset)			{ scrollers[0].setScrollStartOffsetFromLeft(offset); }
    void stopScrollText(void)								{ scrollers[0].stopScrollText(); }
    int  getScrollStatus(void) const						{ return scrollers[0].getScrollStatus(); }

	TextScroller& getScroller(size_t index) { return scrollers[index]; }

    // foreground drawing
    void clearForeground(void);
    void setForegroundFont(fontChoices newFont);
    void drawForegroundPixel(int16_t x, int16_t y, bool opaque = true);
    void drawForegroundChar(int16_t x, int16_t y, char character, bool opaque = true);
    void drawForegroundString(int16_t x, int16_t y, const char text [], bool opaque = true);
    void drawForegroundMonoBitmap(int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t *bitmap, bool opaque = true);
    void displayForegroundDrawing(bool waitUntilComplete = true);

    // configuration
    void setRotation(rotationDegrees rotation);
    uint16_t getScreenWidth(void) const;
    uint16_t getScreenHeight(void) const;
    void setBrightness(uint8_t brightness);
    void setBackgroundBrightness(uint8_t brightness);
    void setColorCorrection(colorCorrectionModes mode);
    void setFont(fontChoices newFont);

private:
	friend class TextScroller;

    // enable ISR access to private member variables
    friend void rowCalculationISR(void);
    friend void rowShiftCompleteISR(void);

    // functions called by ISR
    static void matrixCalculations(void);

    // functions for refreshing
    static void loadMatrixBuffers(unsigned char currentRow);

    static color_chan_t colorCorrection(uint8_t inputcolor);
    static color_chan_t backgroundColorCorrection(uint8_t inputcolor);

    static void getPixel(uint8_t hardwareX, uint8_t hardwareY, rgb24 *xyPixel);
    static rgb24 *getRefreshRow(uint8_t y);
    static void handleBufferSwap(void);
    void handleForegroundDrawingCopy(void);
    void updateForeground(void);
    bool getForegroundPixel(uint8_t x, uint8_t y, rgb24 *xyPixel);
    void redrawForeground(void);

    // drawing functions not meant for user
	void drawHardwareHLine(uint16_t x0, uint16_t x1, uint16_t y, const rgb24& color);
	void drawHardwareVLine(uint16_t x, uint16_t y0, uint16_t y1, const rgb24& color);
    void bresteepline(int16_t x3, int16_t y3, int16_t x4, int16_t y4, const rgb24& color);
    void fillFlatSideTriangleInt(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, const rgb24& color);
    static bool getBitmapPixelAtXY(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bitmap);

    // configuration helper functions
    static void calculateTimerLut(void);
    static void calculateBackgroundLUT(void);

    // configuration
    static colorCorrectionModes _ccmode;
    static screen_config screenConfig;
    static volatile bool brightnessChange;
    static int dimmingFactor;
    static uint8_t backgroundBrightness;
    static const int dimmingMaximum;

	// scrollers
	TextScroller scrollers[MATRIX_SCROLLERS];

    // keeping track of drawing buffers
    static unsigned char currentDrawBuffer;
    static unsigned char currentRefreshBuffer;
    static volatile bool swapPending;
    static volatile bool foregroundCopyPending;
    static bool swapWithCopy;

	static SmartMatrix *m_Singleton;


public:
    // font
    static int getBitmapFontLocation(unsigned char letter, const bitmap_font *font);
    static bool getBitmapFontPixelAtXY(unsigned char letter, unsigned char x, unsigned char y, const bitmap_font *font);
    static bool getBitmapFontPixelAtXY(int location, unsigned char x, unsigned char y, const bitmap_font *font);
    const bitmap_font *fontLookup(fontChoices font) const;
    static uint16_t getBitmapFontRowAtXY(unsigned char letter, unsigned char y, const bitmap_font *font);
    static uint16_t getBitmapFontRowAtXY(int location, unsigned char y, const bitmap_font *font);


	static SmartMatrix& getSingleton()						{ return *m_Singleton; }
};

#endif
