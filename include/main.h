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
#include <Arduino.h>
#include "config.h"

// STRING Macro
#ifndef __STRINGIFY
 #define __STRINGIFY(a) #a
#endif
#define TOSTRING(x) __STRINGIFY(x)

// LOG macro's
#if defined(EMBUI_DEBUG)
        #define LOG(func, ...) Serial.func(__VA_ARGS__)
#else
        #define LOG(func, ...) ;
#endif

#ifdef ESP32
 #include <functional>
#endif

typedef std::function<void(void)> callback_function_t;

#include "lamp.h"

// глобальные переменные для работы с ними в программе
extern SHARED_MEM GSHMEM; // Глобальная разделяемая память эффектов
//extern INTRFACE_GLOBALS iGLOBAL; // объект глобальных переменных интерфейса
extern LAMP myLamp; // Объект лампы
