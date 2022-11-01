/*Game&Light  Clock for GAME&LIGHT and any Attiny series 1 compatible using neopixels
  Flash CPU Speed 5MHz.
  this code is released under GPL v3, you are free to use and modify.
  released on 2022.

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

#define SCREENOFF PORTA.OUT &= ~PIN3_bm;// led ON, also resets the oled screen.
#define SCREENON PORTA.OUT |= PIN3_bm;// led OFF
#define BUTTONLOW !(PORTA.IN & PIN6_bm)// button press low
#define BUTTONHIGH (PORTA.IN & PIN6_bm)// button not pressed high
#define BATTCHR !(PORTA.IN & PIN7_bm)// battery charging
#define MAXVOLTAGE 4100 // max voltage allowed to the battery
#define MINVOLTAGE 3300 // min voltage allowed to be operational
#define SPRITEINVERSECOLOR true
#define SPRITENORMAL false

uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t lastMinute = 0;
uint8_t lastSecond = 0;
uint16_t totalSeconds = 0;
uint16_t totalSecondsTimer = 0;
uint16_t timer = 0;
uint16_t sleepDelayTimer = 0;
bool enableAlwaysOn = false;
volatile boolean shouldWakeUp = true;
volatile boolean resumeOperation = true;
volatile boolean ledPWM = false;
volatile boolean alwaysOn = false;
volatile uint8_t powerSaveMode = 0;
volatile uint16_t interruptTimer = 0;

void setup() {
  PORTA.DIR = 0b00001110; // setup ports in and out
  PORTA.PIN6CTRL = PORT_PULLUPEN_bm;// button pullup
  PORTA.PIN7CTRL = PORT_PULLUPEN_bm;// button pullup

  TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm; // configure TCA as millis counter
  TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc; // set Normal mode
  TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTEI_bm);// disable event counting
  TCA0.SINGLE.PER = 77; // aprox 1ms, set the period(5000000/PER*DESIRED_HZ - 1)
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc // set clock  source (sys_clk/64)
                      | TCA_SINGLE_ENABLE_bm;

  while (RTC.STATUS > 0); //Wait for all register to be synchronized
  RTC.CTRLA = RTC_PRESCALER_DIV1_gc | RTC_RTCEN_bm | RTC_RUNSTDBY_bm; //PRESCALER = (32768/PERIOD)-1
  RTC.CLKSEL = RTC_CLKSEL_INT32K_gc; /* 32KHz Internal Ultra Low Power Oscillator (OSCULP32K) */
  RTC.PITINTCTRL = RTC_PI_bm; /* Periodic Interrupt: enabled */
  RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc /* RTC Clock Cycles 32768 */
                 | RTC_PITEN_bm; /* Enable: enabled */

  sei(); // enable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);// configure sleep mode
  SCREENON
}

void loop() {

  if (resumeOperation) {
    SCREENOFF //reset screen
    __asm__("nop\n\t"); // small delay
    SCREENON
    buttonDebounce();
    oled.begin();// start oled screen
    clearScreen();
    displayTime();
    displayBatt();
    resumeOperation = false;
    sleepDelayTimer = 4000;
  }

  if (BUTTONLOW && (interruptTimer - timer) > 800)  options();// enter menu

  if (totalSeconds != 0) { // timer logic

    if (seconds != lastSecond) {
      lastSecond = seconds;
      uint8_t timerBarValue = map(totalSeconds, 0 , totalSecondsTimer, 0, 126);
      oled.drawLine (timerBarValue, 0, 2, 0B00000000);
      oled.drawLine (0, 0, timerBarValue, 0B00000011);

      if (lastMinute != minutes || totalSeconds == 0) {
        lastMinute = minutes;
        displayTime();
        displayBatt();
      }

      if (totalSeconds--, totalSeconds < 1)  ledON();
    }
  }

  if ((interruptTimer - timer) > sleepDelayTimer && totalSeconds == 0 && !resumeOperation) {
    timer = interruptTimer;
    clearScreen();

    if (!BATTCHR && powerSaveMode < 240) drawAlwaysOnDisplay();// disable always on display if 12hs passed without pressing the button

    if (BATTCHR) drawSprite(0, 3, chargingSprite, 8, 1, SPRITENORMAL);

    if (BUTTONHIGH) goToSleep();
  }
}

void displayTime(void) {
  drawTime(64, 1, hours);
  drawTime(0, 1, minutes);
}

void displayBatt (void) {
  uint16_t actualVoltage = constrain(readSupplyVoltage (), MINVOLTAGE, MAXVOLTAGE);// read batt voltage and discards values outside range
  actualVoltage = map(actualVoltage, MINVOLTAGE, MAXVOLTAGE, 0, 117);
  oled.drawLine (10, 3, actualVoltage, 0B11100000);
  drawSprite(0, 3, battSprite, 8, 1, SPRITENORMAL);// draws battery or charging icon if connected to usb.
}

void ledON (void) {
  ledPWM = true;
  while (BUTTONHIGH && !BATTCHR);
  ledPWM = false;
  resumeOperation = true;
}

void screenInterlace(void) {
  for (uint8_t y = 1; y < 3; y++) {
    for (uint8_t x = 0; x < 127; x += 2) {
      oled.drawLine(x, y, 1, 0x00);
    }
  }
}

void drawAlwaysOnDisplay(void) {
  if (enableAlwaysOn) {

    if (powerSaveMode < 240 && alwaysOn) powerSaveMode++;// after 4 hours if always on display is enable it disables it to save battery

    if (!alwaysOn) {
      oled.ssd1306_send_command(0x81);// brightness controll
      oled.ssd1306_send_command(0x01);
      alwaysOn = true;
      sleepDelayTimer = 20;
    }
    displayTime();
    screenInterlace();
  }
}

void options (void) { // options menu
  int8_t option = 0;
  uint8_t spriteOptionPos [] = {100, 70, 40, 10};
  bool setOptions = true;
  clearScreen();
  drawSprite(spriteOptionPos [0], 1, optionSprites[0], 16, 2, SPRITEINVERSECOLOR);

  for (uint8_t x = 1; x < 4; x++) {
    drawSprite(spriteOptionPos [x], 1, optionSprites[x], 16, 2, SPRITENORMAL);
    option++;
  }
  option = 0;
  buttonDebounce();

  while (setOptions) {

    if (BUTTONLOW) {
      drawSprite(spriteOptionPos [option], 1, optionSprites[option], 16, 2, SPRITENORMAL);
      if (option++, option > 3) {
        setOptions = false;
      } else {
        drawSprite(spriteOptionPos [option], 1, optionSprites[option], 16, 2, SPRITEINVERSECOLOR);
        buttonDebounce();
      }
    }

    if ((interruptTimer - timer) > 2000) {
      timer = interruptTimer;
      switch (option) { // option select menu.
        case 0:// led on
          ledON();
          setOptions = false;
          break;
        case 1:// timer
          setTimer();
          setOptions = false;
          break;
        case 2:// set time
          setTime();
          setOptions = false;
          break;
        case 3:
          enableAlwaysOn = !enableAlwaysOn;
          setOptions = false;
          break;
      }
    }
  }
  lastMinute = minutes;
  lastSecond = seconds;
  resumeOperation = true;
}

void setTimer(void) {
  clearScreen();
  drawSprite(28, 0, arrow, 8, 1, SPRITENORMAL);
  drawTime(0, 1, 0);
  totalSeconds = 0;
  bool setCurrentTimer = true;

  while (setCurrentTimer) {

    if ((interruptTimer - timer) > 400 && BUTTONLOW) {
      timer = interruptTimer;

      if (totalSeconds += 300, totalSeconds > 1800) totalSeconds = 0;
      drawTime(0, 1, (totalSeconds / 60));
    }

    if ((interruptTimer - timer) > 2000) {
      //timer = interruptTimer;
      setCurrentTimer = false;
    }
  }
  totalSecondsTimer = totalSeconds;
}

void setTime(void) {
  clearScreen();
  drawTime(64, 1, hours);
  drawTime(0, 1, minutes);
  drawSprite(92, 0, arrow, 8, 1, SPRITENORMAL); // sleep

  for (uint8_t x = 0; x < 2 ; x++) {
    bool setCurrentTime = true;

    while (setCurrentTime) {

      if ((interruptTimer - timer) > 220 && BUTTONLOW) {
        timer = interruptTimer;
        seconds = 0;

        switch (x) {
          case 0:

            if (hours++, hours > 23) hours = 0;
            drawTime(64, 1, hours);
            break;
          case 1:

            if (minutes++, minutes > 59) minutes = 0;
            drawTime(0, 1, minutes);
            break;
        }
      }

      if ((interruptTimer - timer) > 2000) {
        timer = interruptTimer;
        setCurrentTime = false;
        oled.drawLine(0, 0, 127, 0b00000000);
        drawSprite(28, 0, arrow, 8, 1, SPRITENORMAL);
      }
    }
  }
}

void drawTime (uint8_t firstPixel, uint8_t page, int8_t value) {// this function takes the digit and value from digits.h and draws it without the 0 to the left in hours
  if (value < 0) value = (~value) + 1; // xor value and add one to make it posite
  int8_t valueUnits = value;                      // always draws 2 digits ej: 01.

  if (value < 10) {

    if (firstPixel == 64) {
      for (uint8_t x = 1; x < 3; x++) {
        oled.drawLine (96, x, 32, 0B00000000);
      }
    } else {
      drawSprite((firstPixel + 32), page, numbers[0], 32, 2, SPRITENORMAL);
    }

    drawSprite(firstPixel, page, numbers[value], 32, 2, SPRITENORMAL);

  } else {
    value /= 10; // some math to substract the 0 from the left in hours digits
    drawSprite((firstPixel + 32), page, numbers[value], 32, 2, SPRITENORMAL);
    value *= 10;
    for (int8_t x = 0; x < value ; x++) {
      valueUnits--;
    }
    drawSprite(firstPixel, page, numbers[valueUnits], 32, 2, SPRITENORMAL);
  }
}

void clearScreen (void) {
  for ( uint8_t x = 0; x < 4; x++) {
    oled.drawLine(0, x, 128, 0x00);
  }
}

void drawSprite(uint8_t column, uint8_t page, uint8_t sprite[], uint8_t SpritePixelsHeight, uint8_t SpritePagesWidth, bool inverseColor) {
  uint8_t i = 0;

  for (uint8_t y = 0; y < SpritePagesWidth; y++) {
    oled.setCursor(column, (page + y));
    oled.ssd1306_send_data_start();
    for (uint8_t x = (0 + i); x < (SpritePixelsHeight + i); x++) {

      if (inverseColor) oled.ssd1306_send_data_byte(~(sprite[x]));

      if (!inverseColor) oled.ssd1306_send_data_byte(sprite[x]);
    }
    oled.ssd1306_send_data_stop();
    i += SpritePixelsHeight;
  }
}

void buttonDebounce(void) {
  timer = interruptTimer;
  while ((interruptTimer - timer) < 250 || BUTTONLOW); // super simple button debounce
}

void goToSleep (void) {

  if (!BATTCHR && (!enableAlwaysOn || powerSaveMode >= 240)) oled.ssd1306_send_command(0xAE);
  shouldWakeUp = false;
  PORTA.PIN6CTRL  |= PORT_ISC_FALLING_gc; //attach interrupt to portA pin 6 keeps pull up enabled
  PORTA.PIN7CTRL  |= PORT_ISC_BOTHEDGES_gc;
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
  PORTA.PIN6CTRL &= ~PORT_ISC_gm;   // disable both interrupts.
  PORTA.PIN7CTRL &= ~PORT_ISC_gm;
  VPORTA.INTFLAGS = (1 << 6) | (1 << 7); // clear interrupts flag
  sleep_disable();
  shouldWakeUp = true;
  resumeOperation = true;
  alwaysOn = false;
  powerSaveMode = 0;
}

ISR(TCA0_OVF_vect) {
  /* The interrupt flag has to be cleared manually */
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
  if (ledPWM) PORTA.OUTTGL = PIN3_bm;
  interruptTimer++;
}

ISR(RTC_PIT_vect) {
  /* Clear flag by writing '1': */
  RTC.PITINTFLAGS = RTC_PI_bm;

  if (seconds++, seconds > 59) {// acutal time keeping
    seconds = 0;

    if (alwaysOn) shouldWakeUp  = true;// wake to draw allways on display

    if (minutes++, minutes > 59) {
      minutes = 0;

      if (hours++, hours > 23) {
        hours = 0;
      }
    }
  }
  if (!shouldWakeUp) sleep_cpu();// beacause of interrupt only if true wake up
}
