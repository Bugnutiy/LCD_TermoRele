#include <Arduino.h>
#include <LiquidCrystal.h>
#include <GyverMenu.h>
#include <StringN.h>
#include <EEManager.h>
#include "Timer.h"

#define DEFAULT_MIN_TEMP {22, 25, 98}
#define DEFAULT_MAX_TEMP {21, 24, 95}
#define DEFAULT_TEMP_STEP {0.1, 0.1, 1}
#define DEFAULT_MIN_TEMP_MISC {18, 24, 80}
#define DEFAULT_MAX_TEMP_MISC {27, 28, 110}

#define DEFAULT_DELAY_TIME {60, 5, 10}
#define DEFAULT_SAFE_MODE {false, true, true}
#define DEFAULT_SAFE_TIME {60 * 60, 60, 60}
#define DEFAULT_SAFE_TEMP_UP_STEP {0.1, 0.1, 1}
#define DEFAULT_SAFE_TEMP_DOWN_STEP {1, 1, 1}

#define DEFAULT_AUTO_RESET {false, false, false}
#define DEFAULT_RESET_TIME {60 * 15, 60, 60}

#define DEFAULT_AUTO_STOP {false, false, true}

LiquidCrystal lcd(8, 9, 4, 5, 6, 7, 10, POSITIVE);
GyverMenu menu(16, 2);
#pragma pack(push, 1)
struct Settings
{
  uint8_t mode, backlight=255;
  float TempMax[3] = DEFAULT_MIN_TEMP,
        TempMin[3] = DEFAULT_MAX_TEMP,
        TempStep[3] = DEFAULT_TEMP_STEP,
        TempMinMisc[3] = DEFAULT_MIN_TEMP_MISC,
        TempMaxMisc[3] = DEFAULT_MAX_TEMP_MISC;
  bool DelayTimer[3] = DEFAULT_DELAY_TIME,
       SafeMode[3] = DEFAULT_SAFE_MODE,
       AutoStop[3] = DEFAULT_AUTO_STOP,
       AutoReset[3] = DEFAULT_AUTO_RESET;
  uint32_t DelayTime[3] = DEFAULT_DELAY_TIME,
           SafeTime[3] = DEFAULT_SAFE_TIME,
           ResetTime[3] = DEFAULT_RESET_TIME;
  float SafeTempUpStep[3] = DEFAULT_SAFE_TEMP_UP_STEP;
  float SafeTempDownStep[3] = DEFAULT_SAFE_TEMP_DOWN_STEP;
};
#pragma pack(pop)

Settings settings;
EEManager eeprom(settings);

void setup()
{
  eeprom.begin(0, 12);
  eeprom.setTimeout(10000);
  // Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.setCursor(1, 0);
  lcd.print("Hello!");

  // lcd.backlight();
  analogWrite(10, settings.backlight);
  delay(1000);

  menu.onPrint([](const char *str, size_t len)
               {
        if (str) lcd.Print::write(str, len); });
  menu.onCursor([](uint8_t row, bool chosen, bool active) -> uint8_t
                {
        lcd.setCursor(0, row);
        lcd.print(chosen && !active ? '>' : ' ');
        return 1; });
  menu.onBuild(
      [](gm::Builder &b)
      {
        if (b.Select("Mode", &settings.mode, "Room;Aqua;Boil", [](uint8_t n, const char *str, uint8_t len) {}))
          b.refresh();

        b.Page(
            GM_NEXT, "Temperatures",
            [](gm::Builder &b)
            {
              if (b.ValueFloat("TempMin", &settings.TempMin[settings.mode], settings.TempMinMisc[settings.mode], settings.TempMaxMisc[settings.mode] - settings.TempStep[settings.mode], settings.TempStep[settings.mode], 2, "",
                               [](float v)
                               {
                                 if (v >= settings.TempMax[settings.mode])
                                 {
                                   settings.TempMax[settings.mode] = v + settings.TempStep[settings.mode];
                                 }
                               }))
              {
                b.refresh();
              };
              if (b.ValueFloat("TempMax", &settings.TempMax[settings.mode], settings.TempMinMisc[settings.mode] + settings.TempStep[settings.mode], settings.TempMaxMisc[settings.mode], settings.TempStep[settings.mode], 2, "", [](float v)
                               {
                            if(v<=settings.TempMin[settings.mode])
                            {
                              settings.TempMin[settings.mode]=v-settings.TempStep[settings.mode];
                            } }))
              {
                b.refresh();
              };
            });
        b.Page(
            GM_NEXT,
            (String16) "Advanced " + (settings.mode == 0 ? "Room" : settings.mode == 1 ? "Aqua"
                                                                                       : "Boil"),
            [](gm::Builder &b)
            {
              b.Page(GM_NEXT, "AdvTemp",
                     [](gm::Builder &b)
                     {
                       b.ValueFloat("MinTemp", &settings.TempMinMisc[settings.mode], -200, 200, settings.TempStep[settings.mode], 2, "");
                       b.ValueFloat("MaxTemp", &settings.TempMaxMisc[settings.mode], -200, 200, settings.TempStep[settings.mode], 2, "");
                       b.ValueFloat("TempStep", &settings.TempStep[settings.mode], 0.01, 10, 0.01, 2, "");
                     });

              if (b.Switch("DelayTimer", &settings.DelayTimer[settings.mode]))
                b.refresh();
              if (settings.DelayTimer[settings.mode])
              {
                b.Page(GM_NEXT, "DelayTime",
                       [](gm::Builder &b)
                       {
                         uint8_t h, m, s;
                         h = (uint8_t)(settings.DelayTime[settings.mode] / (60 * 60));
                         m = (uint8_t)((settings.DelayTime[settings.mode] - (uint32_t)h * 60 * 60) / 60);
                         s = (uint8_t)((settings.DelayTime[settings.mode] - (uint32_t)h * 60 * 60) - (uint32_t)m * 60);
                         if (b.ValueInt("DL H", &h, (uint8_t)0, (uint8_t)255, (uint8_t)1))
                           settings.DelayTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                         if (b.ValueInt("DL M", &m, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                           settings.DelayTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                         if (b.ValueInt("DL S", &s, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                           settings.DelayTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                       });
              }

              if (b.Switch("SafeMode", &settings.SafeMode[settings.mode]))
                b.refresh();
              if (settings.SafeMode[settings.mode])
              {
                b.Page(GM_NEXT,
                       "SafeMode Set",
                       [](gm::Builder &b)
                       {
                         b.ValueFloat("T UP", &settings.SafeTempUpStep[settings.mode], 0, 100, 0.1, 2);
                         b.ValueFloat("T DOWN", &settings.SafeTempDownStep[settings.mode], 0, 100, 0.1, 2);
                         b.Page(GM_NEXT,
                                "SafeTime",
                                [](gm::Builder &b)
                                {
                                  uint8_t h, m, s;
                                  h = (uint8_t)(settings.SafeTime[settings.mode] / (60 * 60));
                                  m = (uint8_t)((settings.SafeTime[settings.mode] - (uint32_t)h * 60 * 60) / 60);
                                  s = (uint8_t)((settings.SafeTime[settings.mode] - (uint32_t)h * 60 * 60) - (uint32_t)m * 60);
                                  if (b.ValueInt("Safe H", &h, (uint8_t)0, (uint8_t)255, (uint8_t)1))
                                    settings.SafeTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                                  if (b.ValueInt("Safe M", &m, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                                    settings.SafeTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                                  if (b.ValueInt("Safe S", &s, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                                    settings.SafeTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                                });

                         if (b.Switch("AutoReset", &settings.AutoReset[settings.mode]))
                           b.refresh();
                         if (settings.AutoReset[settings.mode])
                         {
                           b.Page(GM_NEXT,
                                  "ResetTime",
                                  [](gm::Builder &b)
                                  {
                                    uint8_t h, m, s;
                                    h = (uint8_t)(settings.ResetTime[settings.mode] / (60 * 60));
                                    m = (uint8_t)((settings.ResetTime[settings.mode] - (uint32_t)h * 60 * 60) / 60);
                                    s = (uint8_t)((settings.ResetTime[settings.mode] - (uint32_t)h * 60 * 60) - (uint32_t)m * 60);
                                    if (b.ValueInt("RST H", &h, (uint8_t)0, (uint8_t)255, (uint8_t)1))
                                      settings.ResetTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                                    if (b.ValueInt("RST M", &m, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                                      settings.ResetTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                                    if (b.ValueInt("RST S", &s, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                                      settings.ResetTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                                  });
                         }
                       });
              }

              if (b.Switch("AutoStop", &settings.AutoStop[settings.mode]))
                b.refresh();
            });
        b.Page(GM_NEXT,"Screen set",
          [](gm::Builder &b)
          {
            b.ValueInt("Brightness",&settings.backlight,(uint8_t)1,(uint8_t)255,(uint8_t)1);
              // analogWrite(10,settings.backlight);
            
          });
        b.Button("Save settings", []()
                 { eeprom.update(); });
      });

  menu.refresh();
}

void loop()
{
  static uint16_t timerBackLight;

  if(eeprom.tick()){
    analogWrite(10,settings.backlight);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Settings");
    lcd.setCursor(0,1);
    lcd.print("saved!");
    delay(3000);
    timerBackLight = millis();
    menu.refresh();
  }
  unsigned int x = 0;
  x = analogRead(A0);
  TMR16(200, {
    if (x < 100)
    {
      menu.right();
      // delay(200);
    }
    else if (x < 200)
    {
      menu.up();
      // delay(200);
    }
    else if (x < 400)
    {
      menu.down();
      // delay(200);
    }
    else if (x < 600)
    {
      menu.left();
      // delay(200);
    }
    else if (x < 800)
    {
      menu.set();
      // delay(200);
    }
  });
  if (x < 1000)
  {
    analogWrite(10,settings.backlight);
    timerBackLight = millis();
  }
  else
  {
    if ((uint16_t)((uint16_t)millis() - timerBackLight) >= 10000)
    {
      lcd.noBacklight();
    }
  }
}
