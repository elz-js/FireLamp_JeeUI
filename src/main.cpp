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

//#define __IDPREFIX F("JeeUI2-")
#include <Arduino.h>
#include "config.h"
#include "main.h"

// глобальные переменные для работы с ними в программе
SHARED_MEM GSHMEM; // глобальная общая память эффектов
//INTRFACE_GLOBALS iGLOBAL; // объект глобальных переменных интерфейса
LAMP myLamp;

EffectWorker demka;

unsigned int timer1 =0;
unsigned int timer2 =0;


const int period = 10000;

void setup() {
    Serial.begin(115200);

      demka.workerset( (EFF_ENUM)random(1, demka.getModeAmount()) );
}

void loop() {

  // крутим эффект на максимуме
  demka.worker->run(myLamp.getUnsafeLedsArray());
  FastLED.show();

  // каждые 10 сек меняем эффект
  if (millis()-timer1 > period){
      demka.workerset( (EFF_ENUM)random(1, demka.getModeAmount()) );
      timer1 = millis();
  }

  // каждые 5 сек палитру
  if (millis()-timer2 > 5000){
      demka.worker->setscl(random8(0,255));
      demka.worker->scale2pallete();
      timer2 = millis();
  }

}

