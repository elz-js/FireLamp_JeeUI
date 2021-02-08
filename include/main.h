/*
Copyright © 2020 Dmytro Korniienko (kDn)
JeeUI2 lib used under MIT License Copyright (c) 2019 Marsel Akhkamov

    This file is part of FireLamp_JeeUI.

    FireLamp_JeeUI is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FireLamp_JeeUI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FireLamp_JeeUI.  If not, see <https://www.gnu.org/licenses/>.

  (Этот файл — часть FireLamp_JeeUI.

   FireLamp_JeeUI - свободная программа: вы можете перераспространять ее и/или
   изменять ее на условиях Стандартной общественной лицензии GNU в том виде,
   в каком она была опубликована Фондом свободного программного обеспечения;
   либо версии 3 лицензии, либо (по вашему выбору) любой более поздней
   версии.

   FireLamp_JeeUI распространяется в надежде, что она будет полезной,
   но БЕЗО ВСЯКИХ ГАРАНТИЙ; даже без неявной гарантии ТОВАРНОГО ВИДА
   или ПРИГОДНОСТИ ДЛЯ ОПРЕДЕЛЕННЫХ ЦЕЛЕЙ. Подробнее см. в Стандартной
   общественной лицензии GNU.

   Вы должны были получить копию Стандартной общественной лицензии GNU
   вместе с этой программой. Если это не так, см.
   <https://www.gnu.org/licenses/>.)
*/

#ifndef __MAIN_H_
#define __MAIN_H_

#include <Arduino.h>
#include "config.h"

#include "effects.h"

extern CRGB leds[256]; // buffer

static const char TCONST_FFFE[] PROGMEM = "false";
static const char TCONST_FFFF[] PROGMEM = "true";

class MICWORKER{
public:
  MICWORKER(uint8_t a, uint8_t b) {}
  uint8_t getMinPeak() {return 0;}
  uint8_t getMaxPeak() {return 0;}
  uint8_t process(uint8_t v) {return 0;}
  uint8_t fillSizeScaledArray(float *x, uint8_t v) {return 0;} // массив должен передаваться на 1 ед. большего размера
};

class TP{
public:
  String getFormattedShortTime(){return "";}
};

class EMBUI {
public:
  TP timeProcessor;
};

extern EMBUI embui; // см. заголовочник main.h - заглушка

class LAMP
{
public:
  CRGB *getUnsafeLedsArray() {return leds; }
  uint8_t getMicMaxPeak() {return 0;}
  uint8_t getMicFreq() {return 0;};
  uint8_t getMicMapMaxPeak() {return 0;}
  uint8_t getMicMapFreq() {return 0;};
  uint16_t getPixelNumber(uint8_t a, uint8_t b) {return 0;};
  uint16_t getPixelNumberBuff(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {return 0;};
  uint16_t getLampBrightness() {return 0;};
  uint16_t getmaxDim() {return HEIGHT>WIDTH?HEIGHT:WIDTH;};
  uint16_t getminDim() {return HEIGHT<WIDTH?HEIGHT:WIDTH;};
  void setMicAnalyseDivider(uint8_t v) {}
  uint8_t getMicScale() {return 0;};
  uint8_t getMicNoise() {return 0;};
  uint8_t getMicNoiseRdcLevel() {return 0;};
  bool isPrintingNow() {return true;}
  void sendStringToLamp(const char* text = nullptr,  const CRGB &letterColor = CRGB::Black, bool forcePrint = false, const int8_t textOffset = -128, const int16_t fixedPos = 0) {}
};

extern LAMP myLamp; // см. заголовочник main.h - заглушка

#endif
