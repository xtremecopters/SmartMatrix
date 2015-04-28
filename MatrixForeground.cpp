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
 :	bounds({ 0, 0, MATRIX_WIDTH -1, MATRIX_HEIGHT -1 }),
	matrix(matrix)
{
}

void TextScroller::scrollText(const char inputtext[], int numScrolls) {
    if(!inputtext)
		return;

	stopScrollText();

	if(!ringEnabled)
	{
		// reinitialize ring buffer with passed buffer
		textLen = strlen(inputtext);
		text.init((void *)inputtext, textLen);
		text.write(NULL, textLen);

		setup(true);
		scrollCounter = numScrolls;
	} else
	{
		appendRing(inputtext);
	}
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
    // scrollCounter is zero
    scrollCounter = 0;
    // position text at the end of the cycle
    scrollPosition = scrollMin;

    if(ringEnabled)
	{
        text.reset();
		textLen = 0;
	}
}

void TextScroller::setRingBuffer(uint8_t *buffer, size_t size)
{
	stopScrollText();
	if(buffer && size)
	{
		text.init(buffer, size);
		ringEnabled = true;
	} else
	{
		text.init(NULL, 0);
		ringEnabled = false;
	}
}

size_t TextScroller::appendRing(const char *_text)
{
	if(!ringEnabled)
		return 0;


	bool start = (textLen == 0);

	text += _text;
	setup(start);

	return text.remain();
}

bool TextScroller::getRingStatus(size_t &used, size_t &room)
{
	used = text.size();
	room = text.remain();
	return ringEnabled;
}

void TextScroller::setup(bool start)
{
	if(ringEnabled && start)
		textLen = text.size();


	unsigned int textWidth = (textLen * scrollFont->Width);// -1;

	switch(scrollMode) {
		case wrapForward:
		case bounceForward:
		case bounceReverse:
		case wrapForwardFromLeft:
		{
			// don't continue setup when ringed text is already scrolled enough that could
			// make the new text visually pop into place. In such a case the newly appended
			// text will wait until already visible items have scrolled off screen.
			if(ringEnabled && !start && !((scrollMax - scrollPosition) < (textLen * scrollFont->Width)))
				break;

			if(ringEnabled)
			{
				textLen = text.size();
				textWidth = (textLen * scrollFont->Width);// -1;
			}

			scrollMin = bounds.x0 - (int)textWidth;
			scrollMax = bounds.x1 + 1;// matrix->screenConfig.localWidth;

			if(!start)
				break;

			scrollPosition = scrollMax;

			if(scrollMode == bounceReverse)
				scrollPosition = scrollMin;
			else if(scrollMode == wrapForwardFromLeft)
				scrollPosition = fontLeftOffset;

			// TODO: handle special case - put content in fixed location if wider than window

			if(ringEnabled)
				scrollCounter = 1;
			break;
		}

		case stopped:
		case off:
		default:
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
	{
		scrollPosition--;
		if(scrollPosition <= scrollMin) {
			scrollPosition = scrollMax;
			if(scrollCounter > 0) scrollCounter--;
		}

		if(!ringEnabled)
			break;

		if(!scrollCounter && !text.empty())
		{
			setup(true);
			break;
		}

		if((scrollPosition - bounds.x0) > -1)
			break;

		int oschar = abs(scrollPosition - bounds.x0) / scrollFont->Width;
		if(text[oschar] == ringDelimiter)
		{
			text	-= oschar +1;
			textLen -= oschar +1;

			scrollPosition	+= scrollFont->Width * oschar;
			scrollMin		+= scrollFont->Width * (oschar +1);

			if(cbEvents && oschar)
				cbEvents(this, (text.empty()) ? eScrollerEvent::FIFOEmpty : eScrollerEvent::FIFOAvailable);
		}
		break;
	}

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
    if (!scrollCounter)
	{
        resetScrolls = true;

		if(cbEvents)
			cbEvents(this, eScrollerEvent::Stopped);
    }

    // for now, fill the bitmap fresh with each update
    // TODO: reset only when necessary, and update just the pixels that need it
    resetScrolls = true;
	return resetScrolls;
}

bool TextScroller::drawFramebuffer(size_t id)
{
    int j, jLimit, k, l, strideStart, strideEnd;
    int charPosition, textPosition;
    uint8_t charY0, charY1;
	uint32_t maskLeft	= ((~0UL) >> (bounds.x0 % 32));
	uint32_t maskRight	= ((~0UL) << (31 - (bounds.x1 % 32)));


	strideStart	= bounds.x0 / 32;
//	strideEnd	= matrix->screenConfig.localWidth / 32;
	strideEnd	= bounds.x1 / 32;
	if(bounds.x1 % 32)
		strideEnd += 1;

	hasForeground = false;

	// skip rows without text
	jLimit = fontTopOffset + scrollFont->Height;
	jLimit = (jLimit > SmartMatrix::screenConfig.localHeight) ? SmartMatrix::screenConfig.localHeight : jLimit;
	jLimit = (jLimit < 0)? 0 : jLimit;
	j = (fontTopOffset > 0)? fontTopOffset : 0;

	for(; j < jLimit; j++)
	{
        hasForeground = true;
        // now in row with text
        // find the position of the first char
        charPosition = scrollPosition;
        textPosition = 0;

        // move to first character at least partially on screen
        while ((charPosition - bounds.x0 + scrollFont->Width) < 0 )
		{
			if(matrix->getBitmapFontLocation(text[textPosition], scrollFont) > -1)
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

        while((textPosition < textLen) && (charPosition < bounds.x1))
		{
            uint32_t tempBitmask, mask2;
			int fontLocation;

			if((fontLocation = matrix->getBitmapFontLocation(text[textPosition], scrollFont)) > -1)
			{
				// draw character from top to bottom
				for(k = charY0; k < charY1; k++)
				{
					// read in uint8, shift it to be in MSB (font is in the top bits of the uint32)
					tempBitmask = matrix->getBitmapFontRowAtXY(fontLocation, k, scrollFont) << 24;
					for(l = strideStart; l < strideEnd; ++l)
					{
						// character position relative to panel l
						int panelPosition = charPosition - l * 32;
						if(panelPosition > -8 && panelPosition < 0)
						{
							mask2 = (tempBitmask << -panelPosition);
						}
						else if(panelPosition >= 0 && panelPosition < 32)
						{
							mask2 = (tempBitmask >> panelPosition);
						} else
							continue;

						if(charPosition < bounds.x0)
							mask2 &= maskLeft;
						else if(charPosition + scrollFont->Width >= bounds.x1)
							mask2 &= maskRight;

						foregroundBitmap[foregroundRefreshBuffer][j + k - charY0][l] |= mask2;
					}

					if(tempBitmask)
						foregroundColorLines[foregroundRefreshBuffer][j + k - charY0] = (uint8_t)id;
				}

				// get set up for next character
				charPosition += scrollFont->Width;
			}

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
