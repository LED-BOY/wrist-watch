/*Game&Light  Clock for GAME&LIGHT and any Attiny series 1.
  Flash CPU Speed 5MHz.
  this code is released under GPL v3, you are free to use and modify.
  released on 2023.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    To contact us: ledboy.net
    ledboyconsole@gmail.com
*/
#include "tinyOLED.h"
#include "sprites.h"
#include "digits.h"
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define OLED_RST PORTC.OUT &= ~PIN0_bm;// oled  reset
#define OLED_ON PORTC.OUT |= PIN0_bm;// led on
#define OLED_OFF !(PORTC.OUT & PIN0_bm)// screen reset pin low
#define BUTTONLOW !(PORTA.IN & PIN5_bm)// button press low
#define BUTTONHIGH (PORTA.IN & PIN5_bm)// button not pressed high
#define BATTCHR !(PORTA.IN & PIN4_bm)// battery charging
#define MAXVOLTAGE 4100 // max voltage allowed to the battery
#define MINVOLTAGE 3000 // min voltage allowed to be operational
#define LOWBATT 140 // min voltage + 140 mha is low batt indication

uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t lastSecond = 0;
uint8_t powerSaveMode = 0;
uint16_t totalSecondsTimer = 0;
uint16_t totalSeconds = 0;
bool enableAlwaysOn = false;
bool screenDimmed = false;
volatile bool shouldWakeUp = true;
volatile bool resumeOperation = true;
volatile bool alwaysOn = false;
volatile bool chargeLedIndicator = true;
volatile uint8_t ledColor = 0;
volatile uint16_t interruptTimer = 0;
volatile uint16_t timer = 0;

void setup() {

  PORTA.DIR = 0b00001000; // setup ports in and out //  pin2 (GREEN) on
  PORTB.DIR = 0b00000011; // setup ports in and out
  PORTC.DIR = 0b00000001; // setup ports in and out
  PORTA.PIN4CTRL = PORT_PULLUPEN_bm;// charge pin pullup
  PORTA.PIN5CTRL = PORT_PULLUPEN_bm;// button pullup
  PORTA.PIN4CTRL  |= PORT_ISC_BOTHEDGES_gc; //attach interrupt to portA pin 4 keeps pull up enabled

  CCP = 0xD8;
  CLKCTRL.XOSC32KCTRLA = CLKCTRL_ENABLE_bm;   //Enable the external crystal
  while (RTC.STATUS > 0); /* Wait for all register to be synchronized */
  RTC.CTRLA = RTC_PRESCALER_DIV1_gc   /* 1 */
              | 0 << RTC_RTCEN_bp     /* Enable: disabled */
              | 1 << RTC_RUNSTDBY_bp; /* Run In Standby: enabled */
  RTC.CLKSEL = RTC_CLKSEL_TOSC32K_gc; /* 32.768kHz External Crystal Oscillator (XOSC32K) */
  while (RTC.PITSTATUS > 0) {} /* Wait for all register to be synchronized */
  RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc /* RTC Clock Cycles 32768 */
                 | 1 << RTC_PITEN_bp;   /* Enable: enabled */
  RTC.PITINTCTRL = 1 << RTC_PI_bp; /* Periodic Interrupt: enabled */

  TCB0.INTCTRL = TCB_CAPT_bm; // Setup Timer B as compare capture mode to trigger interrupt
  TCB0_CCMP = 2000;// CLK
  TCB0_CTRLA = (1 << 1) | TCB_ENABLE_bm;

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);// configure sleep mode
  sei();
}

void loop() {
  wakeUpFromSleep(resumeOperation);// restart display after wake up
  countdownTimer(); // countdowntimer logic

  if (buttonDebounce(300)) { // dimm screen or enter options menu if batt is too low can not enter the menu

    if (!screenDimmed && displayBatt() != 0) {// only enter menu if batt is above 0 
      options(); // enter options
    } else {
      oled.brightScreen();
      screenDimmed = false;
    }
  }

  if ((interruptTimer - timer) > 4000) { // prepare to sleep or dimm screen
    timer = interruptTimer;
    oled.dimScreen();
    screenDimmed = true;

    if (totalSeconds == 0) { // only if the screen is on and the counter is off continue
      oled.clearScreen();

      if (BATTCHR)  drawSprite(0, 3, chargingSprite, 8, 1);

      if (!BATTCHR && powerSaveMode < 240) drawAlwaysOnDisplay();// disable always on display if 4hs passed without pressing the button
      goToSleep();
    }
  }
}

void displayTime(uint8_t hourPos, uint8_t minutesPos) { // draws time in display
  drawTime(hourPos, 1, 32, hours);
  drawTime(minutesPos, 1, 32, minutes);
}

uint8_t displayBatt (void) {
  uint16_t actualVoltage = constrain(readSupplyVoltage (), MINVOLTAGE, MAXVOLTAGE);// read batt voltage and discards values outside range
  actualVoltage = map(actualVoltage, MINVOLTAGE, MAXVOLTAGE, 0, 117);

  for (uint8_t x = 224; x > 159; x -= 64) { // draw a line in bytes 10100000 or dec. 160 for low batt and 11100000 dec. 224 for normal batt level
    oled.drawLine (10, 3, actualVoltage, x); // indicates batt and low batt on screen

    if (actualVoltage > 20) actualVoltage = 20;// number to display low batt from pixel 0 to pixel 20 of the batt bar
  }

  if (!BATTCHR) drawSprite(0, 3, battSprite, 8, 1);// draws battery or charging icon if connected to usb, doesn´t draw any icon if batt is too low

  if (BATTCHR) drawSprite(0, 3, chargingSprite, 8, 1);
  return actualVoltage;
}

void ledON (bool optionSelected) { // leds on or off

  while ((interruptTimer - timer) < 2000) {

    if (buttonDebounce(250) || optionSelected) {
      timer = interruptTimer;
      optionSelected = false;
      if (ledColor++, ledColor > 3) ledColor = 0;

      switch (ledColor) { // option select menu.
        case 0:
          PORTA.DIR = 0b00000000;
          break;
        case 1:
          PORTA.DIR = (1 << 3);// pin3 (REDLED) as output
          break;
        case 2:
          PORTA.DIR |= (1 << 2);// pin2 (GREEN) as output
          break;
        case 3:
          PORTA.DIR |= (1 << 1);// pin1 (BLUELED) as output
          break;
      }
    }
  }
}

void countdownTimer(void) {

  if (totalSeconds == 0) return; // timer logic

  if (seconds != lastSecond) {
    lastSecond = seconds;
    uint8_t timerBarValue = map(totalSeconds, 0 , totalSecondsTimer, 0, 126);
    oled.drawLine (timerBarValue, 0, 2, 0B00000000);
    oled.drawLine (0, 0, timerBarValue, 0B00000011);

    if (totalSeconds--, totalSeconds < 1) {
      oled.clearScreen();
      PORTA.DIR = (1 << 3);// pin3 (REDLED) as output
      while (BUTTONHIGH);
    }

    if (PORTA.DIR != 0b00000000) {
      ledColor = 0;
      PORTA.DIR = 0b00000000;
    }

    displayBatt();
    displayTime(64, 0);
  }
}

void wakeUpFromSleep(bool activateScreen) {

  if (!activateScreen) return;
  OLED_RST //reset screen
  while (!OLED_OFF); // wait until screen is reseted
  OLED_ON
  buttonDebounce(100);
  oled.begin();// start oled screen
  oled.clearScreen();
  displayTime(64, 0);
  displayBatt();
  screenDimmed = false;
  powerSaveMode = 0;
  resumeOperation = false;
  timer = interruptTimer;
}

void drawAlwaysOnDisplay(void) {

  if (readSupplyVoltage() < (MINVOLTAGE + LOWBATT)) enableAlwaysOn = false; // if voltage is below minvoltage + 200mha it disables allways on display.

  if (enableAlwaysOn) {

    if (powerSaveMode < 240 && alwaysOn) powerSaveMode++;// after 4 hours if always on display is enable it disables it to save battery

    if (!alwaysOn) alwaysOn = true;
    displayTime(76, 8);
  }
}

void options (void) { // options menu
  int8_t option = 0;
  const uint8_t spriteOptionPos [] = {100, 70, 40, 10};
  bool setOptions = true;
  oled.clearScreen();

  for (uint8_t x = 0; x < 4; x++) {
    drawSprite(spriteOptionPos [x], 1, optionSprites[x], 16, 2);
    option++;
  }
  option = 0;

  while (setOptions) {

    if (buttonDebounce(250) || option == 0) {
      oled.drawLine(0, 0, 127, 0B00000000);
      drawSprite((spriteOptionPos [option]) + 4, 0, arrow, 8, 1);
      if (option++, option > 4) setOptions = false;
    }

    if ((interruptTimer - timer) > 2000) {
      timer = interruptTimer;
      switch (option) { // option select menu.
        case 1:// led on
          ledON(true);
          break;
        case 2:// timer
          setTimer();
          break;
        case 3:// set time
          setTime();
          break;
        case 4:
          enableAlwaysOn = !enableAlwaysOn;
          break;
      }
      setOptions = false;
    }
  }
  resumeOperation = true;
}

void setTimer(void) {
  oled.clearScreen();
  drawTime(0, 1, 32, 0);
  totalSeconds = 0;
  bool setCurrentTimer = true;

  while (setCurrentTimer) {

    if (BUTTONLOW) {
      timer = interruptTimer;

      if (totalSeconds += 300, totalSeconds > 1800) totalSeconds = 0;
      drawTime(0, 1, 32, (totalSeconds / 60));
      buttonDebounce(300);
    }

    if ((interruptTimer - timer) > 2000) {
      setCurrentTimer = false;
    }
  }
  totalSecondsTimer = totalSeconds;
}

void setTime(void) {
  oled.clearScreen();
  alwaysOn = true;
  uint8_t arrowPos = 104;
  drawSprite(arrowPos, 0, arrow, 8, 1); // sleep

  for (uint8_t x = 0; x < 3 ; x++) {
    bool setCurrentTime = true;
    drawTime(90, 1, 18, hours);
    drawTime(46, 1, 18, minutes);
    drawTime(0, 1, 18, seconds);

    while (setCurrentTime) {

      if ((interruptTimer - timer) > 250 && BUTTONLOW) {
        timer = interruptTimer;

        switch (x) {
          case 0:

            if (hours++, hours > 23) hours = 0;
            drawTime(90, 1, 18, hours);
            break;
          case 1:

            if (minutes++, minutes > 59) minutes = 0;
            drawTime(46, 1, 18, minutes);
            break;

          case 2:

            if (seconds++, seconds > 59) seconds = 0;
            drawTime(0, 1, 18, seconds);
            break;
        }
      }

      if ((interruptTimer - timer) > 2500) {
        timer = interruptTimer;
        oled.drawLine(0, 0, 127, 0b00000000);
        arrowPos -= 44;
        drawSprite(arrowPos, 0, arrow, 8, 1);
        setCurrentTime = false;
      }
    }
  }
  alwaysOn = false;
}

void drawTime (uint8_t firstPixel, uint8_t page, uint8_t digitHeight, int8_t value) {// this function takes the digit and value from digits.h and draws it without the 0 to the left in hours
  if (value < 0) value = (~value) + 1; // xor value and add one to make it posite
  int8_t valueUnits = value;                      // always draws 2 digits ej: 01.

  if (value < 10) {

    if (firstPixel == 64 || firstPixel == 76) {
      for (uint8_t x = 1; x < 3; x++) {
        oled.drawLine (96, x, 32, 0B00000000);
      }
    } else {
      drawSprite((firstPixel + digitHeight), page, numbers[0], 32, 2);
    }

    drawSprite(firstPixel, page, numbers[value], 32, 2);

  } else {
    value /= 10; // some math to substract the 0 from the left in hours digits
    drawSprite((firstPixel + digitHeight), page, numbers[value], 32, 2);
    value *= 10;
    valueUnits -= value;
    drawSprite(firstPixel, page, numbers[valueUnits], 32, 2);
  }
}

void drawSprite(uint8_t column, uint8_t page, uint8_t *sprite, uint8_t SpritePixelsHeight, uint8_t SpritePagesWidth) {
  uint8_t i = 0;

  for (uint8_t y = 0; y < SpritePagesWidth; y++) {
    oled.setCursor(column, (page + y));
    oled.ssd1306_send_data_start();
    for (uint8_t x = (0 + i); x < (SpritePixelsHeight + i); x++) {

      if (!alwaysOn || x % 2 == 0) oled.ssd1306_send_data_byte(sprite[x]);
    }
    oled.ssd1306_send_data_stop();
    i += SpritePixelsHeight;
  }
}

bool buttonDebounce(uint16_t debounceDelay) {

  if (BUTTONHIGH) return false;
  timer = interruptTimer;

  while ((interruptTimer - timer) < debounceDelay || BUTTONLOW) { // super simple button debounce

    if ((interruptTimer - timer) > 3000 && ledColor != 0) {
      PORTA.DIR = 0b00000000;// led off
      ledColor = 0;
    }

    if ((interruptTimer - timer) > 6000)  _PROTECTED_WRITE(RSTCTRL.SWRR, 1); //reset device
  }
  return true;
}

void goToSleep (void) {
  if (!BATTCHR && (!enableAlwaysOn || powerSaveMode >= 240)) oled.ssd1306_send_command(0xAE);
  shouldWakeUp = false;
  PORTA.PIN5CTRL  |= PORT_ISC_BOTHEDGES_gc; //attach interrupt to portA pin 5 keeps pull up enabled
  // PORTA.PIN4CTRL  |= PORT_ISC_BOTHEDGES_gc;
  sleep_enable();
  sleep_cpu();// go to sleep
}

uint16_t readSupplyVoltage() { //returns value in millivolts  taken from megaTinyCore example
  analogReference(VDD);
  VREF.CTRLA = VREF_ADC0REFSEL_1V5_gc;
  // there is a settling time between when reference is turned on, and when it becomes valid.
  // since the reference is normally turned on only when it is requested, this virtually guarantees
  // that the first reading will be garbage; subsequent readings taken immediately after will be fine.
  // VREF.CTRLB|=VREF_ADC0REFEN_bm;
  // delay(10);
  uint16_t reading = analogRead(ADC_INTREF);
  reading = analogRead(ADC_INTREF);
  uint32_t intermediate = 1023UL * 1500;
  reading = intermediate / reading;
  return reading;
}


ISR(PORTA_PORT_vect) {
  if (PORTA.INTFLAGS & PIN4_bm) {

    if (BATTCHR) {
      chargeLedIndicator = true;
      PORTA.DIR = 0b00001000; // turn on red led for charging
    }
  }
  //PORTA.PIN4CTRL &= ~PORT_ISC_gm;   // disable  interrupts.
  PORTA.PIN5CTRL &= ~PORT_ISC_gm;
  VPORTA.INTFLAGS = (1 << 4) | (1 << 5); // clear interrupts flag
  shouldWakeUp = true;
  resumeOperation = true; // if screen always on is off reset screen after sleep
  alwaysOn = false;
  interruptTimer = 0;
  timer = 0;
  sleep_disable();
}

ISR(TCB0_INT_vect) {
  /* The interrupt flag has to be cleared manually */
  TCB0_INTFLAGS = 1; // clear flag
  interruptTimer++;

  if (chargeLedIndicator && interruptTimer > 2000) {
    chargeLedIndicator = false;
    ledColor = 0;
    PORTA.DIR = 0b00000000;   // turn off all leds.
  }
}

ISR(RTC_PIT_vect) {
  /* Clear flag by writing '1': */
  RTC.PITINTFLAGS = RTC_PI_bm;

  if (seconds++, seconds > 59) {// acutal time keeping
    seconds = 0;

    if (alwaysOn) shouldWakeUp = true;// wake to draw allways on display

    if (minutes++, minutes > 59) {
      minutes = 0;

      if (hours++, hours > 23) {
        hours = 0;
      }
    }
  }

  if (!shouldWakeUp) sleep_cpu();// beacause of interrupt only if true wake up
}
