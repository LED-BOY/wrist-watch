/*
    This file is part of Game&Light Timer

    Foobar is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Foobar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <https://www.gnu.org/licenses/>.

    To contact us: ledboy.net
    ledboyconsole@gmail.com

   SSD1306xLED - Drivers for SSD1306 controlled dot matrix OLED/PLED 128x32 displays

   @created: 2014-08-12
   @author: Neven Boyanov
   @date: 2020-04-14
   @author: Simon Merrett
   @version: 0.2

   Source code for original version available at: https://bitbucket.org/tinusaur/ssd1306xled
   Source code for ATtiny 0 and 1 series with megaTinyCore: https://github.com/SimonMerrett/tinyOLED
   megaTinyCore available at: https://github.com/SpenceKonde/megaTinyCore
   See tinyOLED.h for changelog since last version
*/

// ----------------------------------------------------------------------------


#include "tinyOLED.h"


// ----------------------------------------------------------------------------

// Some code based on "IIC_wtihout_ACK" by http://www.14blog.com/archives/1358

const uint8_t ssd1306_init_sequence [] /*PROGMEM*/ = {	// Initialization Sequence //**
 // 0xAE,     // Display OFF (sleep mode)
  0x20, 0b10,   // Set Memory Addressing Mode
  // 00=Horizontal Addressing Mode; 01=Vertical Addressing Mode;
  // 10=Page Addressing Mode (RESET); 11=Invalid
  0xB7,     // Set Page Start Address for Page Addressing Mode, 0-7 //** was B7
  0xC8,     // Set COM Output Scan Direction
  0x00,     // ---set low column address
  0x10,     // ---set high column address
  0x40,     // --set start line address
  0x81, 0x3F,   // Set contrast control register //** maybe 0x8F
  0xA1,     // Set Segment Re-map. A0=address mapped; A1=address 127 mapped.
  0xA6,     // Set display mode. A6=Normal; A7=Inverse
  0xA8, 0x1F,   // Set multiplex ratio(1 to 64) //** was 0x3F
  0xA4,     // Output RAM to Display
  // 0xA4=Output follows RAM content; 0xA5,Output ignores RAM content
  0xD3, 0x00,   // Set display offset. 00 = no offset
  0xD5,     // --set display clock divide ratio/oscillator frequency
  0xF0,     // --set divide ratio
  0xD9, 0x22,   // Set pre-charge period
  0xDA, 0x02,   // Set com pins hardware configuration    //** was 0xDA, 0x12
  0xDB,     // --set vcomh
  0x20,     // 0x20,0.77xVcc
  0x8D, 0x14,   // Set DC-DC enable
  0xAF      // Display ON in normal mode
};

SSD1306Device::SSD1306Device(void) {}

void SSD1306Device::begin(void) {
  TinyI2C.init();
  for (uint8_t i = 0; i < sizeof (ssd1306_init_sequence); i++) {
    ssd1306_send_command(ssd1306_init_sequence[i]);
  }
}

void SSD1306Device::ssd1306_send_command_start(void) {
  TinyI2C.start(SSD1306, 0);
  TinyI2C.write(0x00); // write command
}

void SSD1306Device::ssd1306_send_command_stop(void) {
  TinyI2C.stop();
}

void SSD1306Device::ssd1306_send_data_byte(uint8_t byte) {
  TinyI2C.write(byte);
}

void SSD1306Device::ssd1306_send_command(uint8_t command) {
  ssd1306_send_command_start();
  TinyI2C.write(command);
  ssd1306_send_command_stop();
}

void SSD1306Device::ssd1306_send_data_start(void) {
  TinyI2C.start(SSD1306, 0);
  TinyI2C.write(0x40); // write data
}

void SSD1306Device::ssd1306_send_data_stop(void) {
  TinyI2C.stop();
}

void SSD1306Device::setCursor(uint8_t x, uint8_t y) {
  ssd1306_send_command_start();
  TinyI2C.write(0xb0 + y);
  TinyI2C.write(((x & 0xf0) >> 4) | 0x10); // | 0x10
  TinyI2C.write((x & 0x0f) | 0x01); // | 0x01
  ssd1306_send_command_stop();
}

void SSD1306Device::drawLine(uint8_t x0, uint8_t y0, uint8_t lineLenght, uint8_t dataValue) {
  setCursor(x0, y0);
  ssd1306_send_data_start();
  for (uint8_t x = 0; x < lineLenght; x++) {
    ssd1306_send_data_byte(dataValue);
  }
  ssd1306_send_data_stop();
}
// ----------------------------------------------------------------------------
