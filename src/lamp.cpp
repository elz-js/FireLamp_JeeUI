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

#include "lamp.h"
#include "main.h"

extern LAMP myLamp; // Объект лампы

void LAMP::lamp_init()
{
  FastLED.addLeds<WS2812B, LAMP_PIN, COLOR_ORDER>(leds, NUM_LEDS)  /*.setCorrection(TypicalLEDStrip)*/;
  //brightness(0, false);                          // начинаем с полностью потушеной матрицы 0-й яркости
  FastLED.clear();                                            // очистка матрицы
}

//void LAMP::handle(){}

//void LAMP::effectsTick(){}
// end of void LAMP::effectsTick()

/*
 * вывод готового кадра на матрицу,
 * и перезапуск эффект-процессора
 */
void LAMP::frameShow(const uint32_t ticktime){
  FastLED.show();
}


LAMP::LAMP()
    {
      MIRR_V = false; // отзрекаливание по V
      MIRR_H = false; // отзрекаливание по H
      _brt =0;
      _steps = 0;
      _brtincrement = 0;
      lamp_init(); // инициализация и настройка лампы
    }

    uint32_t LAMP::getPixelNumber(uint16_t x, uint16_t y) // получить номер пикселя в ленте по координатам
    {
      if ((THIS_Y % 2 == 0) || MATRIX_TYPE)                     // если чётная строка
      {
        return ((uint32_t)THIS_Y * SEGMENTS * _WIDTH + THIS_X)%NUM_LEDS;
      }
      else                                                      // если нечётная строка
      {
        return ((uint32_t)THIS_Y * SEGMENTS * _WIDTH + _WIDTH - THIS_X - 1)%NUM_LEDS;
      }
    }

    uint32_t LAMP::getPixColor(uint32_t thisSegm) // функция получения цвета пикселя по его номеру
    {
      uint32_t thisPixel = thisSegm * SEGMENTS;
      if (thisPixel > NUM_LEDS - 1) return 0;
      return (((uint32_t)leds[thisPixel].r << 16) | ((uint32_t)leds[thisPixel].g << 8 ) | (uint32_t)leds[thisPixel].b);
    }

    void LAMP::fillAll(CRGB color) // залить все
    {
      for (int32_t i = 0; i < NUM_LEDS; i++)
      {
        leds[i] = color;
      }
    }

    void LAMP::drawPixelXY(int16_t x, int16_t y, CRGB color) // функция отрисовки точки по координатам X Y
    {
      if (x < 0 || x > (int16_t)(WIDTH - 1) || y < 0 || y > (int16_t)(HEIGHT - 1)) return;
      uint32_t thisPixel = getPixelNumber((uint16_t)x, (uint16_t)y) * SEGMENTS;
      for (uint16_t i = 0; i < SEGMENTS; i++)
      {
        leds[thisPixel + i] = color;
      }
    }

/*
 * Change global brightness with or without fade effect
 * fade applied in non-blocking way
 * FastLED dim8 function applied internaly for natural brightness controll
 * @param uint8_t _brt - target brigtness level 0-255
 * @param bool fade - use fade effect on brightness change
 */
void LAMP::setBrightness(const uint8_t _brt, const bool fade, const bool natural){
}

/*
 * Get current brightness
 * FastLED brighten8 function applied internaly for natural brightness compensation
 * @param bool natural - return compensated or absolute brightness
 */
uint8_t LAMP::getBrightness(const bool natural){
    return (natural ? brighten8_raw(FastLED.getBrightness()) : FastLED.getBrightness());
}


/*
 * Set global brightness
 * @param bool natural
 */
void LAMP::brightness(const uint8_t _brt, bool natural){
    uint8_t _cur = natural ? brighten8_video(FastLED.getBrightness()) : FastLED.getBrightness();
    if ( _cur == _brt) return;

    if (_brt) {
      FastLED.setBrightness(natural ? dim8_video(_brt) : _brt);
    } else {
      FastLED.setBrightness(0); // полностью гасим лапу если нужна 0-я яркость
      FastLED.show();
    }
}


