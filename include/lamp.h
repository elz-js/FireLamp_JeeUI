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

#pragma once
#include "config.h" // подключаем эффекты, там же их настройки
#include "effects.h"


// смена эффекта
typedef enum _EFFSWITCH {
    SW_NONE = 0,    // пустой
    SW_NEXT,        // следующий
    SW_PREV,        // предыдущий
    SW_RND,         // случайный
    SW_DELAY,       // сохраненный (для фейдера)
    SW_SPECIFIC,    // переход на конкретный эффект по индексу/имени
    SW_NEXT_DEMO,    // следующий для ДЕМО, исключая отключенные
    SW_WHITE_HI,
    SW_WHITE_LO,
} EFFSWITCH;

// управление Тикером
typedef enum _SCHEDULER {
    T_DISABLE = 0,    // Выкл
    T_ENABLE,         // Вкл
    T_RESET,          // сброс
} SCHEDULER;
/*
 минимальная задержка между обсчетом и выводом кадра, мс
 нужна для обработки других задач в loop() между длинными вызовами
 калькулятора эффектов и его отрисовки. С другой стороны это время будет
 потеряно в любом случае, даже если остальные таски будут обработаны быстрее
 пока оставим тут, это крутилка не для общего конфига
 */
#define LED_SHOW_DELAY 1


class LAMP {
private:
#pragma pack(push,1)
 struct {
    bool MIRR_V:1; // отзрекаливание по V
    bool MIRR_H:1; // отзрекаливание по H
 };
 #pragma pack(pop)

    byte globalBrightness = BRIGHTNESS; // глобальная яркость, пока что будет использоваться для демо-режимов
#ifdef LAMP_DEBUG
    uint8_t fps = 0;    // fps counter
#endif
    const int MODE_AMOUNT = sizeof(_EFFECTS_ARR)/sizeof(EFFECT);     // количество режимов
    const uint16_t maxDim = ((WIDTH>HEIGHT)?WIDTH:HEIGHT);
    const uint16_t minDim = ((WIDTH<HEIGHT)?WIDTH:HEIGHT);

    EFF_ENUM storedEffect = EFF_NONE;

    // async fader and brightness control vars and methods
    uint8_t _brt, _steps;
    int8_t _brtincrement;

    /*
     * вывод готового кадра на матрицу,
     * и перезапуск эффект-процессора
     */
    void frameShow(const uint32_t ticktime);


public:
    EffectWorker effects; // объект реализующий доступ к эффектам

    LAMP();

//    void handle();          // главная функция обработки эффектов
    void lamp_init();       // первичная инициализация Лампы

    // ---------- служебные функции -------------
    uint16_t getmaxDim() {return maxDim;}
    uint16_t getminDim() {return minDim;}

    uint32_t getPixelNumber(uint16_t x, uint16_t y); // получить номер пикселя в ленте по координатам
    uint32_t getPixColor(uint32_t thisSegm); // функция получения цвета пикселя по его номеру
    uint32_t getPixColorXY(uint16_t x, uint16_t y) { return getPixColor(getPixelNumber(x, y)); } // функция получения цвета пикселя в матрице по его координатам

    void fillAll(CRGB color); // залить все
    void drawPixelXY(int16_t x, int16_t y, CRGB color); // функция отрисовки точки по координатам X Y

    CRGB *getUnsafeLedsArray(){return leds;}
    CRGB *setLeds(uint16_t idx, CHSV val) { leds[idx] = val; return &leds[idx]; }
    CRGB *setLeds(uint16_t idx, CRGB val) { leds[idx] = val; return &leds[idx]; }
    void setLedsfadeToBlackBy(uint16_t idx, uint8_t val) { leds[idx].fadeToBlackBy(val); }
    void setLedsNscale8(uint16_t idx, uint8_t val) { leds[idx].nscale8(val); }

    //fadeToBlackBy
    void dimAll(uint8_t value) { for (uint16_t i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(value); } }
    CRGB getLeds(uint16_t idx) { return leds[idx]; }
    void blur2d(uint8_t val) {::blur2d(leds,WIDTH,HEIGHT,val);}

    /*
     * Change global brightness with or without fade effect
     * fade applied in non-blocking way
     * FastLED dim8 function applied internaly for natural brightness controll
     * @param uint8_t _tgtbrt - target brigtness level 0-255
     * @param bool fade - use fade effect on brightness change
     * @param bool natural - apply dim8 function for natural brightness controll
     */
    void setBrightness(const uint8_t _tgtbrt, const bool fade=FADE, const bool natural=true);

    /*
     * Get current brightness
     * FastLED brighten8 function applied internaly for natural brightness compensation
     * @param bool natural - return compensated or absolute brightness
     */
    uint8_t getBrightness(const bool natural=true);

    uint8_t getLampBrightness(){return _brt;};

    void brightness(const uint8_t _brt, bool natural);

    /*
     * переключатель эффектов для других методов,
     * может использовать фейдер, выбирать случайный эффект для демо
     * @param EFFSWITCH action - вид переключения (пред, след, случ.)
     * @param fade - переключаться через фейдер или сразу
     * @param effnb - номер эффекта
     * skip - системное поле - пропуск фейдера
     */
    void switcheffect(EFFSWITCH action = SW_NONE, bool fade = FADE, EFF_ENUM effnb = EFF_ENUM::EFF_NONE, bool skip = false);


    ~LAMP() {}
private:
    LAMP(const LAMP&);  // noncopyable
    LAMP& operator=(const LAMP&);  // noncopyable
    CRGB leds[NUM_LEDS];
#ifdef USELEDBUF
    //CRGB ledsbuff[NUM_LEDS]; // буфер под эффекты
    std::vector<CRGB> ledsbuff;
#endif
};

