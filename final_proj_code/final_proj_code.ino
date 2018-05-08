#define ENCODER_DO_NOT_USE_INTERRUPTS

#include <Adafruit_WS2801.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Encoder.h>


LiquidCrystal_I2C lcd(0x3F, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd_recipe(0x27, 16, 2);
//helpful structs
//typedef struct col clr;
struct clr {
  byte r;
  byte g;
  byte b;
};

struct rcp {
  clr colors[3];
  clr flag[3];
  bool like;
  bool bright;
  char rcpName[16];
  bool written;
};

rcp newRecipe() {
  rcp newRcp = { {stringToCol("WHITE"), stringToCol("WHITE"), stringToCol("WHITE")},
    {stringToCol("WHITE"), stringToCol("WHITE"), stringToCol("WHITE")},
    false, false, false, "              \n"
  };
  return newRcp;
}

// the pins
const int dataPin = 12;
const int clockPin = 11;
const int yesButton = 7;
const int noButton = 6;
const int encA = 8;
const int encB = 9;
const int encButton = 10;
const int prevButton = 5;
const int nextButton = 4;
const int likeButton = 2;
const int recSwitch = 3;

// strip vars
int numPix = 50;
const int f1 = 7;
const int f2 = 9;
const int f3 = 11;
const int likeLED = 13;

Encoder myEnc(encB, encA);

//encoder vars
int encoder0Pos = 0;
int encoder0PinALast = LOW;
int n = LOW;

//mode bools
bool rec = false; //recording mode

//lcd and enc vars
unsigned long timer;

int lastButtonPressed = 0;

//counters
int offIndex = 0;
int writeCount = 0;
int dispCount = 0;
int writeOffset = 38;

//other global vars
rcp currRcp = newRecipe();

//sensors stuff
Adafruit_WS2801 strip = Adafruit_WS2801(numPix, dataPin, clockPin);


void setup() {
  //set up the pins
  pinMode(dataPin, INPUT);
  pinMode(clockPin, INPUT);
  pinMode(yesButton, INPUT);
  pinMode(noButton, INPUT);
  pinMode(prevButton, INPUT);
  pinMode(nextButton, INPUT);
  pinMode(likeButton, INPUT);
  pinMode(encButton, INPUT);
  pinMode(recSwitch, INPUT);
  pinMode(encA, INPUT_PULLUP);
  pinMode(encB, INPUT_PULLUP);

  rcp getFromMem;
  while (EEPROM.get(writeCount, getFromMem).written == true) {writeCount += writeOffset;}

  strip.begin();  // initialize the strip
  // make sure it is visible
  //strip.clear();  // Initialize all pixels to 'off'

  for (int i = 0; i < numPix; i++) strip.setPixelColor(i, 0);
  strip.show(); 
  
  lcd.init();
  lcd.backlight();
  lcd_recipe.init();
  lcd_recipe.backlight();
  lcd.setCursor(0, 0);
  lcd_recipe.setCursor(0, 0);
  lcd.print("controller");
  lcd.print("base");

  Serial.begin(9600);
}

uint32_t Color(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}

uint32_t getColor(int i, clr c1, clr c2, clr c3, bool bright) {
  byte r = 0;
  byte g = 0;
  byte b = 0;
  if (i % 3 == 0) {
    r = c1.r;
    g = c1.g;
    b = c1.b;
  }
  else if (i % 2 == 0) {
    r = c2.r;
    g = c2.g;
    b = c2.b;
  }
  else {
    r = c3.r;
    g = c3.g;
    b = c3.b;
  }
  if (bright == false) return Color(r / 2, g / 2, b / 2);
  else return Color(r, g, b);
}

void recordingLoop() {
  lcd_recipe.off();
  lcd_recipe.noBacklight();
  lcd.on();
  lcd.backlight();
  lcd.setCursor(0,0);
  if (rec == false) {
    for (int i = 0; i < numPix; i++) {strip.setPixelColor(i, 0); }
    strip.show();
    Serial.println("just switched to rec");
    //reset all the global vars
    String inputs[4] = {"", "", "", ""};
    getInputs(inputs);
    String cuis = inputs[0];
    String temp = inputs[1];
    String flavor = inputs[2];
    String weight = inputs[3];
    inputsToTree(temp, flavor, currRcp.colors);
    cuisToFlag(cuis, currRcp.flag);
    currRcp.like = false;
    if (weight == "HEAVY") currRcp.bright = true;
    else currRcp.bright = false;
    rec = true;
    offIndex = 0;
  }
  else {
    Serial.println("already in rec");
    colorsLoop(currRcp.colors, currRcp.bright);
    showFlag(currRcp.flag);
  }
}

void colorsLoop(clr colors[3], bool bright) {
  for ( int i = 0; i < numPix; i++ ) {
    int j = (i + offIndex) % 50;
    if (i % 2 == 0) {
      strip.setPixelColor(i, getColor(j / 2, colors[0], colors[1], colors[2], bright));
    }
    else if (i != 7 && i != 9 && i != 11 && i != 13) strip.setPixelColor(i, Color(0, 0, 0));
  }
  offIndex = (offIndex + 1) % 50;
  //strip.show();
  delay(300);
}

void displayLoop() {
  lcd.off();
  lcd.noBacklight();
  lcd_recipe.on();
  lcd_recipe.backlight();
  lcd_recipe.setCursor(0,0);
  if (rec == true) { //just switched to display mode
    Serial.println("just switched to display");
    lcd_recipe.clear();
    getRecipe(currRcp.rcpName);
    Serial.println("got recipe");
    //save recipe to EPPROM
    rcp getFromMem;
    while (EEPROM.get(writeCount, getFromMem).written == true) {writeCount += writeOffset;}
    currRcp.written = true;
    EEPROM.put(writeCount, currRcp);
    writeCount += writeOffset;
    //display most recent
    offIndex = 0;
    displayRecipe(dispCount, currRcp.like);
    dispCount += 1;
    //reset all the global vars
    rec = false;
  }
  else { //already in display mode
    Serial.println("already in display");
    rcp getFromMem;
    if (EEPROM.get(0, getFromMem).written == false) return;
    //check for read of next/prev buttons
    getDisplayButtons();
    //check if "liked" & light up LED if so
    lcd_recipe.clear();
    displayRecipe(dispCount, digitalRead(likeButton));
  }
}

void loop() {
  if (digitalRead(recSwitch) == 0) {
    recordingLoop();
  }
  else {
    displayLoop();
  }
}

void getDisplayButtons() {
  if (buttonP2U(nextButton)) {
    if (((dispCount + 1) * writeOffset) >= writeCount) return;
    else {
      dispCount++;
      
    }
  }
  else if (buttonP2U(prevButton)) {
    if (dispCount == 0) return;
    else {
      dispCount--;
    }
  }
}

void displayRecipe(int dispCount, bool liked) {
  unsigned int address = dispCount * writeOffset;
  rcp disp;
  if (liked) {
    //disp.like = true;
    Serial.println(address + 18);
    EEPROM.update(address + 18, true);
    Serial.println("test");
//    Serial.println(EEPROM.get(address + 37, test));
    Serial.println(disp.like);
  }
  EEPROM.get(address, disp);
  lcd_recipe.clear();
  lcd_recipe.backlight();
  lcd_recipe.setCursor(0,0);
  lcd_recipe.print(disp.rcpName);
  colorsLoop(disp.colors, disp.bright);
  showFlag(disp.flag);
  if (disp.like == true || liked == true) {
    strip.setPixelColor(likeLED, Color(255, 0, 0));
  }
  else strip.setPixelColor(likeLED, 0);
}

void showFlag(clr flag[3]) {
  strip.setPixelColor(f1, Color(flag[0].r, flag[0].g, flag[0].b));
  strip.setPixelColor(f2, Color(flag[1].r, flag[1].g, flag[1].b));
  strip.setPixelColor(f3, Color(flag[2].r, flag[2].g, flag[2].b));
  strip.show();
}

void getRecipe(char output[16]) {
  long position  = -999;
  char curr = '@';
  char space = ' ';
  int cursorPos = 0;
  int ctr = 0;
  while (!digitalRead(7) && ctr < 16) {
    long newPos = myEnc.read() / 4;
    if (newPos != position) {
      if (newPos > position) curr = getLetter(true, curr);//, cursorPos);
      else if (newPos < position) curr = getLetter(false, curr);//, cursorPos);
      position = newPos;
      Serial.println((curr == '@') ? space : curr);
      Serial.println(position);
      lcd_recipe.setCursor(cursorPos, 0);
      lcd_recipe.print((curr == '@') ? space : curr);
    }
    //bool btn = ());
    //Serial.println(btn);
    if (buttonP2UEnc(10) && ctr < 16) {
      output[ctr] = ((curr == '@') ? space : curr);
      cursorPos++;
      ctr++;
      lcd_recipe.setCursor(cursorPos, 0);
      lcd_recipe.print((curr == '@') ? space : curr);
    }
  }
}

char getLetter(bool back, char curr) {
  if (curr == '@' && back) return curr;
  if (curr > '@' && back) return curr - 1;
  if (curr == 'Z' && !back) return curr;
  if (curr < 'Z' && !back) return curr + 1;
  return curr;
}

void getInputs(String outputs[4]) {
  sendToLCD00(lcd, "What cuisine?");
  outputs[0] = askCuisine();
  Serial.println(outputs[0]);
  sendToLCD00(lcd, "Hot or cold?");
  outputs[1] = askTemp();
  Serial.println(outputs[1]);
  sendToLCD00(lcd, "What flavor?");
  outputs[2] = askFlavor();
  Serial.println(outputs[2]);
  sendToLCD00(lcd, "Heavy or light?");
  outputs[3] = askHeavy();
  Serial.println(outputs[3]);
  Serial.println("finished getting inputs");
}

// button pressed to unpressed
bool buttonP2U (int buttonPin) {
  // button is pressed, set global variable
  if (digitalRead(buttonPin)) {
    // loop forever for at least 3 ms
    timer = millis();
    while (millis() - timer < 300) {
      // if mode is switched, exit this loop and return false
      // not implemented currently
    }
    // loop while button is still pressed
    while (digitalRead(buttonPin)) {
      // if mode is switched, exit this loop and return false
      // not implemented currently
    }
    Serial.println("unpressed!");
    delay(300);
    lastButtonPressed = buttonPin;
    return true;
  }
  return false;
}

// button pressed to unpressed
bool buttonP2UEnc (int buttonPin) {
  // button is pressed, set global variable
  if (digitalRead(buttonPin) == 0) {
    // loop forever for at least 3 ms
    timer = millis();
    while (millis() - timer < 300) {
      // if mode is switched, exit this loop and return false
      // not implemented currently
    }
    // loop while button is still pressed
    while (digitalRead(buttonPin) == 0) {
      // if mode is switched, exit this loop and return false
      // not implemented currently
    }
    Serial.println("unpressed!");
    delay(300);
    lastButtonPressed = buttonPin;
    return true;
  }
  return false;
}

void sendToLCD01 (LiquidCrystal_I2C lcdDisp, String s) {
  lcdDisp.setCursor(0, 1);
  lcdDisp.print("                ");
  lcdDisp.setCursor(0, 1);
  lcdDisp.print(s);
}

void sendToLCD00 (LiquidCrystal_I2C lcdDisp, String s) {
  lcdDisp.setCursor(0, 0);
  lcdDisp.print("                ");
  lcdDisp.setCursor(0, 0);
  lcdDisp.print(s);
}

String askCuisine()
{
  sendToLCD01(lcd, "ITALIAN?");
  while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

  if (lastButtonPressed == yesButton) {
    sendToLCD01(lcd, "ITALIAN!");
    delay(1000);
    return "ITALIAN";
  } else if (lastButtonPressed == noButton) {
    sendToLCD01(lcd, "INDIAN?");
    while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

    if (lastButtonPressed == yesButton) {
      sendToLCD01(lcd, "INDIAN!");
      delay(1000);
      return "INDIAN";
    } else if (lastButtonPressed == noButton) {
      sendToLCD01(lcd, "AMERICAN?");
      while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

      if (lastButtonPressed == yesButton) {
        sendToLCD01(lcd, "AMERICAN!");
        delay(1000);
        return "AMERICAN";
      } else if (lastButtonPressed == noButton) {
        sendToLCD01(lcd, "ASIAN?");
        while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

        if (lastButtonPressed == yesButton) {
          sendToLCD01(lcd, "ASIAN!");
          delay(1000);
          return "ASIAN";
        } else if (lastButtonPressed == noButton) {
          sendToLCD01(lcd, "OTHER?");
          while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

          if (lastButtonPressed == yesButton) {
            sendToLCD01(lcd, "OTHER!");
            delay(1000);
            return "OTHER";
          } else if (lastButtonPressed == noButton) {
            // re-enter the function
            return askCuisine();
          }
        }
      }
    }
  }
  // hopefully, it doesn't get to this point
  return "OTHER";
}

String askTemp()
{
  sendToLCD01(lcd, "HOT?");
  while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

  if (lastButtonPressed == yesButton) {
    sendToLCD01(lcd, "HOT!");
    delay(1000);
    return "HOT";
  } else if (lastButtonPressed == noButton) {
    sendToLCD01(lcd, "COLD?");
    while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

    if (lastButtonPressed == yesButton) {
      sendToLCD01(lcd, "COLD!");
      delay(1000);
      return "COLD";
    } else if (lastButtonPressed == noButton) {
      return askTemp();
    }
  }
  // hopefully, it doesn't get to this point
  return "COLD";
}

String askFlavor()
{
  sendToLCD01(lcd, "SPICY?");
  while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

  if (lastButtonPressed == yesButton) {
    sendToLCD01(lcd, "SPICY!");
    delay(1000);
    return "SPICY";
  } else if (lastButtonPressed == noButton) {
    sendToLCD01(lcd, "SOUR?");
    while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

    if (lastButtonPressed == yesButton) {
      sendToLCD01(lcd, "SOUR!");
      delay(1000);
      return "SOUR";
    } else if (lastButtonPressed == noButton) {
      sendToLCD01(lcd, "SWEET?");
      while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

      if (lastButtonPressed == yesButton) {
        sendToLCD01(lcd, "SWEET!");
        delay(1000);
        return "SWEET";
      } else if (lastButtonPressed == noButton) {
        sendToLCD01(lcd, "BITTER?");
        while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

        if (lastButtonPressed == yesButton) {
          sendToLCD01(lcd, "BITTER!");
          delay(1000);
          return "BITTER";
        } else if (lastButtonPressed == noButton) {
          sendToLCD01(lcd, "SAVORY?");
          while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

          if (lastButtonPressed == yesButton) {
            sendToLCD01(lcd, "SAVORY!");
            delay(1000);
            return "SAVORY";
          } else if (lastButtonPressed == noButton) {
            // re-enter the function
            return askFlavor();
          }
        }
      }
    }
  }
  // hopefully, it doesn't get to this point
  return "SWEET";
}

String askHeavy()
{
  sendToLCD01(lcd, "HEAVY?");
  while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

  if (lastButtonPressed == yesButton) {
    sendToLCD01(lcd, "HEAVY!");
    delay(1000);
    return "HEAVY";
  } else if (lastButtonPressed == noButton) {
    sendToLCD01(lcd, "LIGHT?");
    while (!buttonP2U(noButton) && !buttonP2U(yesButton)) {};

    if (lastButtonPressed == yesButton) {
      sendToLCD01(lcd, "LIGHT!");
      delay(1000);
      return "LIGHT";
    } else if (lastButtonPressed == noButton) {
      return askHeavy();
    }
  }
  // hopefully, it doesn't get to this point
  return "HEAVY";
}



void cuisToFlag(String cuis, clr out[3]) {
  if (cuis == "ITALIAN") {
    out[0] = stringToCol("GREEN");
    out[1] = stringToCol("WHITE");
    out[2] = stringToCol("RED");
  }
  else if (cuis == "INDIAN") {
    out[0] = stringToCol("GREEN");
    out[1] = stringToCol("WHITE");
    out[2] = stringToCol("ORANGE");
  }
  else if (cuis == "AMERICAN") {
    out[0] = stringToCol("RED");
    out[1] = stringToCol("WHITE");
    out[2] = stringToCol("BLUE");
  }
  else if (cuis == "ASIAN") {
    out[0] = stringToCol("RED");
    out[1] = stringToCol("WHITE");
    out[2] = stringToCol("YELLOW");
  }
  else {
    out[0] = stringToCol("OFF");
    out[1] = stringToCol("OFF");
    out[2] = stringToCol("OFF");
  }

}

clr stringToCol (String cstring) {
  clr col;
  if (cstring == "RED")
  {
    col.r = 0xFF;
    col.g = 0x00;
    col.b = 0x00;
  }
  else if (cstring == "ORANGE")
  {
    // not sure if LED will actually produce ORANGE
    col.r = 0x8F;
    col.g = 0x15;
    col.b = 0x00;
  }
  else if (cstring == "YELLOW")
  {
    // not sure if LED will actually produce YELLOW
    col.r = 0xFF;
    col.g = 0x7F;
    col.b = 0x00;
  }
  else if (cstring == "GREEN")
  {
    col.r = 0x00;
    col.g = 0xFF;
    col.b = 0x00;
  }
  else if (cstring == "BLUE")
  {
    // actually its LIGHT PINK
    col.r = 0x00;
    col.g = 0x00;
    col.b = 0xFF;
  }
  else if (cstring == "CYAN")
  {
    col.r = 0x00;
    col.g = 0xFF;
    col.b = 0xFF;
  }
  else if (cstring == "PURPLE")
  {
    // actually its DARK VIOLET
    col.r = 0x80;
    col.g = 0x00;
    col.b = 0xFF;
  }
  else if (cstring == "PINK")
  {
    // actually its LIGHT PINK
    col.r = 0xFF;
    col.g = 0x00;
    col.b = 0xFF;
  }
  else if (cstring == "WHITE")
  {
    col.r = 0xFF;
    col.g = 0xFF;
    col.b = 0xFF;
  }
  else
  {
    col.r = 0x00;
    col.g = 0x00;
    col.b = 0x00;
  }
  return col;
}

void inputsToTree(String input2, String input3, clr colors[3])
{
  if (input2 == "HOT") {
    colors[0] = stringToCol("RED");
  }
  else
    // input2 == "COLD"
  {
    colors[0] = stringToCol("CYAN");
  }
  if (input3  == "SPICY")
  {
    colors[1] = stringToCol("ORANGE");
    colors[2] = stringToCol("RED");
  }
  else if (input3 == "SOUR")
  {
    colors[1] = stringToCol("YELLOW");
    colors[2] = stringToCol("GREEN");
  }
  else if (input3 == "SWEET")
  {
    colors[1] = stringToCol("PINK");
    colors[2] = stringToCol("WHITE");
  }
  else if (input3 == "BITTER")
  {
    colors[1] = stringToCol("GREEN");
    colors[2] = stringToCol("PURPLE");
  }
  else // input3 == "SAVORY"
  {
    colors[1] = stringToCol("ORANGE");
    colors[2] = stringToCol("YELLOW");
  }
  return colors;
}


