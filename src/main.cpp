#include "main.h"

CRGB leds[256]; // buffer
LAMP myLamp; // см. заголовочник main.h - заглушка
EMBUI embui; // см. заголовочник main.h - заглушка

LAMPSTATE lampstate;

EffectWorker demka(&lampstate);
void setup() {
    Serial.begin(460800);
    demka.workerset( (EFF_ENUM)random(1, demka.getModeAmount()), false );
}

void loop() {
  static uint64_t timer1=0, timer2=0;

  // крутим эффект "по-дефолту"
  demka.worker->run(myLamp.getUnsafeLedsArray());
  FastLED.show();

  // каждые 10 сек меняем эффект на случайный
  if (millis()-timer1 > 10000){
      EFF_ENUM eff = (EFF_ENUM)random(1, 255);
      demka.workerset( eff, false );
      timer1 = millis();
      Serial.print("next effect:");
      Serial.println(eff);
  }

  // каждые 5 сек ставим случайную палитру
  if (millis()-timer2 > 5000){
      demka.worker->setscl(random8(0,255));
      demka.worker->scale2pallete();
      timer2 = millis();
  }
}