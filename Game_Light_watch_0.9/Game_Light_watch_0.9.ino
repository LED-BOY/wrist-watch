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
#define MAXVOLTAGE 4100 // max voltage allowed to the battery
#define MINVOLTAGE 3100 // min voltage allowed to be operational

uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t lastMinute = 0;
uint16_t timer = 0;
uint8_t minutesCounter = 0;
volatile boolean shouldWakeUp = true;
volatile boolean resumeOperation = true;
volatile boolean ledPWM = false;
volatile uint16_t interruptTimer = 0;

void setup() {
  PORTA.DIR = 0b10001110; // setup ports in and out
  PORTA.PIN6CTRL = PORT_PULLUPEN_bm;// button pullup
  PORTA.OUT |= PIN7_bm; //mosfet high, off

  TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm; // configure TCA as millis counter
  TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc; // set Normal mode
  TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTEI_bm);// disable event counting
  TCA0.SINGLE.PER = 77; // aprox 1ms, set the period(5000000/PER*DESIRED_HZ - 1)
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc // set clock  source (sys_clk/64)
                      | TCA_SINGLE_ENABLE_bm;

  RTC.CTRLA = RTC.CTRLA = RTC_PRESCALER_DIV1_gc | RTC_RTCEN_bm | RTC_RUNSTDBY_bm; //PRESCALER = (32768/PERIOD)-1
  //while (RTC.STATUS > 0); //Wait for all register to be synchronized
  RTC.CLKSEL = RTC_CLKSEL_INT32K_gc; /* 32KHz Internal Ultra Low Power Oscillator (OSCULP32K) */
  RTC.PITINTCTRL = RTC_PI_bm; /* Periodic Interrupt: enabled */
  RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc /* RTC Clock Cycles 32768 */
                 | RTC_PITEN_bm; /* Enable: enabled */

  sei(); // enable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);// configure sleep mode
}

void loop() {

  if (resumeOperation) {
    SCREENOFF
    SCREENON
    buttonDebounce();
    oled.begin();// start oled screen
    clearScreen();
    drawTime(64, 1, hours);
    drawTime(0, 1, minutes);
    drawSprite(0, 3, battIcon, false);
    uint16_t actualVoltage = readSupplyVoltage ();
    actualVoltage = map(actualVoltage, MINVOLTAGE, MAXVOLTAGE, 0, 117);
    oled.drawLine (10, 3, constrain(actualVoltage, 0, 117), 0B11100000);
    resumeOperation = false;
  }

  if (BUTTONLOW && (interruptTimer - timer) > 800) {
    timer = interruptTimer;
    options();
  }

  if (lastMinute != minutes && minutesCounter != 0) {
    lastMinute = minutes;
    if (minutesCounter--, minutesCounter < 1) {
      ledPWM = true;
      while (BUTTONHIGH);
      ledPWM = false;      
    }
    resumeOperation = true;
  }

  if ((interruptTimer - timer) > 4000 && minutesCounter == 0 && !resumeOperation) {
    timer = interruptTimer;
    goToSleep();
  }
}

void options (void) { // options menu
  int8_t option = 0;
  bool setOptions = true;
  clearScreen();
  drawSprite(90, 2, ledIcon, 0);
  drawSprite(70, 2, sandClock, 0);
  drawSprite(50, 2, setIcon, 0);
  buttonDebounce();

  while (setOptions) {

    if (BUTTONLOW) {
      if (option++, option > 3) option = 0;
      oled.drawLine(0, 1, 127, 0b00000000);
      buttonDebounce();
    }

    switch (option) { // option select menu.
      case 0:
        drawSprite(90, 1, arrow, 0); // led on

        if ((interruptTimer - timer) > 2000) {
          timer = interruptTimer;
          ledPWM = true;
          while (BUTTONHIGH);
          ledPWM = false;
          setOptions = false;
        }
        break;
      case 1:
        drawSprite(70, 1, arrow, 0); // timer
        while (BUTTONHIGH) {

          if ((interruptTimer - timer) > 2000) {
            clearScreen();
            timer = interruptTimer;
            if (minutesCounter += 5, minutesCounter > 30) minutesCounter = 0;
            drawTime(0, 1, minutesCounter);
            setOptions = false;
          }
        }
        break;
      case 2:
        drawSprite(50, 1, arrow, 0); // set time

        if ((interruptTimer - timer) > 2000) {
          timer = interruptTimer;
          setTime();
          setOptions = false;
        }
        break;
      case 3:
        setOptions = false;
        break;
    }
  }
  lastMinute = minutes;
  resumeOperation = true;
}

void setTime(void) {
  drawTime(64, 1, hours);
  drawTime(0, 1, minutes);
  drawSprite(92, 0, arrow, 0); // sleep

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
        drawSprite(28, 0, arrow, 0);
      }
    }
  }
}

void drawTime (uint8_t firstPixel, uint8_t page, int8_t value) {// this function takes the digit and value from characters.h and draws it without the 0 to the left in hours
  if (value < 0) value = (~value) + 1; // xor value and add one to make it posite
  int8_t valueUnits = value;                      // always draws 2 digits.

  if (value < 10) {

    if (firstPixel == 64) {
      for (uint8_t x = 1; x < 3; x++) {
        oled.drawLine (96, x, 32, 0B00000000);
      }
    } else {
      drawSprite((firstPixel + 32), page, numbers[0], true);
    }
    drawSprite(firstPixel, page, numbers[value], true);
  } else {
    value /= 10; // some math to substract the 0 from the left in hours digits
    drawSprite((firstPixel + 32), page, numbers[value], true);
    value *= 10;

    for (int8_t x = 0; x < value ; x++) {
      valueUnits--;
    }
    drawSprite(firstPixel, page, numbers[valueUnits], true);
  }
}

void clearScreen (void) {
  for ( uint8_t x = 0; x < 4; x++) {
    oled.drawLine(0, x, 128, 0x00);
  }
}

void drawSprite (uint8_t column, uint8_t page, uint8_t sprite [], bool digit) {
  uint8_t i = 0;

  if (digit) {

    for (uint8_t y = 0; y < 2; y++) {
      oled.setCursor(column, (page + y));
      oled.ssd1306_send_data_start();

      for (uint8_t x = (0 + i); x < (32 + i); x++) {
        oled.ssd1306_send_data_byte(sprite[x]);
      }
      oled.ssd1306_send_data_stop();
      i += 32;
    }
  } else {
    oled.setCursor(column, page);
    oled.ssd1306_send_data_start();
    for (uint8_t x = 0; x < 8; x++) {
      oled.ssd1306_send_data_byte(sprite[x]);
    }
    oled.ssd1306_send_data_stop();
  }
}

void buttonDebounce(void) {
  while ((interruptTimer - timer) < 200 || BUTTONLOW); // super simple button debounce
  timer = interruptTimer;
}

void goToSleep (void) {
  clearScreen();
  oled.ssd1306_send_command(0xAE);
  PORTA.PIN6CTRL  |= PORT_ISC_FALLING_gc; //attach interrupt to portA pin 3 keeps pull up enabled
  shouldWakeUp = false;
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

  if (PORTA.INTFLAGS & PIN6_bm) {
    sleep_disable();
    PORTA.PIN6CTRL &= ~ PORT_ISC_FALLING_gc; // detach interrupt
    PORTA.INTFLAGS &= PIN6_bm; // clear interrupt flag
    shouldWakeUp = true;
    resumeOperation = true;
  }
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

    if (minutes++, minutes > 59) {
      minutes = 0;
      seconds = 17;  // internal crystal is slow to keep time accurate, compensate that adding some seconds every hour

      if (hours++, hours > 23) {
        hours = 0;
      }
    }
  }

  if (!shouldWakeUp) {// beacause of interrupt only if true wake up
    sleep_cpu();
  }
}