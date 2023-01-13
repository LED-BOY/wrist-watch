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
*/

const  uint8_t battSprite[] = {
  0x00, 0xf0, 0x90, 0x90, 0x90, 0x90, 0xf0, 0x60
};

const  uint8_t ledIcon1 [] = {
  0xcf, 0xcf, 0x43, 0xc3, 0xc3, 0x43, 0xc3, 0xc3
};

const  uint8_t ledIcon2 [] = {
  0x39, 0x79, 0xd8, 0xd9, 0xd9, 0xd8, 0x79, 0x39
};

const  uint8_t setIcon1 [] = {
  0xcf, 0xcf, 0x48, 0xcf, 0xcf, 0x41, 0xcf, 0xcf
};

const  uint8_t setIcon2 [] = {
  0x31, 0x31, 0x30, 0x31, 0x31, 0x30, 0x79, 0x79
};

const  uint8_t arrow [] = {
  0x00, 0x03, 0x07, 0x0f, 0x1f, 0x0f, 0x07, 0x03
};

const  uint8_t countDown [] = {
  0xe7, 0xe7, 0x24, 0xe7, 0xe7, 0x84, 0xe7, 0xe7
};

const  uint8_t brightSprite [] = {
  0x00, 0x80, 0x80, 0x18, 0xd8, 0xe0, 0xf0, 0xf6, 0xf6, 0xf0, 0xe0, 0xd8, 0x18, 0x80, 0x80,
  0x00, 0x00, 0x01, 0x01, 0x18, 0x1b, 0x07, 0x0f, 0x6f, 0x6f, 0x0f, 0x07, 0x1b, 0x18, 0x01, 0x01,
  0x00
};

const  uint8_t sandClockSprite [] = {
  0xfe, 0x4c, 0x8c, 0x0c, 0x0c, 0x18, 0x30, 0xa0, 0x20, 0xb0, 0x58, 0xac, 0x5c, 0xac, 0x4c,
  0xfe, 0x7f, 0x35, 0x32, 0x31, 0x30, 0x19, 0x0c, 0x04, 0x05, 0x0c, 0x19, 0x32, 0x35, 0x3a, 0x35,
  0x7f
};

const  uint8_t clockSprite [] = {
  0xf0, 0xf8, 0x1c, 0x8e, 0x87, 0x83, 0x83, 0x83, 0x83, 0x03, 0x03, 0x07, 0x0e, 0x1c, 0xf8,
  0xf0, 0x0f, 0x1f, 0x38, 0x71, 0xe1, 0xc1, 0xc1, 0xc1, 0xc3, 0xc7, 0xc6, 0xe0, 0x70, 0x38, 0x1f,
  0x0f
};

const  uint8_t allwaysOnSprite [] = {
  0xe0, 0x80, 0x80, 0xfe, 0x46, 0x56, 0x56, 0x46, 0xfe, 0x02, 0xe2, 0xa2, 0xa2, 0xe2, 0x02,
  0xfe, 0x1f, 0x07, 0x07, 0xff, 0xf7, 0xe6, 0xf7, 0xc4, 0xff, 0x80, 0x8a, 0x8a, 0x8a, 0x86, 0x80,
  0xff
};

const  uint8_t chargingSprite [] = {
  0x18, 0x38, 0x70, 0xe0, 0xfc, 0x38, 0x70, 0xe0
};

const  uint16_t optionSprites [] = {
  brightSprite, sandClockSprite, clockSprite, allwaysOnSprite
};
