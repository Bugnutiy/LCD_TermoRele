#include <Arduino.h>
#include <LiquidCrystal.h>
#include <GyverMenu.h>
#include <StringN.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7, 10, POSITIVE);
GyverMenu menu(16, 2);

uint8_t mode;
float SetTemp[3];

void setup()
{
  Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.setCursor(1, 0);
  lcd.print("Hello!");

  lcd.backlight();
  delay(1000);

   menu.onPrint([](const char* str, size_t len) {
        if (str) lcd.Print::write(str, len);
    });
    menu.onCursor([](uint8_t row, bool chosen, bool active) -> uint8_t {
        lcd.setCursor(0, row);
        lcd.print(chosen && !active ? '>' : ' ');
        return 1;
    });

    menu.onBuild([](gm::Builder& b) {
        if(b.Select("Mode", &mode, "Room;Aqua;Boil", [](uint8_t n, const char* str, uint8_t len) {})) b.refresh();
        b.ValueStr("ModeVal:",(String8)mode);
        
    });

    menu.refresh();
  // pinMode(10, OUTPUT);
  // put your setup code here, to run once:
}

void loop()
{
  int x = analogRead(A0);

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
