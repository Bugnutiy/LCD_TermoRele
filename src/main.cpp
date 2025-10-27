#include <Arduino.h>
#include <LiquidCrystal.h>
#include <GyverMenu.h>
#include <StringN.h>
#include <EEManager.h>

#define DEFAULT_MIN_TEMP {22, 25, 98}
#define DEFAULT_MAX_TEMP {21, 24, 95}
#define DEFAULT_TEMP_STEP {0.1, 0.1, 5}
#define DEFAULT_MIN_TEMP_MISC {18, 24, 80}
#define DEFAULT_MAX_TEMP_MISC {27, 28, 110}

LiquidCrystal lcd(8, 9, 4, 5, 6, 7, 10, POSITIVE);
GyverMenu menu(16, 2);

struct Settings
{
  uint8_t mode;
  float TempMax[3] = DEFAULT_MIN_TEMP, TempMin[3] = DEFAULT_MAX_TEMP, TempStep[3] = DEFAULT_TEMP_STEP, TempMinMisc[3] = DEFAULT_MIN_TEMP_MISC, TempMaxMisc[3] = DEFAULT_MAX_TEMP_MISC;
};

Settings settings;
EEManager eeprom(settings);

void setup()
{
  // eeprom.begin(0, 78);
  // Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.setCursor(1, 0);
  lcd.print("Hello!");

  // lcd.backlight();
  digitalWrite(10, HIGH);
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
              if (b.ValueFloat("TempMin", &settings.TempMin[settings.mode], settings.TempMinMisc[settings.mode], settings.TempMaxMisc[settings.mode]-settings.TempStep[settings.mode], settings.TempStep[settings.mode], 2, "",
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
              if (b.ValueFloat("TempMax", &settings.TempMax[settings.mode], settings.TempMinMisc[settings.mode]+settings.TempStep[settings.mode], settings.TempMaxMisc[settings.mode], settings.TempStep[settings.mode], 2, "", [](float v)
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
            (String16)"Advanced " + (settings.mode==0?"Room":settings.mode==1?"Aqua":"Boil"),
            [](gm::Builder &b)
            {
              b.ValueFloat("MinTemp",&settings.TempMinMisc[settings.mode],-200,200,settings.TempStep[settings.mode],2,"");
              b.ValueFloat("MaxTemp",&settings.TempMaxMisc[settings.mode],-200,200,settings.TempStep[settings.mode],2,"");
              b.ValueFloat("TempStep",&settings.TempStep[settings.mode],0.01,10,0.01,2,"");
            }
          );
      });

  menu.refresh();
  // pinMode(10, OUTPUT);
  // put your setup code here, to run once:
}

void loop()
{
  unsigned int x = 0;
  x = analogRead(A0);
  if (x < 100)
  {
    menu.right();
    delay(200);
  }
  else if (x < 200)
  {
    menu.up();
    delay(200);
  }
  else if (x < 400)
  {
    menu.down();
    delay(200);
  }
  else if (x < 600)
  {
    menu.left();
    delay(200);
  }
  else if (x < 800)
  {
    menu.set();
    delay(200);
  }
}
