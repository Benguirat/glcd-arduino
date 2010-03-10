/*
  glcd_Device.cpp - Arduino library support for graphic LCDs 
  Copyright (c) 2009, 2010 Michael Margolis and Bill Perry 
  
  vi:ts=4  

  This file is part of the Arduino GLCD library.

  GLCD is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  GLCD is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with GLCD.  If not, see <http://www.gnu.org/licenses/>.
 
  The glcd_Device class impliments the protocol for sending and receiving data and commands to a GLCD device.
  It uses glcd_io.h to for the io primitives and glcd_COnfig.h for user specific configuration.

*/

#include <avr/io.h>
#include <wiring.h> // needed for arduino io methods

#include "include/glcd_Device.h"
#include "include/glcd_io.h"



/*
 * Experimental define
 */

//#define TRUE_WRITE	// does writes to glcd memory on page crossings vs ORs
						// This option only affects writes that span LCD pages.
						// None of the graphic rouintes nor the NEW_FONTDRAW rendering option do this.
						// Only the old font rendering and bitmap rendering do unaligned PAGE writes.
						// While this fixes a few issus for the old routines,
						// it also creates new ones.
						// The issue is routines like the bitmap rendering
						// routine attempt to use a drawing method that does not work.
						// when this is on, pixels are no longer ORd in but are written in.
						// so all the good/desired pixels get set, but then so do some
						// undesired pixels.
						//
						// current RECOMMENDED setting: OFF

	
glcd_Device::glcd_Device(){
  
}

/**
 * set pixel at x,y to the given color
 *
 * @param x X coordinate, a value from 0 to GLCD.Width-1
 * @param y Y coordinate, a value from 0 to GLCD.Heigh-1
 * @param color WHITE or BLACK
 *
 * Sets the pixel at location x,y to the specified color.
 * x and y are relative to the 0,0 origin of the display which
 * is the upper left corner.
 * Requests to set pixels outside the range of the display will be ignored.
 *
 * @note If the display has been set to INVERTED mode then the colors
 * will be automically reversed.
 *
 */

void glcd_Device::SetDot(uint8_t x, uint8_t y, uint8_t color) 
{
	uint8_t data;

	if((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT))
		return;
	
	this->GotoXY(x, y-y%8);					// read data from display memory
  	
	data = this->ReadData();
	if(color == BLACK){
		data |= 0x01 << (y%8);				// set dot
	} else {
		data &= ~(0x01 << (y%8));			// clear dot
	}	
	this->WriteData(data);					// write data back to display
}

/**
 * set an area of pixels
 *
 * @param x X coordinate of upper left corner
 * @param y Y coordinate of upper left corner
 * @param x2 X coordinate of lower right corner
 * @param y2 Y coordinate of lower right corner
 *
 * sets the pixels an area bounded by x,y to x2,y2 inclusive
 * to the specified color.
 *
 * The width of the area is x2-x + 1. 
 * The height of the area is y2-y+1 
 * 
 *
 */

// set pixels from upper left edge x,y to lower right edge x1,y1 to the given color
// the width of the region is x1-x + 1, height is y1-y+1 

void glcd_Device::SetPixels(uint8_t x, uint8_t y,uint8_t x2, uint8_t y2, uint8_t color)
{
uint8_t mask, pageOffset, h, i, data;
uint8_t height = y2-y+1;
uint8_t width = x2-x+1;
	
	pageOffset = y%8;
	y -= pageOffset;
	mask = 0xFF;
	if(height < 8-pageOffset) {
		mask >>= (8-height);
		h = height;
	} else {
		h = 8-pageOffset;
	}
	mask <<= pageOffset;
	
	this->GotoXY(x, y);
	for(i=0; i < width; i++) {
		data = this->ReadData();
		
		if(color == BLACK) {
			data |= mask;
		} else {
			data &= ~mask;
		}

		this->WriteData(data);
	}
	
	while(h+8 <= height) {
		h += 8;
		y += 8;
		this->GotoXY(x, y);
		
		for(i=0; i <width; i++) {
			this->WriteData(color);
		}
	}
	
	if(h < height) {
		mask = ~(0xFF << (height-h));
		this->GotoXY(x, y+8);
		
		for(i=0; i < width; i++) {
			data = this->ReadData();
		
			if(color == BLACK) {
				data |= mask;
			} else {
				data &= ~mask;
			}
	
			this->WriteData(data);
		}
	}
}

/**
 * set current x,y coordinate on display device
 *
 * @param x X coordinate
 * @param y Y coordinate
 *
 * Sets the current pixel location to x,y.
 * x and y are relative to the 0,0 origin of the display which
 * is the upper left most pixel on the display.
 */

void glcd_Device::GotoXY(uint8_t x, uint8_t y)
{
  uint8_t chip, cmd;

  if( (x > DISPLAY_WIDTH-1) || (y > DISPLAY_HEIGHT-1) ) {	// exit if coordinates are not legal  
    return;
  }

  this->Coord.x = x;								// save new coordinates
  this->Coord.y = y;

  chip = glcd_DevXYval2Chip(x, y);

	if(y/8 != this->Coord.chip[chip].page){
  		this->Coord.chip[chip].page = y/8;
		cmd = LCD_SET_PAGE | this->Coord.chip[chip].page;
	   	this->WriteCommand(cmd, chip);	
	}
	x = glcd_DevXval2ChipCol(x);
	
	/*
	 * NOTE: For now, the "if" below is intentionally commented out.
	 * In order for this to work, the code must properly track
	 * the x coordinate of the chips and not allow it go beyond proper
	 * boundaries. It isnn't complicated to do, it jsut isn't done that
	 * way right now.
	 */

	//if(x != this->Coord.chip[chip].col)
	{
//		this->Coord.chip[chip].col = x;		// No column tracking yet
#ifdef LCD_SET_ADDLO
		cmd = LCD_SET_ADDLO | glcd_DevCol2addrlo(x);
	   	this->WriteCommand(cmd, chip);	

		cmd = LCD_SET_ADDHI | glcd_DevCol2addrhi(x);
	   	this->WriteCommand(cmd, chip);	
#else
		cmd = LCD_SET_ADD | x;
	   	this->WriteCommand(cmd, chip);	
#endif
	}
}
/**
 * Low level h/w initialization of display and AVR pins
 *
 * @param invert specifices whether display is in normal mode or inverted mode.
 *
 * This should only be called prior by other library code.
 *
 * It does all the low level hardware initalization of the display device.
 *
 * The optional invert parameter specifies if the display should be run in a normal
 * mode, dark pixels on light background or inverted, light pixels on a dark background.
 *
 * To specify dark pixels use the define @b NON-INVERTED and to use light pixels use
 * the define @b INVERTED
 *
 * Upon completion of the initialization, then entire display will be cleared.
 *
 */

void glcd_Device::Init(uint8_t invert)
{  

	/*
	 * Now setup the pinmode for all of our control pins.
	 * The data lines will be configured as necessary when needed.
	 */

	lcdPinMode(glcdDI,OUTPUT);	
	lcdPinMode(glcdRW,OUTPUT);	

#ifdef glcdE1
	lcdPinMode(glcdE1,OUTPUT);	
	lcdfastWrite(glcdE1,LOW); 	
#endif
#ifdef glcdE2
	lcdPinMode(glcdE2,OUTPUT);	
	lcdfastWrite(glcdE2,LOW); 	
#endif

#ifdef glcdEN
	lcdPinMode(glcdEN,OUTPUT);	
	lcdfastWrite(glcdEN, LOW);
#endif

#ifdef glcdCSEL1
	lcdPinMode(glcdCSEL1,OUTPUT);
	lcdfastWrite(glcdCSEL1, LOW);
#endif

#ifdef glcdCSEL2
	lcdPinMode(glcdCSEL2,OUTPUT);
	lcdfastWrite(glcdCSEL2, LOW);
#endif

#ifdef glcdCSEL3
	lcdPinMode(glcdCSEL3,OUTPUT);
	lcdfastWrite(glcdCSEL3, LOW);
#endif

#ifdef glcdCSEL4
	lcdPinMode(glcdCSEL4,OUTPUT);
	lcdfastWrite(glcdCSEL4, LOW);
#endif

	/*
	 * If reset control
	 */
#ifdef glcdRES
	lcdPinMode(glcdRES,OUTPUT);
#endif


	lcdfastWrite(glcdDI, LOW);
	lcdfastWrite(glcdRW, LOW);

	this->Coord.x = 0;
	this->Coord.y = 0;
	
	this->Inverted = invert;

	/*
	 * Reset the glcd module if there is a reset pin defined
	 */ 
#ifdef glcdRES
	lcdReset();
	lcdDelayMilliseconds(2);  
	lcdUnReset();
	lcdDelayMilliseconds(5);
#endif
	
#ifdef glcd_DeviceInit // this provides override for chip specific init -  mem 8 Dec 09
	
	for(uint8_t chip=0; chip < glcd_CHIP_COUNT; chip++){

		/*
		 * flush out internal state to force first GotoXY() to work
		 */
		this->Coord.chip[chip].page = 0xff;
		//this->Coord.chip[chip].col = 0xff; // not used yet

       	lcdDelayMilliseconds(10);  			
        glcd_DeviceInit(chip);  // call device specific initialization if defined    
	}
#else
	for(uint8_t chip=0; chip < glcd_CHIP_COUNT; chip++){
       		lcdDelayMilliseconds(10);  

		/*
		 * flush out internal state to force first GotoXY() to work
		 */
		this->Coord.chip[chip].page = 0xff;
		//this->Coord.chip[chip].col = 0xff;// not used yet

		this->WriteCommand(LCD_ON, chip);			// power on
		this->WriteCommand(LCD_DISP_START, chip);	// display start line = 0

	}
#endif
	/*
	 * All hardware initialization is complete.
	 *
	 * Now, clear the screen and home the cursor to ensure that the display always starts
	 * in an identical state after being initialized.
	 *
	 * Note: the reason that SetPixels() below always uses WHITE, is that once the
	 * the invert flag is in place, the lower level read/write code will invert data
	 * as needed.
	 * So clearing an areas to WHITE when the mode is INVERTED will set the area to BLACK
	 * as is required.
	 */

	this->SetPixels(0,0, DISPLAY_WIDTH-1,DISPLAY_HEIGHT-1, WHITE);
	this->GotoXY(0,0);
}

#ifdef glcd_CHIP0  // if at least one chip select string
__inline__ void glcd_Device::SelectChip(uint8_t chip)
{  

#ifdef XXX
	if(chip == 0) lcdChipSelect(glcd_CHIP0);
	else lcdChipSelect(glcd_CHIP1);
#endif
	

	if(chip == 0) lcdChipSelect(glcd_CHIP0);
#ifdef glcd_CHIP1
	else if(chip == 1) lcdChipSelect(glcd_CHIP1);
#endif
#ifdef glcd_CHIP2
	else if(chip == 2) lcdChipSelect(glcd_CHIP2);
#endif
#ifdef glcd_CHIP3
	else if(chip == 3) lcdChipSelect(glcd_CHIP3);
#endif
#ifdef glcd_CHIP4
	else if(chip == 4) lcdChipSelect(glcd_CHIP4);
#endif

}
#endif

void glcd_Device::WaitReady( uint8_t chip)
{
	// wait until LCD busy bit goes to zero
	glcd_DevSelectChip(chip);
	lcdDataDir(0x00);
	lcdfastWrite(glcdDI, LOW);	
	lcdfastWrite(glcdRW, HIGH);	
	lcdDelayNanoseconds(GLCD_tAS);
	glcd_DevENstrobeHi(chip);
	lcdDelayNanoseconds(GLCD_tDDR);

	while(lcdIsBusy()){
       ;
	}
	glcd_DevENstrobeLo(chip);
}

uint8_t glcd_Device::DoReadData(uint8_t first)
{
	uint8_t data, chip;

	chip = glcd_DevXYval2Chip(this->Coord.x, this->Coord.y);
	this->WaitReady(chip);
	lcdfastWrite(glcdDI, HIGH);		// D/I = 1
	lcdfastWrite(glcdRW, HIGH);		// R/W = 1
	
	lcdDelayNanoseconds(GLCD_tAS);
	glcd_DevENstrobeHi(chip);
	lcdDelayNanoseconds(GLCD_tDDR);

	data = lcdDataIn();	// Read the data bits from the LCD

	glcd_DevENstrobeLo(chip);
    if(first == 0) 
	  this->GotoXY(this->Coord.x, this->Coord.y);	
	if(this->Inverted)
		data = ~data;
	return data;
}
/**
 * read a data byte from display device memory
 *
 * @return the data byte at the current x,y position
 *
 * @note the current x,y location is not modified by the routine.
 *	This allows a read/modify/write operation.
 *	Code can call ReadData() modify the data then
 *  call WriteData() and update the same location.
 *
 * @see WriteData()
 */

inline uint8_t glcd_Device::ReadData(void)
{  
	if(this->Coord.x >= DISPLAY_WIDTH){
		return(0);
	}
	this->DoReadData(1);				// dummy read
	return this->DoReadData(0);			// "real" read
}

void glcd_Device::WriteCommand(uint8_t cmd, uint8_t chip)
 {
	this->WaitReady(chip);
	lcdfastWrite(glcdDI, LOW);					// D/I = 0
	lcdfastWrite(glcdRW, LOW);					// R/W = 0	
	lcdDataDir(0xFF);

	lcdDataOut(cmd);		/* This could be done before or after raising E */
	lcdDelayNanoseconds(GLCD_tAS);
	glcd_DevENstrobeHi(chip);
	lcdDelayNanoseconds(GLCD_tWH);
	glcd_DevENstrobeLo(chip);
}

/**
 * Write a byte to display device memory
 *
 * @param data date byte to write to memory
 *
 * The data specified is written to glcd memory at the current
 * x,y position. If the y location is not on a byte boundary, the write
 * is fragemented up into multiple writes.
 *
 * @note the full behavior of this during split byte writes
 * currently varies depending on a compile time define. 
 * The code can be configured to either OR in 1 data bits or set all
 * the data bits.
 * @b TRUE_WRITE controls this behavior.
 *
 * @note the x,y address will not be the same as it was prior to this call.
 * 	The y address will remain the aame but the x address will advance by one.
 *	This allows back to writes to write sequentially through memory without having
 *	to do additional x,y positioning.
 *
 * @see ReadData()
 *
 */

void glcd_Device::WriteData(uint8_t data) {
	uint8_t displayData, yOffset, chip;
	//showHex("wrData",data);
    //showXY("wr", this->Coord.x,this->Coord.y);

#ifdef GLCD_DEBUG
	volatile uint16_t i;
	for(i=0; i<5000; i++);
#endif

	if(this->Coord.x >= DISPLAY_WIDTH){
		return;
	}

    chip = glcd_DevXYval2Chip(this->Coord.x, this->Coord.y);
	
	yOffset = this->Coord.y%8;

	if(yOffset != 0) {
		// first page
		displayData = this->ReadData();
		this->WaitReady(chip);
   	    lcdfastWrite(glcdDI, HIGH);				// D/I = 1
	    lcdfastWrite(glcdRW, LOW);				// R/W = 0
		lcdDataDir(0xFF);						// data port is output
		lcdDelayNanoseconds(GLCD_tAS);
		glcd_DevENstrobeHi(chip);
		
#ifdef TRUE_WRITE
		/*
		 * Strip out bits we need to update.
		 */
		displayData &= (_BV(yOffset)-1);
#endif

		displayData |= data << yOffset;

		if(this->Inverted){
			displayData = ~displayData;
		}
		lcdDataOut( displayData);					// write data
		lcdDelayNanoseconds(GLCD_tWH);
		glcd_DevENstrobeLo(chip);

		// second page

		/*
		 * Make sure to goto y address of start of next page
		 * and ensure that we don't fall off the bottom of the display.
		 */
		uint8_t ysave = this->Coord.y;
		if(((ysave+8) & ~7) >= DISPLAY_HEIGHT)
		{
			this->GotoXY(this->Coord.x+1, ysave);
			return;
		}
	
		this->GotoXY(this->Coord.x, ((ysave+8) & ~7));

		displayData = this->ReadData();
		this->WaitReady(chip);

   	    lcdfastWrite(glcdDI, HIGH);					// D/I = 1
	    lcdfastWrite(glcdRW, LOW); 					// R/W = 0	
		lcdDataDir(0xFF);				// data port is output
		lcdDelayNanoseconds(GLCD_tAS);
		glcd_DevENstrobeHi(chip);

#ifdef TRUE_WRITE
		/*
		 * Strip out bits we need to update.
		 */
		displayData &= ~(_BV(yOffset)-1);

#endif
		displayData |= data >> (8-yOffset);
		if(this->Inverted){
			displayData = ~displayData;
		}
		lcdDataOut(displayData);		// write data
		lcdDelayNanoseconds(GLCD_tWH);
		glcd_DevENstrobeLo(chip);
		this->GotoXY(this->Coord.x+1, ysave);
	}else 
	{
    	this->WaitReady(chip);

		lcdfastWrite(glcdDI, HIGH);				// D/I = 1
		lcdfastWrite(glcdRW, LOW);  				// R/W = 0	
		lcdDataDir(0xFF);						// data port is output

		// just this code gets executed if the write is on a single page
		if(this->Inverted)
			data = ~data;	  

		lcdDelayNanoseconds(GLCD_tAS);
		glcd_DevENstrobeHi(chip);
	
		lcdDataOut(data);				// write data

		lcdDelayNanoseconds(GLCD_tWH);

		glcd_DevENstrobeLo(chip);

		/*
		 * NOTE/WARNING:
		 * This bump can cause the s/w X coordinate to bump beyond a legal value
		 * for the display. This is allowed because after writing to the display
		 * display, the column (x coordinate) is always bumped. However,
		 * when writing to the the very last column, the resulting column location 
		 * inside the hardware is somewhat undefined.
		 * Some chips roll it back to 0, some stop the maximu of the LCD, and others
		 * advance further as the chip supports more pixels than the LCD shows.
		 *
		 * So to ensure that the s/w is never indicating a column (x value) that is
		 * incorrect, we allow it bump beyond the end.
		 *
		 * Future read/writes will not attempt to talk to the chip until this
		 * condition is remedied (by a GotoXY()) and by having this somewhat
		 * "invalid" value, it also ensures that the next GotoXY() will always send
		 * both a set column and set page address to reposition the glcd hardware.
		 */

		this->Coord.x++;


		/*
		 * Check for crossing into the next chip.
		 */
		if( glcd_DevXYval2Chip(this->Coord.x, this->Coord.y) != chip)
		{
			if(this->Coord.x < DISPLAY_WIDTH)
				this->GotoXY(this->Coord.x, this->Coord.y);
 		}
	    //showXY("WrData",this->Coord.x, this->Coord.y); 
	}
}


void glcd_Device::write(uint8_t) // for Print base class
{

}

#ifndef USE_ARDUINO_FLASHSTR
// functions to store and print strings in Progmem
// these should be removed when Arduino supports FLASH strings in the base print class
void glcd_Device::printFlash(FLASHSTRING str)
{
  char c;
  const prog_char *p = (const prog_char *)str;

  while (c = pgm_read_byte(p++))
    write(c);
}

void glcd_Device::printFlashln(FLASHSTRING str)
{
  printFlash(str);
  write('\n');
}
#endif

