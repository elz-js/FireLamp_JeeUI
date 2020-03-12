#ifndef __OTA_H
#define __OTA_H
/*
 * 11.07.2019
 * Класс, который отслеживает действия пользователя по запросу обновления прошивки по воздуху и выполняет эту прошивку.
 * Запрос на обновление - это вызов метода RequestOtaUpdate(), его нужно поместить, например, в обработчик нажатия кнопки, приёма UDP пакета и т.д.
 * Для обновления пользователь должен ДВАЖДЫ запросить обновление в течение заданного промежутка времени (CONFIRMATION_TIMEOUT) во избежание случайного перехода в режим обновления.
 * Режим обновления - это прослушивание специального порта (ESP_OTA_PORT) в ожидании команды обновления прошивки по воздуху (по сети).
 * Режим обновления работает параллельно с основным режимом функционирования, только при WiFi клиент, т.к. требует доступа к ESP по локальной сети.
 * Режим обновления активен в течение заданного промежутка времени (ESP_CONF_TIMEOUT). Потом ESP автоматически перезагружается.
 * Обновление производится из Arduino IDE: меню Инструменты - Порт - <Выбрать обнаруженный СЕТЕВОЙ COM порт из списка> (если он не обнаружен, значит что-то настроено неправильно), затем обычная команда "Загрузка" для прошивки.
 * Для включения опции обновления по воздуху в основном файле должен быть определён идентификатор OTA "#define OTA" и режим "#define ESP_MODE (1U)" (а также в данном проекте должна быть подключена кнопка).
*/

#include "config.h"
#include "JeeUI2.h"
#ifdef OTA
#include <ArduinoOTA.h>
//#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>

#define ESP_OTA_PORT          (3232U)                       // номер порта, который будет "прослушиваться" в ожидании команды прошивки по воздуху 8266U/3232U
#define CONFIRMATION_TIMEOUT  (30U)                         // время в сеундах, в течение которого нужно дважды подтвердить старт обновлениЯ по воздуху (иначе сброс в None)
#define ESP_CONF_TIMEOUT      (120U)                        // время ожидания ОТА
#define OTA_PASS "12345"
typedef void (*ShowWarningDelegate)(CRGB color, uint32_t duration, uint16_t blinkHalfPeriod);
//extern void showWarning(CRGB color,uint32_t duration,uint16_t blinkHalfPeriod);

enum OtaPhase                                               // определение стадий процесса обновления по воздуху: нет, получено первое подтверждение, получено второе подтверждение, получено второе подтверждение - в процессе, обновление окончено
{
  None = 0,
  GotFirstConfirm,
  GotSecondConfirm,
  InProgress,
  Done
};

class OtaManager
{
  private:
    OtaPhase OtaFlag = OtaPhase::None;
  public:

    OtaManager(ShowWarningDelegate showWarningDelegate)
    {
      this->showWarningDelegate = showWarningDelegate;
    }

    bool RequestOtaUpdate()                                 // пользователь однократно запросил обновление по воздуху; возвращает true, когда переходит в режим обновления - startOtaUpdate()
    {
      if (OtaFlag == OtaPhase::None)
      {
        OtaFlag = OtaPhase::GotFirstConfirm;
        momentOfFirstConfirmation = millis();

#ifdef LAMP_DEBUG
        LOG.print(F("Получено первое подтверждение обновления по воздуху\nОжидание второго подтверждения\n"));
#endif

        return false;
      }

      if (OtaFlag == OtaPhase::GotFirstConfirm)
      {
        OtaFlag = OtaPhase::GotSecondConfirm;
        momentOfOtaStart = millis();

#ifdef LAMP_DEBUG
        LOG.print(F("Получено второе подтверждение обновления по воздуху\nСтарт режима обновления\n"));
#endif

        showWarningDelegate(CRGB::Yellow, 2000U, 500U);     // мигание жёлтым цветом 2 секунды (2 раза) - готовность к прошивке
        startOtaUpdate();
        return true;
      }

      return false;
    }

    void HandleOtaUpdate()
    {
      if (OtaFlag == OtaPhase::GotFirstConfirm &&
          millis() - momentOfFirstConfirmation >= CONFIRMATION_TIMEOUT * 1000)
      {
        OtaFlag = OtaPhase::None;
        momentOfFirstConfirmation = 0;

#ifdef LAMP_DEBUG
        LOG.print(F("Таймаут ожидания второго подтверждения превышен\nСброс флага в исходное состояние\n"));
#endif

        return;
      }

      if ((OtaFlag == OtaPhase::GotSecondConfirm || OtaFlag == OtaPhase::InProgress) &&
          millis() - momentOfOtaStart >= ESP_CONF_TIMEOUT * 1000)
      {
        OtaFlag = OtaPhase::None;
        momentOfOtaStart = 0;

#ifdef LAMP_DEBUG
        LOG.print(F("Таймаут ожидания прошивки по воздуху превышен\nСброс флага в исходное состояние\nПерезагрузка\n"));
        delay(500);
#endif

        showWarningDelegate(CRGB::Red, 2000U, 500U);        // мигание красным цветом 2 секунды (2 раза) - ожидание прошивки по воздуху прекращено, перезагрузка

        ESP.restart();
        return;
      }

      if (OtaFlag == OtaPhase::InProgress)
      {
        ArduinoOTA.handle();
      }
    }

  private:
    uint64_t momentOfFirstConfirmation = 0;                 // момент времени, когда получено первое подтверждение и с которого начинается отсчёт ожидания второго подтверждения
    uint64_t momentOfOtaStart = 0;                          // момент времени, когда развёрнута WiFi точка доступа для обновления по воздуху
    ShowWarningDelegate showWarningDelegate;

    void startOtaUpdate()
    {
      char espHostName[65];
      String id = WiFi.softAPmacAddress();
      id.replace(F(":"), F(""));
      sprintf_P(espHostName, PSTR("%s%s"),__IDPREFIX, id.c_str());

      ArduinoOTA.setPort(ESP_OTA_PORT);
      ArduinoOTA.setHostname(espHostName);
      ArduinoOTA.setPassword(OTA_PASS);

      ArduinoOTA.onStart([this]()
      {
        OtaFlag = OtaPhase::InProgress;
        char type[16];
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
          strcpy_P(type, PSTR("sketch"));
        }
        else // U_SPIFFS
        {
          strcpy_P(type, PSTR("filesystem"));
        }

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()

#ifdef LAMP_DEBUG
        LOG.printf_P(PSTR("Start updating %s\n"), type);
#endif
      });

      ArduinoOTA.onEnd([this]()
      {
        OtaFlag = OtaPhase::Done;
        momentOfFirstConfirmation = 0;
        momentOfOtaStart = 0;

#ifdef LAMP_DEBUG
        LOG.print(F("Обновление по воздуху выполнено\nПерезапуск"));
        delay(500);
#endif
      });

      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
      {
#ifdef LAMP_DEBUG
        LOG.printf_P(PSTR("Ход выполнения: %u%%\r"), (progress / (total / 100)));
#endif
      });

      ArduinoOTA.onError([this](ota_error_t error)
      {
        OtaFlag = OtaPhase::None;
        momentOfFirstConfirmation = 0;
        momentOfOtaStart = 0;

#ifdef LAMP_DEBUG
        LOG.printf_P(PSTR("Обновление по воздуху завершилось ошибкой [%u]: "), error);
#endif

        if (error == OTA_AUTH_ERROR)
        {
#ifdef LAMP_DEBUG
          LOG.println(F("Auth Failed"));
#endif
        }
        else if (error == OTA_BEGIN_ERROR)
        {
#ifdef LAMP_DEBUG
          LOG.println(F("Begin Failed"));
#endif
        }
        else if (error == OTA_CONNECT_ERROR)
        {
#ifdef LAMP_DEBUG
          LOG.println(F("Connect Failed"));
#endif
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
  #ifdef LAMP_DEBUG
          LOG.println(F("Receive Failed"));
  #endif
        }
        else if (error == OTA_END_ERROR)
        {
  #ifdef LAMP_DEBUG
          LOG.println(F("End Failed"));
  #endif
        }

#ifdef LAMP_DEBUG
        LOG.print(F("Сброс флага в исходное состояние\nПереход в режим ожидания запроса прошивки по воздуху\n"));
#endif
      });

      ArduinoOTA.setRebootOnSuccess(true);
      ArduinoOTA.begin();
      OtaFlag = OtaPhase::InProgress;

      #ifdef LAMP_DEBUG
      LOG.printf_P(PSTR("Для обновления в Arduino IDE выберите пункт меню Инструменты - Порт - '%s at "), espHostName);
      LOG.print(WiFi.localIP());
      LOG.println(F("'"));
      LOG.printf_P(PSTR("Затем нажмите кнопку 'Загрузка' в течение %u секунд и по запросу введите пароль '%s'\n"), ESP_CONF_TIMEOUT, OTA_PASS);
      LOG.println(F("Устройство с Arduino IDE должно быть в одной локальной сети с модулем ESP!"));
      #endif
    }
};

#endif
#endif