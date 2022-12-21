#include <EEPROM.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> /*Functionare display*/


#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int DS1307 = 0x68;

/***********************************************************************/
#define alimentareSenzorApa PB0   // D8
#define senzorApaPin PC0          // A0 NivelApa();
#define butSetup PD4              // D4
#define pornirePompa PD5          // D5 void FunctionarePompe();
#define butonManual_Auto PD7      // D7
#define builtinLed PB5            //builtinLed Builtin pentru eroare RTC
#define citire 0
#define scriere 1
byte ultimaCitireNivel;
byte valEEPROM = 0;
byte oraReferinta = 0;            //Ora la care vor porni Pompele
byte divizorMinut = 60;           //divizor pentru a porni pompa (%10, %20, %30)                                                                         
unsigned long tmpFunc = 0L;       // o luam din EEPROM
unsigned long idleMenu = 0L;
const byte eroareApa = 5;         // cod eroare pentru lipsa senzorului de apa
/*********************************************************************/

#define builtinLed PB5           //builtinLed Builtin pentru eroare RTC
#define pinA PD2 
#define pinB PD3 
#define butEncoder PD6

const byte  numarMeniuri = 4;
volatile byte aFlag = 0;
volatile byte bFlag = 0;
volatile byte encoderPos = 0;
volatile byte reading = 0; 
volatile boolean butonApasat = 0;
/*********************16.12.2022**************************************/
byte ora = 0;
byte minut = 0;
byte secunda = 0;
byte weekday = 0;
byte zi = 0;
byte luna = 0;
byte an = 0;
const byte coordYRandUnu = 12;
const byte coordYRandDoi = 27;
const byte coordYRandTrei = 42;
byte coordonateX[7] ={3, 25, 47, 69, 3, 25, 47};
const char* zile[] = {"Dum" ,"Lun", "Mar", "Mie", "Joi", "Vin", "Sam"};

const char menu0[] PROGMEM ={"Start Pompa"};
const char menu1[] PROGMEM ={"Setare Parametri"};
const char menu2[] PROGMEM ={"Set Data/Ora"};
const char menu3[] PROGMEM ={"EXIT"};
const char menu4[] PROGMEM ={"Ora irigat"};
const char menu5[] PROGMEM ={"Divizor minut"};
const char menu6[] PROGMEM ={"Timp Funct."};
const char menu7[] PROGMEM ={"Zile Functionare"};
const char menu8[] PROGMEM ={"Afis. Parametri"};
const char *const meniuri[] PROGMEM = {menu0, menu1, menu2, menu3, menu4, menu5, menu6, menu7, menu8};
/*********************************************************************/

//void(* resetFunc) (void) = 0;  // declare reset fuction at address 0  resetFunc(); //call reset
uint8_t OperatiiEprom(boolean , byte, byte);
uint8_t ZileFunctionare(byte );
uint8_t SetareCeas();
uint8_t BcdToDec(byte );
uint8_t DecToBcd(byte );
uint8_t SetVal(byte, byte, const char* );
uint8_t NivelApa();

bool FunctionarePompe(); 
bool FunctionarePompeManual();

void EcranOLED(byte);
void Afisare();
void MenuPrincipal();
void DisplayMenu();
void modifVal(byte );
void CitireTimp();
void ScriereTimp();


void setup() {
  DDRB  |= (1 << alimentareSenzorApa) | (1 << builtinLed);;                                                                      // D8 - OUTPUT
  PORTB |= (0 << alimentareSenzorApa) | (0 << builtinLed);                                                                     // PB4(D12) ??? LOW - Alimentaresenzor - 0V

  DDRD  |= (0 << pinA) | (0 << pinB) | (1 << pornirePompa) | (0 << butSetup) | (0 << butEncoder) | (0 << butonManual_Auto);   /*Setam D2, D3 - intr. encoder, D5 Output-uri, D4 - butSetup, D6 - butEncoder, D7 - butManual*/
  PORTD |= (1 << pornirePompa) | (1 << pinA) | (1 << pinB) | (1 << butSetup) | (1 << butEncoder);                             // PD5 HIGH - releu OFF
  
  DDRC  |= (0 << senzorApaPin);                                                                           //pinMode(senzorApaPin, INPUT);

  EICRA |= 0xF;
  PCICR |= (1 << PCIE2);
  sei();
  EIMSK  |= 0x3;
  PCMSK2 |= (1 << PCINT20) | (1 << PCINT22);

  while(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
    PORTB ^= _BV(builtinLed);
    delay(1000);
  }
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);
  display.clearDisplay();
  display.display();

  oraReferinta = OperatiiEprom(citire, 0, 0);
  divizorMinut = OperatiiEprom(citire, 1, 0);
  tmpFunc = (OperatiiEprom(citire, 2, 0) * 60000) / 60; //de ce nu functioneaza *1000 ????????
  delay(10);
}


void loop() {
  CitireTimp();
  ultimaCitireNivel = NivelApa();
  if(ultimaCitireNivel == 0 || ultimaCitireNivel == 5){
    EcranOLED(ultimaCitireNivel);
    delay(500);
  }
  FunctionarePompe();
  Afisare();
  if(butonApasat) MenuPrincipal();
 } //loop

/*  1 - start irigatie
    2 - setare parametri
    3 - constructie ...setare ceas???*/
void MenuPrincipal(){                                               //DE FACUT FUNCTIE DE EXIT TEMPORIZATA
  butonApasat = 0; 
  idleMenu = millis();
  byte lastEncoderPos = 0;
  while(1){
  if(encoderPos > numarMeniuri) encoderPos = 1;
  if(encoderPos < 1) encoderPos = numarMeniuri;
  if(lastEncoderPos != encoderPos) idleMenu = millis();
  if (lastEncoderPos == encoderPos && millis() - idleMenu > 15000 ) break;       //temporizare exit IDLE menu
  DisplayMenu();
  if(butonApasat && encoderPos == 1){
    butonApasat = 0;
    FunctionarePompeManual();
    break; 
  }
  if(butonApasat && encoderPos == 2){
    butonApasat = 0;
    while(1){
      if(encoderPos > 9) encoderPos = 5;
      if(encoderPos < 5) encoderPos = 9;
      if(lastEncoderPos != encoderPos) idleMenu = millis();
      if (lastEncoderPos == encoderPos && millis() - idleMenu > 15000 ) break;    //temporizare exit IDLE menu
      DisplayMenu();
      if(butonApasat && encoderPos == 5){   // ora referinta SetVal()
        butonApasat = 0;
        OperatiiEprom(scriere, 0 , SetVal(1, 23, "Ora irigat"));
        delay(50);
        oraReferinta = OperatiiEprom(citire, 0, 0);
        break;
      }
      if(butonApasat && encoderPos == 6){   //divizor minut
        butonApasat = 0;
        OperatiiEprom(scriere, 1 , SetVal(2, 59, "Divizor minut"));
        delay(50);
        divizorMinut = OperatiiEprom(citire, 1, 0);
        break;
      }
      if(butonApasat && encoderPos == 7){   //timp functionare, de scris in EEPROM
        butonApasat = 0;
        OperatiiEprom(scriere, 2, SetVal(2, 250, "Timp. Funct."));
        delay(50);
        tmpFunc = (OperatiiEprom(citire, 2, 0) * 60000) / 60;        
        delay(10);
        break;
      }
      if(butonApasat && encoderPos == 8){   //timp functionare, de scris in EEPROM
        butonApasat = 0;
        ZileFunctionare(0);
        break;
      }
      if(butonApasat && encoderPos == 9){   //timp functionare, de scris in EEPROM
        butonApasat = 0;
        display.clearDisplay();
        while(!butonApasat){
          display.setCursor(0, 0);
          display.print("Ora:  ");
          display.print(oraReferinta);
          display.setCursor(0, 10);
          display.print("Div min.:  ");
          display.print(divizorMinut);
          display.setCursor(0, 20);
          display.print("Funct.:  ");
          display.print(tmpFunc / 1000);
          display.print(" ");
          display.print("sec.");
      
          for (byte adresaEEPROM = 0; adresaEEPROM < 7; adresaEEPROM ++){
            if(OperatiiEprom(citire, adresaEEPROM + 3, citire) == adresaEEPROM){
              display.setCursor(0,30);
              display.print("Zi Functionare:");
              if (adresaEEPROM < 4) display.setCursor(coordonateX[adresaEEPROM], coordYRandTrei);
              if (adresaEEPROM >= 4) display.setCursor(coordonateX[adresaEEPROM], coordYRandTrei + 10);
              display.print(zile[adresaEEPROM]);
              display.print(", ");
            }
          }
          display.display();
        }
        butonApasat = 0;
        break;
      }
    lastEncoderPos = encoderPos;
    } //while menu setare parametri
    break;
  }   //if setare parammetri

    if(butonApasat && encoderPos == 3){
      butonApasat = 0;
      SetareCeas();
      delay(10);
      ScriereTimp();
      break;
    }
    if(butonApasat && encoderPos == 4){
      butonApasat = 0;
    break;
    }
    lastEncoderPos = encoderPos;
  }//while(1)
}

void DisplayMenu(){
  display.clearDisplay();
  byte coorddonataY[4] = {5, 22, 39, 55};
  const byte coordX = 10;
  char buffer[18];
  byte limitaIteratii = 0;
  if(encoderPos <= 4) limitaIteratii = 0;
  else if(encoderPos > 4 && encoderPos < 9) limitaIteratii = 4;
  else if(encoderPos == 9) limitaIteratii = 5;

  for(byte numarator = limitaIteratii; numarator < limitaIteratii + 4; numarator ++){
    if(limitaIteratii == 0){
      display.setCursor(coordX, coorddonataY[numarator]);
      strcpy_P(buffer, (char *)pgm_read_word(&(meniuri[numarator])));
      display.print(buffer);
      if(encoderPos - 1 == numarator){
        display.drawRect(coordX - 3, coorddonataY[numarator] - 4, 8 , 13, WHITE); 
      }
    }
    else if(limitaIteratii == 4){
      display.setCursor(coordX, coorddonataY[numarator - 4]);
      strcpy_P(buffer, (char *)pgm_read_word(&(meniuri[numarator])));
      display.print(buffer);
      if(encoderPos - 1 == numarator){
        display.drawRect(coordX - 3, coorddonataY[numarator - 4] - 4, 8 , 13, WHITE); 
      }
    }
    else if(limitaIteratii == 5){
      display.setCursor(coordX, coorddonataY[numarator - 5]);
      strcpy_P(buffer, (char *)pgm_read_word(&(meniuri[numarator])));
      display.print(buffer);
      if(encoderPos - 1  == numarator){
        display.drawRect(coordX - 3, coorddonataY[numarator - 5] - 4, 8 , 13, WHITE); 
      }
    }
  }
  display.display();
} 

/*operatie 1 - scriere, 0 citire
  adresa 0 - Ora irigat, 1 - Divizor minut, 2 - Timp functionare*/
uint8_t OperatiiEprom(boolean operatie, byte addr, byte val){
  if(operatie){
    display.clearDisplay();
    display.setCursor(50, 0);
    display.print(val);
    EEPROM.write(addr, val);
    return 0;
  }
  else return EEPROM.read(addr);
}

  /*
  citim toate 7 adrese din Eprom
  daca valoare == 255 sari
  le punem de la 0 la 7?  
  */
uint8_t ZileFunctionare(byte operatie){
  encoderPos = 1;
  byte zileEEPROM[7] = {254, 254, 254, 254, 254, 254, 254};
  for(byte numarator = 3; numarator < 10; numarator ++) zileEEPROM[numarator - 3] = OperatiiEprom(citire, numarator, citire); //ne amintim zilele dupa reset
  if(operatie){
    for (byte numarator = 0; numarator < 7; numarator ++) {
      if(zileEEPROM[numarator] == weekday) return true;  //pentru verificarea zilei in care irigam
    } 
    return false;
  }
  while(1){
    display.clearDisplay();
    for(byte adresaEEPROM = 4; adresaEEPROM <= 10; adresaEEPROM ++){
      display.setCursor(12,0);
      display.print("Zile Functionare:");
      if(adresaEEPROM - 4 < 4){
        display.setCursor(coordonateX[adresaEEPROM -4], 15);
        display.print(zile[adresaEEPROM - 4]);     
      }
      else{
        display.setCursor(coordonateX[adresaEEPROM -4], 30);
        display.print(zile[adresaEEPROM - 4]);
      }
    }
    display.setCursor(10,45);
    display.print("Salveaza");
    if(encoderPos > 8) encoderPos = 1;
    if(encoderPos < 1) encoderPos = 8;
    if(encoderPos - 1 < 4 && encoderPos < 8) {
      display.setCursor(coordonateX[encoderPos - 1], coordYRandUnu + 9);
      display.print("_");   
    }
    else if(encoderPos - 1 >= 4 && encoderPos < 8){
      display.setCursor(coordonateX[encoderPos -1], coordYRandDoi + 9);
      display.print("_");      
    }

    if(encoderPos == 8) display.drawRect(10, coordYRandTrei, 56, 14, WHITE);
    /*Salvam in zileEEPROM valorile*/
    if(butonApasat && encoderPos <= 7){
      butonApasat = 0;
      if(zileEEPROM[encoderPos - 1] == encoderPos - 1) zileEEPROM[encoderPos - 1 ] = 254;
      else zileEEPROM[encoderPos - 1] = encoderPos - 1;      
    }
    /*trasam "_" pe zilele selectate*/
    for(byte numarator = 0; numarator < 7; numarator ++){
      if(zileEEPROM[numarator] == numarator ) {
        if(numarator < 4) {
          display.setCursor(coordonateX[numarator] + 6, coordYRandUnu + 9);
          display.print(".");    
        }
        else{
          display.setCursor(coordonateX[numarator] + 6, coordYRandDoi + 9);
          display.print(".");
        } 
      }
    }
    display.display();
     /*Scriem zile Functionare in EEPROM addr: 3 -> 9*/  
    if(butonApasat && encoderPos == 8){
      butonApasat = 0;
      for(byte adresaEEPROM = 0; adresaEEPROM < 7; adresaEEPROM ++) OperatiiEprom(1, adresaEEPROM + 3, zileEEPROM[adresaEEPROM]);
      break;    
    }
  }//While(1)
  return false;
}

uint8_t SetareCeas(){
  while(1){
  display.clearDisplay();
  for(byte numarator = 0; numarator < 8; numarator ++){
    switch(numarator){
      case 0:
        display.setCursor(coordonateX[numarator], coordYRandUnu);
        display.print(zile[weekday]);
        break;
      case 1:
        display.setCursor(coordonateX[numarator], coordYRandUnu);
        if(zi < 10) display.print("0");
        display.print(zi);
        display.setCursor(coordonateX[numarator + 1] - 8, coordYRandUnu);
        display.write(47);
        break;
      case 2:
        display.setCursor(coordonateX[numarator], coordYRandUnu);
        if(luna < 10) display.print("0");  
        display.print(luna);
        display.setCursor(coordonateX[numarator + 1] - 8, coordYRandUnu);
        display.write(47);
        break;
      case 3:
        display.setCursor(coordonateX[numarator], coordYRandUnu);
        display.print("20");
        display.print(an);
        break;
      case 4:
        display.setCursor(coordonateX[numarator], coordYRandDoi);
        if(ora < 10) display.print("0");
        display.print(ora);
        display.setCursor(coordonateX[numarator + 1] - 8, coordYRandDoi);
        display.write(58);
        break;
      case 5:
        display.setCursor(coordonateX[numarator], coordYRandDoi);
        if(minut < 10) display.print("0");
        display.print(minut);
        display.setCursor(coordonateX[numarator + 1] - 8, coordYRandDoi);
        display.write(58);
        break;
      case 6:
        display.setCursor(coordonateX[numarator], coordYRandDoi);
        if(secunda < 10) display.print("0");
        display.print(secunda);
        break;
      case 7:
      display.setCursor(coordonateX[0], coordYRandTrei);
      display.print("Salveaza");
      break;
      }
    } //for afisare
    
    if(encoderPos > 8) encoderPos = 1;
    if(encoderPos < 1) encoderPos = 8;
    if (encoderPos == 1){
      display.drawLine(3 ,coordYRandUnu + 8, 15, coordYRandUnu + 8, WHITE);
      modifVal(encoderPos);
    }
    if (encoderPos == 2){
      display.drawLine(25 ,coordYRandUnu + 8, 33, coordYRandUnu + 8, WHITE);
      modifVal(encoderPos);
    }
    if (encoderPos == 3){
      display.drawLine(47 ,coordYRandUnu + 8, 55, coordYRandUnu + 8, WHITE);
      modifVal(encoderPos);
    }
    if (encoderPos == 4){
      display.drawLine(69 ,coordYRandUnu + 8, 92, coordYRandUnu + 8, WHITE);
      modifVal(encoderPos);
    }
    if (encoderPos == 5){
      display.drawLine(3 ,coordYRandDoi + 8, 11, coordYRandDoi + 8, WHITE);
      modifVal(encoderPos);
    }
    if (encoderPos == 6){
      display.drawLine(25 ,coordYRandDoi + 8, 33, coordYRandDoi + 8, WHITE);
      modifVal(encoderPos);
    }
    if (encoderPos == 7){
      display.drawLine(47 ,coordYRandDoi + 8, 55, coordYRandDoi + 8, WHITE);
      modifVal(encoderPos);
    }
    if (encoderPos == 8){
      display.drawRect(3 ,coordYRandTrei -3 , 50, 12, WHITE);
      if (encoderPos == 8 && butonApasat){
        butonApasat = 0;
        return true;
      }
    }
  if ((luna % 2 == 0) && luna < 7 && zi == 31) zi = 30;
  if ((luna % 2 != 0) && luna > 8 && zi == 31) zi = 30;
  if ((an % 4 == 0) && luna == 2 && zi > 29) zi = 29;
  if ((an % 4 != 0) && luna == 2 && zi > 28) zi = 28;  
  display.display();
  } //While
}//SetareCeas

void modifVal(byte coordonata){
  byte raza = 0;
    if((encoderPos == coordonata) && butonApasat){
    butonApasat = 0;
    encoderPos = 0;
    while(!butonApasat){
      switch(coordonata){
        case 1:
          raza = 7; //-1 weekday
          break;
        case 2:   // ziua
          raza = 31;
          break;
        case 3:   //luna
          raza = 12;
          break;
        case 4:   //an
          raza = 50;
          break;
        case 5:
          raza = 24; //-1 ora
          break;
        case 6:
          raza = 60; //-1 minut
          break;
        case 7:   //secunda
          raza = 49;
          break;
      }
      if(encoderPos > raza) encoderPos = 1;
      if(encoderPos < 1) encoderPos = raza;
      switch(coordonata){
        case 1:
          weekday = encoderPos -1; //-1
          break;
        case 2:
          zi = encoderPos;
          break;
        case 3:
          luna = encoderPos;
          break;
        case 4:
          an = encoderPos;
          break;
        case 5:
          ora = encoderPos - 1; //-1
          break;
        case 6:
          minut = encoderPos - 1; //-1
          break;
        case 7:
          secunda = encoderPos;
          break;
      }
      if((coordonata -1) < 4) display.setCursor(coordonateX[coordonata - 1], coordYRandUnu); //rand 1 ecran
      else display.setCursor(coordonateX[coordonata - 1], coordYRandDoi); //rand doi ecran
      display.setTextColor(BLACK, BLACK);
      if (coordonata == 1) display.print("   ");
      else if(coordonata > 1 && coordonata != 4 && coordonata < 7 ) display.print("  ");
      else if (coordonata == 4) display.print("    "); 
      display.display();
      display.setTextColor(WHITE, BLACK);
      if((coordonata - 1) < 4) display.setCursor(coordonateX[coordonata - 1], coordYRandUnu);
      else display.setCursor(coordonateX[coordonata - 1], coordYRandDoi);
      switch(coordonata){
        case 1:
        display.print(zile[weekday]);
        break;
        case 2:
        if(zi < 10) display.print("0");
        display.print(zi);
        break;
        case 3:
        if(luna < 10) display.print("0");
        display.print(luna);
        break;
        case 4:
        display.print("20");
        if (an < 10) display.print("0");
        display.print(an);
        break;
        case 5:
        if(ora < 10) display.print("0");
        display.print(ora);
        break;
        case 6:
        if(minut < 10) display.print("0");
        display.print(minut);
        break;
        case 7:
        if(secunda < 10) display.print("0");
        display.print(secunda); 
        break;      
      }
      display.display();
    }
    butonApasat = 0;
    encoderPos = coordonata;
  }
 }

void Afisare(){
  display.clearDisplay();
 /* display.setCursor(0,0);
  display.print(ZileFunctionare(1));
  display.setCursor(32,0);
  display.print(weekday);*/
  for(byte numarator = 0; numarator < 8; numarator ++){
  switch(numarator){
    case 0:
      display.setCursor(coordonateX[numarator], coordYRandUnu);
      display.print(zile[weekday]);
      break;
    case 1:
      display.setCursor(coordonateX[numarator], coordYRandUnu);
      if(zi < 10) display.print("0");
      display.print(zi);
      display.setCursor(coordonateX[numarator + 1] - 8, coordYRandUnu);
      display.write(47);
      break;
    case 2:
      display.setCursor(coordonateX[numarator], coordYRandUnu);
      if(luna < 10) display.print("0");  
      display.print(luna);
      display.setCursor(coordonateX[numarator + 1] - 8, coordYRandUnu);
      display.write(47);
      break;
    case 3:
      display.setCursor(coordonateX[numarator], coordYRandUnu);
      display.print("20");
      display.print(an);
      break;
    case 4:
      display.setCursor(coordonateX[numarator], coordYRandDoi);
      if(ora < 10) display.print("0");
      display.print(ora);
      display.setCursor(coordonateX[numarator + 1] - 8, coordYRandDoi);
      display.write(58);
      break;
    case 5:
      display.setCursor(coordonateX[numarator], coordYRandDoi);
      if(minut < 10) display.print("0");
      display.print(minut);
      display.setCursor(coordonateX[numarator + 1] - 8, coordYRandDoi);
      display.write(58);
      break;
    case 6:
      display.setCursor(coordonateX[numarator], coordYRandDoi);
      if(secunda < 10) display.print("0");
      display.print(secunda);
      break;
    case 7:
    display.setCursor(18, coordYRandTrei);
    display.setTextSize(2);
    switch(ultimaCitireNivel){
      case 0:
        display.print("APA: 5%");
        break;
      case 1:
        display.print("APA: 25%");
        break;
      case 2:
        display.print("APA: 50%");
        break;
      case 3:
        display.print("APA: 75%");
        break;
      case 4:
        display.print("APA: 100%");
        break;
    }
    break;
    }
  } //for afisare
display.display();
display.setTextSize(1);
}

void CitireTimp() {
  Wire.beginTransmission(DS1307);
  Wire.write(byte(0));
  Wire.endTransmission();
  Wire.requestFrom(DS1307, 7);
  secunda = BcdToDec(Wire.read());
  minut = BcdToDec(Wire.read());
  ora = BcdToDec(Wire.read());
  weekday = BcdToDec(Wire.read());
  if(weekday == 7) weekday = 0;
  zi = BcdToDec(Wire.read());
  luna = BcdToDec(Wire.read());
  an = BcdToDec(Wire.read());
}

void ScriereTimp(){
  Wire.beginTransmission(DS1307);
  Wire.write(byte(0));
  Wire.write(DecToBcd(secunda));
  Wire.write(DecToBcd(minut));
  Wire.write(DecToBcd(ora));
  Wire.write(DecToBcd(weekday));
  Wire.write(DecToBcd(zi));
  Wire.write(DecToBcd(luna));
  Wire.write(DecToBcd(an));
  Wire.write(byte(0));
  Wire.endTransmission();
  // Ends transmission of data */
}

/*  
  * Setam valorile "raza" pentru Ora/Data
  * 1 - 0 la raza
  * 2 - 1 la raza
  * 3 - ziua saptamanii
  * 4 - anul >=  22
  */
uint8_t SetVal(byte caz, byte raza, const char* text){
  display.clearDisplay();
  display.setTextSize(1);
  encoderPos = 0;
  switch (caz){
    case 1:
      while(!butonApasat){
        if(encoderPos > raza) encoderPos = 1;
        if(encoderPos < 1) encoderPos = raza;
        display.clearDisplay();
        display.setCursor(0,0);
        display.print(text);
        display.setCursor(50, 28);
        display.print(encoderPos - 1);
        display.display();
      }
      -- encoderPos;
      break;
      case 2:
        while(!butonApasat){
          if(encoderPos > raza) encoderPos = 1;
          if(encoderPos < 1) encoderPos = raza;
          display.clearDisplay();
          display.setCursor(0,0);
          display.print(text);
          display.setCursor(50, 28);
          display.print(encoderPos);
          display.display();
        }
        break;     
    } 
  butonApasat = 0;
  display.clearDisplay();
  return encoderPos;
}


/* 1 - daca pot sa functionez
   0 - daca nu pot sa functionez   */
bool FunctionarePompeManual(){   // 13.12.2022 
byte numarator = 0;
  if(ultimaCitireNivel == eroareApa || ultimaCitireNivel == 0) return false; // sarim mai departe in meniu
  unsigned long acum = millis();
  PORTD &= ~_BV(pornirePompa);     //PD5(pornirePompa) LOW - releu ON
  EcranOLED(3); 
  while(millis() - acum <= tmpFunc){ 
    ultimaCitireNivel = NivelApa();
    if(butonApasat){
      numarator ++;
      butonApasat = 0;
    }
    if (numarator >= 1)
    {
      butonApasat = 0;
      PORTD |= _BV(pornirePompa);
      return false;
    }
    if(ultimaCitireNivel == 0 || ultimaCitireNivel == eroareApa){
      butonApasat = 0;
      PORTD |= _BV(pornirePompa);
      return false;  // ecran principal
    }
    delay(3000);                  //Verificam nivel apa din 3 in 3 sec.????
  }
  butonApasat = 0;
  PORTD |= _BV(pornirePompa);
  return true; 		//ecran principal
}

/*Functionez daca: nivel apa != 0 si eroareApa
			 buton Automat apasat
			 oraReferinta, divizorMinut ok
			 */
bool FunctionarePompe(){   //13.12.2022
  if(!(PIND & 0x80) || ultimaCitireNivel == eroareApa || ultimaCitireNivel == 0) return false; 
  if(ora != oraReferinta || minut == 0  || minut % divizorMinut != 0) return false;
  if(!ZileFunctionare(1)) return false; 
  unsigned long durata = millis();
  PORTD &= ~_BV(pornirePompa);  //PD5 LOW - releu ON
  EcranOLED(4);
  while(millis() - durata <= tmpFunc){
    ultimaCitireNivel = NivelApa();
    if(!(PIND & 0x80) || ultimaCitireNivel == eroareApa || ultimaCitireNivel == 0) {
      PORTD |= _BV(pornirePompa);
      return true;
    }
    delay(3000);
  }
  PORTD |= _BV(pornirePompa);
  butonApasat = 0; 
  return true; 
}

/* RETURNEAZA:
  0 - 0%, 1 - 25%, 2 - 50%, 3 - 75%, 4 - 100%
  5 - Functionare eronata zenzor apa */
uint8_t NivelApa()  {
  PORTB |= (1 << alimentareSenzorApa); //bou cu TZATZE, 1/2 zi te-ai chinuit
  delay(200);
  int nivelCitit = analogRead(senzorApaPin);
  PORTB &= (0 << alimentareSenzorApa); 
  int vectVal[5] = {443, 475, 573, 702, 910};
  for(int i = 0; i < 5; i++){
    if(nivelCitit > (vectVal[i] * 0.9) && nivelCitit < (vectVal[i] * 1.1)) return i;
  }
  return eroareApa; 
}

/*  ARGUMENT
  0 - Rezervor gol
  5 - Eroare senzor apa 
  3 - Functionare manuala 
  4 - Functionare automata  */
void EcranOLED(byte codE){
  display.clearDisplay();
  switch(codE){
    case 0:    
      display.setCursor(0, 20);
      display.print("Vas GOL");
      display.setCursor(0, 40);     
      display.print("APA: 0%");
      delay(1000);
      break;
    case 5:
      display.setCursor(0, 20);
      display.print("Eroare");
      display.setCursor(0, 40);     
      display.print("senzor APA");
      delay(1000);
      break;
    case 3:
      display.setCursor(0, 20);
      display.print("Functionare manuala");
      display.setCursor(40, 40);     
      display.print(tmpFunc / 1000);
      display.print(" ");
      display.print("sec.");
      break;
    case 4:
      display.setCursor(0, 20);
      display.print("Functionare automata");
      display.setCursor(10, 40);     
      display.print(tmpFunc / 1000);
      display.print(" ");
      display.print("sec.");
      break;
  }
  display.display();
}


byte DecToBcd(byte val) {
  return ((val/10*16) + (val % 10));
}
byte BcdToDec(byte val) {
  return ((val/16*10) + (val % 16));
}

//Rotary encoder interrupt service routine for the encoder pinA
void PinA(){
  cli(); 
  reading = PIND & 0xC;
  if(reading == B00001100 && aFlag) {
    encoderPos --;
    bFlag = 0;
    aFlag = 0;
  }
  else if (reading == B00000100) bFlag = 1;
  sei();
  
}

//Rotary encoder interrupt service routine for the encoder pinB
void PinB(){
  cli();
  reading = PIND & 0xC;
  if (reading == B00001100 && bFlag) {
    encoderPos ++;
    bFlag = 0;
    aFlag = 0;
  }
  else if (reading == B00001000) aFlag = 1;
  sei();
}

ISR (INT0_vect){
  PinA();
}

ISR (INT1_vect){
  PinB();
}

ISR(PCINT2_vect){  //interrupt service routine PORTD
  cli();
  if(digitalRead(butEncoder) == LOW) butonApasat = 1;
  sei();  
}