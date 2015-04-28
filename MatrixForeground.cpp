/*
 * SmartMatrix Library - Methods for interacting with foreground layer
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

#include <string.h>
#include "SmartMatrix.h"


//bitmap size is 32 rows (supporting maximum dimension of screen height in all rotations), by 32 bits
// double buffered to prevent flicker while drawing
static uint32_t				foregroundBitmap[2][MATRIX_HEIGHT][MATRIX_WIDTH / 32];
static uint8_t				foregroundColorLines[2][MATRIX_HEIGHT];  // stores palett index per row
volatile bool				SmartMatrix::foregroundCopyPending = false;
static const unsigned char	foregroundDrawBuffer    = 0;
static const unsigned char	foregroundRefreshBuffer = 1;
static bool					majorForegroundChange	= false;
bool						hasForeground			= false;


void SmartMatrix::clearForeground(void) {
    memset(foregroundBitmap[foregroundDrawBuffer], 0x00, sizeof(foregroundBitmap[0]));
}

void SmartMatrix::displayForegroundDrawing(bool waitUntilComplete) {
    hasForeground = true;

    while (foregroundCopyPending);

    foregroundCopyPending = true;

    while (waitUntilComplete && foregroundCopyPending);
}

void SmartMatrix::handleForegroundDrawingCopy(void) {
    if (!foregroundCopyPending)
        return;

    memcpy(foregroundBitmap[foregroundRefreshBuffer], foregroundBitmap[foregroundDrawBuffer], sizeof(foregroundBitmap[0]));
    memcpy(foregroundColorLines[foregroundRefreshBuffer], foregroundColorLines[foregroundDrawBuffer], sizeof(foregroundColorLines[0]));
    redrawForeground();
    foregroundCopyPending = false;
}

void SmartMatrix::drawForegroundPixel(int16_t x, int16_t y, bool opaque) {
    uint32_t tempBitmask;
    uint8_t index = x >> 5;
    x -= (index * 32);

    if(opaque) {
        tempBitmask = 0x80000000 >> x;
        foregroundBitmap[foregroundDrawBuffer][y][index] |= tempBitmask;
    } else {
        tempBitmask = ~(0x80000000 >> x);
        foregroundBitmap[foregroundDrawBuffer][y][index] &= tempBitmask;
    }
}

bitmap_font *foregroundfont = (bitmap_font *) &apple3x5;


void SmartMatrix::setForegroundFont(fontChoices newFont) {
    foregroundfont = (bitmap_font *)fontLookup(newFont);
    majorForegroundChange = true;
}

void SmartMatrix::drawForegroundChar(int16_t x, int16_t y, char character, bool opaque) {
    uint32_t tempBitmask;
    int k;

    for (k = y; k < y+foregroundfont->Height; k++) {
        // ignore rows that are not on the screen
        if(k < 0) continue;
        if (k > SmartMatrix::screenConfig.localHeight) return;

        // read in uint8, shift it to be in MSB (font is in the top bits of the uint32)
        tempBitmask = getBitmapFontRowAtXY(character, k - y, foregroundfont) << 24;
        if (x < 0)
            foregroundBitmap[foregroundDrawBuffer][k][0] |= tempBitmask << -x;
        else
            foregroundBitmap[foregroundDrawBuffer][k][0] |= tempBitmask >> x;
    }
}

void SmartMatrix::drawForegroundString(int16_t x, int16_t y, const char text [], bool opaque) {
    int16_t i=0;

    while(*text != '\0' && *text != '\n')
    {
        drawForegroundChar(i * foregroundfont->Width + x, y, *text, opaque);
        ++i;
        ++text;
    }
}

void SmartMatrix::drawForegroundMonoBitmap(int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t *bitmap, bool opaque) {
    int xcnt, ycnt;

    for (ycnt = 0; ycnt < height; ycnt++) {
        for (xcnt = 0; xcnt < width; xcnt++) {
            if (getBitmapPixelAtXY(xcnt, ycnt, width, height, bitmap)) {
                drawForegroundPixel(x + xcnt, y + ycnt, opaque);
            }
        }
    }
}


TextScroller::TextScroller(SmartMatrix *matrix)
 :	matrix(matrix)
{
}

void TextScroller::scrollText(const char inputtext[], int numScrolls) {
    int length = strlen((const char *)inputtext);
    text = inputtext;
    textLen = length;
    scrollCounter = numScrolls;

    setScrollMinMax();
}

// TODO: recompute stuff after changing mode, font, etc
void TextScroller::setScrollMode(ScrollMode mode) {
    scrollMode = mode;
}

void TextScroller::setScrollSpeed(unsigned char pixels_per_second) {
    framesPerScroll = (MATRIX_REFRESH_RATE) / pixels_per_second;
}

void TextScroller::setScrollFont(fontChoices newFont) {
    scrollFont = matrix->fontLookup(newFont);
}

void TextScroller::setScrollOffsetFromTop(int offset) {
    fontTopOffset = offset;
    majorForegroundChange = true;
}

void TextScroller::setScrollStartOffsetFromLeft(int offset) {
    fontLeftOffset = offset;
}

// stops the scrolling text on the next refresh
void TextScroller::stopScrollText(void) {
    // setup conditions for ending scrolling:
    // scrollcounter is next to zero
    scrollCounter = 1;
    // position text at the end of the cycle
    scrollPosition = scrollMin;
}

void TextScroller::setScrollMinMax(void) {
    int textWidth = (textLen * scrollFont->Width) - 1;

    switch (scrollMode) {
    case wrapForward:
    case bounceForward:
    case bounceReverse:
    case wrapForwardFromLeft:
        scrollMin = -textWidth;
        scrollMax = SmartMatrix::screenConfig.localWidth;

        scrollPosition = scrollMax;

        if (scrollmode == bounceReverse)
            scrollPosition = scrollMin;
        else if(scrollmode == wrapForwardFromLeft)
            scrollPosition = fontLeftOffset;

        // TODO: handle special case - put content in fixed location if wider than window

        break;

    case stopped:
    case off:
        scrollMin = scrollMax = scrollPosition = 0;
        break;
    }

}

bool TextScroller::updateScrolling()
{
    bool resetScrolls = false;

    // return if not ready to update
	if(!scrollCounter || ++frameCurrent <= framesPerScroll)
		return false;

    frameCurrent = 0;

    switch (scrollMode) {
    case wrapForward:
    case wrapForwardFromLeft:
        scrollPosition--;
        if(scrollPosition <= scrollMin) {
            scrollPosition = scrollMax;
            if(scrollCounter > 0) scrollCounter--;
        }
        break;

    case bounceForward:
        scrollPosition--;
        if (scrollPosition <= scrollMin) {
            scrollMode = bounceReverse;
            if (scrollCounter > 0) scrollCounter--;
        }
        break;

    case bounceReverse:
        scrollPosition++;
        if (scrollPosition >= scrollMax) {
            scrollMode = bounceForward;
            if (scrollCounter > 0) scrollCounter--;
        }
        break;

    default:
    case stopped:
        scrollPosition = fontLeftOffset;
        resetScrolls = true;
        break;
    }

    // done scrolling - move text off screen and disable
    if (!scrollCounter) {
        resetScrolls = true;
    }

    // for now, fill the bitmap fresh with each update
    // TODO: reset only when necessary, and update just the pixels that need it
    resetScrolls = true;
	return resetScrolls;
}

bool TextScroller::drawFramebuffer(size_t id)
{
    int j, k, l, stride;
    int charPosition, textPosition, fontLocation;
    uint8_t charY0, charY1;


    stride = SmartMatrix::screenConfig.localWidth / 32;
    hasForeground = false;

    for (j = 0; j < SmartMatrix::screenConfig.localHeight; j++) {

        // skip rows without text
        if (j < fontTopOffset || j >= fontTopOffset + scrollFont->Height)
            continue;

        hasForeground = true;
        // now in row with text
        // find the position of the first char
        charPosition = scrollPosition;
        textPosition = 0;

        // move to first character at least partially on screen
        while (charPosition + scrollFont->Width < 0 ) {
            charPosition += scrollFont->Width;
            textPosition++;
        }

        // find rows within character bitmap that will be drawn (0-font->height unless text is partially off screen)
        charY0 = j - fontTopOffset;

        if (SmartMatrix::screenConfig.localHeight < (fontTopOffset + scrollFont->Height))
		{
            charY1 = SmartMatrix::screenConfig.localHeight - fontTopOffset;
        } else {
            charY1 = scrollFont->Height;
        }


        while (textPosition < textLen && charPosition < SmartMatrix::screenConfig.localWidth) {
            uint32_t tempBitmask;

            // lookup bitmap location for character glyph
            fontLocation = getBitmapFontLocation(text[textPosition], scrollFont);

            // draw character from top to bottom
            for (k = charY0; k < charY1; k++) {
                // read in uint8, shift it to be in MSB (font is in the top bits of the uint32)
                tempBitmask = getBitmapFontRowAtXY(fontLocation, k, scrollFont) << 24;
                for (l = 0; l < stride; ++l) {
                    // character position relative to panel l
                    int panelPosition = charPosition - l * 32;
                    if (panelPosition > -8 && panelPosition < 0)
                        foregroundBitmap[foregroundRefreshBuffer][j + k - charY0][l] |= tempBitmask << -panelPosition;
                    else if (panelPosition >= 0 && panelPosition < 32)
                        foregroundBitmap[foregroundRefreshBuffer][j + k - charY0][l] |= tempBitmask >> panelPosition;
                }

                if(tempBitmask)
                    foregroundColorLines[foregroundRefreshBuffer][j + k - charY0] = (uint8_t)id;
            }

            // get set up for next character
            charPosition += scrollFont->Width;
            textPosition++;
        }

        j += (charY1 - charY0) - 1;
    }

	return hasForeground;
}


// if font size or position changed since the last call, redraw the whole frame
void SmartMatrix::redrawForeground(void)
{
	// clear framebuffer
	memset(&foregroundBitmap[foregroundRefreshBuffer][0][0], 0, sizeof(foregroundBitmap[0]));
	memset(&foregroundColorLines[foregroundRefreshBuffer][0], 0, sizeof(foregroundColorLines[0]));
	hasForeground = false;

	//
	for(size_t i=MATRIX_SCROLLERS; i>0; --i)
	{
		TextScroller &scroll = scrollers[i -1];

		if(!scroll.scrollCounter)
			continue;

		if(scroll.drawFramebuffer(i -1))
			hasForeground = true;
	}
}

// called once per frame to update foreground (virtual) bitmap
void SmartMatrix::updateForeground(void)
{
	bool doRedraw = false;


	for(size_t i=MATRIX_SCROLLERS; i>0; --i)
	{
		TextScroller &scroll = scrollers[i -1];

		if(scroll.updateScrolling())
			doRedraw = true;
	}

	if(doRedraw)
		redrawForeground();
}

// returns true and copies color to xyPixel if pixel is opaque, returns false if not
bool SmartMatrix::getForegroundPixel(uint8_t hardwareX, uint8_t hardwareY, rgb24 *xyPixel) {
    uint8_t localScreenX, localScreenY;

    // convert hardware x/y to the pixel in the local screen
    switch( SmartMatrix::screenConfig.rotation ) {
      case rotation0 :
        localScreenX = hardwareX;
        localScreenY = hardwareY;
        break;
      case rotation180 :
        localScreenX = (MATRIX_WIDTH - 1) - hardwareX;
        localScreenY = (MATRIX_HEIGHT - 1) - hardwareY;
        break;
      case  rotation90 :
        localScreenX = hardwareY;
        localScreenY = (MATRIX_WIDTH - 1) - hardwareX;
        break;
      case  rotation270 :
        localScreenX = (MATRIX_HEIGHT - 1) - hardwareY;
        localScreenY = hardwareX;
        break;
      default:
        // TODO: Should throw an error
        return false;
    };

    uint8_t panelIndex = localScreenX >> 5;
    uint8_t panelScreenX = localScreenX - (panelIndex * 32);
    uint32_t bitmask = 0x01 << (31 - panelScreenX);

	if(foregroundBitmap[foregroundRefreshBuffer][localScreenY][panelIndex] & bitmask)
	{
		size_t i = foregroundColorLines[foregroundRefreshBuffer][localScreenY];
		if(i < MATRIX_SCROLLERS)
			*xyPixel = scrollers[i].textColor;
		else
			*xyPixel = scrollers[0].textColor;

		return true;
	}

    return false;
}
