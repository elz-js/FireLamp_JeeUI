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

#include "main.h"
#include "interface.h"
#include "effects.h"
#include "ui.h"
#include LANG                  //"text_res.h"

#ifdef ESP32
    #include "esp_littlefs.h"
#endif


Ticker optionsTicker;          // планировщик заполнения списка
Ticker sysTicker;              // системный планировщик
String tmpData;                // временное хранилище для отложенных операций

bool check_recovery_state(bool isSet){
    bool state = false; //return state;
    if(LittleFS.begin()){
        if(isSet && LittleFS.exists(F("/recovery.state"))){ // если перед созданием такой файл уже есть, то похоже бутлуп
            state = true;
            LittleFS.remove(F("/recovery.state"));
        } else if(isSet){ // создаем файл-маркер, раз его не было
            File f = LittleFS.open(F("/recovery.state"), "w");
            f.print(embui.param(FPSTR(TCONST_0016)).c_str());
            f.flush();
            f.close();
            delay(100);
        } else { // удаляем файл-маркер, инициализация завершена
            LittleFS.remove(F("/recovery.state"));
        }
    }
    return state;
}

void resetAutoTimers() // сброс таймера демо и настройка автосохранений
{
    embui.autoSaveReset(); // автосохранение конфига будет отсчитываться от этого момента
    myLamp.demoTimer(T_RESET);
    myLamp.DelayedAutoEffectConfigSave(CFG_AUTOSAVE_TIMEOUT); // настройка отложенной записи
}

#ifdef AUX_PIN
void AUX_toggle(bool key)
{
    if (key)
    {
        digitalWrite(AUX_PIN, AUX_LEVEL);
        embui.var(FPSTR(TCONST_000E), ("1"));
    }
    else
    {
        digitalWrite(AUX_PIN, !AUX_LEVEL);
        embui.var(FPSTR(TCONST_000E), ("0"));
    }
}
#endif

// Вывод значка микрофона в списке эффектов
#ifdef MIC_EFFECTS
    #define MIC_SYMBOL (micSymb ? (pgm_read_byte(T_EFFVER + (uint8_t)eff->eff_nb) % 2 == 0 ? " \U0001F399" : "") : "")
    #define MIC_SYMB bool micSymb = myLamp.getLampSettings().effHasMic
#else
    #define MIC_SYMBOL ""
    #define MIC_SYMB
#endif

// Вывод номеров эффектов в списке, в WebUI
//#define EFF_NUMBER (numList ? (String(eff->eff_nb) + ". ") : "")
#define EFF_NUMBER   (numList ? (eff->eff_nb <= 255 ? (String(eff->eff_nb) + ". ") : (String((byte)(eff->eff_nb & 0xFF)) + "." + String((byte)(eff->eff_nb >> 8) - 1U) + ". ")) : "")

void pubCallback(Interface *interf){
    if (!interf) return;
    //return; // Временно для увеличения стабильности. Пока разбираюсь с падениями.
    interf->json_frame_value();
    interf->value(FPSTR(TCONST_0001), embui.timeProcessor.getFormattedShortTime(), true);
    interf->value(FPSTR(TCONST_0002), String(ESP.getFreeHeap()), true);
    //interf->value(FPSTR(TCONST_008F), String(millis()/1000), true);
    char fuptime[16];
    uint32_t tm = millis()/1000;
    sprintf_P(fuptime, PSTR("%u.%02u:%02u:%02u"),tm/86400,(tm/3600)%24,(tm/60)%60,tm%60);
    interf->value(FPSTR(TCONST_008F), String(fuptime), true);

#ifdef ESP8266
    FSInfo fs_info;
    LittleFS.info(fs_info);
    //LOG(printf_P,PSTR("FS INFO: fs_info.totalBytes=%d,fs_info.usedBytes=%d\n"),fs_info.totalBytes,fs_info.usedBytes);
    interf->value(FPSTR(TCONST_00C2), String(fs_info.totalBytes-fs_info.usedBytes), true);
#endif

#ifdef ESP32
    // size_t t_bytes=0, u_bytes=0;
    // esp_littlefs_info(nullptr, &t_bytes, &u_bytes);
    // interf->value(FPSTR(TCONST_00C2), String(t_bytes - u_bytes), true);
    interf->value(FPSTR(TCONST_00C2), String(LittleFS.totalBytes() - LittleFS.usedBytes()), true);
#endif

    int32_t rssi = WiFi.RSSI();
    interf->value(FPSTR(TCONST_00CE), String(constrain(map(rssi, -85, -40, 0, 100),0,100)) + F("% (") + String(rssi) + F("dBm)"), true);

    interf->json_frame_flush();
}

void block_menu(Interface *interf, JsonObject *data){
    if (!interf) return;
    // создаем меню
    embui.autoSaveReset(); // автосохранение конфига будет отсчитываться от этого момента
    interf->json_section_menu();

    interf->option(FPSTR(TCONST_0000), FPSTR(TINTF_000));   //  Эффекты
    interf->option(FPSTR(TCONST_0003), FPSTR(TINTF_001));   //  Вывод текста
    interf->option(FPSTR(TCONST_00C8), FPSTR(TINTF_0CE));   //  Рисование
    interf->option(FPSTR(TCONST_0004), FPSTR(TINTF_002));   //  настройки
#ifdef SHOWSYSCONFIG
    if(myLamp.isShowSysMenu())
        interf->option(FPSTR(TCONST_009A), FPSTR(TINTF_08F));
#endif
    interf->json_section_end();
}

static EffectListElem *confEff = nullptr;
/**
 * Страница с контролами параметров эфеекта
 * 
 */
void block_effects_config_param(Interface *interf, JsonObject *data){
    if (!interf || !confEff) return;

    String tmpName, tmpSoundfile;
    myLamp.effects.loadeffname(tmpName,confEff->eff_nb);
    myLamp.effects.loadsoundfile(tmpSoundfile,confEff->eff_nb);
    interf->json_section_begin(FPSTR(TCONST_0005));
    interf->text(FPSTR(TCONST_0092), tmpName, FPSTR(TINTF_089), false);
#ifdef MP3PLAYER
    interf->text(FPSTR(TCONST_00AB), tmpSoundfile, FPSTR(TINTF_0B2), false);
#endif
    interf->checkbox(FPSTR(TCONST_0006), confEff->canBeSelected()? "1" : "0", FPSTR(TINTF_003), false);
    interf->checkbox(FPSTR(TCONST_0007), confEff->isFavorite()? "1" : "0", FPSTR(TINTF_004), false);

    interf->spacer();

    interf->select(FPSTR(TCONST_0050), FPSTR(TINTF_040));
    interf->option(String(SORT_TYPE::ST_BASE), FPSTR(TINTF_041));
    interf->option(String(SORT_TYPE::ST_END), FPSTR(TINTF_042));
    interf->option(String(SORT_TYPE::ST_IDX), FPSTR(TINTF_043));
    interf->option(String(SORT_TYPE::ST_AB), FPSTR(TINTF_085));
    interf->option(String(SORT_TYPE::ST_AB2), FPSTR(TINTF_08A));
#ifdef MIC_EFFECTS
    interf->option(String(SORT_TYPE::ST_MIC), FPSTR(TINTF_08D));  // эффекты с микрофоном
#endif
    interf->json_section_end();

    interf->button_submit(FPSTR(TCONST_0005), FPSTR(TINTF_008), FPSTR(TCONST_0008));
    interf->button_submit_value(FPSTR(TCONST_0005), FPSTR(TCONST_0009), FPSTR(TINTF_005));
    //if (confEff->eff_nb&0xFF00) { // пока удаление только для копий, но в теории можно удалять что угодно
        // interf->button_submit_value(FPSTR(TCONST_0005), FPSTR(TCONST_000A), FPSTR(TINTF_006), FPSTR(TCONST_000C));
    //}

    interf->json_section_line();
    interf->button_submit_value(FPSTR(TCONST_0005), FPSTR(TCONST_00B0), FPSTR(TINTF_0B5), FPSTR(TCONST_00B3));
    interf->button_submit_value(FPSTR(TCONST_0005), FPSTR(TCONST_00B1), FPSTR(TINTF_0B4), FPSTR(TCONST_000C));
    interf->json_section_end();

    interf->button_submit_value(FPSTR(TCONST_0005), FPSTR(TCONST_000B), FPSTR(TINTF_007), FPSTR(TCONST_000D));
    interf->button_submit_value(FPSTR(TCONST_0005), FPSTR(TCONST_0093), FPSTR(TINTF_08B), FPSTR(TCONST_000D));

    interf->json_section_end();
}

/**
 * Сформировать и вывести контролы для настроек параметров эффекта
 */
void show_effects_config_param(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_effects_config_param(interf, data);
    interf->json_frame_flush();
}

void delayedcall_effects_main();

/**
 * обработчик установок эффекта
 */
void set_effects_config_param(Interface *interf, JsonObject *data){
    if (!confEff || !data) return;
    if(optionsTicker.active())
        optionsTicker.detach();
    
    SETPARAM(FPSTR(TCONST_0050), myLamp.effects.setEffSortType((*data)[FPSTR(TCONST_0050)].as<SORT_TYPE>()));
    
    String act = (*data)[FPSTR(TCONST_0005)];
    if (act == FPSTR(TCONST_0009)) {
        myLamp.effects.copyEffect(confEff); // копируем текущий
    //} else if (act == FPSTR(TCONST_000A)) {
    } else if (act == FPSTR(TCONST_00B0) || act == FPSTR(TCONST_00B1)) {
        uint16_t tmpEffnb = confEff->eff_nb;
        bool isCfgRemove = (act == FPSTR(TCONST_00B1));
        LOG(printf_P,PSTR("confEff->eff_nb=%d\n"), tmpEffnb);
        if(tmpEffnb==myLamp.effects.getCurrent()){
            myLamp.effects.directMoveBy(EFF_ENUM::EFF_NONE);
            myLamp.effects.deleteEffect(confEff, isCfgRemove); // удаляем текущий
            remote_action(RA_EFF_NEXT, NULL);
        } else {
            myLamp.effects.deleteEffect(confEff, isCfgRemove); // удаляем текущий
        }
        String tmpStr=F("- ");
        tmpStr+=String(tmpEffnb);
        myLamp.sendString(tmpStr.c_str(), CRGB::Red);
        //confEff = myLamp.effects.getEffect(EFF_ENUM::EFF_NONE);
        if(isCfgRemove){
#ifndef DELAYED_EFFECTS
            sysTicker.once(5,std::bind([]{
#else
            sysTicker.once(1,std::bind([]{
#endif
                myLamp.effects.makeIndexFileFromFS(); // создаем индекс по файлам ФС и на выход
                delayedcall_effects_main();
            }));
        } else {
#ifndef DELAYED_EFFECTS
            sysTicker.once(5,std::bind([]{
#else
            sysTicker.once(1,std::bind([]{
#endif
                myLamp.effects.makeIndexFileFromList(); // создаем индекс по текущему списку и на выход
                delayedcall_effects_main();
            }));
        }
        section_main_frame(interf, data);
        return;
    } else if (act == FPSTR(TCONST_000B)) {
#ifndef DELAYED_EFFECTS
        sysTicker.once(5,std::bind([]{
#else
        sysTicker.once(1,std::bind([]{
#endif
            myLamp.effects.makeIndexFileFromFS(); // создаем индекс по файлам ФС и на выход
            delayedcall_effects_main();
        }));
        section_main_frame(interf, data);
        return;
    } else if (act == FPSTR(TCONST_0093)) {
        LOG(printf_P,PSTR("confEff->eff_nb=%d\n"), confEff->eff_nb);
        if(confEff->eff_nb==myLamp.effects.getCurrent()){
            myLamp.effects.directMoveBy(EFF_ENUM::EFF_NONE);
            myLamp.effects.removeConfig(confEff->eff_nb);
            remote_action(RA_EFF_NEXT, NULL);
        } else {
            myLamp.effects.removeConfig(confEff->eff_nb);
        }
        String tmpStr=F("- ");
        tmpStr+=confEff->eff_nb;
        myLamp.sendString(tmpStr.c_str(), CRGB::Red);
        //confEff = myLamp.effects.getEffect(EFF_ENUM::EFF_NONE);
        section_main_frame(interf, data);
        return;
    }
    else {
        confEff->canBeSelected((*data)[FPSTR(TCONST_0006)] == "1");
        confEff->isFavorite((*data)[FPSTR(TCONST_0007)] == "1");
        myLamp.effects.setSoundfile((*data)[FPSTR(TCONST_00AB)], confEff);
#ifdef CASHED_EFFECTS_NAMES
        confEff->setName((*data)[FPSTR(TCONST_0092)]);
#endif
        myLamp.effects.setEffectName((*data)[FPSTR(TCONST_0092)], confEff);
    }

    resetAutoTimers();
    myLamp.effects.makeIndexFileFromList(); // обновить индексный файл после возможных изменений
    section_main_frame(interf, data);
}

void block_effects_config(Interface *interf, JsonObject *data, bool fast=true){
    if (!interf) return;

    interf->json_section_main(FPSTR(TCONST_000F), FPSTR(TINTF_009));
    confEff = myLamp.effects.getSelectedListElement();
    interf->select(FPSTR(TCONST_0010), String((int)confEff->eff_nb), String(FPSTR(TINTF_00A)), true);

    uint32_t timest = millis();
    if(fast){
        // Сначала подгрузим дефолтный список, а затем спустя время - подтянем имена из конфига

        //interf->option(String(myLamp.effects.getSelected()), myLamp.effects.getEffectName());
        String effname((char *)0);
        EffectListElem *eff = nullptr;
        MIC_SYMB;
        //bool numList = myLamp.getLampSettings().numInList;
        while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
            effname = FPSTR(T_EFFNAMEID[(uint8_t)eff->eff_nb]);
            interf->option(String(eff->eff_nb),
                //EFF_NUMBER + 
                String(eff->eff_nb) + (eff->eff_nb>255 ? String(F(" (")) + String(eff->eff_nb&0xFF) + String(F(")")) : String("")) + String(F(". ")) +
                String(effname) + 
                MIC_SYMBOL
            );
            #ifdef ESP8266
            ESP.wdtFeed();
            #elif defined ESP32
            delay(1);
            #endif
        }
        //interf->option(String(0),"");
    } else {
        EffectListElem *eff = nullptr;
        LOG(println,F("DBG1: using slow Names generation"));
        String effname((char *)0);
        MIC_SYMB;
        //bool numList = myLamp.getLampSettings().numInList;
        while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
            myLamp.effects.loadeffname(effname, eff->eff_nb);
            interf->option(String(eff->eff_nb),
                //EFF_NUMBER + 
                String(eff->eff_nb) + (eff->eff_nb>255 ? String(F(" (")) + String(eff->eff_nb&0xFF) + String(F(")")) : String("")) + String(F(". ")) +
                String(effname) + 
                MIC_SYMBOL
            );
            #ifdef ESP8266
            ESP.wdtFeed();
            #elif defined ESP32
            delay(1);
            #endif
        }
    }
    //interf->option(String(0),"");
    interf->json_section_end();
    LOG(printf_P,PSTR("DBG1: generating Names list took %ld ms\n"), millis() - timest);

    block_effects_config_param(interf, nullptr);

    interf->spacer();
    interf->button(FPSTR(TCONST_0000), FPSTR(TINTF_00B));
    interf->json_section_end();
}

void delayedcall_show_effects_config(){
    Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 3000) : nullptr;
    if (!interf) return;
    interf->json_frame_interface();
    interf->json_section_content();
    interf->select(FPSTR(TCONST_0010), String((int)confEff->eff_nb), String(FPSTR(TINTF_00A)), true, true); // не выводить метку
    EffectListElem *eff = nullptr;
    String effname((char *)0);
    MIC_SYMB;
    //bool numList = myLamp.getLampSettings().numInList;
    while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
        myLamp.effects.loadeffname(effname, eff->eff_nb);
        interf->option(String(eff->eff_nb),
            //EFF_NUMBER + 
            String(eff->eff_nb) + (eff->eff_nb>255 ? String(F(" (")) + String(eff->eff_nb&0xFF) + String(F(")")) : String("")) + String(F(". ")) +
            String(effname) + 
            MIC_SYMBOL                
        );
        #ifdef ESP8266
        ESP.wdtFeed();
        #elif defined ESP32
        delay(1);
        #endif
    }
    //interf->option(String(0),"");
    interf->json_section_end();
    interf->json_section_end();
    interf->json_frame_flush();
    delete interf;
    if(optionsTicker.active())
        optionsTicker.detach();
}

void show_effects_config(Interface *interf, JsonObject *data){
#ifdef DELAYED_EFFECTS
    if (!interf) return;
    interf->json_frame_interface();
    block_effects_config(interf, data);
    interf->json_frame_flush();
    if(!optionsTicker.active())
        optionsTicker.once(10,std::bind(delayedcall_show_effects_config));
#else
    if (!interf) return;
    interf->json_frame_interface();
    block_effects_config(interf, data, false);
    interf->json_frame_flush();
#endif
}

void set_effects_config_list(Interface *interf, JsonObject *data){
    if (!data) return;
    uint16_t num = (*data)[FPSTR(TCONST_0010)].as<uint16_t>();

    // Так  нельзя :(, поскольку интерфейс отдаст только effListConf, а не весь блок...
    // А мне хотелось бы попереключать список, сделав несколько изменений в флагах, не нажимая для каждого раза "сохранить"
    // Есть красивый способ это сделать по переключению списка?
    if(confEff){ // если переключаемся, то сохраняем предыдущие признаки в эффект до переключения
        LOG(printf_P, PSTR("eff_sel: %d eff_fav : %d\n"), (*data)[FPSTR(TCONST_0006)].as<bool>(),(*data)[FPSTR(TCONST_0007)].as<bool>());
        // confEff->canBeSelected((*data)[FPSTR(TCONST_0006)] == "1");
        // confEff->isFavorite((*data)[FPSTR(TCONST_0007)] == "1");
    }

    confEff = myLamp.effects.getEffect(num);
    show_effects_config_param(interf, data);
    resetAutoTimers();
}

void publish_ctrls_vals()
{
  embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_00AE), myLamp.effects.geteffconfig(String(myLamp.effects.getCurrent()).toInt(), myLamp.getNormalizedLampBrightness()), true);
}

void block_effects_param(Interface *interf, JsonObject *data){
    //if (!interf) return;
    bool isinterf = (interf != nullptr); // буду публиковать, даже если WebUI клиентов нет

    if(isinterf) interf->json_section_begin(FPSTR(TCONST_0011));

    LList<UIControl*>&controls = myLamp.effects.getControls();
    uint8_t ctrlCaseType; // тип контрола, старшие 4 бита соответствуют CONTROL_CASE, младшие 4 - CONTROL_TYPE
    for(int i=0; i<controls.size();i++){
        ctrlCaseType = controls[i]->getType();
        switch(ctrlCaseType>>4){
            case CONTROL_CASE::HIDE :
                continue;
                break;
            case CONTROL_CASE::ISMICON :
#ifdef MIC_EFFECTS
                if(!myLamp.isMicOnOff()) continue;
#else
                continue;
#endif          
                break;
            case CONTROL_CASE::ISMICOFF :
#ifdef MIC_EFFECTS
                if(myLamp.isMicOnOff()) continue;
#else
                continue;
#endif   
                break;
            default: break;
        }
        bool isRandDemo = (myLamp.getLampSettings().dRand && myLamp.getMode()==LAMPMODE::MODE_DEMO);
        switch(ctrlCaseType&0x0F){
            case CONTROL_TYPE::RANGE :
                {
                    String ctrlId = controls[i]->getId()==0 ? String(FPSTR(TCONST_0012))
                        : controls[i]->getId()==1 ? String(FPSTR(TCONST_0013))
                        : controls[i]->getId()==2 ? String(FPSTR(TCONST_0014))
                        : String(FPSTR(TCONST_0015)) + String(controls[i]->getId());
                    String ctrlName = i ? controls[i]->getName() : (myLamp.IsGlobalBrightness() ? FPSTR(TINTF_00C) : FPSTR(TINTF_00D));
                    if(isRandDemo && controls[i]->getId()>0)
                        ctrlName=String(FPSTR(TINTF_0C9))+ctrlName;
                    int value = i ? controls[i]->getVal().toInt() : myLamp.getNormalizedLampBrightness();
                    if(isinterf) interf->range(
                        ctrlId
                        ,value
                        ,controls[i]->getMin().toInt()
                        ,controls[i]->getMax().toInt()
                        ,controls[i]->getStep().toInt()
                        , ctrlName
                        , true);
                    if(controls[i]->getId()<3)
                        embui.publish(String(FPSTR(TCONST_008B)) + ctrlId, String(value), true);
                }
                break;
            case CONTROL_TYPE::EDIT :
                {
                    String ctrlName = controls[i]->getName();
                    if(isRandDemo && controls[i]->getId()>0)
                        ctrlName=String(FPSTR(TINTF_0C9))+ctrlName;
                    
                    if(isinterf) interf->text(String(FPSTR(TCONST_0015)) + String(controls[i]->getId())
                    , controls[i]->getVal()
                    , ctrlName
                    , true
                    );
                    //embui.publish(String(FPSTR(TCONST_008B)) + String(FPSTR(TCONST_0015)) + String(controls[i]->getId()), String(controls[i]->getVal()), true);
                    break;
                }
            case CONTROL_TYPE::CHECKBOX :
                {
                    String ctrlName = controls[i]->getName();
                    if(isRandDemo && controls[i]->getId()>0)
                        ctrlName=String(FPSTR(TINTF_0C9))+ctrlName;

                    if(isinterf) interf->checkbox(String(FPSTR(TCONST_0015)) + String(controls[i]->getId())
                    , controls[i]->getVal()
                    , ctrlName
                    , true
                    );
                    //embui.publish(String(FPSTR(TCONST_008B)) + String(FPSTR(TCONST_0015)) + String(controls[i]->getId()), String(controls[i]->getVal()), true);
                    break;
                }
            default:
                break;
        }
    }
    publish_ctrls_vals();
    if(isinterf) interf->json_section_end();
}

void show_effects_param(Interface *interf, JsonObject *data){
    //if (!interf) return;
    bool isinterf = (interf != nullptr); // буду публиковать, даже если WebUI клиентов нет
    if(isinterf) interf->json_frame_interface();
    block_effects_param(interf, data);
    if(isinterf) interf->json_frame_flush();
}

void set_effects_list(Interface *interf, JsonObject *data){
    if (!data) return;
    uint16_t num = (*data)[FPSTR(TCONST_0016)].as<uint16_t>();
    uint16_t curr = myLamp.effects.getSelected();
    EffectListElem *eff = myLamp.effects.getEffect(num);
    if (!eff) return;

    myLamp.setDRand(myLamp.getLampSettings().dRand); // сборосить флаг рандомного демо
    LOG(printf_P, PSTR("EFF LIST n:%d, o:%d, on:%d, md:%d\n"), eff->eff_nb, curr, myLamp.isLampOn(), myLamp.getMode());
    if (eff->eff_nb != curr) {
        if (!myLamp.isLampOn()) {
            myLamp.effects.directMoveBy(eff->eff_nb); // переходим на выбранный эффект для начальной инициализации
        } else {
            myLamp.switcheffect(SW_SPECIFIC, myLamp.getFaderFlag(), eff->eff_nb);
        }
        if(myLamp.getMode()==LAMPMODE::MODE_NORMAL)
            embui.var(FPSTR(TCONST_0016), (*data)[FPSTR(TCONST_0016)]);
        resetAutoTimers();
    }

    show_effects_param(interf, data);
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0082), String(eff->eff_nb), true);
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_00AE), myLamp.effects.geteffconfig(String(eff->eff_nb).toInt(), myLamp.getNormalizedLampBrightness()), true); // publish_ctrls_vals
}

void set_effects_bright(Interface *interf, JsonObject *data){
    if (!data) return;

    byte bright = (*data)[FPSTR(TCONST_0012)];
    if (myLamp.getNormalizedLampBrightness() != bright) {
        myLamp.setLampBrightness(bright);
        if(myLamp.isLampOn())
            myLamp.setBrightness(myLamp.getNormalizedLampBrightness(), !((*data)[FPSTR(TCONST_0017)]));
        if (myLamp.IsGlobalBrightness()) {
            embui.var(FPSTR(TCONST_0018), (*data)[FPSTR(TCONST_0012)]);
        }
        if(myLamp.effects.worker && myLamp.effects.getEn())
            myLamp.effects.worker->setbrt((*data)[FPSTR(TCONST_0012)].as<byte>()); // передача значения в эффект
        LOG(printf_P, PSTR("Новое значение яркости: %d\n"), myLamp.getNormalizedLampBrightness());
    }
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0012), String(bright), true);
    publish_ctrls_vals();
    resetAutoTimers();
}

void set_effects_speed(Interface *interf, JsonObject *data){
    if (!data) return;

    if(!myLamp.effects.getEn()) return;
    myLamp.effects.getControls()[1]->setVal((*data)[FPSTR(TCONST_0013)]);
    if(myLamp.effects.worker && myLamp.effects.getEn())
        myLamp.effects.worker->setspd((*data)[FPSTR(TCONST_0013)].as<byte>()); // передача значения в эффект
    LOG(printf_P, PSTR("Новое значение скорости: %d\n"), (*data)[FPSTR(TCONST_0013)].as<byte>());
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0013), (*data)[FPSTR(TCONST_0013)], true);
    publish_ctrls_vals();
    resetAutoTimers();
}

void set_effects_scale(Interface *interf, JsonObject *data){
    if (!data) return;

    if(!myLamp.effects.getEn()) return;
    myLamp.effects.getControls()[2]->setVal((*data)[FPSTR(TCONST_0014)]);
    if(myLamp.effects.worker && myLamp.effects.getEn())
        myLamp.effects.worker->setscl((*data)[FPSTR(TCONST_0014)].as<byte>()); // передача значения в эффект
    LOG(printf_P, PSTR("Новое значение масштаба: %d\n"), (*data)[FPSTR(TCONST_0014)].as<byte>());
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0014), (*data)[FPSTR(TCONST_0014)], true);
    publish_ctrls_vals();
    resetAutoTimers();
}

void set_effects_dynCtrl(Interface *interf, JsonObject *data){
    if (!data) return;

    if(!myLamp.effects.getEn()) return;
    String ctrlName;
    LList<UIControl*>&controls = myLamp.effects.getControls();
    for(int i=3; i<controls.size();i++){
        ctrlName = String(FPSTR(TCONST_0015))+String(controls[i]->getId());
        if((*data).containsKey(ctrlName)){
            controls[i]->setVal((*data)[ctrlName]);
            LOG(printf_P, PSTR("Новое значение дин. контрола %d: %s\n"), controls[i]->getId(), (*data)[ctrlName].as<String>().c_str());
            if(myLamp.effects.worker && myLamp.effects.getEn())
                myLamp.effects.worker->setDynCtrl(controls[i]);
            //embui.publish(String(FPSTR(TCONST_008B)) + ctrlName, (*data)[ctrlName], true);
            publish_ctrls_vals();
        }
    }
    resetAutoTimers();
}

/**
 * Блок с наборами основных переключателей лампы
 * вкл/выкл, демо, кнопка и т.п.
 */
void block_main_flags(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_begin(FPSTR(TCONST_0019));
    interf->json_section_line("");
    interf->checkbox(FPSTR(TCONST_001A), String(myLamp.isLampOn()), FPSTR(TINTF_00E), true);
    interf->checkbox(FPSTR(TCONST_001B), String(myLamp.getMode() == MODE_DEMO), FPSTR(TINTF_00F), true);
    interf->checkbox(FPSTR(TCONST_001C), String(myLamp.IsGlobalBrightness()), FPSTR(TINTF_010), true);
    interf->checkbox(FPSTR(TCONST_001D), String(myLamp.IsEventsHandled()), FPSTR(TINTF_011), true);
    interf->checkbox(FPSTR(TCONST_00C4), String(myLamp.isDrawOn()), FPSTR(TINTF_0CE), true);

#ifdef MIC_EFFECTS
    interf->checkbox(FPSTR(TCONST_001E), myLamp.isMicOnOff()? "1" : "0", FPSTR(TINTF_012), true);
#endif
#ifdef AUX_PIN
    interf->checkbox(FPSTR(TCONST_000E), FPSTR(TCONST_000E), true);
#endif
#ifdef ESP_USE_BUTTON
    interf->checkbox(FPSTR(TCONST_001F), myButtons->isButtonOn()? "1" : "0", FPSTR(TINTF_013), true);
#endif
#ifdef MP3PLAYER
    interf->checkbox(FPSTR(TCONST_009D), myLamp.isONMP3()? "1" : "0", FPSTR(TINTF_099), true);
#endif
#ifdef LAMP_DEBUG
    interf->checkbox(FPSTR(TCONST_0095), myLamp.isDebugOn()? "1" : "0", FPSTR(TINTF_08E), true);
#endif
    interf->json_section_end();
#ifdef MP3PLAYER
    interf->json_section_line(F("line124")); // спец. имя - разбирается внутри html
    if(mp3->isMP3Mode()){
        interf->button(FPSTR(TCONST_00BE), FPSTR(TINTF_0BD), FPSTR(TCONST_0008));
        interf->button(FPSTR(TCONST_00BF), FPSTR(TINTF_0BE), FPSTR(TCONST_0008));
        interf->button(FPSTR(TCONST_00C0), FPSTR(TINTF_0BF), FPSTR(TCONST_0008));
        interf->button(FPSTR(TCONST_00C1), FPSTR(TINTF_0C0), FPSTR(TCONST_0008));
    }
    //interf->button("time", FPSTR(TINTF_016), FPSTR(TCONST_0025));    
    interf->json_section_end();
    interf->range(FPSTR(TCONST_00A2), 1, 30, 1, FPSTR(TINTF_09B), true);
#endif
    interf->json_section_end();
}

/**
 * Формирование и вывод интерфейса с основными переключателями
 * вкл/выкл, демо, кнопка и т.п.
 */
void show_main_flags(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_main_flags(interf, data);
    interf->spacer();
    interf->button(FPSTR(TCONST_0000), FPSTR(TINTF_00B));
    interf->json_frame_flush();
}

void delayedcall_effects_main(){
    Interface *interf = embui.ws.count()? new Interface(&embui, &embui.ws, 3000) : nullptr;
    if (!interf) return;
    interf->json_frame_interface();
    interf->json_section_content();
    interf->select(FPSTR(TCONST_0016), String(myLamp.effects.getSelected()), String(FPSTR(TINTF_00A)), true, true); // не выводить метку
    EffectListElem *eff = nullptr;
    String effname((char *)0);
    bool isEmptyHidden=false;
    MIC_SYMB;
    bool numList = myLamp.getLampSettings().numInList;
    while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
        if (eff->canBeSelected()) {
            myLamp.effects.loadeffname(effname, eff->eff_nb);
            interf->option(String(eff->eff_nb),
                EFF_NUMBER + 
                String(effname) + 
                MIC_SYMBOL
            );
            #ifdef ESP8266
            ESP.wdtFeed();
            #elif defined ESP32
            delay(1);
            #endif
        } else if(!eff->eff_nb){
            isEmptyHidden=true;
        }
    }
    if(isEmptyHidden)
        interf->option(String(0),"");
    interf->json_section_end();
    interf->json_section_end();
    interf->json_frame_flush();
    delete interf;
    if(optionsTicker.active())
        optionsTicker.detach();
}

// Страница "Управление эффектами"
void block_effects_main(Interface *interf, JsonObject *data, bool fast=true){
#ifndef DELAYED_EFFECTS
    fast=false;
#endif

    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_0000), FPSTR(TINTF_000));

    interf->json_section_line(FPSTR(TCONST_0019));
    interf->checkbox(FPSTR(TCONST_001A), myLamp.isLampOn()? "1" : "0", FPSTR(TINTF_00E), true);
    interf->button(FPSTR(TCONST_0020), FPSTR(TINTF_014));
    interf->json_section_end();

    interf->json_section_line(FPSTR(TCONST_0021));
    interf->button(FPSTR(TCONST_0022), FPSTR(TINTF_015), FPSTR(TCONST_0024));
    interf->button(FPSTR(TCONST_0023), FPSTR(TINTF_016), FPSTR(TCONST_0025));
    interf->json_section_end();

    interf->select(FPSTR(TCONST_0016), String(myLamp.effects.getSelected()), String(FPSTR(TINTF_00A)), true);
    LOG(printf_P,PSTR("Создаю список эффектов (%d):\n"),myLamp.effects.getModeAmount());
    EffectListElem *eff = nullptr;

    uint32_t timest = millis();

    if(fast){
        // Сначала подгрузим дефолтный список, а затем спустя время - подтянем имена из конфига

        //interf->option(String(myLamp.effects.getSelected()), myLamp.effects.getEffectName());
        String effname((char *)0);
        bool isEmptyHidden=false;
        MIC_SYMB;
        bool numList = myLamp.getLampSettings().numInList;
        while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
            if (eff->canBeSelected()) {
                effname = FPSTR(T_EFFNAMEID[(uint8_t)eff->eff_nb]);
                interf->option(String(eff->eff_nb), 
                    EFF_NUMBER + 
                    String(effname) + 
                    MIC_SYMBOL                    
                );
                #ifdef ESP8266
                ESP.wdtFeed();
                #elif defined ESP32
                delay(1);
                #endif
            } else if(!eff->eff_nb){
                isEmptyHidden=true;
            }
        }
        if(isEmptyHidden)
            interf->option(String(0),"");
    } else {
        LOG(println,F("DBG2: using slow Names generation"));
        bool isEmptyHidden=false;
        String effname((char *)0);
        MIC_SYMB;
        bool numList = myLamp.getLampSettings().numInList;
        while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
            if (eff->canBeSelected()) {
                myLamp.effects.loadeffname(effname, eff->eff_nb);
                interf->option(String(eff->eff_nb), 
                    EFF_NUMBER + 
                    String(effname) + 
                    MIC_SYMBOL                    
                );
                #ifdef ESP8266
                ESP.wdtFeed();
                #elif defined ESP32
                yield();
                #endif
            } else if(!eff->eff_nb){
                isEmptyHidden=true;
            }
        }
        if(isEmptyHidden)
            interf->option(String(0),"");
    }
    interf->json_section_end();
    LOG(printf_P,PSTR("DBG2: generating Names list took %ld ms\n"), millis() - timest);

    block_effects_param(interf, data);

    interf->button(FPSTR(TCONST_000F), FPSTR(TINTF_009));

    interf->json_section_end();
#ifdef DELAYED_EFFECTS
    if(!optionsTicker.active())
        optionsTicker.once(10,std::bind(delayedcall_effects_main));
#endif
}

void set_eff_prev(Interface *interf, JsonObject *data){
    remote_action(RA::RA_EFF_PREV, NULL);
}

void set_eff_next(Interface *interf, JsonObject *data){
    remote_action(RA::RA_EFF_NEXT, NULL);
}

/**
 * Обработка вкл/выкл лампы
 */
void set_onflag(Interface *interf, JsonObject *data){
    if (!data) return;
    //bool newpower = TOGLE_STATE((*data)[FPSTR(TCONST_001A)], myLamp.isLampOn());
    bool newpower = (*data)[FPSTR(TCONST_001A)].as<unsigned int>();
    if (newpower != myLamp.isLampOn()) {
        if (newpower) {
            // включаем через switcheffect, т.к. простого isOn недостаточно чтобы запустить фейдер и поменять яркость (при необходимости)
            myLamp.switcheffect(SW_SPECIFIC, myLamp.getFaderFlag(), myLamp.effects.getEn());
            myLamp.changePower(newpower);
#ifdef MP3PLAYER
            if(myLamp.getLampSettings().isOnMP3)
                mp3->setIsOn(true);
#endif
#ifndef ESP_USE_BUTTON
            if(millis()<10000)
                sysTicker.once(3,std::bind([]{
                    myLamp.sendString(WiFi.localIP().toString().c_str(), CRGB::White);
                }));
#endif
            embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0070), "1", true);
            embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0021), String(myLamp.getMode()), true);
            embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_00AA), String(myLamp.getMode()==LAMPMODE::MODE_DEMO?"1":"0"), true);
        } else {
            resetAutoTimers();; // автосохранение конфига будет отсчитываться от этого момента
            //myLamp.changePower(newpower);
            sysTicker.once(0.3,std::bind([]{ // при выключении бывает эксепшен, видимо это слишком длительная операция, разносим во времени и отдаем управление
                myLamp.changePower(false);
#ifdef MP3PLAYER
                mp3->setIsOn(false);
#endif
#ifdef RESTORE_STATE
                save_lamp_flags(); // злобный баг, забыть передернуть флаги здесь)))), не вздумать убрать!!! Отлавливал его кучу времени
#endif
                embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0070), "0", true);
                embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0021), String(myLamp.getMode()), true);
                embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_00AA), String(myLamp.getMode()==LAMPMODE::MODE_DEMO?"1":"0"), true);
            }));
        }
    }
#ifdef RESTORE_STATE
    save_lamp_flags();
#endif
}

void set_demoflag(Interface *interf, JsonObject *data){
    if (!data) return;
    resetAutoTimers();
    // Специально не сохраняем, считаю что демо при старте не должно запускаться
    //bool newdemo = TOGLE_STATE((*data)[FPSTR(TCONST_001B)], (myLamp.getMode() == MODE_DEMO));
    bool newdemo = (*data)[FPSTR(TCONST_001B)].as<unsigned int>();
    switch (myLamp.getMode()) {
        case MODE_OTA:
        case MODE_ALARMCLOCK:
        case MODE_NORMAL:
            if (newdemo) myLamp.startDemoMode(embui.param(FPSTR(TCONST_0026)).toInt()); break;
        case MODE_DEMO:
        case MODE_WHITELAMP:
            if (!newdemo) myLamp.startNormalMode(); break;
        default:;
    }
#ifdef RESTORE_STATE
    embui.var(FPSTR(TCONST_001B), (*data)[FPSTR(TCONST_001B)]);
#endif
    myLamp.setDRand(myLamp.getLampSettings().dRand);
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0021), String(myLamp.getMode()), true);
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_00AA), String(myLamp.getMode()==LAMPMODE::MODE_DEMO?"1":"0"), true);
}

#ifdef OTA
void set_otaflag(Interface *interf, JsonObject *data){
    //if (!data) return;
    //myLamp.startOTAUpdate();
    remote_action(RA_OTA, NULL, NULL);

    interf->json_frame_interface();
    interf->json_section_content();
    interf->button(FPSTR(TCONST_0027), FPSTR(TINTF_017), FPSTR(TCONST_0008));
    interf->json_section_end();
    interf->json_frame_flush();
    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_0021), String(myLamp.getMode()), true);
}
#endif

#ifdef AUX_PIN
void set_auxflag(Interface *interf, JsonObject *data){
    if (!data) return;
    if (((*data)[FPSTR(TCONST_000E)] == "1") != (digitalRead(AUX_PIN) == AUX_LEVEL ? true : false)) {
        AUX_toggle(!(digitalRead(AUX_PIN) == AUX_LEVEL ? true : false));
    }
}
#endif

void set_gbrflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setIsGlobalBrightness((*data)[FPSTR(TCONST_001C)] == "1");
    embui.publish(String(FPSTR(TCONST_008B)) + String(FPSTR(TCONST_00B4)), String(myLamp.IsGlobalBrightness() ? "1" : "0"), true);
    save_lamp_flags();
    if (myLamp.isLampOn()) {
        myLamp.setBrightness(myLamp.getNormalizedLampBrightness());
    }
    show_effects_param(interf, data);
}

void block_lamp_config(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_hidden(FPSTR(TCONST_0028), FPSTR(TINTF_018));

    interf->json_section_begin(FPSTR(TCONST_0029));
    String filename=embui.param(FPSTR(TCONST_002A));
    String cfg(FPSTR(TINTF_018)); cfg+=" ("; cfg+=filename; cfg+=")";

    // проверка на наличие конфигураций
    if(LittleFS.begin()){
#ifdef ESP32
        File tst = LittleFS.open(FPSTR(TCONST_002B));
        if(tst.openNextFile())
#else
        Dir tst = LittleFS.openDir(FPSTR(TCONST_002B));
        if(tst.next())
#endif    
        {
            interf->select(FPSTR(TCONST_002A), cfg);
#ifdef ESP32
            File root = LittleFS.open(FPSTR(TCONST_002B));
            File file = root.openNextFile();
#else
            Dir dir = LittleFS.openDir(FPSTR(TCONST_002B));
#endif
            String fn;
#ifdef ESP32
            while (file) {
                fn=file.name();
                if(!file.isDirectory()){
#else
            while (dir.next()) {
                fn=dir.fileName();
#endif

                fn.replace(FPSTR(TCONST_002C),F(""));
                //LOG(println, fn);
                interf->option(fn, fn);
#ifdef ESP32
                    file = root.openNextFile();
                }
            }
#else
            }
#endif
            interf->json_section_end(); // select

            interf->json_section_line();
                interf->button_submit_value(FPSTR(TCONST_0029), FPSTR(TCONST_002D), FPSTR(TINTF_019), FPSTR(TCONST_002F));
                interf->button_submit_value(FPSTR(TCONST_0029), FPSTR(TCONST_002E), FPSTR(TINTF_008));
                interf->button_submit_value(FPSTR(TCONST_0029), FPSTR(TCONST_00B2), FPSTR(TINTF_006), FPSTR(TCONST_000C));
            interf->json_section_end(); // json_section_line
            filename.clear();
            interf->spacer();
        }
    }
    interf->json_section_begin(FPSTR(TCONST_0030));
        interf->text(FPSTR(TCONST_00CF), filename, FPSTR(TINTF_01A), false);
        interf->button_submit(FPSTR(TCONST_0030), FPSTR(TINTF_01B));
    interf->json_section_end();

    interf->json_section_end(); // json_section_begin
    interf->json_section_end(); // json_section_hidden
}

void show_lamp_config(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_lamp_config(interf, data);
    interf->json_frame_flush();
}

void edit_lamp_config(Interface *interf, JsonObject *data){
    // Рбоата с конфигурациями в ФС
    if (!data) return;
    String name = (data->containsKey(FPSTR(TCONST_002A)) ? (*data)[FPSTR(TCONST_002A)] : (*data)[FPSTR(TCONST_00CF)]);
    String act = (*data)[FPSTR(TCONST_0029)];

    if(name.isEmpty() || act.isEmpty())
        name = (*data)[FPSTR(TCONST_00CF)].as<String>();
    LOG(printf_P, PSTR("name=%s, act=%s\n"), name.c_str(), act.c_str());

    if(name.isEmpty()) return;

    if (act == FPSTR(TCONST_00B2)) { // удаление
        String filename = String(FPSTR(TCONST_0031)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);

        filename = String(FPSTR(TCONST_002C)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);

        filename = String(FPSTR(TCONST_0032)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);
#ifdef ESP_USE_BUTTON
        filename = String(FPSTR(TCONST_0033)) + name;
        if (LittleFS.begin()) LittleFS.remove(filename);
#endif
    } else if (act == FPSTR(TCONST_002D)) { // загрузка
        //myLamp.changePower(false);
        resetAutoTimers();

        String filename = String(FPSTR(TCONST_0031)) + name;
        embui.load(filename.c_str());

        filename = String(FPSTR(TCONST_002C)) + name;
        myLamp.effects.initDefault(filename.c_str());

        filename = String(FPSTR(TCONST_0032)) + name;
        myLamp.events.loadConfig(filename.c_str());
#ifdef ESP_USE_BUTTON
        filename = String(FPSTR(TCONST_0033)) + name;
        myButtons->clear();
        if (!myButtons->loadConfig()) {
            default_buttons();
        }
#endif
        //embui.var(FPSTR(TCONST_002A), name);

        String str = String(F("CFG:")) + name;
        myLamp.sendString(str.c_str(), CRGB::Red);

        sysTicker.once(3,std::bind([](){
            sync_parameters();
        }));
        //myLamp.changePower(true);
    } else { // создание
        if(!name.endsWith(F(".json"))){
            name.concat(F(".json"));
        }

        String filename = String(FPSTR(TCONST_0031)) + name;
        embui.save(filename.c_str(), true);

        filename = String(FPSTR(TCONST_002C)) + name;
        myLamp.effects.makeIndexFileFromList(filename.c_str());

        filename = String(FPSTR(TCONST_0032)) + name;
        myLamp.events.saveConfig(filename.c_str());
#ifdef ESP_USE_BUTTON
        filename = String(FPSTR(TCONST_0033)) + name;
        myButtons->saveConfig(filename.c_str());
#endif
    }

    show_lamp_config(interf, data);
}

void block_lamp_textsend(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_begin(FPSTR(TCONST_0034));

    interf->spacer(FPSTR(TINTF_01C));
    interf->text(FPSTR(TCONST_0035), FPSTR(TINTF_01D));
    interf->color(FPSTR(TCONST_0036), FPSTR(TINTF_01E));
    interf->button_submit(FPSTR(TCONST_0034), FPSTR(TINTF_01F), FPSTR(TCONST_0008));

    interf->json_section_hidden(FPSTR(TCONST_00B9), FPSTR(TINTF_002));
        interf->json_section_begin(FPSTR(TCONST_00BA));
            interf->spacer(FPSTR(TINTF_001));
                interf->range(FPSTR(TCONST_0051), 10, 100, 5, FPSTR(TINTF_044));
                interf->range(FPSTR(TCONST_0052), -1, (HEIGHT>6?HEIGHT:6)-6, 1, FPSTR(TINTF_045));
                interf->range(FPSTR(TCONST_00C3), 0, 255, 1, FPSTR(TINTF_0CA));
                
                interf->select(FPSTR(TCONST_0053), FPSTR(TINTF_046));
                    interf->option(String(PERIODICTIME::PT_NOT_SHOW), FPSTR(TINTF_047));
                    interf->option(String(PERIODICTIME::PT_EVERY_60), FPSTR(TINTF_048));
                    interf->option(String(PERIODICTIME::PT_EVERY_30), FPSTR(TINTF_049));
                    interf->option(String(PERIODICTIME::PT_EVERY_15), FPSTR(TINTF_04A));
                    interf->option(String(PERIODICTIME::PT_EVERY_10), FPSTR(TINTF_04B));
                    interf->option(String(PERIODICTIME::PT_EVERY_5), FPSTR(TINTF_04C));
                    interf->option(String(PERIODICTIME::PT_EVERY_1), FPSTR(TINTF_04D));
                interf->json_section_end();

            interf->spacer(FPSTR(TINTF_04E));
                interf->number(FPSTR(TCONST_0054), FPSTR(TINTF_04F));
                //interf->number(FPSTR(TCONST_0055), FPSTR(TINTF_050));
                String datetime;
                TimeProcessor::getDateTimeString(datetime, embui.param(FPSTR(TCONST_0055)).toInt());
                interf->text(FPSTR(TCONST_0055), datetime, FPSTR(TINTF_050), false);
                interf->button_submit(FPSTR(TCONST_00BA), FPSTR(TINTF_008), FPSTR(TCONST_0008));
            interf->spacer();
                //interf->button(FPSTR(TCONST_0000), FPSTR(TINTF_00B));
                interf->button(FPSTR(TCONST_0003), FPSTR(TINTF_00B));
        interf->json_section_end();
    interf->json_section_end();

    interf->json_section_end();
}

void set_lamp_textsend(Interface *interf, JsonObject *data){
    if (!data) return;
    resetAutoTimers(); // откладываем автосохранения
    String tmpStr = (*data)[FPSTR(TCONST_0036)];
    embui.var(FPSTR(TCONST_0036), tmpStr);
    embui.var(FPSTR(TCONST_0035), (*data)[FPSTR(TCONST_0035)]);

    tmpStr.replace(F("#"), F("0x"));
    myLamp.sendString((*data)[FPSTR(TCONST_0035)], (CRGB::HTMLColorCode)strtol(tmpStr.c_str(), NULL, 0));
}

void block_drawing(Interface *interf, JsonObject *data){
    //Страница "Рисование"
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_00C8), FPSTR(TINTF_0CE));

    DynamicJsonDocument doc(512);
    JsonObject param = doc.to<JsonObject>();

    param[FPSTR(TCONST_00CD)] = WIDTH;
    param[FPSTR(TCONST_00CC)] = HEIGHT;
    param[FPSTR(TCONST_00CB)] = FPSTR(TINTF_0CF);

    interf->checkbox(FPSTR(TCONST_00C4), myLamp.isDrawOn()? "1" : "0", FPSTR(TINTF_0CE), true);
    interf->custom(String(FPSTR(TCONST_00C9)),String(FPSTR(TCONST_00C8)),embui.param(FPSTR(TCONST_0036)),String(FPSTR(TINTF_0D0)), param);
    param.clear();

    interf->json_section_end();
}

void set_drawing(Interface *interf, JsonObject *data){
    if (!data) return;

    String value = (*data)[FPSTR(TCONST_00C9)];
    if((*data).containsKey(FPSTR(TCONST_00C9)) && value!=F("null"))
        remote_action(RA_DRAW, value.c_str(), NULL);
    else {
        String key = String(FPSTR(TCONST_00C9))+String(F("_fill"));
        if((*data).containsKey(key)){
            value = (*data)[key].as<String>();
            remote_action(RA_FILLMATRIX, value.c_str(), NULL);
        }
    }
}

void block_lamptext(Interface *interf, JsonObject *data){
    //Страница "Вывод текста"
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_0003), FPSTR(TINTF_001));

    block_lamp_textsend(interf, data);

    interf->json_section_end();
}

void set_text_config(Interface *interf, JsonObject *data){
    if (!data) return;

    SETPARAM(FPSTR(TCONST_0051), myLamp.setTextMovingSpeed((*data)[FPSTR(TCONST_0051)]));
    SETPARAM(FPSTR(TCONST_0052), myLamp.setTextOffset((*data)[FPSTR(TCONST_0052)]));
    SETPARAM(FPSTR(TCONST_0053), myLamp.setPeriodicTimePrint((PERIODICTIME)(*data)[FPSTR(TCONST_0053)].as<long>()));
    SETPARAM(FPSTR(TCONST_0054), myLamp.setNYMessageTimer((*data)[FPSTR(TCONST_0054)]));
    SETPARAM(FPSTR(TCONST_00C3), myLamp.setBFade((*data)[FPSTR(TCONST_00C3)]));

    String newYearTime = (*data)[FPSTR(TCONST_0055)]; // Дата/время наструпления нового года с интерфейса
    struct tm t;
    tm *tm=&t;
    localtime_r(TimeProcessor::now(), tm);  // reset struct to local now()

    // set desired date
    tm->tm_year = newYearTime.substring(0,4).toInt()-TM_BASE_YEAR;
    tm->tm_mon  = newYearTime.substring(5,7).toInt()-1;
    tm->tm_mday = newYearTime.substring(8,10).toInt();
    tm->tm_hour = newYearTime.substring(11,13).toInt();
    tm->tm_min  = newYearTime.substring(14,16).toInt();
    tm->tm_sec  = 0;

    time_t ny_unixtime = mktime(tm);
    LOG(printf_P, PSTR("Set New Year at %d %d %d %d %d (%ld)\n"), tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, ny_unixtime);

    //SETPARAM(FPSTR(TCONST_0055), myLamp.setNYUnixTime(ny_unixtime));
    embui.var(FPSTR(TCONST_0055),String(ny_unixtime)); myLamp.setNYUnixTime(ny_unixtime);

    if(!interf){
        interf = embui.ws.count()? new Interface(&embui, &embui.ws, 3000) : nullptr;
        section_text_frame(interf, data);
        delete interf;
    } else
        section_text_frame(interf, data);
}

#ifdef MP3PLAYER
void block_settings_mp3(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_00A1), FPSTR(TINTF_099));

    interf->checkbox(FPSTR(TCONST_009D), myLamp.isONMP3()? "1" : "0", FPSTR(TINTF_099), true);
    interf->range(FPSTR(TCONST_00A2), 1, 30, 1, FPSTR(TINTF_09B), true);
    
    interf->json_section_begin(FPSTR(TCONST_00A0));
    interf->spacer(FPSTR(TINTF_0B1));
    interf->json_section_line(); // расположить в одной линии
        interf->checkbox(FPSTR(TCONST_00A4), myLamp.getLampSettings().playName ? "1" : "0", FPSTR(TINTF_09D), false);
    interf->json_section_end();
    interf->json_section_line(); // расположить в одной линии
        interf->checkbox(FPSTR(TCONST_00A5), myLamp.getLampSettings().playEffect ? "1" : "0", FPSTR(TINTF_09E), false);
        interf->checkbox(FPSTR(TCONST_00A8), myLamp.getLampSettings().playMP3 ? "1" : "0", FPSTR(TINTF_0AF), false);
    interf->json_section_end();

    //interf->checkbox(FPSTR(TCONST_00A3), myLamp.getLampSettings().playTime ? "1" : "0", FPSTR(TINTF_09C), false);
    interf->select(FPSTR(TCONST_00A3), String(myLamp.getLampSettings().playTime), String(FPSTR(TINTF_09C)), false);
    interf->option(String(TIME_SOUND_TYPE::TS_NONE), FPSTR(TINTF_0B6));
    interf->option(String(TIME_SOUND_TYPE::TS_VER1), FPSTR(TINTF_0B7));
    interf->option(String(TIME_SOUND_TYPE::TS_VER2), FPSTR(TINTF_0B8));
    interf->json_section_end();

    interf->select(FPSTR(TCONST_00A6), String(myLamp.getLampSettings().alarmSound), String(FPSTR(TINTF_0A3)), false);
    interf->option(String(ALARM_SOUND_TYPE::AT_NONE), FPSTR(TINTF_09F));
    interf->option(String(ALARM_SOUND_TYPE::AT_FIRST), FPSTR(TINTF_0A0));
    interf->option(String(ALARM_SOUND_TYPE::AT_SECOND), FPSTR(TINTF_0A4));
    interf->option(String(ALARM_SOUND_TYPE::AT_THIRD), FPSTR(TINTF_0A5));
    interf->option(String(ALARM_SOUND_TYPE::AT_FOURTH), FPSTR(TINTF_0A6));
    interf->option(String(ALARM_SOUND_TYPE::AT_FIFTH), FPSTR(TINTF_0A7));
    interf->option(String(ALARM_SOUND_TYPE::AT_RANDOM), FPSTR(TINTF_0A1));
    interf->option(String(ALARM_SOUND_TYPE::AT_RANDOMMP3), FPSTR(TINTF_0A2));
    interf->json_section_end();
    interf->checkbox(FPSTR(TCONST_00AF), myLamp.getLampSettings().limitAlarmVolume ? "1" : "0", FPSTR(TINTF_0B3), false);

    interf->select(FPSTR(TCONST_00A7), String(myLamp.getLampSettings().MP3eq), String(FPSTR(TINTF_0A8)), false);
    interf->option(String(DFPLAYER_EQ_NORMAL), FPSTR(TINTF_0A9));
    interf->option(String(DFPLAYER_EQ_POP), FPSTR(TINTF_0AA));
    interf->option(String(DFPLAYER_EQ_ROCK), FPSTR(TINTF_0AB));
    interf->option(String(DFPLAYER_EQ_JAZZ), FPSTR(TINTF_0AC));
    interf->option(String(DFPLAYER_EQ_CLASSIC), FPSTR(TINTF_0AD));
    interf->option(String(DFPLAYER_EQ_BASS), FPSTR(TINTF_0AE));
    interf->json_section_end();
    
    interf->number(FPSTR(TCONST_00A9), mp3->getMP3count(), FPSTR(TINTF_0B0), false);
    //SETPARAM(FPSTR(TCONST_00A9), mp3->setMP3count((*data)[FPSTR(TCONST_00A9)].as<int>())); // кол-во файлов в папке мп3

    interf->button_submit(FPSTR(TCONST_00A0), FPSTR(TINTF_008), FPSTR(TCONST_0008));
    interf->json_section_end();

    interf->spacer();
    interf->button(FPSTR(TCONST_0004), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_mp3(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_mp3(interf, data);
    interf->json_frame_flush();
}

void set_settings_mp3(Interface *interf, JsonObject *data){
    if (!data) return;

    resetAutoTimers(); // сдвинем таймеры автосейва, т.к. длительная операция
    uint8_t val = (*data)[FPSTR(TCONST_00A7)].as<uint8_t>(); myLamp.setEqType(val); mp3->setEqType(val); // пишет в плеер!

    myLamp.setPlayTime((*data)[FPSTR(TCONST_00A3)].as<int>());
    myLamp.setPlayName((*data)[FPSTR(TCONST_00A4)]=="1");
    myLamp.setPlayEffect((*data)[FPSTR(TCONST_00A5)]=="1"); mp3->setPlayEffect(myLamp.getLampSettings().playEffect);
    myLamp.setAlatmSound((ALARM_SOUND_TYPE)(*data)[FPSTR(TCONST_00A6)].as<int>());
    myLamp.setPlayMP3((*data)[FPSTR(TCONST_00A8)]=="1"); mp3->setPlayMP3(myLamp.getLampSettings().playMP3);
    myLamp.setLimitAlarmVolume((*data)[FPSTR(TCONST_00AF)]=="1");

    SETPARAM(FPSTR(TCONST_00A9), mp3->setMP3count((*data)[FPSTR(TCONST_00A9)].as<int>())); // кол-во файлов в папке мп3
    //SETPARAM(FPSTR(TCONST_00A2), mp3->setVolume((*data)[FPSTR(TCONST_00A2)].as<int>()));
    SETPARAM(FPSTR(TCONST_00A2)); // тоже пишет в плеер, разносим во времени
    // sysTicker.once(0.3,std::bind([](){
    //     mp3->setVolume(embui.param(FPSTR(TCONST_00A2)).toInt());
    // }));

    save_lamp_flags();
    section_settings_frame(interf, data);
}
#endif

#ifdef MIC_EFFECTS
void block_settings_mic(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_0037), FPSTR(TINTF_020));

    interf->checkbox(FPSTR(TCONST_001E), myLamp.isMicOnOff()? "1" : "0", FPSTR(TINTF_012), true);

    interf->json_section_begin(FPSTR(TCONST_0038));
    if (!myLamp.isMicCalibration()) {
        interf->number(FPSTR(TCONST_0039), (float)(round(myLamp.getMicScale() * 100) / 100), FPSTR(TINTF_022), 0.01);
        interf->number(FPSTR(TCONST_003A), (float)(round(myLamp.getMicNoise() * 100) / 100), FPSTR(TINTF_023), 0.01);
        interf->range(FPSTR(TCONST_003B), (int)myLamp.getMicNoiseRdcLevel(), 0, 4, (float)1.0, FPSTR(TINTF_024), false);

        interf->button_submit(FPSTR(TCONST_0038), FPSTR(TINTF_008), FPSTR(TCONST_0008));
        interf->json_section_end();

        interf->spacer();
        interf->button(FPSTR(TCONST_003C), FPSTR(TINTF_025), FPSTR(TCONST_000C));
    } else {
        interf->button(FPSTR(TCONST_003C), FPSTR(TINTF_027), FPSTR(TCONST_000C) );
    }

    interf->spacer();
    interf->button(FPSTR(TCONST_0004), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_mic(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_mic(interf, data);
    interf->json_frame_flush();
}

void set_settings_mic(Interface *interf, JsonObject *data){
    if (!data) return;
    float scale = (*data)[FPSTR(TCONST_0039)]; //atof((*data)[FPSTR(TCONST_0039)].as<String>().c_str());
    float noise = (*data)[FPSTR(TCONST_003A)]; //atof((*data)[FPSTR(TCONST_003A)].as<String>().c_str());
    MIC_NOISE_REDUCE_LEVEL rdl = (*data)[FPSTR(TCONST_003B)];

    // LOG(printf_P, PSTR("scale=%2.3f noise=%2.3f rdl=%d\n"),scale,noise,rdl);
    // String tmpStr;
    // serializeJson(*data, tmpStr);
    // LOG(printf_P, PSTR("*data=%s\n"),tmpStr.c_str());

    SETPARAM(FPSTR(TCONST_0039), myLamp.setMicScale(scale));
    SETPARAM(FPSTR(TCONST_003A), myLamp.setMicNoise(noise));
    SETPARAM(FPSTR(TCONST_003B), myLamp.setMicNoiseRdcLevel(rdl));

    section_settings_frame(interf, data);
}

void set_micflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setMicOnOff((*data)[FPSTR(TCONST_001E)] == "1");
    save_lamp_flags();
    show_effects_param(interf,data);
}

void set_settings_mic_calib(Interface *interf, JsonObject *data){
    //if (!data) return;
    if (!myLamp.isMicOnOff()) {
        myLamp.sendStringToLamp(String(FPSTR(TINTF_026)).c_str(), CRGB::Red);
    } else if(!myLamp.isMicCalibration()) {
        myLamp.sendStringToLamp(String(FPSTR(TINTF_025)).c_str(), CRGB::Red);
        myLamp.setMicCalibration();
    } else {
        myLamp.sendStringToLamp(String(FPSTR(TINTF_027)).c_str(), CRGB::Red);
    }

    show_settings_mic(interf, data);
}
#endif

// формирование интерфейса настроек WiFi/MQTT
void block_settings_wifi(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_003D), FPSTR(TINTF_028));
    // форма настроек Wi-Fi

    interf->json_section_hidden(FPSTR(TCONST_003E), FPSTR(TINTF_029));
    interf->spacer(FPSTR(TINTF_02A));
    interf->text(FPSTR(TCONST_003F), FPSTR(TINTF_02B));
    interf->text(FPSTR(TCONST_0040), WiFi.SSID(), FPSTR(TINTF_02C), false);
    interf->password(FPSTR(TCONST_0041), FPSTR(TINTF_02D));
    interf->button_submit(FPSTR(TCONST_003E), FPSTR(TINTF_02E), FPSTR(TCONST_0008));
    interf->json_section_end();

    interf->json_section_hidden(FPSTR(TCONST_0042), FPSTR(TINTF_02F));
    interf->text(FPSTR(TCONST_003F), FPSTR(TINTF_02B));
    interf->spacer(FPSTR(TINTF_031));
    interf->comment(FPSTR(TINTF_032));
    interf->checkbox(FPSTR(TCONST_0043), FPSTR(TINTF_033));
    interf->password(FPSTR(TCONST_0044), FPSTR(TINTF_034));
    interf->button_submit(FPSTR(TCONST_0042), FPSTR(TINTF_008), FPSTR(TCONST_0008));
    interf->json_section_end();

    // форма настроек MQTT
    interf->json_section_hidden(FPSTR(TCONST_0045), FPSTR(TINTF_035));
    interf->text(FPSTR(TCONST_0046), FPSTR(TINTF_036));
    interf->number(FPSTR(TCONST_0047), FPSTR(TINTF_037));
    interf->text(FPSTR(TCONST_0048), FPSTR(TINTF_038));
    interf->text(FPSTR(TCONST_0049), FPSTR(TINTF_02D));
    interf->text(FPSTR(TCONST_007B), FPSTR(TINTF_08C));
    interf->number(FPSTR(TCONST_004A), FPSTR(TINTF_039));
    interf->button_submit(FPSTR(TCONST_0045), FPSTR(TINTF_03A), FPSTR(TCONST_0008));
    interf->json_section_end();

    interf->spacer();
    interf->button(FPSTR(TCONST_0004), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_wifi(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_wifi(interf, data);
    interf->json_frame_flush();
}

/**
 * настройка подключения WiFi в режиме AP
 */
void set_settings_wifiAP(Interface *interf, JsonObject *data){
    if (!data) return;

    SETPARAM(FPSTR(TCONST_003F));
    SETPARAM(FPSTR(TCONST_0043));
    SETPARAM(FPSTR(TCONST_0044));

    embui.save();
    embui.wifi_connect();

    section_settings_frame(interf, data);
}

/**
 * настройка подключения WiFi в режиме клиента
 */
void set_settings_wifi(Interface *interf, JsonObject *data){
    if (!data) return;

    SETPARAM(FPSTR(TCONST_003F));

    const char *ssid = (*data)[FPSTR(TCONST_0040)];
    const char *pwd = (*data)[FPSTR(TCONST_0041)];

    if(ssid){
        embui.wifi_connect(ssid, pwd);
    } else {
        LOG(println, F("WiFi: No SSID defined!"));
    }

    section_settings_frame(interf, data);
}

void set_settings_mqtt(Interface *interf, JsonObject *data){
    if (!data) return;
    SETPARAM(FPSTR(TCONST_0046));
    SETPARAM(FPSTR(TCONST_0048));
    SETPARAM(FPSTR(TCONST_0049));
    SETPARAM(FPSTR(TCONST_007B));
    SETPARAM(FPSTR(TCONST_0047));
    SETPARAM(FPSTR(TCONST_004A), myLamp.semqtt_int((*data)[FPSTR(TCONST_004A)]));

    embui.save();
    embui.mqttReconnect();

    section_settings_frame(interf, data);
}

void block_settings_other(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_004B), FPSTR(TINTF_002));

    interf->spacer(FPSTR(TINTF_030));
    interf->checkbox(FPSTR(TCONST_004C), myLamp.getLampSettings().MIRR_H ? "1" : "0", FPSTR(TINTF_03B), false);
    interf->checkbox(FPSTR(TCONST_004D), myLamp.getLampSettings().MIRR_V ? "1" : "0", FPSTR(TINTF_03C), false);
    interf->checkbox(FPSTR(TCONST_004E), myLamp.getLampSettings().isFaderON ? "1" : "0", FPSTR(TINTF_03D), false);
    interf->checkbox(FPSTR(TCONST_008E), myLamp.getLampSettings().isEffClearing ? "1" : "0", FPSTR(TINTF_083), false);
    interf->checkbox(FPSTR(TCONST_004F), myLamp.getLampSettings().dRand ? "1" : "0", FPSTR(TINTF_03E), false);
    interf->checkbox(FPSTR(TCONST_009E), myLamp.getLampSettings().showName ? "1" : "0", FPSTR(TINTF_09A), false);
    interf->range(FPSTR(TCONST_0026), 30, 250, 5, FPSTR(TINTF_03F));
    interf->checkbox(FPSTR(TCONST_0090), myLamp.getLampSettings().numInList ? "1" : "0", FPSTR(TINTF_090), false); // нумерация в списке эффектов
#ifdef MIC_EFFECTS
    interf->checkbox(FPSTR(TCONST_0091), myLamp.getLampSettings().effHasMic ? "1" : "0", FPSTR(TINTF_091), false); // значек микрофона в списке эффектов
#endif
    interf->select(FPSTR(TCONST_0050), FPSTR(TINTF_040));
    interf->option(String(SORT_TYPE::ST_BASE), FPSTR(TINTF_041));
    interf->option(String(SORT_TYPE::ST_END), FPSTR(TINTF_042));
    interf->option(String(SORT_TYPE::ST_IDX), FPSTR(TINTF_043));
    interf->option(String(SORT_TYPE::ST_AB), FPSTR(TINTF_085));
    interf->option(String(SORT_TYPE::ST_AB2), FPSTR(TINTF_08A));
#ifdef MIC_EFFECTS
    interf->option(String(SORT_TYPE::ST_MIC), FPSTR(TINTF_08D));  // эффекты с микрофоном
#endif
    interf->json_section_end();
#ifdef SHOWSYSCONFIG
    interf->checkbox(FPSTR(TCONST_0096), myLamp.getLampSettings().isShowSysMenu ? "1" : "0", FPSTR(TINTF_093), false); // отображение системного меню
#endif
    interf->spacer(FPSTR(TINTF_0BA));
    interf->range(FPSTR(TCONST_00BB), myLamp.getAlarmP(), 1, 15, 1, FPSTR(TINTF_0BB), false);
    interf->range(FPSTR(TCONST_00BC), myLamp.getAlarmT(), 1, 15, 1, FPSTR(TINTF_0BC), false);

    interf->button_submit(FPSTR(TCONST_004B), FPSTR(TINTF_008), FPSTR(TCONST_0008));

    interf->spacer();
    interf->button(FPSTR(TCONST_0004), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_other(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_other(interf, data);
    interf->json_frame_flush();
}

void set_settings_other(Interface *interf, JsonObject *data){
    if (!data) return;

    serializeJson(*data,tmpData);
    resetAutoTimers();
    sysTicker.once(0.3,std::bind([]{
        DynamicJsonDocument docum(1024);
        deserializeJson(docum, tmpData);
        JsonObject dataStore = docum.as<JsonObject>();
        JsonObject *data = &dataStore;

        myLamp.setMIRR_H((*data)[FPSTR(TCONST_004C)] == "1");
        myLamp.setMIRR_V((*data)[FPSTR(TCONST_004D)] == "1");
        myLamp.setFaderFlag((*data)[FPSTR(TCONST_004E)] == "1");
        myLamp.setClearingFlag((*data)[FPSTR(TCONST_008E)] == "1");
        myLamp.setDRand((*data)[FPSTR(TCONST_004F)] == "1");
        myLamp.setShowName((*data)[FPSTR(TCONST_009E)] == "1");
        myLamp.setNumInList((*data)[FPSTR(TCONST_0090)] == "1");

        SETPARAM(FPSTR(TCONST_0026), ({if (myLamp.getMode() == MODE_DEMO){ myLamp.demoTimer(T_DISABLE); myLamp.demoTimer(T_ENABLE, embui.param(FPSTR(TCONST_0026)).toInt()); }}));
        SETPARAM(FPSTR(TCONST_0050), myLamp.effects.setEffSortType((*data)[FPSTR(TCONST_0050)].as<SORT_TYPE>()));
    #ifdef MIC_EFFECTS
        myLamp.setEffHasMic((*data)[FPSTR(TCONST_0091)] == "1");
    #endif
        myLamp.setIsShowSysMenu((*data)[FPSTR(TCONST_0096)] == "1");

        uint8_t alatmPT = ((*data)[FPSTR(TCONST_00BB)]).as<uint8_t>()<<4; // старшие 4 бита
        alatmPT = alatmPT | ((*data)[FPSTR(TCONST_00BC)]).as<uint8_t>(); // младшие 4 бита
        embui.var(FPSTR(TCONST_00BD), String(alatmPT)); myLamp.setAlarmPT(alatmPT);
        //SETPARAM(FPSTR(TCONST_00BD), myLamp.setAlarmPT(alatmPT));
        //LOG(printf_P, PSTR("alatmPT=%d, alatmP=%d, alatmT=%d\n"), alatmPT, myLamp.getAlarmP(), myLamp.getAlarmT());

        save_lamp_flags();
        tmpData.clear();
    }));

    section_settings_frame(interf, data);
}

void block_settings_time(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_0056), FPSTR(TINTF_051));

    interf->comment(FPSTR(TINTF_052));
    interf->text(FPSTR(TCONST_0057), FPSTR(TINTF_053));
    interf->text(FPSTR(TCONST_0058), FPSTR(TINTF_054));
    interf->comment(FPSTR(TINTF_055));
    interf->text(FPSTR(TCONST_0059), "", "", false);
    interf->hidden(FPSTR(TCONST_00B8),""); // скрытое поле для получения времени с устройства
    interf->button_submit(FPSTR(TCONST_0056), FPSTR(TINTF_008), FPSTR(TCONST_0008));

    interf->spacer();
    interf->button(FPSTR(TCONST_0004), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_time(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_time(interf, data);
    interf->json_frame_flush();
}

void set_settings_time(Interface *interf, JsonObject *data){
    if (!data) return;

    LOG(printf_P,PSTR("devicedatetime=%s\n"),(*data)[FPSTR(TCONST_00B8)].as<String>().c_str());
    
    String datetime=(*data)[FPSTR(TCONST_0059)];
    if (datetime.length())
        embui.timeProcessor.setTime(datetime);
    else if(!embui.sysData.wifi_sta) {
        datetime=(*data)[FPSTR(TCONST_00B8)].as<String>();
        if (datetime.length())
            embui.timeProcessor.setTime(datetime);
    }

    SETPARAM(FPSTR(TCONST_0057), embui.timeProcessor.tzsetup((*data)[FPSTR(TCONST_0057)]));
    SETPARAM(FPSTR(TCONST_0058), embui.timeProcessor.setcustomntp((*data)[FPSTR(TCONST_0058)]));

    myLamp.sendStringToLamp(String(F("%TM")).c_str(), CRGB::Green);

    section_settings_frame(interf, data);
}

void block_settings_update(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_hidden(FPSTR(TCONST_005A), FPSTR(TINTF_056));
#ifdef OTA
    interf->spacer(FPSTR(TINTF_057));
    if (myLamp.getMode() != MODE_OTA) {
        interf->button(FPSTR(TCONST_0027), FPSTR(TINTF_058), FPSTR(TCONST_005B));
    } else {
        interf->button(FPSTR(TCONST_0027), FPSTR(TINTF_017), FPSTR(TCONST_0008));
    }
#endif
    interf->spacer(FPSTR(TINTF_059));
    interf->file(FPSTR(TCONST_005A), FPSTR(TCONST_005A), FPSTR(TINTF_05A));
    interf->json_section_end();
}

void block_settings_event(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_005C), FPSTR(TINTF_011));

    interf->checkbox(FPSTR(TCONST_001D), myLamp.IsEventsHandled()? "1" : "0", FPSTR(TINTF_086), true);

    int num = 1;
    EVENT *next = nullptr;
    interf->json_section_begin(FPSTR(TCONST_005D));
    interf->select(FPSTR(TCONST_005E), String(0), String(FPSTR(TINTF_05B)), false);
    while ((next = myLamp.events.getNextEvent(next)) != nullptr) {
        interf->option(String(num), next->getName());
        ++num;
    }
    interf->json_section_end();

    interf->json_section_line();
    interf->button_submit_value(FPSTR(TCONST_005D), FPSTR(TCONST_005F), FPSTR(TINTF_05C), FPSTR(TCONST_002F));
    interf->button_submit_value(FPSTR(TCONST_005D), FPSTR(TCONST_00B6), FPSTR(TINTF_006), FPSTR(TCONST_000C));
    interf->json_section_end();

    interf->json_section_end();

    interf->button(FPSTR(TCONST_005D), FPSTR(TINTF_05D));

    interf->spacer();
    interf->button(FPSTR(TCONST_0004), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_event(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_event(interf, data);
    interf->json_frame_flush();
}

void set_eventflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setIsEventsHandled((*data)[FPSTR(TCONST_001D)] == "1");
    save_lamp_flags();
}

void set_event_conf(Interface *interf, JsonObject *data){
    EVENT event;
    String act;
    if (!data) return;

    //String output;
    //serializeJson((*data), output);
    //LOG(println, output.c_str());

    if (data->containsKey(FPSTR(TCONST_005E))) {
        EVENT *curr = nullptr;
        int i = 1, num = (*data)[FPSTR(TCONST_005E)];
        while ((curr = myLamp.events.getNextEvent(curr)) && i != num) ++i;
        if (!curr) return;
        myLamp.events.delEvent(*curr);
    }

    if (data->containsKey(FPSTR(TCONST_0060))) {
        event.isEnabled = ((*data)[FPSTR(TCONST_0060)] == "1");
    } else {
        event.isEnabled = true;
    }

    event.d1 = ((*data)[FPSTR(TCONST_0061)] == "1");
    event.d2 = ((*data)[FPSTR(TCONST_0062)] == "1");
    event.d3 = ((*data)[FPSTR(TCONST_0063)] == "1");
    event.d4 = ((*data)[FPSTR(TCONST_0064)] == "1");
    event.d5 = ((*data)[FPSTR(TCONST_0065)] == "1");
    event.d6 = ((*data)[FPSTR(TCONST_0066)] == "1");
    event.d7 = ((*data)[FPSTR(TCONST_0067)] == "1");
    event.event = (EVENT_TYPE)(*data)[FPSTR(TCONST_0068)].as<long>();
    event.repeat = (*data)[FPSTR(TCONST_0069)];
    event.stopat = (*data)[FPSTR(TCONST_006A)];
    String tmEvent = (*data)[FPSTR(TCONST_006B)];

    struct tm t;
    tm *tm=&t;
    localtime_r(TimeProcessor::now(), tm);  // reset struct to local now()

    // set desired date
    tm->tm_year=tmEvent.substring(0,4).toInt()-TM_BASE_YEAR;
    tm->tm_mon = tmEvent.substring(5,7).toInt()-1;
    tm->tm_mday=tmEvent.substring(8,10).toInt();
    tm->tm_hour=tmEvent.substring(11,13).toInt();
    tm->tm_min=tmEvent.substring(14,16).toInt();
    tm->tm_sec=0;

    LOG(printf_P, PSTR("Set Event at %d %d %d %d %d\n"), tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);

    event.unixtime = mktime(tm);
    event.message = (char*)((*data)[FPSTR(TCONST_0035)].as<char*>());

    myLamp.events.addEvent(event);
    myLamp.events.saveConfig();
    show_settings_event(interf, data);
}

void show_event_conf(Interface *interf, JsonObject *data){
    EVENT event;
    String act;
    bool edit = false;
    int i = 1, num = 0;
    if (!interf || !data) return;

    if (data->containsKey(FPSTR(TCONST_005E))) {
        EVENT *curr = nullptr;
        num = (*data)[FPSTR(TCONST_005E)];
        while ((curr = myLamp.events.getNextEvent(curr)) && i != num) ++i;
        if (!curr) return;
        act = (*data)[FPSTR(TCONST_005D)].as<String>();
        event = *curr;
        edit = true;
    }

    if (act == FPSTR(TCONST_00B6)) {
        myLamp.events.delEvent(event);
        myLamp.events.saveConfig();
        show_settings_event(interf, data);
        return;
    } else
    if (data->containsKey(FPSTR(TCONST_002E))) {
        set_event_conf(interf, data);
        return;
    }


    interf->json_frame_interface();

    if (edit) {
        interf->json_section_main(FPSTR(TCONST_006C), FPSTR(TINTF_05C));
        interf->constant(FPSTR(TCONST_005E), String(num), event.getName());
        interf->checkbox(FPSTR(TCONST_0060), (event.isEnabled? "1" : "0"), FPSTR(TINTF_05E), false);
    } else {
        interf->json_section_main(FPSTR(TCONST_006C), FPSTR(TINTF_05D));
    }

    interf->select(FPSTR(TCONST_0068), String(event.event), String(FPSTR(TINTF_05F)), false);
    interf->option(String(EVENT_TYPE::ON), FPSTR(TINTF_060));
    interf->option(String(EVENT_TYPE::OFF), FPSTR(TINTF_061));
    interf->option(String(EVENT_TYPE::DEMO_ON), FPSTR(TINTF_062));
    interf->option(String(EVENT_TYPE::ALARM), FPSTR(TINTF_063));
    interf->option(String(EVENT_TYPE::LAMP_CONFIG_LOAD), FPSTR(TINTF_064));
    interf->option(String(EVENT_TYPE::EFF_CONFIG_LOAD), FPSTR(TINTF_065));
    interf->option(String(EVENT_TYPE::EVENTS_CONFIG_LOAD), FPSTR(TINTF_066));
    interf->option(String(EVENT_TYPE::SEND_TEXT), FPSTR(TINTF_067));
    interf->option(String(EVENT_TYPE::SEND_TIME), FPSTR(TINTF_068));
    interf->option(String(EVENT_TYPE::PIN_STATE), FPSTR(TINTF_069));
#ifdef AUX_PIN
    interf->option(String(EVENT_TYPE::AUX_ON), FPSTR(TINTF_06A));
    interf->option(String(EVENT_TYPE::AUX_OFF), FPSTR(TINTF_06B));
    interf->option(String(EVENT_TYPE::AUX_TOGGLE), FPSTR(TINTF_06C));
#endif
    interf->option(String(EVENT_TYPE::SET_EFFECT), FPSTR(TINTF_00A));
    interf->option(String(EVENT_TYPE::SET_WARNING), FPSTR(TINTF_0CB));
    interf->json_section_end();

    interf->datetime(FPSTR(TCONST_006B), event.getDateTime(), FPSTR(TINTF_06D));
    interf->number(FPSTR(TCONST_0069), event.repeat, FPSTR(TINTF_06E));
    interf->number(FPSTR(TCONST_006A), event.stopat, FPSTR(TINTF_06F));
    interf->text(FPSTR(TCONST_0035), String(event.message), FPSTR(TINTF_070), false);

    interf->json_section_hidden(FPSTR(TCONST_0069), FPSTR(TINTF_071));
    interf->checkbox(FPSTR(TCONST_0061), (event.d1? "1" : "0"), FPSTR(TINTF_072), false);
    interf->checkbox(FPSTR(TCONST_0062), (event.d2? "1" : "0"), FPSTR(TINTF_073), false);
    interf->checkbox(FPSTR(TCONST_0063), (event.d3? "1" : "0"), FPSTR(TINTF_074), false);
    interf->checkbox(FPSTR(TCONST_0064), (event.d4? "1" : "0"), FPSTR(TINTF_075), false);
    interf->checkbox(FPSTR(TCONST_0065), (event.d5? "1" : "0"), FPSTR(TINTF_076), false);
    interf->checkbox(FPSTR(TCONST_0066), (event.d6? "1" : "0"), FPSTR(TINTF_077), false);
    interf->checkbox(FPSTR(TCONST_0067), (event.d7? "1" : "0"), FPSTR(TINTF_078), false);
    interf->json_section_end();

    if (edit) {
        interf->hidden(FPSTR(TCONST_002E), "1");
        interf->button_submit(FPSTR(TCONST_006C), FPSTR(TINTF_079));
    } else {
        interf->button_submit(FPSTR(TCONST_006C), FPSTR(TINTF_05D), FPSTR(TCONST_002F));
    }

    interf->spacer();
    interf->button(FPSTR(TCONST_005C), FPSTR(TINTF_00B));

    interf->json_section_end();
    interf->json_frame_flush();
}
#ifdef ESP_USE_BUTTON
void block_settings_butt(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_section_main(FPSTR(TCONST_006D), FPSTR(TINTF_013));

    interf->checkbox(FPSTR(TCONST_001F), myButtons->isButtonOn()? "1" : "0", FPSTR(TINTF_07B), true);

    interf->json_section_begin(FPSTR(TCONST_006E));
    interf->select(FPSTR(TCONST_006F), String(0), String(FPSTR(TINTF_07A)), false);
    for (int i = 0; i < myButtons->size(); i++) {
        interf->option(String(i), (*myButtons)[i]->getName());
    }
    interf->json_section_end();

    interf->json_section_line();
    interf->button_submit_value(FPSTR(TCONST_006E), FPSTR(TCONST_005F), FPSTR(TINTF_05C), FPSTR(TCONST_002F));
    interf->button_submit_value(FPSTR(TCONST_006E), FPSTR(TCONST_00B6), FPSTR(TINTF_006), FPSTR(TCONST_000C));
    interf->json_section_end();

    interf->json_section_end();

    interf->button(FPSTR(TCONST_006E), FPSTR(TINTF_05D));

    interf->spacer();
    interf->button(FPSTR(TCONST_0004), FPSTR(TINTF_00B));

    interf->json_section_end();
}

void show_settings_butt(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    block_settings_butt(interf, data);
    interf->json_frame_flush();
}

void set_butt_conf(Interface *interf, JsonObject *data){
    if (!data) return;

    Button *btn = nullptr;
    bool on = ((*data)[FPSTR(TCONST_0070)] == "1");
    bool hold = ((*data)[FPSTR(TCONST_0071)] == "1");
    bool onetime = ((*data)[FPSTR(TCONST_0072)] == "1");
    uint8_t clicks = (*data)[FPSTR(TCONST_0073)];
    String param = (*data)[FPSTR(TCONST_00B5)].as<String>();
    BA action = (BA)(*data)[FPSTR(TCONST_0074)].as<long>();

    if (data->containsKey(FPSTR(TCONST_006F))) {
        int num = (*data)[FPSTR(TCONST_006F)];
        if (num < myButtons->size()) {
            btn = (*myButtons)[num];
        }
    }
    if (btn) {
        btn->action = action;
        btn->flags.on = on;
        btn->flags.hold = hold;
        btn->flags.click = clicks;
        btn->flags.onetime = onetime;
        btn->setParam(param);
    } else {
        myButtons->add(new Button(on, hold, clicks, onetime, action, param));
    }

    myButtons->saveConfig();
    show_settings_butt(interf, data);
}

void show_butt_conf(Interface *interf, JsonObject *data){
    if (!interf || !data) return;

    Button *btn = nullptr;
    String act;
    int num = 0;

    if (data->containsKey(FPSTR(TCONST_006F))) {
        num = (*data)[FPSTR(TCONST_006F)];
        if (num < myButtons->size()) {
            act = (*data)[FPSTR(TCONST_006E)].as<String>();
            btn = (*myButtons)[num];
        }
    }

    if (act == FPSTR(TCONST_00B6)) {
        myButtons->remove(num);
        myButtons->saveConfig();
        show_settings_butt(interf, data);
        return;
    } else
    if (data->containsKey(FPSTR(TCONST_002E))) {
        set_butt_conf(interf, data);
        return;
    }


    interf->json_frame_interface();

    if (btn) {
        interf->json_section_main(FPSTR(TCONST_0075), FPSTR(TINTF_05C));
        interf->constant(FPSTR(TCONST_006F), String(num), btn->getName());
    } else {
        interf->json_section_main(FPSTR(TCONST_0075), FPSTR(TINTF_05D));
    }

    interf->select(FPSTR(TCONST_0074), String(btn? btn->action : 0), String(FPSTR(TINTF_07A)), false);
    for (int i = 1; i < BA::BA_END; i++) {
        interf->option(String(i), FPSTR(btn_get_desc((BA)i)));
    }
    interf->json_section_end();

    interf->text(FPSTR(TCONST_00B5),(btn? btn->getParam() : String("")),FPSTR(TINTF_0B9),false);

    interf->checkbox(FPSTR(TCONST_0070), (btn? btn->flags.on : 0)? "1" : "0", FPSTR(TINTF_07C), false);
    interf->checkbox(FPSTR(TCONST_0071), (btn? btn->flags.hold : 0)? "1" : "0", FPSTR(TINTF_07D), false);
    interf->number(FPSTR(TCONST_0073), (btn? btn->flags.click : 0), FPSTR(TINTF_07E), 0, 7);
    interf->checkbox(FPSTR(TCONST_0072), (btn? btn->flags.onetime&1 : 0)? "1" : "0", FPSTR(TINTF_07F), false);

    if (btn) {
        interf->hidden(FPSTR(TCONST_002E), "1");
        interf->button_submit(FPSTR(TCONST_0075), FPSTR(TINTF_079));
    } else {
        interf->button_submit(FPSTR(TCONST_0075), FPSTR(TINTF_05D), FPSTR(TCONST_002F));
    }

    interf->spacer();
    interf->button(FPSTR(TCONST_0076), FPSTR(TINTF_00B));

    interf->json_section_end();
    interf->json_frame_flush();
}

void set_btnflag(Interface *interf, JsonObject *data){
    // в отдельном классе, т.е. не входит в флаги лампы!
    if (!data) return;
    SETPARAM(FPSTR(TCONST_001F), myButtons->setButtonOn((*data)[FPSTR(TCONST_001F)] == "1"));
}
#endif

void set_debugflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setDebug((*data)[FPSTR(TCONST_0095)] == "1");
    save_lamp_flags();
}

void set_drawflag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setDrawBuff((*data)[FPSTR(TCONST_00C4)] == "1");
    save_lamp_flags();
}

#ifdef MP3PLAYER
void set_mp3flag(Interface *interf, JsonObject *data){
    if (!data) return;
    myLamp.setONMP3((*data)[FPSTR(TCONST_009D)] == "1");
    if(myLamp.isLampOn())
        mp3->setIsOn(myLamp.isONMP3(), true); // при включенной лампе - форсировать воспроизведение
    else
        mp3->setIsOn(myLamp.isONMP3(), (myLamp.getLampSettings().isOnMP3 && millis()>5000)); // при выключенной - только для mp3 и не после перезагрузки
    save_lamp_flags();
}

void set_mp3volume(Interface *interf, JsonObject *data){
    if (!data) return;
    int volume = (*data)[FPSTR(TCONST_00A2)];
    SETPARAM(FPSTR(TCONST_00A2), mp3->setVolume(volume));
}

void set_mp3_player(Interface *interf, JsonObject *data){
    if (!data) return;

    if(!myLamp.isONMP3()) return;
    uint16_t cur_palyingnb = mp3->getCurPlayingNb();
    if(data->containsKey(FPSTR(TCONST_00BE))){
        mp3->playEffect(cur_palyingnb-1,"");
    } else if(data->containsKey(FPSTR(TCONST_00BF))){
        mp3->playEffect(cur_palyingnb+1,"");
    } else if(data->containsKey(FPSTR(TCONST_00C0))){
        mp3->playEffect(cur_palyingnb-5,"");
    } else if(data->containsKey(FPSTR(TCONST_00C1))){
        mp3->playEffect(cur_palyingnb+5,"");
    }
}

#endif

void section_effects_frame(Interface *interf, JsonObject *data){
    if(optionsTicker.active())
        optionsTicker.detach();
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));
    block_effects_main(interf, data);
    interf->json_frame_flush();
}

void section_text_frame(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));
    block_lamptext(interf, data);
    interf->json_frame_flush();
}

void section_drawing_frame(Interface *interf, JsonObject *data){
    // Рисование
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));
    block_drawing(interf, data);
    interf->json_frame_flush();
}

void section_settings_frame(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface(FPSTR(TINTF_080));

    interf->json_section_main(FPSTR(TCONST_0004), FPSTR(TINTF_002));

    interf->button(FPSTR(TCONST_0077), FPSTR(TINTF_051));

    interf->button(FPSTR(TCONST_0078), FPSTR(TINTF_081));
#ifdef MIC_EFFECTS
    interf->button(FPSTR(TCONST_0079), FPSTR(TINTF_020));
#endif
#ifdef MP3PLAYER
    interf->button(FPSTR(TCONST_009F), FPSTR(TINTF_099));
#endif

    interf->button(FPSTR(TCONST_005C), FPSTR(TINTF_011));
#ifdef ESP_USE_BUTTON
    interf->button(FPSTR(TCONST_0076), FPSTR(TINTF_013));
#endif
    interf->button(FPSTR(TCONST_007A), FPSTR(TINTF_082));

    block_lamp_config(interf, data);

    interf->spacer();
    block_settings_update(interf, data);

    interf->json_section_end();
    interf->json_frame_flush();
}

void section_main_frame(Interface *interf, JsonObject *data){
    if (!interf) return;

    interf->json_frame_interface(FPSTR(TINTF_080));

    block_menu(interf, data);
    block_effects_main(interf, data);

    interf->json_frame_flush();

    if(!embui.sysData.wifi_sta && embui.param(FPSTR(TCONST_0043))=="0"){
        // форсируем выбор вкладки настройки WiFi если контроллер не подключен к внешней AP
        show_settings_wifi(interf, data);
    }
}

void section_sys_settings_frame(Interface *interf, JsonObject *data){
    if (!interf) return;

    interf->json_frame_interface(FPSTR(TINTF_08F));

    block_menu(interf, data);
    interf->json_section_main(FPSTR(TCONST_0099), FPSTR(TINTF_08F));
        interf->spacer(FPSTR(TINTF_092)); // заголовок
        interf->json_section_line(FPSTR(TINTF_092)); // расположить в одной линии
#ifdef ESP_USE_BUTTON
            interf->number(FPSTR(TCONST_0097),FPSTR(TINTF_094),0,16);
#endif
#ifdef MP3PLAYER
            interf->number(FPSTR(TCONST_009B),FPSTR(TINTF_097),0,16);
            interf->number(FPSTR(TCONST_009C),FPSTR(TINTF_098),0,16);
#endif
        interf->json_section_end(); // конец контейнера
        interf->spacer();
        interf->number(FPSTR(TCONST_0098),FPSTR(TINTF_095),0,16000);

        //interf->json_section_main(FPSTR(TCONST_005F), "");
        interf->frame2(FPSTR(TCONST_005F), FPSTR(TCONST_005F));
        //interf->json_section_end();

        interf->button_submit(FPSTR(TCONST_0099), FPSTR(TINTF_008), FPSTR(TCONST_0008));

        interf->spacer();
        interf->button(FPSTR(TCONST_0000), FPSTR(TINTF_00B));
    interf->json_section_end();
    
    interf->json_frame_flush();
}

void set_sys_settings(Interface *interf, JsonObject *data){
    if(!data) return;

#ifdef ESP_USE_BUTTON
    {String tmpChk = (*data)[FPSTR(TCONST_0097)]; if(tmpChk.toInt()>16) return;}
#endif
#ifdef MP3PLAYER
    {String tmpChk = (*data)[FPSTR(TCONST_009B)]; if(tmpChk.toInt()>16) return;}
    {String tmpChk = (*data)[FPSTR(TCONST_009C)]; if(tmpChk.toInt()>16) return;}
#endif
    {String tmpChk = (*data)[FPSTR(TCONST_0098)]; if(tmpChk.toInt()>16000) return;}

#ifdef ESP_USE_BUTTON
    SETPARAM(FPSTR(TCONST_0097));
#endif
#ifdef MP3PLAYER
    SETPARAM(FPSTR(TCONST_009B));
    SETPARAM(FPSTR(TCONST_009C));
#endif
    SETPARAM(FPSTR(TCONST_0098));
    myLamp.sendString(String(FPSTR(TINTF_096)).c_str(), CRGB::Red);
    sysTicker.once(10,std::bind([]{
        embui.save();
        ESP.restart();
    }));
    section_effects_frame(interf,data);
}

void set_lamp_flags(Interface *interf, JsonObject *data){
    if(!data) return;
    SETPARAM(FPSTR(TCONST_0094));
}

void save_lamp_flags(){
    DynamicJsonDocument doc(128);
    JsonObject obj = doc.to<JsonObject>();
    obj[FPSTR(TCONST_0094)] = myLamp.getLampFlags();
    set_lamp_flags(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
}

/**
 * Набор конфигурационных переменных и обработчиков интерфейса
 */
void create_parameters(){
    LOG(println, F("Создание дефолтных параметров"));
    // создаем дефолтные параметры для нашего проекта
    embui.var_create(FPSTR(TCONST_0094), "0"); // Дефолтный набор флагов // myLamp.getLampFlags()

    //WiFi
    embui.var_create(FPSTR(TCONST_003F), F(""));
    embui.var_create(FPSTR(TCONST_0043),  "0");     // режим AP-only (только точка доступа), не трогать
    embui.var_create(FPSTR(TCONST_0044), F(""));      // пароль внутренней точки доступа

    // параметры подключения к MQTT
    embui.var_create(FPSTR(TCONST_0046), F("")); // Дефолтные настройки для MQTT
    embui.var_create(FPSTR(TCONST_0047), F("1883"));
    embui.var_create(FPSTR(TCONST_0048), F(""));
    embui.var_create(FPSTR(TCONST_0049), F(""));
    embui.var_create(FPSTR(TCONST_007B), embui.mc);  // m_pref == MAC по дефолту
    embui.var_create(FPSTR(TCONST_004A), F("30")); // интервал отправки данных по MQTT в секундах (параметр в энергонезависимой памяти)
    embui.var_create(FPSTR(TCONST_0016), F("1"));
    //embui.var_create(FPSTR(TCONST_002A), F("cfg1.json"));
#ifdef ESP_USE_BUTTON
    embui.var_create(FPSTR(TCONST_001F), "1"); // не трогать пока...
#endif
#ifdef AUX_PIN
    embui.var_create(FPSTR(TCONST_000E), "0");
#endif
    embui.var_create(FPSTR(TCONST_0035), F(""));
    embui.var_create(FPSTR(TCONST_0036), FPSTR(TCONST_007C));
    embui.var_create(FPSTR(TCONST_00C3), String(FADETOBLACKVALUE));
    embui.var_create(FPSTR(TCONST_0051), F("100"));
    embui.var_create(FPSTR(TCONST_0052), F("0"));
    embui.var_create(FPSTR(TCONST_0053), F("0"));
    embui.var_create(FPSTR(TCONST_0050), F("1"));
    embui.var_create(FPSTR(TCONST_0018), F("127"));

    // date/time related vars
    embui.var_create(FPSTR(TCONST_0057), "");
    embui.var_create(FPSTR(TCONST_0058), "");
    embui.var_create(FPSTR(TCONST_0054), F("0"));
    embui.var_create(FPSTR(TCONST_0055), FPSTR(TCONST_007D));

#ifdef MIC_EFFECTS
    embui.var_create(FPSTR(TCONST_0039),F("1.28"));
    embui.var_create(FPSTR(TCONST_003A),F("0.00"));
    embui.var_create(FPSTR(TCONST_003B),F("0"));
#endif

#ifdef RESTORE_STATE
    embui.var_create(FPSTR(TCONST_001B), "0");
#endif

    embui.var_create(FPSTR(TCONST_0026), String(F("60"))); // Дефолтное значение, настраивается из UI
    embui.var_create(FPSTR(TCONST_00BD), String(F("85"))); // 5<<4+5, старшие и младшие 4 байта содержат 5

    // пины и системные настройки
#ifdef ESP_USE_BUTTON
    embui.var_create(FPSTR(TCONST_0097), String(BTN_PIN)); // Пин кнопки
#endif
#ifdef MP3PLAYER
    embui.var_create(FPSTR(TCONST_009B), String(MP3_RX_PIN)); // Пин RX плеера
    embui.var_create(FPSTR(TCONST_009C), String(MP3_TX_PIN)); // Пин TX плеера
    embui.var_create(FPSTR(TCONST_00A2),F("15")); // громкость
    embui.var_create(FPSTR(TCONST_00A9),F("255")); // кол-во файлов в папке mp3
#endif
    embui.var_create(FPSTR(TCONST_0098), String(CURRENT_LIMIT)); // Лимит по току

    // далее идут обработчики параметров
    embui.section_handle_add(FPSTR(TCONST_0099), set_sys_settings);

    embui.section_handle_add(FPSTR(TCONST_0094), set_lamp_flags);

    embui.section_handle_add(FPSTR(TCONST_007E), section_main_frame);
    embui.section_handle_add(FPSTR(TCONST_0020), show_main_flags);

    embui.section_handle_add(FPSTR(TCONST_0000), section_effects_frame);
    embui.section_handle_add(FPSTR(TCONST_0011), show_effects_param);
    embui.section_handle_add(FPSTR(TCONST_0016), set_effects_list);
    embui.section_handle_add(FPSTR(TCONST_0012), set_effects_bright);
    embui.section_handle_add(FPSTR(TCONST_0013), set_effects_speed);
    embui.section_handle_add(FPSTR(TCONST_0014), set_effects_scale);
    embui.section_handle_add(FPSTR(TCONST_007F), set_effects_dynCtrl);

    embui.section_handle_add(FPSTR(TCONST_0022), set_eff_prev);
    embui.section_handle_add(FPSTR(TCONST_0023), set_eff_next);

    embui.section_handle_add(FPSTR(TCONST_000F), show_effects_config);
    embui.section_handle_add(FPSTR(TCONST_0010), set_effects_config_list);
    embui.section_handle_add(FPSTR(TCONST_0005), set_effects_config_param);

    embui.section_handle_add(FPSTR(TCONST_001A), set_onflag);
    embui.section_handle_add(FPSTR(TCONST_001B), set_demoflag);
    embui.section_handle_add(FPSTR(TCONST_001C), set_gbrflag);
#ifdef OTA
    embui.section_handle_add(FPSTR(TCONST_0027), set_otaflag);
#endif
#ifdef AUX_PIN
    embui.section_handle_add(FPSTR(TCONST_000E), set_auxflag);
#endif
    embui.section_handle_add(FPSTR(TCONST_00C8), section_drawing_frame);
    embui.section_handle_add(FPSTR(TCONST_009A), section_sys_settings_frame);
    embui.section_handle_add(FPSTR(TCONST_0003), section_text_frame);
    embui.section_handle_add(FPSTR(TCONST_0034), set_lamp_textsend);
    embui.section_handle_add(FPSTR(TCONST_0030), edit_lamp_config);
    embui.section_handle_add(FPSTR(TCONST_0029), edit_lamp_config);

    embui.section_handle_add(FPSTR(TCONST_00BA), set_text_config);

    embui.section_handle_add(FPSTR(TCONST_0004), section_settings_frame);
    embui.section_handle_add(FPSTR(TCONST_0078), show_settings_wifi);
    embui.section_handle_add(FPSTR(TCONST_003E), set_settings_wifi);
    embui.section_handle_add(FPSTR(TCONST_0042), set_settings_wifiAP);
    embui.section_handle_add(FPSTR(TCONST_0045), set_settings_mqtt);
    embui.section_handle_add(FPSTR(TCONST_007A), show_settings_other);
    embui.section_handle_add(FPSTR(TCONST_004B), set_settings_other);
    embui.section_handle_add(FPSTR(TCONST_0077), show_settings_time);
    embui.section_handle_add(FPSTR(TCONST_0056), set_settings_time);

    embui.section_handle_add(FPSTR(TCONST_00CA), set_drawing);

#ifdef MIC_EFFECTS
    embui.section_handle_add(FPSTR(TCONST_0079), show_settings_mic);
    embui.section_handle_add(FPSTR(TCONST_0038), set_settings_mic);
    embui.section_handle_add(FPSTR(TCONST_001E), set_micflag);
    embui.section_handle_add(FPSTR(TCONST_003C), set_settings_mic_calib);
#endif
    embui.section_handle_add(FPSTR(TCONST_005C), show_settings_event);
    embui.section_handle_add(FPSTR(TCONST_005D), show_event_conf);
    embui.section_handle_add(FPSTR(TCONST_006C), set_event_conf);
    embui.section_handle_add(FPSTR(TCONST_001D), set_eventflag);
#ifdef ESP_USE_BUTTON
    embui.section_handle_add(FPSTR(TCONST_0076), show_settings_butt);
    embui.section_handle_add(FPSTR(TCONST_006E), show_butt_conf);
    embui.section_handle_add(FPSTR(TCONST_0075), set_butt_conf);
    embui.section_handle_add(FPSTR(TCONST_001F), set_btnflag);
#endif

    embui.section_handle_add(FPSTR(TCONST_00C4), set_drawflag);

#ifdef LAMP_DEBUG
    embui.section_handle_add(FPSTR(TCONST_0095), set_debugflag);
#endif

#ifdef MP3PLAYER
    embui.section_handle_add(FPSTR(TCONST_009D), set_mp3flag);
    embui.section_handle_add(FPSTR(TCONST_00A2), set_mp3volume);
    embui.section_handle_add(FPSTR(TCONST_009F), show_settings_mp3);
    embui.section_handle_add(FPSTR(TCONST_00A0), set_settings_mp3);

    embui.section_handle_add(FPSTR(TCONST_00BE), set_mp3_player);
    embui.section_handle_add(FPSTR(TCONST_00BF), set_mp3_player);
    embui.section_handle_add(FPSTR(TCONST_00C0), set_mp3_player);
    embui.section_handle_add(FPSTR(TCONST_00C1), set_mp3_player);
#endif
}

void sync_parameters(){
    DynamicJsonDocument doc(1024);
    //https://arduinojson.org/v6/api/jsondocument/
    //JsonDocument::to<T>() clears the document and converts it to the specified type. Don’t confuse this function with JsonDocument::as<T>() that returns a reference only if the requested type matches the one in the document.
    JsonObject obj = doc.to<JsonObject>();

    if(check_recovery_state(true)){
        LOG(printf_P,PSTR("Critical Error: Lamp recovered from corrupted effect number: %s\n"),String(embui.param(FPSTR(TCONST_0016))).c_str());
        embui.var(FPSTR(TCONST_0016),String(0)); // что-то пошло не так, был ребут, сбрасываем эффект
    }

    myLamp.semqtt_int(embui.param(FPSTR(TCONST_004A)).toInt());

    LAMPFLAGS tmp;
    tmp.lampflags = embui.param(FPSTR(TCONST_0094)).toInt();

    obj[FPSTR(TCONST_00C4)] = tmp.isDraw ? "1" : "0";
    set_drawflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>(); // https://arduinojson.org/v6/how-to/reuse-a-json-document/

#ifdef LAMP_DEBUG
    obj[FPSTR(TCONST_0095)] = tmp.isDebug ? "1" : "0";
    set_debugflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
#endif

    //LOG(printf_P,PSTR("tmp.isEventsHandled=%d\n"), tmp.isEventsHandled);
    obj[FPSTR(TCONST_001D)] = tmp.isEventsHandled ? "1" : "0";
    set_eventflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
    embui.timeProcessor.attach_callback(std::bind(&LAMP::setIsEventsHandled, &myLamp, myLamp.IsEventsHandled())); // только после синка будет понятно включены ли события

    obj[FPSTR(TCONST_001C)] = tmp.isGlobalBrightness ? "1" : "0";
    set_gbrflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    if (myLamp.IsGlobalBrightness()) {
        CALL_SETTER(FPSTR(TCONST_0012), embui.param(FPSTR(TCONST_0018)), set_effects_bright);
    } else {
        myLamp.setGlobalBrightness(embui.param(FPSTR(TCONST_0018)).toInt()); // починить бросок яркости в 255 при первом включении
    }

#ifdef RESTORE_STATE
    obj[FPSTR(TCONST_001A)] = tmp.ONflag ? "1" : "0";
    if(tmp.ONflag){ // если лампа включена, то устанавливаем эффект ДО включения
        CALL_SETTER(FPSTR(TCONST_0016), embui.param(FPSTR(TCONST_0016)), set_effects_list);
    }
    set_onflag(nullptr, &obj);
    if(!tmp.ONflag){ // иначе - после
        CALL_SETTER(FPSTR(TCONST_0016), embui.param(FPSTR(TCONST_0016)), set_effects_list);
    }
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
    if(myLamp.isLampOn())
        CALL_SETTER(FPSTR(TCONST_001B), embui.param(FPSTR(TCONST_001B)), set_demoflag); // Демо через режимы, для него нужнен отдельный флаг :(
#else
    CALL_SETTER(FPSTR(TCONST_0016), embui.param(FPSTR(TCONST_0016)), set_effects_list);
#endif

#ifdef MP3PLAYER
    //obj[FPSTR(TCONST_00A2)] = embui.param(FPSTR(TCONST_00A2));  // пишет в плеер!
    obj[FPSTR(TCONST_00A3)] = tmp.playTime;
    obj[FPSTR(TCONST_00A4)] = tmp.playName ? "1" : "0";
    obj[FPSTR(TCONST_00A5)] = tmp.playEffect ? "1" : "0";
    obj[FPSTR(TCONST_00A6)] = String(tmp.alarmSound);
    obj[FPSTR(TCONST_00A7)] = String(tmp.MP3eq); // пишет в плеер!
    obj[FPSTR(TCONST_00A8)] = tmp.playMP3 ? "1" : "0";
    obj[FPSTR(TCONST_00A9)] = embui.param(FPSTR(TCONST_00A9));
    obj[FPSTR(TCONST_00AF)] = tmp.limitAlarmVolume ? "1" : "0";

    set_settings_mp3(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    mp3->setupplayer(myLamp.effects.getEn(), myLamp.effects.getSoundfile()); // установить начальные значения звука
    obj[FPSTR(TCONST_009D)] = tmp.isOnMP3 ? "1" : "0";
    set_mp3flag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    CALL_SETTER(FPSTR(TCONST_00A2), embui.param(FPSTR(TCONST_00A2)), set_mp3volume);

#endif

#ifdef AUX_PIN
    CALL_SETTER(FPSTR(TCONST_000E), embui.param(FPSTR(TCONST_000E)), set_auxflag);
#endif

    myLamp.setClearingFlag(tmp.isEffClearing);

#ifdef ESP_USE_BUTTON
    // в отдельном классе, в список флагов лампы не входит!
    CALL_SETTER(FPSTR(TCONST_001F), embui.param(FPSTR(TCONST_001F)), set_btnflag);
#endif

    obj[FPSTR(TCONST_0051)] = embui.param(FPSTR(TCONST_0051));
    obj[FPSTR(TCONST_0052)] = embui.param(FPSTR(TCONST_0052));
    obj[FPSTR(TCONST_0053)] = embui.param(FPSTR(TCONST_0053));
    obj[FPSTR(TCONST_0054)] = embui.param(FPSTR(TCONST_0054));
    obj[FPSTR(TCONST_00C3)] = embui.param(FPSTR(TCONST_00C3));

    String datetime;
    TimeProcessor::getDateTimeString(datetime, embui.param(FPSTR(TCONST_0055)).toInt());
    obj[FPSTR(TCONST_0055)] = datetime;
    
    set_text_config(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    obj[FPSTR(TCONST_004E)] = tmp.isFaderON ? "1" : "0";
    obj[FPSTR(TCONST_008E)] = tmp.isEffClearing ? "1" : "0";
    obj[FPSTR(TCONST_004C)] = tmp.MIRR_H ? "1" : "0";
    obj[FPSTR(TCONST_004D)] = tmp.MIRR_V ? "1" : "0";
    obj[FPSTR(TCONST_0090)] = tmp.numInList ? "1" : "0";
    obj[FPSTR(TCONST_004F)] = tmp.dRand ? "1" : "0";
    obj[FPSTR(TCONST_009E)] = tmp.showName ? "1" : "0";
    obj[FPSTR(TCONST_0091)] = tmp.effHasMic ? "1" : "0";
    obj[FPSTR(TCONST_0096)] = tmp.isShowSysMenu ? "1" : "0";

    uint8_t alarmPT = embui.param(FPSTR(TCONST_00BD)).toInt();
    obj[FPSTR(TCONST_00BB)] = alarmPT>>4;
    obj[FPSTR(TCONST_00BC)] = alarmPT&0x0F;

    SORT_TYPE type = (SORT_TYPE)embui.param(FPSTR(TCONST_0050)).toInt();
    obj[FPSTR(TCONST_0050)] = type;

    set_settings_other(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

#ifdef MIC_EFFECTS
    obj[FPSTR(TCONST_001E)] = tmp.isMicOn ? "1" : "0";
    myLamp.setMicAnalyseDivider(0);
    set_micflag(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();

    // float scale = atof(embui.param(FPSTR(TCONST_0039)).c_str());
    // float noise = atof(embui.param(FPSTR(TCONST_003A)).c_str());
    // MIC_NOISE_REDUCE_LEVEL lvl=(MIC_NOISE_REDUCE_LEVEL)embui.param(FPSTR(TCONST_003B)).toInt();

    obj[FPSTR(TCONST_0039)] = embui.param(FPSTR(TCONST_0039)); //scale;
    obj[FPSTR(TCONST_003A)] = embui.param(FPSTR(TCONST_003A)); //noise;
    obj[FPSTR(TCONST_003B)] = embui.param(FPSTR(TCONST_003B)); //lvl;
    set_settings_mic(nullptr, &obj);
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
#endif

    check_recovery_state(false); // удаляем маркер, считаем что у нас все хорошо...
    LOG(println, F("sync_parameters() done"));
}

void remote_action(RA action, ...){
    LOG(printf_P, PSTR("RA %d: "), action);
    DynamicJsonDocument doc(512);
    JsonObject obj = doc.to<JsonObject>();

    char *key = NULL, *val = NULL, *value = NULL;
    va_list prm;
    va_start(prm, action);
    while ((key = (char *)va_arg(prm, char *)) && (val = (char *)va_arg(prm, char *))) {
        LOG(printf_P, PSTR("%s = %s"), key, val);
        obj[key] = val;
    }
    va_end(prm);
    if (key && !val) {
        value = key;
        LOG(printf_P, PSTR("%s"), value);
    }
    LOG(println);

    switch (action) {
        case RA::RA_ON:
            CALL_INTF(FPSTR(TCONST_001A), "1", set_onflag);
            break;
        case RA::RA_OFF: {
                // нажатие кнопки точно отключает ДЕМО и белую лампу возвращая в нормальный режим
                LAMPMODE mode = myLamp.getMode();
                if(mode!=LAMPMODE::MODE_NORMAL){
                    CALL_INTF(FPSTR(TCONST_001B), "0", set_demoflag); // отключить демо, если было включено
                    if (myLamp.IsGlobalBrightness()) {
                        embui.var(FPSTR(TCONST_0018), String(myLamp.getLampBrightness())); // сохранить восстановленную яркость в конфиг, если она глобальная
                    }
                }
                CALL_INTF(FPSTR(TCONST_001A), "0", set_onflag);
            }
            break;
        case RA::RA_DEMO:
            CALL_INTF(FPSTR(TCONST_001A), "1", set_onflag); // включим, если было отключено
            if(value && String(value)=="0"){
                CALL_INTF(FPSTR(TCONST_001B), "0", set_demoflag);
                myLamp.startNormalMode();
            } else {
                CALL_INTF(FPSTR(TCONST_001B), "1", set_demoflag);
                resetAutoTimers();
                myLamp.startDemoMode();
            }
            break;
        case RA::RA_DEMO_NEXT:
            if (myLamp.getLampSettings().dRand) {
                myLamp.switcheffect(SW_RND, myLamp.getFaderFlag());
            } else {
                myLamp.switcheffect(SW_NEXT_DEMO, myLamp.getFaderFlag());
            }
            sysTicker.once(3,std::bind([]{
                remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
            }));
            break;
        case RA::RA_EFFECT: {
            if(myLamp.getMode()==LAMPMODE::MODE_NORMAL)
                embui.var(FPSTR(TCONST_0016), value); // сохранить в конфиг изменившийся эффект
            CALL_INTF(FPSTR(TCONST_0016), value, set_effects_list); // публикация будет здесь
            break;
        }
        case RA::RA_GLOBAL_BRIGHT:
            CALL_INTF(FPSTR(TCONST_001C), value, set_gbrflag);
            break;
        case RA::RA_BRIGHT_NF:
            obj[FPSTR(TCONST_0017)] = true;
        case RA::RA_BRIGHT:
            CALL_INTF(FPSTR(TCONST_0012), value, set_effects_bright);
            break;
        case RA::RA_SPEED:
            CALL_INTF(FPSTR(TCONST_0013), value, set_effects_speed);
            break;
        case RA::RA_SCALE:
            CALL_INTF(FPSTR(TCONST_0014), value, set_effects_scale);
            break;
        case RA::RA_EXTRA:
            //CALL_INTF(FPSTR(TCONST_0015), value, set_effects_dynCtrl);
            CALL_INTF_OBJ(set_effects_dynCtrl);
            break;
#ifdef MIC_EFFECTS
        case RA::RA_MIC:
            CALL_INTF_OBJ(show_settings_mic);
            break;
#endif
        case RA::RA_EFF_NEXT:
            resetAutoTimers(); // сборс таймера демо, если есть перемещение
            myLamp.switcheffect(SW_NEXT, myLamp.getFaderFlag());
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_EFF_PREV:
            resetAutoTimers(); // сборс таймера демо, если есть перемещение
            myLamp.switcheffect(SW_PREV, myLamp.getFaderFlag());
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_EFF_RAND:
            myLamp.switcheffect(SW_RND, myLamp.getFaderFlag());
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_WHITE_HI:
            myLamp.switcheffect(SW_WHITE_HI);
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_WHITE_LO:
            myLamp.switcheffect(SW_WHITE_LO);
            return remote_action(RA::RA_EFFECT, String(myLamp.effects.getSelected()).c_str(), NULL);
        case RA::RA_ALARM:
            myLamp.startAlarm(value);
            break;
        case RA::RA_ALARM_OFF:
            myLamp.stopAlarm();
            break;
        case RA::RA_REBOOT:
            remote_action(RA::RA_WARNING, F("[16711680,3000,500]"), NULL);
            sysTicker.once(3,std::bind([]{
                ESP.restart(); // так лучше :)
            }));
            break;
        case RA::RA_WIFI_REC:
            CALL_INTF(FPSTR(TINTF_028), FPSTR(TCONST_0080), set_settings_wifi);
            break;
        case RA::RA_LAMP_CONFIG:
            if (value && *value) {
                String filename = String(FPSTR(TCONST_0031));
                filename.concat(value);
                embui.load(filename.c_str());
                sync_parameters();
            }
            break;
        case RA::RA_EFF_CONFIG:
            if (value && *value) {
                String filename = String(FPSTR(TCONST_002C));
                filename.concat(value);
                myLamp.effects.initDefault(filename.c_str());
            }
            break;
        case RA::RA_EVENTS_CONFIG:
            if (value && *value) {
                String filename = String(FPSTR(TCONST_0032));
                filename.concat(value);
                myLamp.events.loadConfig(filename.c_str());
            }
            break;
        case RA::RA_SEND_TEXT: {
            String tmpStr = embui.param(FPSTR(TCONST_0036));
            if (value && *value) {
                String tmpStr = embui.param(FPSTR(TCONST_0036));
                tmpStr.replace(F("#"),F("0x"));
                CRGB::HTMLColorCode color = (CRGB::HTMLColorCode)strtol(tmpStr.c_str(), NULL, 0);

                myLamp.sendString(value, color);
            }
            break;
        }
        case RA::RA_WARNING: {
            String str=value;
            DynamicJsonDocument doc(256);
            deserializeJson(doc,str);
            JsonArray arr = doc.as<JsonArray>();
            uint32_t col=CRGB::Red, dur=1000, per=250, type=0;

            for (size_t i = 0; i < arr.size(); i++) {
                switch(i){
                    case 0: {
                        String tmpStr = arr[i];
                        tmpStr.replace(F("#"), F("0x"));
                        long val = strtol(tmpStr.c_str(), NULL, 0);
                        col = val;
                        break;
                    }
                    case 1: dur = arr[i]; break;
                    case 2: per = arr[i]; break;
                    case 3: type = arr[i]; break;
                    default : break;
                }
			}
            myLamp.showWarning2(col,dur,per,type);
            break; 
        }

        case RA::RA_DRAW: {
            String str=value;
            DynamicJsonDocument doc(256);
            deserializeJson(doc,str);
            JsonArray arr = doc.as<JsonArray>();
            CRGB col=CRGB::White;
            uint16_t x=WIDTH/2U, y=HEIGHT/2U;

            for (size_t i = 0; i < arr.size(); i++) {
                switch(i){
                    case 0: {
                        String tmpStr = arr[i];
                        tmpStr.replace(F("#"), F("0x"));
                        unsigned long val = strtol(tmpStr.c_str(), NULL, 0);
                        LOG(printf_P, PSTR("%s:%ld\n"), tmpStr.c_str(), val);
                        col = val;
                        break;
                    }
                    case 1: x = arr[i]; break;
                    case 2: y = arr[i]; break;
                    default : break;
                }
			}
            myLamp.writeDrawBuf(col,x,y);
            break; 
        }

        case RA::RA_FILLMATRIX: {
            String tmpStr = value;
            tmpStr.replace(F("#"), F("0x"));
            long val = strtol(tmpStr.c_str(), NULL, 0);
            LOG(printf_P, PSTR("%s:%ld\n"), tmpStr.c_str(), val);
            CRGB color=CRGB(val);
            myLamp.fillDrawBuf(color);
            break; 
        }

        case RA::RA_SEND_IP:
            myLamp.sendString(WiFi.localIP().toString().c_str(), CRGB::White);
            break;
        case RA::RA_SEND_TIME:
            myLamp.sendString(String(F("%TM")).c_str(), CRGB::Green);
            break;
#ifdef OTA
        case RA::RA_OTA:
            myLamp.startOTAUpdate();
            break;
#endif
#ifdef AUX_PIN
        case RA::RA_AUX_ON:
            obj[FPSTR(TCONST_000E)] = true;
            set_auxflag(nullptr, &obj);
            break;
        case RA::RA_AUX_OFF:
            obj[FPSTR(TCONST_000E)] = false;
            set_auxflag(nullptr, &obj);
            break;
        case RA::RA_AUX_TOGLE:
            AUX_toggle(!digitalRead(AUX_PIN));
            break;
#endif
        default:;
    }
    doc.clear(); doc.garbageCollect(); obj = doc.to<JsonObject>();
}

String httpCallback(const String &param, const String &value, bool isset){
    String result = F("Ok");
    RA action = RA_UNKNOWN;
    LOG(printf_P, PSTR("HTTP: %s - %s\n"), param.c_str(), value.c_str());

    if(!isset) {
        LOG(println, F("GET"));
        if (param == FPSTR(TCONST_0070))
            { result = myLamp.isLampOn() ? "1" : "0"; }
        else if (param == FPSTR(TCONST_0081))
            { result = !myLamp.isLampOn() ? "1" : "0"; }
        else if (param == FPSTR(TCONST_00B4))
            { result = myLamp.IsGlobalBrightness() ? "1" : "0"; }
        else if (param == FPSTR(TCONST_00AA))
            { result = myLamp.getMode() == LAMPMODE::MODE_DEMO ? "1" : "0"; }
        else if (param == FPSTR(TCONST_0012))
            { result = String(myLamp.getLampBrightness()); }
        else if (param == FPSTR(TCONST_0013))
            { result = myLamp.effects.getControls()[1]->getVal(); }
        else if (param == FPSTR(TCONST_0014))
            { result = myLamp.effects.getControls()[2]->getVal(); }
        else if (param == FPSTR(TCONST_0082))
            { result = String(myLamp.effects.getCurrent());  }
        else if (param == FPSTR(TCONST_00B7))
            { myLamp.showWarning2(CRGB::Orange,5000,500); }
        else if (param == FPSTR(TCONST_00AE)) {
                String result = myLamp.effects.geteffconfig(myLamp.effects.getCurrent(), myLamp.getNormalizedLampBrightness());
                embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_00AE), result, true);
                return result;
            }
        else if (param == FPSTR(TCONST_00D0)) {
            LList<UIControl*>&controls = myLamp.effects.getControls();
            for(int i=0; i<controls.size();i++){
                if(value == String(controls[i]->getId())){
                    result = String(F("[")) + controls[i]->getId() + String(F(",\"")) + (controls[i]->getId()==0 ? String(myLamp.getNormalizedLampBrightness()) : controls[i]->getVal()) + String(F("\"]"));
                    embui.publish(String(FPSTR(TCONST_008B)) + FPSTR(TCONST_00D0), result, true);
                    return result;
                }
            }
        }
        // что-то случилось с сокетом... уйдем на другой эффект, отсрочим сохранение, удалим конфиг эффекта, вернемся
        else if (param == F("sys_WS_EVT_ERROR"))  {
            resetAutoTimers();
            uint16_t effNum = myLamp.effects.getSelected();
            myLamp.effects.directMoveBy(EFF_NONE);
            myLamp.effects.removeConfig(effNum);
            myLamp.effects.directMoveBy(effNum);
            //remote_action(RA_EFFECT, String(effNum).c_str(), NULL);
            String tmpStr=F("- ");
            tmpStr+=effNum;
            myLamp.sendString(tmpStr.c_str(), CRGB::Red);
            return result;
        }
        // реализация AUTODISCOVERY
        else if (param == F("sys_AUTODISCOVERY"))  {
            DynamicJsonDocument hass_discover(1024);
            String name = embui.param(FPSTR(TCONST_003F));
            String unique_id = embui.mc;

            hass_discover[F("~")] = embui.id("embui/"); // embui.param(FPSTR(TCONST_007B)) + F("/embui/") //String(F("homeassistant/light/"))+name;
            hass_discover[F("name")] = name;                // name
            hass_discover[F("uniq_id")] = unique_id;        // String(ESP.getChipId(), HEX); // unique_id
            hass_discover[F("avty_t")] = F("~pub/online");  // availability_topic
            hass_discover[F("pl_avail")] = F("true");       // payload_available
            hass_discover[F("pl_not_avail")] = F("false");  // payload_not_available

            hass_discover[F("cmd_t")] = F("~set/on");             // command_topic
            hass_discover[F("stat_t")] = F("~pub/on");            // state_topic

            hass_discover[F("bri_cmd_t")] = F("~set/bright");     // brightness_command_topic
            hass_discover[F("bri_stat_t")] = F("~pub/bright");    // brightness_state_topic
            hass_discover[F("bri_scl")] = 255;

            hass_discover[F("clr_temp_cmd_t")] = F("~set/speed");     // speed as color temperature
            hass_discover[F("clr_temp_stat_t")] = F("~pub/speed");    // speed as color temperature
            hass_discover[F("min_mireds")] = 1;
            hass_discover[F("max_mireds")] = 255;

            hass_discover[F("whit_val_cmd_t")] = F("~set/scale");     // scale as white level
            hass_discover[F("whit_val_stat_t")] = F("~pub/scale");    // scale as white level
            hass_discover[F("whit_val_scl")] = 255;

            hass_discover[F("fx_cmd_t")] = F("~set/effect");                                   // effect_command_topic
            hass_discover[F("fx_stat_t")] = F("~pub/eff_config");                              // effect_state_topic
            hass_discover[F("fx_tpl")] = F("{{ value_json.nb + ':' + value_json.name }}");     // effect_template

            hass_discover[F("json_attr_t")] = F("~pub/eff_config");                            // json_attributes_topic

            // hass_discover[F("rgb_cmd_t")] = "~rgb/set";       // rgb_command_topic
            // hass_discover[F("rgb_stat_t")] = "~rgb/status";   // rgb_state_topic

            String hass_discover_str;
            serializeJson(hass_discover, hass_discover_str);
            hass_discover.clear();

            embui.publishto(String(F("homeassistant/light/")) + name + F("/config"), hass_discover_str, true);
            return hass_discover_str;
        }
        else if (param == F("showlist"))  {
            result = F("[");
            bool first=true;
            EffectListElem *eff = nullptr;
            String effname((char *)0);
            while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
                if (eff->canBeSelected()) {
                    result = result + String(first ? F("") : F(",")) + eff->eff_nb;
                    first=false;
                }
            }
            result = result + F("]");
        }
        else if (param == F("demolist"))  {
            result = F("[");
            bool first=true;
            EffectListElem *eff = nullptr;
            String effname((char *)0);
            while ((eff = myLamp.effects.getNextEffect(eff)) != nullptr) {
                if (eff->isFavorite()) {
                    result = result + String(first ? F("") : F(",")) + eff->eff_nb;
                    first=false;
                }
            }
            result = result + F("]");
        }
        else if (param == F("effname"))  {
            String effname((char *)0);
            uint16_t effnum=String(value).toInt();
            myLamp.effects.loadeffname(effname, effnum);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
        }
        else if (param == F("effoname"))  {
            String effname((char *)0);
            uint16_t effnum=String(value).toInt();
            effname = FPSTR(T_EFFNAMEID[(uint8_t)effnum]);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
        }
        else if (param == FPSTR(TCONST_0083)) { action = RA_EFF_NEXT;  remote_action(action, value.c_str(), NULL); }
        else if (param == FPSTR(TCONST_0084)) { action = RA_EFF_PREV;  remote_action(action, value.c_str(), NULL); }
        else if (param == FPSTR(TCONST_0085)) { action = RA_EFF_RAND;  remote_action(action, value.c_str(), NULL); }
        else if (param == FPSTR(TCONST_0086)) { action = RA_REBOOT;  remote_action(action, value.c_str(), NULL); }
        else if (param == FPSTR(TCONST_0087)) { result = myLamp.isAlarm() ? "1" : "0"; }
        else if (param == FPSTR(TCONST_00C6)) { char buf[32]; sprintf_P(buf, PSTR("[%d,%d]"), WIDTH, HEIGHT);  result = buf; }
        
        embui.publish(String(FPSTR(TCONST_008B)) + param, result, true);
        return result;
    } else {
        LOG(println, F("SET"));
        if (param == FPSTR(TCONST_0070)) { action = (value!="0" ? RA_ON : RA_OFF); }
        else if (param == FPSTR(TCONST_0081)) { action = (value!="0" ? RA_OFF : RA_ON); }
        else if (param == FPSTR(TCONST_00AA)) action = RA_DEMO;
        else if (param == FPSTR(TCONST_0035)) action = RA_SEND_TEXT;
        else if (param == FPSTR(TCONST_0012)) action = RA_BRIGHT;
        else if (param == FPSTR(TCONST_0013)) action = RA_SPEED;
        else if (param == FPSTR(TCONST_0014)) action = RA_SCALE;
        else if (param == FPSTR(TCONST_0082)) action = RA_EFFECT;
        else if (param == FPSTR(TCONST_0083)) action = RA_EFF_NEXT;
        else if (param == FPSTR(TCONST_0084)) action = RA_EFF_PREV;
        else if (param == FPSTR(TCONST_0085)) action = RA_EFF_RAND;
        else if (param == FPSTR(TCONST_0086)) action = RA_REBOOT;
        else if (param == FPSTR(TCONST_0087)) action = RA_ALARM;
        else if (param == FPSTR(TCONST_00B4)) action = RA_GLOBAL_BRIGHT;
        else if (param == FPSTR(TCONST_00B7)) action = RA_WARNING;
        else if (param == FPSTR(TCONST_00C5)) action = RA_DRAW;
        else if (param == FPSTR(TCONST_00C7)) action = RA_FILLMATRIX;
        //else if (param.startsWith(FPSTR(TCONST_0015))) { action = RA_EXTRA; remote_action(action, param.c_str(), value.c_str(), NULL); return result; }
        else if (param == FPSTR(TCONST_00AE)) {
            return httpCallback(param, "", false); // set пока не реализована
        }
        else if (param == FPSTR(TCONST_00D0)) {
            String str=value;
            DynamicJsonDocument doc(256);
            deserializeJson(doc,str);
            JsonArray arr = doc.as<JsonArray>();
            uint16_t id=0;
            String val="";

            if(arr.size()<2){
                return httpCallback(param, value, false);
            }

            for (size_t i = 0; i < arr.size(); i++) {
                switch(i){
                    case 0: {
                        id = arr[i].as<uint16_t>();
                        break;
                    }
                    case 1: val = arr[i].as<String>(); break;
                    default : break;
                }
			}
            switch(id){
                case 0: {action = RA_BRIGHT; remote_action(action, val.c_str(), NULL); break;}
                case 1: {action = RA_SPEED; remote_action(action, val.c_str(), NULL); break;}
                case 2: {action = RA_SCALE; remote_action(action, val.c_str(), NULL); break;}
                default: {action = RA_EXTRA; String to=String(FPSTR(TCONST_0015))+id; remote_action(action, to.c_str(), val.c_str(), NULL); break;}
            }
            return httpCallback(param, String(id), false);
        }
        else if (param == F("effname"))  {
            String effname((char *)0);
            uint16_t effnum=String(value).toInt();
            myLamp.effects.loadeffname(effname, effnum);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
            embui.publish(String(FPSTR(TCONST_008B)) + param, result, true);
            return result;
        }
        else if (param == F("effoname"))  {
            String effname((char *)0);
            uint16_t effnum=String(value).toInt();
            effname = FPSTR(T_EFFNAMEID[(uint8_t)effnum]);
            result = String(F("["))+effnum+String(",\"")+effname+String("\"]");
            embui.publish(String(FPSTR(TCONST_008B)) + param, result, true);
            return result;
        }
#ifdef OTA
        else if (param == FPSTR(TCONST_0027)) action = RA_OTA;
#endif
#ifdef AUX_PIN
        else if (param == FPSTR(TCONST_0088)) action = RA_AUX_ON;
        else if (param == FPSTR(TCONST_0089))  action = RA_AUX_OFF;
        else if (param == FPSTR(TCONST_008A))  action = RA_AUX_TOGLE;
#endif
        remote_action(action, value.c_str(), NULL);
    }
    return result;
}

// обработка эвентов лампы
void event_worker(const EVENT *event){
    RA action = RA_UNKNOWN;
    LOG(printf_P, PSTR("%s - %s\n"), ((EVENT *)event)->getName().c_str(), embui.timeProcessor.getFormattedShortTime().c_str());

    switch (event->event) {
    case EVENT_TYPE::ON: action = RA_ON; break;
    case EVENT_TYPE::OFF: action = RA_OFF; break;
    case EVENT_TYPE::DEMO_ON: action = RA_DEMO; break;
    case EVENT_TYPE::ALARM: action = RA_ALARM; break;
    case EVENT_TYPE::LAMP_CONFIG_LOAD: action = RA_LAMP_CONFIG; break;
    case EVENT_TYPE::EFF_CONFIG_LOAD:  action = RA_EFF_CONFIG; break;
    case EVENT_TYPE::EVENTS_CONFIG_LOAD: action = RA_EVENTS_CONFIG; break;
    case EVENT_TYPE::SEND_TEXT:  action = RA_SEND_TEXT; break;
    case EVENT_TYPE::SEND_TIME:  action = RA_SEND_TIME; break;
#ifdef AUX_PIN
    case EVENT_TYPE::AUX_ON: action = RA_AUX_ON; break;
    case EVENT_TYPE::AUX_OFF: action = RA_AUX_OFF; break;
    case EVENT_TYPE::AUX_TOGGLE: action = RA_AUX_TOGLE; break;
#endif
    case EVENT_TYPE::PIN_STATE: {
        if (event->message == nullptr) break;

        String tmpS(event->message);
        tmpS.replace(F("'"),F("\"")); // так делать не красиво, но шопаделаешь...
        StaticJsonDocument<256> doc;
        deserializeJson(doc, tmpS);
        JsonArray arr = doc.as<JsonArray>();
        for (size_t i = 0; i < arr.size(); i++) {
            JsonObject item = arr[i];
            uint8_t pin = item[FPSTR(TCONST_008C)].as<int>();
            String action = item[FPSTR(TCONST_008D)].as<String>();
            pinMode(pin, OUTPUT);
            switch(action.c_str()[0]){
                case 'H':
                    digitalWrite(pin, HIGH); // LOW
                    break;
                case 'L':
                    digitalWrite(pin, LOW); // LOW
                    break;
                case 'T':
                    digitalWrite(pin, !digitalRead(pin)); // inverse
                    break;
                default:
                    break;
            }
        }
        break;
    }
    case EVENT_TYPE::SET_EFFECT: action = RA_EFFECT; break;
    case EVENT_TYPE::SET_WARNING: action = RA_WARNING; break;
    default:;
    }

    remote_action(action, event->message, NULL);
}
#ifdef ESP_USE_BUTTON
void default_buttons(){
    myButtons->clear();
    // Выключена
    myButtons->add(new Button(false, false, 1, true, BA::BA_ON)); // 1 клик - ON
    myButtons->add(new Button(false, false, 2, true, BA::BA_DEMO)); // 2 клика - Демо
    myButtons->add(new Button(false, true, 0, true, BA::BA_WHITE_LO)); // удержание Включаем белую лампу в мин яркость
    myButtons->add(new Button(false, true, 1, true, BA::BA_WHITE_HI)); // удержание + 1 клик Включаем белую лампу в полную яркость
    myButtons->add(new Button(false, true, 0, false, BA::BA_BRIGHT)); // удержание из выключенного - яркость
    myButtons->add(new Button(false, true, 1, false, BA::BA_BRIGHT)); // удержание из выключенного - яркость

    // Включена
    myButtons->add(new Button(true, false, 1, true, BA::BA_OFF)); // 1 клик - OFF
    myButtons->add(new Button(true, false, 2, true, BA::BA_EFF_NEXT)); // 2 клика - след эффект
    myButtons->add(new Button(true, false, 3, true, BA::BA_EFF_PREV)); // 3 клика - пред эффект
#ifdef OTA
    myButtons->add(new Button(true, false, 4, true, BA::BA_OTA)); // 4 клика - OTA
#endif
    myButtons->add(new Button(true, false, 5, true, BA::BA_SEND_IP)); // 5 клика - показ IP
    myButtons->add(new Button(true, false, 6, true, BA::BA_SEND_TIME)); // 6 клика - показ времени
    myButtons->add(new Button(true, false, 7, true, BA::BA_EFFECT, String(F("254")))); // 7 кликов - эффект часы
    myButtons->add(new Button(true, true, 0, false, BA::BA_BRIGHT)); // удержание яркость
    myButtons->add(new Button(true, true, 1, false, BA::BA_SPEED)); // удержание + 1 клик скорость
    myButtons->add(new Button(true, true, 2, false, BA::BA_SCALE)); // удержание + 2 клика масштаб
}
#endif

void show_progress(Interface *interf, JsonObject *data){
    if (!interf) return;
    interf->json_frame_interface();
    interf->json_section_hidden(FPSTR(TCONST_005A), String(FPSTR(TINTF_056)) + String(F(" : ")) + (*data)[FPSTR(TINTF_05A)].as<String>()+ String("%"));
    interf->json_section_end();
    interf->json_frame_flush();
}

uint8_t uploadProgress(size_t len, size_t total){
    DynamicJsonDocument doc(256);
    JsonObject obj = doc.to<JsonObject>();
    static int prev = 0;
    float part = total / 50.0;
    int curr = len / part;
    uint8_t progress = 100*len/total;
    if (curr != prev) {
        prev = curr;
        for (int i = 0; i < curr; i++) Serial.print(F("="));
        Serial.print(F("\n"));
        obj[FPSTR(TINTF_05A)] = String(progress);
        CALL_INTF_OBJ(show_progress);
    }
#ifdef VERTGAUGE
    myLamp.GaugeShow(len, total, 100);
#endif
    return progress;
}