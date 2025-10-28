#include <Arduino.h>
#include <LiquidCrystal.h>
#include <GyverMenu.h>
#include <StringN.h>
#include <EEManager.h>
#include <GyverDS18Single.h>
#include "Timer.h"
// #define MY_DEBUG
#include "My_Debug.h"
#include "Relay.h"

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
#define DEFAULT_BACKLIGHT_TIME {15, 60 * 15, 60 * 15}
#define DEFAULT_STANDBY_TIME {5, 5, 5}

LiquidCrystal lcd(8, 9, 4, 5, 6, 7, 10, POSITIVE); // Свободные : (0,1) 2,3 ,11,12,13
GyverMenu menu(16, 2);
#pragma pack(push, 1)
struct Settings
{
  uint8_t mode, backlight = 100;
  float TempMax[3] = DEFAULT_MIN_TEMP,
        TempMin[3] = DEFAULT_MAX_TEMP,
        TempStep[3] = DEFAULT_TEMP_STEP,
        TempMinMisc[3] = DEFAULT_MIN_TEMP_MISC,
        TempMaxMisc[3] = DEFAULT_MAX_TEMP_MISC;
  bool DelayTimer[3] = DEFAULT_DELAY_TIME,
       SafeMode[3] = DEFAULT_SAFE_MODE,
       AutoStop[3] = DEFAULT_AUTO_STOP,
       AutoReset[3] = DEFAULT_AUTO_RESET;
  uint8_t AutoStopCounter[3] = {1, 1, 1};
  uint32_t DelayTime[3] = DEFAULT_DELAY_TIME,
           SafeTime[3] = DEFAULT_SAFE_TIME,
           ResetTime[3] = DEFAULT_RESET_TIME,
           BacklightTime[3] = DEFAULT_BACKLIGHT_TIME,
           StandbyScreenTime[3] = DEFAULT_STANDBY_TIME;
  float SafeTempUpStep[3] = DEFAULT_SAFE_TEMP_UP_STEP;
  float SafeTempDownStep[3] = DEFAULT_SAFE_TEMP_DOWN_STEP;
};
#pragma pack(pop)

Settings settings;
EEManager eeprom(settings);
GyverDS18Single ds(11);
Relay relay(12, 0);
float cTemperature = 0, pTemperature = 0;

uint32_t timerBackLight, timerStandBy;
uint8_t StopCounter;
uint64_t safeTimer = 0, resetTimer = 0;
bool standbyFlag = 0, workFlag = 0, safeFlag = 0, stopFlag = 0;
void setup()
{
  ds.requestTemp();

  DEBUG_INIT
  eeprom.begin(0, 121);
  eeprom.setTimeout(1000);
  relay.setMinChangeTime(settings.DelayTime[settings.mode] * 1000);
  relay.resetTimer(1);
  // Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.setCursor(1, 0);
  lcd.print("Hello!");
  // lcd.backlight();
  analogWrite(10, settings.backlight);
  // while (!ds.ready())
  {
  }
  if (ds.readTemp())
  {
    cTemperature = ds.getTemp();
    pTemperature = cTemperature;
    ds.requestTemp();
  }
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
              {
                b.refresh();
                if (settings.DelayTimer[settings.mode])
                {
                  relay.setMinChangeTime(settings.DelayTime[settings.mode] * 1000);
                }
                else
                {
                  relay.setMinChangeTime(0);
                }
              }
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
                         {
                           settings.DelayTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                           relay.setMinChangeTime(settings.DelayTime[settings.mode] * 1000);
                         }

                         if (b.ValueInt("DL M", &m, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                         {
                           settings.DelayTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                           relay.setMinChangeTime(settings.DelayTime[settings.mode] * 1000);
                         }

                         if (b.ValueInt("DL S", &s, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                         {
                           settings.DelayTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                           relay.setMinChangeTime(settings.DelayTime[settings.mode] * 1000);
                         }
                       });
              }

              if (b.Switch("SafeMode", &settings.SafeMode[settings.mode]))
              {
                b.refresh();
              }
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
              else
              {
                safeFlag = 0;
              }

              if (b.Switch("AutoStop", &settings.AutoStop[settings.mode]))
                b.refresh();
              if (settings.AutoStop[settings.mode])
              {
                b.ValueInt("A-S Counter", &settings.AutoStopCounter[settings.mode], (uint8_t)1, (uint8_t)255, (uint8_t)1);
              }
            });
        b.Page(GM_NEXT, "Screen set",
               [](gm::Builder &b)
               {
                 b.ValueInt("Brightness", &settings.backlight, (uint8_t)1, (uint8_t)255, (uint8_t)1);
                 b.Page(GM_NEXT,
                        "BacklightTime",
                        [](gm::Builder &b)
                        {
                          uint8_t h, m, s;
                          h = (uint8_t)(settings.BacklightTime[settings.mode] / (60 * 60));
                          m = (uint8_t)((settings.BacklightTime[settings.mode] - (uint32_t)h * 60 * 60) / 60);
                          s = (uint8_t)((settings.BacklightTime[settings.mode] - (uint32_t)h * 60 * 60) - (uint32_t)m * 60);
                          if (b.ValueInt("H", &h, (uint8_t)0, (uint8_t)255, (uint8_t)1))
                            settings.BacklightTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                          if (b.ValueInt("M", &m, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                            settings.BacklightTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                          if (b.ValueInt("S", &s, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                            settings.BacklightTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                        });
                 b.Page(GM_NEXT,
                        "StandbyTime",
                        [](gm::Builder &b)
                        {
                          uint8_t h, m, s;
                          h = (uint8_t)(settings.StandbyScreenTime[settings.mode] / (60 * 60));
                          m = (uint8_t)((settings.StandbyScreenTime[settings.mode] - (uint32_t)h * 60 * 60) / 60);
                          s = (uint8_t)((settings.StandbyScreenTime[settings.mode] - (uint32_t)h * 60 * 60) - (uint32_t)m * 60);
                          if (b.ValueInt("H", &h, (uint8_t)0, (uint8_t)255, (uint8_t)1))
                            settings.StandbyScreenTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                          if (b.ValueInt("M", &m, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                            settings.StandbyScreenTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;

                          if (b.ValueInt("S", &s, (uint8_t)0, (uint8_t)59, (uint8_t)1))
                            settings.StandbyScreenTime[settings.mode] = 60 * 60 * (uint32_t)h + 60 * (uint32_t)m + s;
                        });
               });
        b.Button("Save settings",
                 []()
                 {
                   eeprom.update();
                   lcd.setCursor(0, 0);
                   lcd.print("                ");
                   lcd.setCursor(0, 0);
                   lcd.print("Saving...");
                   while (!eeprom.tick())
                   {
                     /* code */
                   }
                   lcd.clear();
                   lcd.setCursor(0, 0);
                   lcd.print("Settings");
                   lcd.setCursor(0, 1);
                   lcd.print("saved!");
                   delay(2000);
                   menu.refresh();
                 });
      });

  menu.refresh();
}

void blinkSymbol(uint8_t x, uint8_t y, const char *s, bool nstate)
{
  lcd.setCursor(x, y);
  if (nstate)
  {
    lcd.print(s);
  }
  else
  {
    lcd.print(" ");
  }
}
void loop()
{

  // читаем температуру
  TMR16(500, {
    if (ds.ready())
    {
      if (ds.readTemp())
      {
        cTemperature = ds.getTemp();
      }
      ds.requestTemp();
    }
  });
  // Читаем кнопки
  unsigned int x = 0;
  x = analogRead(A0);
  if (x < 800)
  {
    DD(x, 100);

    TMR16(200, {
      if (x < 50)
      {
        menu.right();
        // delay(200);
      }
      else if (x < 200)
      {
        menu.up();
        // delay(200);
      }
      else if (x < 300)
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
    analogWrite(10, settings.backlight);
    timerBackLight = millis();
    timerStandBy = millis();
    standbyFlag = 0;
    menu.refresh();
  }
  else // Если кнопки не нажаты
  {
    if ((uint32_t)((uint32_t)millis() - timerBackLight) >= settings.BacklightTime[settings.mode] * (uint32_t)1000)
    {
      lcd.noBacklight();
    }
    if (((uint32_t)((uint32_t)millis() - timerStandBy) >= settings.StandbyScreenTime[settings.mode] * (uint32_t)1000) and settings.StandbyScreenTime[settings.mode] and !standbyFlag)
    {
      standbyFlag = 1;
      lcd.clear();
    }
  }

  // Обрабатываем температуру и настройки...
  if (cTemperature < settings.TempMin[settings.mode])
  {
    workFlag = 1;
  }
  if (cTemperature >= settings.TempMax[settings.mode])
  {
    workFlag = 0;
    StopCounter++;
  }
  if (settings.SafeMode[settings.mode]) // если включен safe mode
  {

    if (relay.getState() and !safeFlag)
    {
      // если температура упала во время нагрева.
      if (pTemperature - cTemperature > settings.SafeTempDownStep[settings.mode])
      {
        safeFlag = 1;
        relay.setNow(0);
        relay.resetTimer();
        resetTimer = millis();
      }
      // если температура не нагрелась за определенное время
      if (((millis() - safeTimer) > (settings.SafeTime[settings.mode] * 1000)) and !safeFlag)
      {
        if (cTemperature - pTemperature < settings.SafeTempUpStep[settings.mode])
        {
          safeFlag = 1;
          relay.setNow(0);
          resetTimer = millis();
        }
        else
        {
          pTemperature = cTemperature;
          safeTimer = millis();
        }
      }
    }
  }
  if (settings.AutoReset[settings.mode]) // Reset
  {
    if (safeFlag and ((millis() - resetTimer) > (settings.ResetTime[settings.mode] * 1000)))
    {
      pTemperature = cTemperature;
      safeFlag = 0;
      safeTimer = millis();
    }
  }
  if (settings.AutoStop[settings.mode])
  {
    if (StopCounter >= settings.AutoStopCounter[settings.mode])
    {
      stopFlag = 1;
    }
  }

  if (relay.ready())
  {
    if (relay.set(workFlag and !safeFlag and !stopFlag))
    {
      safeTimer = millis();
    }
  }
  // Вывод standby инфы на экран
  if (standbyFlag)
  {
    TMR16(1000, {
      static bool wfs;

      lcd.setCursor(0, 0);
      lcd.print("T: ");
      lcd.print(cTemperature);
      lcd.print("/");
      lcd.print(settings.TempMax[settings.mode]);

      if (workFlag)
      {
        if (relay.getState())
        {
          lcd.setCursor(15, 0);
          lcd.print(">");
        }
        else
        {
          blinkSymbol(15, 0, ">", wfs);
        }
      }
      else
      {
        if (relay.getState())
        {
          blinkSymbol(15, 0, "<", wfs);
        }
        else
        {
          lcd.setCursor(15, 0);
          lcd.print("<");
        }
      }
      lcd.setCursor(0, 1);
      lcd.print("D:");
      if (!relay.ready())
      {
        blinkSymbol(2, 1, settings.DelayTimer[settings.mode] ? "+" : "-", wfs);
      }
      else
      {
        lcd.setCursor(2, 1);
        lcd.print(settings.DelayTimer[settings.mode] ? "+" : "-");
      }
      lcd.print(" S:");
      if (safeFlag)
      {
        blinkSymbol(6, 1, settings.SafeMode[settings.mode] ? "+" : "-", wfs);
      }
      else
      {
        lcd.setCursor(6, 1);
        lcd.print(settings.SafeMode[settings.mode] ? "+" : "-");
      }
      lcd.print(" R:");
      if (safeFlag and settings.AutoReset[settings.mode])
      {
        blinkSymbol(10, 1, settings.AutoReset[settings.mode] ? "+" : "-", wfs);
      }
      else
      {
        lcd.setCursor(10, 1);
        lcd.print(settings.AutoReset[settings.mode] ? "+" : "-");
      }
      lcd.print(" A:");
      if (settings.AutoStop[settings.mode])
      {
        if (settings.AutoStopCounter[settings.mode] - StopCounter < 100)
          lcd.print(settings.AutoStopCounter[settings.mode] - StopCounter);
        else
          lcd.print("+");
      }
      else
      {
        lcd.print("-");
      }
      wfs = !wfs;
    });
  }
}
