//Načtení všech potřebných knihoven
#include <WiFi.h>                   
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>             
#include <HTTPClient.h>             
#include <cbuf.h>
#include <SPI.h>
#include <Arduino.h>
#include <Preferences.h>
#include "web_content.h"
#include <LiquidCrystal_I2C.h>      
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <BluetoothA2DPSink.h>
#include <Adafruit_VS1053.h>
#include <VS1053.h>                 
#include <RotaryEncoder.h>


//Definování pinů

//Zvuková karta VS1053b
#define VS1053_CS    5  
#define VS1053_DCS   16 
#define VS1053_DREQ  4  
#define VS1053_CSSD 17
#define CLK 18       
#define MISO 19      
#define MOSI 23  

//Infračervený přijímač (OUT)
#define IR_REC 2 

//Tlačítko pro přepínání mezi WiFi a Bluetooth režimy
#define BT_PIN 33 

//Rotační enkódér
#define BUTTON_PIN 12 
#define DT_PIN 13     
#define CLK_PIN 14    

//Název zařízení v Bluetooth režimu
#define BT_NAME "ESP32 RADIO"

//Velikost bufferu (vyrovnávací paměti) pro knihovnu Adafruit_VS1053.h - režim Bluetooth
#define BUFFSIZE 32 

//Konstruktor pro SharedPrefs (ukládání výchozích proměnných při startu rádia)
Preferences preferences;

//Konstruktor pro zvukovou kartu a definice proměnných (knihovna Adafruit_VS1053.h, kterou jsem použil pro implementaci Bluetooth, na klasické VS1053.h se mi to nedařilo)
Adafruit_VS1053_FilePlayer bt_prehravac (VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_CSSD);

//Konstruktor pro zvukovou kartu (knihovna VS1053.h, je mnou používaná jako výchozí knihovna pro internetový stream)
VS1053 stream_prehravac(VS1053_CS, VS1053_DCS, VS1053_DREQ); 

int lcd_adresa = 0x27; // Adresa LCD displeje (viz Tabulka 3.1 v dokumentu práce)

//Konstruktor pro LCD displej (16x2)
LiquidCrystal_I2C lcd(lcd_adresa, 16, 2); // Plošky A0, A1 a A2 ve výchozím stavu, proto zadám do proměnné lcd_adresa 0x27 

//Konstruktor pro rotační enkódér
RotaryEncoder *enkoder = nullptr;

//Testovací proměnné pro serial
bool IPdone = false;
String serverIP;

//Konstruktor pro webový server ESP v případě nepodaření připojení se k WiFi
AsyncWebServer server(80);


//Definice režimů

//Módy enkódéru
enum enk_mod {
  enkoder_hlasitost,
  enkoder_stanice
};

//Módy přehrávání
enum rad_mod {
  BT_REZIM,
  WIFI_REZIM
};

//Módy rádia samotného
enum server_mod{
  RADIO,
  SERVER
};


//Výchozí hodnoty při startu rádia
//Jedná se o WiFi režim, nespouštět server a nastavení volby enkódérém na změnu hlasitosti
rad_mod rad_vych_mod = WIFI_REZIM;

server_mod server_vych_mod = RADIO;

enk_mod enk_vych_mod = enkoder_hlasitost;


//Zadání WiFi údajů
char ssid[] = "projekt";   
char pass[] = "projekt1"; 

//Vstupní údaje pro přístup k serveru (jakmile se spustí - objeví se jeho IP na displeji spolu s nápisem SERVER REZIM)
char server_ssid[] = "esp";
char server_heslo[] = "esp32test";

//Definice pole mp3buff o velikosti BUFFSIZE 
uint8_t mp3buff[BUFFSIZE];

//Definice pole hlavičky pro Bluetooth (nutné, bez tohoto naplnění by komunikace nebyla možná)
unsigned char bt_wav_header[44] = {
    0x52, 0x49, 0x46, 0x46, 
    0xFF, 0xFF, 0xFF, 0xFF, 
    0x57, 0x41, 0x56, 0x45, 
    0x66, 0x6d, 0x74, 0x20, 
    0x10, 0x00, 0x00, 0x00, 
    0x01, 0x00,             
    0x02, 0x00,             
    0x44, 0xac, 0x00, 0x00, 
    0x10, 0xb1, 0x02, 0x00, 
    0x04, 0x00,             
    0x10, 0x00,             
    0x64, 0x61, 0x74, 0x61, 
    0xFF, 0xFF, 0xFF, 0xFF  
};

//Konstruktor pro Bluetooth komunikaci
BluetoothA2DPSink a2dp_sink;

//Definice proměnné pro informace o Bluetooth zasílané připojeným zdrojem audia (např. interpret)
char bluetooth_media_title[255];
//Definice boolu pro dotaz na doručená data 
bool f_bluetoothsink_metadata_received = false;
//Konstruktor pro circular buffer (cirkulární vyrovnávací paměť - FIFO (First In - First Out))
cbuf circBuffer(1024 * 24);

//Definice hosta (serveru),cesty za streamem, o který se žádá a portu
const char *host;                                                                                                                                                                                                                                                                                   // Zde se uloží adresa serveru s požadovaným streamem
const char *cesta;          


//Definice proměnných pro stanice
int ID_stanice=1;        //Číslo aktuálně přehráváné stanice, je výchozí po resetu hodnot
int pamet_ID_stanice;    //Paměť poslední přehráváné stanice
String nazev="";         //Název aktuálně přehrávané stanice
String nazevCely="";
String nar="";


//Definice proměnných LCD displeje
int x1 = 16;   //Počet řádků displeje
int x2 = 0;    //Kde se nachází počáteční znak
int delkaTextu = 0;    //Výchozí délka textu
String odsazeni = "                "; //Odsazení pro scrolling displeje

int Obnova;              //Obnova určuje, jak často se má displej obnovovat, kdyby se obnovoval s rychlostí taktu, s jakým pracuje MCU, zobrazily by se artefakty
int Volume = 80;           //Výhozí hlasitost při zapnutí v rádio režimu, je výchozí po resetu hodnot
int VolumeBT = 40;         //Výhozí hlasitost při zapnutí v Bluetooth režimu, je výchozí po resetu hodnot
int VolumeBTKontrola;      //Porovnání hodnot Bluetooth hlasitosti s předchozí pro aktualizaci
int predVolume = Volume;   //Porovnání hodnot hlasitosti v rádio režimu
int zalohaVolume;          //Slouží k uložení proměnné pro hlasitost výstupu v rádio režimu
int zalohaVolumeBT;        //Slouží k uložení proměnné pro hlasitost výstupu v Bluetooth režimu
int zaloha_stanice;        //Slouží k uložení proměnné pro ID stanice

//Definice počtu stanic (potřebná pro nastavení intervalů možností otáčení enkódérem nebo přepínání pomocí ovladače)
int pocet_stanic = 30;     //Potřebné upravit!!! Shodné s počtem case ve WIFI_REZIM   

bool toggle = true; //Proměnná pro mute v rádio režimu
bool toggleBT = true; //Proměnná pro přepnutí ovladačem v rádio i bluetooth režimu
bool toggleBTVol = true; //Proměnná pro mute v bluetooth režimu

//Definice ostatních proměnných
volatile int enkoderPozST = ID_stanice;
volatile int enkoderPozVO = Volume;

int buttonState = HIGH;        
int lastButtonState = HIGH;    
int BTbuttonState = HIGH; 

//Detekce nechtěného přepínání mezi rádio a BT režimem
unsigned long BTlastButtonPressTime = 0;
unsigned long BTdebounceDelay = 2000;
bool buttonPressed = false;

//Konstruktor pro webclienta (nutné pro načítání streamů ze serveru)
WiFiClient webclient;

//Konstruktory pro infračervenou diodu
IRrecv irrecv(IR_REC);
decode_results decod_prij;

String Scroll_displeje(String LCD_text){
  String LCD_hot;
  String LCD_proces = odsazeni + LCD_text + odsazeni;
  LCD_hot = LCD_proces.substring(x1,x2);
  x1++;
  x2++;
  if (x1>LCD_proces.length()){
    x1=16;
    x2=0;
  }
  return LCD_hot;
}  


//Funkce pro spouštění serveru v případě, že se ESP32 nedokázalo připojit k žádné WiFi
void serverRun() {
    WiFi.softAP(server_ssid, server_heslo);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", obsahWeb); //Vytvoří se server na IP zobrazené na displeji, kde se zobrazí html kód z proměnné obsahWeb, která je uložená ve flash spolu s programem
    });
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
        if(request->hasParam("ssid", true) && request->hasParam("password", true)) {
            AsyncWebParameter* ssidParam = request->getParam("ssid", true);
            AsyncWebParameter* passwordParam = request->getParam("password", true);

            String newSSID = ssidParam->value();
            String newPassword = passwordParam->value();
            //Ukládání zadaného SSID a hesla do sharedPrefs inicializované při spuštění ESP
            preferences.begin("wifi", false);
            preferences.putString("ssid", newSSID);
            preferences.putString("password", newPassword);
            preferences.end();
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "text/plain", "Chybí potřebné parametry");
        }
    });
    server.begin();
    //Serial.println("Server běží!");
}


//Testovací případ pro reset hodnot do původního stavu
void resetHodnot(){
  preferences.begin("radio", false);
      preferences.putInt("Volume", 80);
      preferences.putInt("VolumeBT", 40);
      preferences.putInt("ID_stanice", 1);
      preferences.end();
  preferences.begin("wifi", false);
            preferences.putString("ssid", "My tady");
            preferences.putString("password", "ST59RE65KR97PA01");
            preferences.end();
  
}

//Testovací případ pro reset hodnot s testovaným špatně zadaným SSID
void resetTest(){
   preferences.begin("wifi", false);
            preferences.putString("ssid", "d");
            preferences.putString("password", "d");
            preferences.end();
}


//Metadata z připojeného zařízení skrze Bluetooth (interpret, atd.) - využívám jen vypisování do Serialu
void metadata(uint8_t data1, const uint8_t *data2) {
    Serial.printf("Metadata: ID: 0x%x, %s\n", data1, data2);
    if (data1 == 0x1) { 
        strncpy(bluetooth_media_title, (char *)data2, sizeof(bluetooth_media_title) - 1);
    } else if (data1 == 0x2) {
        strncat(bluetooth_media_title, " - ", sizeof(bluetooth_media_title) - 1);
        strncat(bluetooth_media_title, (char *)data2, sizeof(bluetooth_media_title) - 1);
        f_bluetoothsink_metadata_received = true;
    }
   }

//Průběh Bluetooth streamu, volá se ve smyčce
void handle_stream() {
  if (circBuffer.available()) { 
    if (bt_prehravac.readyForData()) { //Dotaz na zvukovou kartu, zda může přijímat další data
      int bytesRead = circBuffer.read((char *)mp3buff, BUFFSIZE); 
      //Zachycení chybného čtení dat
      if (bytesRead != BUFFSIZE) Serial.printf("Málo dat v bufferu, kvalita audia může být ovlivněna", bytesRead);
      bt_prehravac.playData(mp3buff, bytesRead); //Zaslání dat do zvukové karty
    }
  }
}

//Naplnění bufferu
void dat_stream(const uint8_t *data, uint32_t length) {
  if (circBuffer.room() > length) { 
    if (length > 0) { 
      circBuffer.write((char *)data, length);       
    }
  } else {
    Serial.println("\nŽádná streamovací data!");
  }  
}



//Funkce volána při přepnutí z WiFi režimu do Bluetooth režimu
void startBluetooth(){
Serial.println("Přepínám na BT režim");
 if (rad_vych_mod != BT_REZIM){
    lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("BLUETOOTH");
 lcd.setCursor(0,1); 
 lcd.print("Nastavovani..."); //Vypsání BLUETOOTH na displej
  }
 
stream_prehravac.stopSong(); //Pozastavení inicializované knihovny VS1053.h
stream_prehravac.softReset();
delay(1000);
if (!bt_prehravac.begin()) {
     Serial.println(F("Chyba VS1053b, zkontroluj zapojení pinů!"));
     lcd.clear();
     lcd.setCursor(0,0);
     lcd.print("Chyba E1");
     while(1);
  } 
bt_prehravac.begin(); //Start zvukové karty pomocí knihovny Adafruit_VS1053.h

  
  ArduinoOTA.end(); //Vypnutí OTA 
  WiFi.mode(WIFI_OFF); //Vypnutí WiFi
  WiFi.disconnect();
  Serial.println("WiFi režim: " + WiFi.getMode()); //Testovací výpis pro kontrolu
  delay(1000);
  circBuffer.flush(); //Vyprázdnění vyrovnávací paměti
  Serial.println(stream_prehravac.isChipConnected()); //Testovací výpis pro kontrolu
  a2dp_sink.set_stream_reader(dat_stream, false); //Nastavení Bluetooth streamu
  a2dp_sink.set_avrc_metadata_callback(metadata); //Nastavení metadat - nepoužívám jej pro výpis jinde, než v konzoli, není třeba
  a2dp_sink.start(BT_NAME); //Start Bluetooth se jménem v BT_NAME
  Serial.println(a2dp_sink.get_connected_source_name()); //Testovací výpis pro kontrolu
  delay(1000);
  circBuffer.write((char *)bt_wav_header, 44);
  delay(1000);

  Serial.println("Bluetooth režim"); //Testovací výpis pro kontrolu

  if (rad_vych_mod != BT_REZIM){ //Výpis informace o režimu na LCD displej spolu s názvem BT
    lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("BLUETOOTH");
 lcd.setCursor(0,1); 
 lcd.print(BT_NAME);
  }
  
  rad_vych_mod = BT_REZIM; //Přepnutí režimu na BT_REZIM
}

//Tato funkce je volána při přepínání z Bluetooth do rádiového režimu
void stopBluetooth(){
   lcd.clear(); //Výpis toho, co se s rádiem děje
 lcd.setCursor(0, 0);
 lcd.print("BLUETOOTH");
 lcd.setCursor(0,1); 
 lcd.print("Vypinani...");
 a2dp_sink.end();
 delay(500);

   Serial.println("Přepínám na WiFi režim");
   bt_prehravac.stopPlaying();
   bt_prehravac.reset();
   ESP.restart();
   /*delay(2000);

  stream_prehravac.begin();
  stream_prehravac.setVolume(Volume);
  Serial.println(stream_prehravac.isChipConnected());
  delay(10000);


  //WiFi.mode(WIFI_STA);         // Mód WiFi
  //delay(10000);
  WiFi.begin(ssid, pass);
   Serial.println("WiFi režim: " + WiFi.getMode());
  delay(10000);
 Serial.println("WiFi režim");
//  ArduinoOTA.setHostname("ESP32");
//  ArduinoOTA.begin();
//   delay(5000);
//   Serial.println("OTA aktivní");
  
  rad_vych_mod = WIFI_REZIM; // Set the mode to WIFI_REZIM
*/
}


void getPoz()
{
  enkoder->tick(); //Zjištění stavu enkóderu
}


//Funkce pro detekci příkazů v serialu
void Prikazy() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');

    if (command == "getip") {
      //Získání IP adresy
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    } else if (command == "getstation") {
      //Získání aktuální stanice
      Serial.println("Aktuální stanice: "); 
      Serial.println(nazev);
    } else if (command == "getvolume") {
      //Získání hlasitosti
      Serial.print("Hlasitost (Volume): "); 
      Serial.println(Volume);
    } else if (command == "state") {
      //Získání režimu)
      Serial.print("Režim: ");
      if (rad_vych_mod == WIFI_REZIM) {
        Serial.println("WiFi");
      } else if (rad_vych_mod == BT_REZIM) {
        Serial.println("Bluetooth");
      }
    }
    else if (command == "wifi") {
      //Získání režimu WiFi
      Serial.print("Wifi: "); 
      Serial.println(WiFi.getMode());
    }
    else if (command == "restart") {
      //Restart ESP
      Serial.print("Restart za 2s!!!"); 
      delay(2000);
      ESP.restart();
    }
    
    else if (command == "restartFull"){
      //Kompletní restart s resetem hodnot
      Serial.print("Úplný restart za 2s!!!"); 
      resetHodnot();
      delay(2000);
      ESP.restart();
    } 

    else if (command == "BTrezim"){
      //Vynucení přepnutí do BT režimu
      Serial.print("Přepínám do BT");
      startBluetooth();
    }

    else if (command == "restartTest"){
      //Restart ESP s nahráním testovacích hodnot
      Serial.print("Úplný restart za 2s!!!"); 
      resetTest();
      delay(2000);
      ESP.restart();
    } 


    else if (command == "cred"){
      //Vypsání SSID a hesla, pomocí kterého se připojilo ESP k WiFi
      Serial.println(ssid);
      Serial.println(pass);
      }
    
     else {
      //Ošetření pro nesprávně zadaný příkaz
      Serial.println("Neznámý příkaz");
    }
  }
}

void Enkoder() {
  static int poz = 1;
  int novaPoz = enkoder->getPosition(); //Získání pozice
  int smerST; 

  switch(rad_vych_mod){ //Různé funkce pro enkódér v závislosti na rad_vych_mod
    case WIFI_REZIM:
  if (poz != novaPoz) {
    switch (enk_vych_mod) { //Volba režimu enkódéru
      case enkoder_stanice: //Přepínání stanic
        //Zjištění směru otáčení
        smerST = (int)(enkoder->getDirection());
        //Podmínky pro otáčení enkódérem
        if (smerST > 0 && ID_stanice < pocet_stanic) {
          ID_stanice++;
        } else if (smerST < 0 && ID_stanice > 1) {
          ID_stanice--;
        }
        Serial.print("pozST:"); //Vypisování do serialu (pro testování)
        Serial.print(ID_stanice); 
        Serial.print("smerST:");
        Serial.println(smerST); 
        poz = novaPoz; //Uložení pozice do poz
        zaloha_stanice = ID_stanice; //Uložení ID stanice do zaloha_stanice
        break;
      case enkoder_hlasitost: //Změna hlasitosti
        int smerVO = (int)(enkoder->getDirection()); //smerVO nabývá hodnot 1 a -1 dle knihovny RotaryEncoder.h 
        if (smerVO > 0 && Volume < 100) {//Pokud je 1, zvýšení hlasitosti o 1
          Volume++;
        } else if (smerVO < 0 && Volume > 29) {//Pokud je -1, snížení hlasitosti o 1
          Volume--;
        }
        Serial.print("pozVO: ");//Vypisování do serialu (pro testování)
        Serial.print(enkoderPozVO); 
        Serial.print("smerVO: ");
        Serial.println(smerVO); 
        Serial.print("Volume: ");
        Serial.print(Volume); 
        poz = novaPoz;//Zápis nové polohy
        break;
    }
    }
  break;

  case BT_REZIM:

if (poz != novaPoz) {
    switch (enk_vych_mod) {
      case enkoder_stanice: //Využil jsem již existující enkoder_stanice (přepíná skladby v playlistu připojeného zařízení)
        //Zjištění směru otáčení
        smerST = (int)(enkoder->getDirection());
        if (smerST > 0) {
          a2dp_sink.next(); //Přepnutí vpřed
        } else if (smerST < 0) {
          a2dp_sink.previous();//Přepnutí vzad          
        }
        Serial.print("posST:");//Vypisování do serialu (pro testování)
        Serial.print(" dirST:");
        Serial.println(smerST); 
        Serial.print(bluetooth_media_title);
        poz = novaPoz;//Zápis nové polohy
        break;

      case enkoder_hlasitost://Jako u WiFi režimu je zde změna hlasitosti
        //Zjištění směru otáčení
        int smerVO = (int)(enkoder->getDirection());
        //Podmínky pro otáčení enkódérem
        if (smerVO > 0 &&  VolumeBT > 1) {
          VolumeBT--;
        } else if (smerVO < 0 && VolumeBT < 101) {
          VolumeBT++;
        }
        Serial.print("posVO:");//Vypisování do serialu (pro testování)
        Serial.print(enkoderPozVO); 
        Serial.print(" dirVO:");
        Serial.println(smerVO); 
        Serial.print(" VolumeBT:");
        Serial.print(VolumeBT); 
        poz = novaPoz;//Zápis nové polohy
        break;
    }
    }
  break;
  }
}

//Funkce pro tlačítko, které přepíná režimy enkódéru
void handleButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;

        //přepnutí stavu
      if (enk_vych_mod == enkoder_stanice) {
        enk_vych_mod = enkoder_hlasitost;
        Serial.println("Měním hlasitost!");
      } else {
        enk_vych_mod = enkoder_stanice;
        Serial.println("Přepínám stanice!");
      }
    }
  } else {
    buttonPressed = false;
  }
}


//Funkce pro chování IR diody
void IROvladac() {
  if (irrecv.decode(&decod_prij)) {
    Serial.println(decod_prij.value, HEX);

    switch (rad_vych_mod){ //Pro WiFi a Bluetooth rozlišné funkce

      case WIFI_REZIM:

    if (decod_prij.value == 0xFFB04F) { // tlačítko "#" 
      
      Serial.println("Restart za 2s!");
      delay(1500);
      ESP.restart();
    } 

if (decod_prij.value == 0xFF38C7) { //tlačitko "OK"
      if (toggleBT) {
        Serial.println(toggleBT);
        startBluetooth();
      } else {
        Serial.println("Vypínám BT");
        stopBluetooth();
      }
      toggleBT = !toggleBT; // přepnutí stavu
    }
    
    if (decod_prij.value == 0xFF4AB5 && Volume > 29) { //tlačítko "šipka dolů"
      Volume--;
      Serial.println("Hlasitost snížena!");
      Serial.println(Volume);
      
    } else if (decod_prij.value == 0xFF18E7 && Volume < 100) { //tlačítko "šipka nahoru"
      Volume++;
      Serial.println("Hlasitost zvýšena!");
      Serial.println(Volume);
      
    }

    if (decod_prij.value == 0xFF5AA5 && ID_stanice < pocet_stanic) {
      ID_stanice++;
      Serial.println("Stanice přepnuta o 1 vpřed!"); //tlačitko "šipka doprava"
      
    } else if (decod_prij.value == 0xFF10EF && ID_stanice > 1) {
      ID_stanice--;
      Serial.println("Stanice přepnuta o 1 zpět!"); //tlačitko "šipka doleva"
      
    }

    if (decod_prij.value == 0xFF6897) { //tlačítko "*" - slouží pro mute (vypnutí zvuku)
      if (toggle) {
        zalohaVolume = Volume;
        Volume = 29; // při nule repráky chrčí, 30 a méně je mute prakticky zaručen napříč reproduktory
        Serial.println(Volume);
        Serial.println(toggle);
      } else {
        Volume = zalohaVolume;
        Serial.println(Volume);
        Serial.println(toggle);
      }
      toggle = !toggle; // přepnutí stavu
    }

    break;

    case BT_REZIM:

    if (decod_prij.value == 0xFFB04F) {// tlačítko "#" 
      
      Serial.println("Restart za 1s!");

      delay(1000);
      ESP.restart();
      
    } 

    if (decod_prij.value == 0xFF38C7) {// tlačítko "OK" 
      if (toggleBT) {
        Serial.println(toggleBT);
        startBluetooth();
      } else {
        Serial.println(toggleBT);
        stopBluetooth();
      }
      toggleBT = !toggleBT; // přepnutí stavu
    }

     if (decod_prij.value == 0xFF4AB5 && VolumeBT < 101) {// tlačítko "šipka dolů" 

      VolumeBT++;
      
      Serial.println("Hlasitost snížena!");

      Serial.println(VolumeBT);

      
    } else if (decod_prij.value == 0xFF18E7 && VolumeBT > 0) {// tlačítko "šipka nahoru"
      
      VolumeBT--;

      Serial.println("Hlasitost zvýšena!");

      Serial.println(VolumeBT);

      
    }

    if (decod_prij.value == 0xFF6897) {// tlačítko "*" - mute (vypnutí zvuku)
      if (toggleBTVol) {
        zalohaVolumeBT = VolumeBT;
        VolumeBT = 254; 
        Serial.println(VolumeBT);
        Serial.println(toggleBTVol);
      } else {
        VolumeBT = zalohaVolumeBT;
        Serial.println(VolumeBT);
        Serial.println(toggleBTVol);
      }
      toggleBTVol = !toggleBTVol; // přepnutí stavu
    }

     if (decod_prij.value == 0xFF5AA5) {// tlačítko "šipka doprava"
      a2dp_sink.next();
      Serial.println("Skladba přepnuta o 1 vpřed!"); 
    } else if (decod_prij.value == 0xFF10EF) {// tlačítko "šipka doleva"
      a2dp_sink.previous();
      Serial.println("Skladba přepnuta o 1 zpět!");
    }
    
    break;

    }
    irrecv.resume(); //pokračování v příjmu IR
  }
}

//Funkce pro tlačítko, které přepíná mezi režimy WiFi a BT
void BTbutton() {
  //Ošetření nechtěného přepínání
  BTbuttonState = digitalRead(BT_PIN);
  unsigned long currentTime = millis();
  if (BTbuttonState == LOW) {
    if (currentTime - BTlastButtonPressTime >= BTdebounceDelay) {
      delay(50);

      if (rad_vych_mod == WIFI_REZIM) {
        //Zapnutí Bluetooth, pokud jsme ve WiFi režimu
        startBluetooth();
        Serial.println("Přepínám do BT režimu");
        delay(1000);
        Serial.println("Přepnuto!");
      } else if (rad_vych_mod == BT_REZIM) {
        //Zastavení Bluetooth, pokud jsme v Bluetooth režimu
        stopBluetooth();
        Serial.println("Přepínám do WiFi režimu");
        delay(1000);
        Serial.println("Přepnuto!");
      }
      BTlastButtonPressTime = currentTime;
    }
  }
}

//Funkce pro samotný stream
void stream(){
  if(!webclient.connected() || pamet_ID_stanice!=ID_stanice){    
    
      if(webclient.connect(host, 80)){                                 //Připojení se k požadovanému streamu
      webclient.print(String("GET ") + cesta+ " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
      pamet_ID_stanice=ID_stanice;
      Serial.println(nazev); //Vypsání názvu do serialu, pro testovací účely
      lcd.clear(); //čistka displeje
      }

      else {
      Serial.println("Nepodařilo se připojit k serveru.");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.println("Chyba E2");
      stream_prehravac.stopSong();
    }
     }
  
    if(webclient.available() > 0){                                          
      uint8_t bytesread = webclient.read(mp3buff,32);           //naplnění bufferu při dostupnosti serveru            
    stream_prehravac.playChunk(mp3buff,bytesread);              //přehrávání audio streamu z bufferu na výstup VS1053
    }  
}

//Přípravná fáze ESP32 (spouští se pouze při startu)
void setup ()
{
 Serial.begin(115200);
 delay(500);
 SPI.begin(); //Inicializace SPI
 if (!bt_prehravac.begin()) {
     Serial.println(F("Nedetekováno žádné VS1053B, zkontroluj zapojení pinů! Zařízení nebude fungovat!"));
          while(1);
  } 
 stream_prehravac.begin();  // Start zvukové karty VS1053B pomocí knihovny VS1053.h
 
pinMode(BUTTON_PIN, INPUT_PULLUP); //Nastavení GPIO pinů jako vstup (tlačítko na enkódéru a Bluetooth tlačítko)
pinMode(BT_PIN, INPUT_PULLUP);
  
Serial.println("Enkódér úspěšně inicializován!");
enkoder = new RotaryEncoder(DT_PIN, CLK_PIN, RotaryEncoder::LatchMode::TWO03);//Inicializace rotačního enkódéru
irrecv.enableIRIn(); //Inicializace IR diody
attachInterrupt(digitalPinToInterrupt(DT_PIN), getPoz, CHANGE); //Nastavení přerušení při změně na DT_PIN nebo CLK_PIN
attachInterrupt(digitalPinToInterrupt(CLK_PIN), getPoz, CHANGE);

 lcd.init();     // Inicializace LCD displeje
 lcd.backlight();        
 lcd.clear();
 lcd.setCursor(0,0);
 lcd.print("Radio - SIL0074");
 
 
 lcd.setCursor(0,1);
 lcd.print("Start radia");
 
 Serial.println("Startuji");  


 WiFi.mode(WIFI_STA);   //Nastavení režimu WiFi (STA - station mode)
 delay(1000);
// WiFi.begin(ssid, pass);
//serverRun();
WiFi.begin(ssid, pass); //Připojení pomocí SSID a hesla
 Serial.print("SSID default: ");
    Serial.println(ssid);
    Serial.print("PASS default: ");
    Serial.println(pass);
 Serial.println("Připojuju se k WiFi s výchozími hodnotami..."); //Snaha o připojení pomocí parametrů v kódu
 delay(1000);
 WiFi.waitForConnectResult();


if (!WiFi.isConnected()){ 

  Serial.print("SSID před nahráním: "); //Testovací serial výstupy
    Serial.println(ssid);
    Serial.print("PASS před nahráním: ");
    Serial.println(pass);
  preferences.begin("wifi", false); //Hodnoty nahrané v sharedPrefs
    String storedSSID = preferences.getString("ssid", "");
    String storedPassword = preferences.getString("password", "");
    preferences.end();
    char ssid[storedSSID.length() + 1];
    char pass[storedPassword.length() + 1];
    storedSSID.toCharArray(ssid, storedSSID.length() + 1);
    storedPassword.toCharArray(pass, storedPassword.length() + 1);

    Serial.print("SSID po nahrání: ");
    Serial.println(ssid);
    Serial.print("PASS po nahrání: ");
    Serial.println(pass);
    WiFi.begin(ssid, pass); //Zkouška připojit se k WiFi pomocí sharedPrefs parametrů
    Serial.println("Připojuju se k WiFi s uloženými hodnotami...");
delay(1000);

}

WiFi.waitForConnectResult();


// if (!WiFi.isConnected()){
    

//     Serial.println("Připojuju se k WiFi s výchozími hodnotami...");

//     // Connect to WiFi using the stored or default credentials
//     WiFi.begin(ssid, pass);

// }
    

    
  
 if (!WiFi.isConnected()) {  //Jestliže se nepodaří ani to, informuje se uživatele o chybě spojení na serial
Serial.println("Chyba připojení!");

 //Spuštění serveru a přepnutí do režimu SERVER
  if (server_vych_mod == RADIO){
    serverRun();
  
  Serial.println("Přepínám na režim SERVER");
  server_vych_mod = SERVER;
  }

  else {
    serverRun();
  }
  }

  else if (WiFi.isConnected()){
    Serial.println("Spojení s WiFi úspěšně navázáno");
  }

  //Zprovoznění OTA
  ArduinoOTA.setHostname("ESP32");
  //ArduinoOTA.setPassword("SIL0074"); //možnost nastavení hesla
  ArduinoOTA.begin();

delay(1000);
webclient.print(String("GET ") + cesta + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
                


preferences.begin("radio", true); //Získání hodnot z sharedPrefs
Volume = preferences.getInt("Volume", Volume);
VolumeBT = preferences.getInt("VolumeBT", VolumeBT);
ID_stanice = preferences.getInt("ID_stanice", ID_stanice);
preferences.end();
lcd.clear(); //Čistka displeje
}


//Hlavní smyčka programu
void loop()
{
switch(server_vych_mod){//Přepínání mezi RADIO a SERVER režimem
  case RADIO:

switch(rad_vych_mod)//Přepínání mezi WiFi a Bluetooth režimem

{
case WIFI_REZIM:

if (Obnova >= 450){//Po kolika cyklech se má obnovit LCD

  Obnova = 0; //Reset

  
  String nazevCely =  nazev + "|" + nar + "|" + ID_stanice;
  lcd.setCursor(0, 0);
  String pohybText = Scroll_displeje(nazevCely);
  lcd.print(pohybText); //vypsání infa o stanici na první řádek LCD
  
  
  int mapVolume = map(Volume, 30, 100, 1, 100); //Namapování hodnot u WiFi hlasitosti
              
 if (Volume != predVolume) {
                lcd.setCursor(0, 1);
                lcd.print("               "); //Smazání druhého řádku LCD displeje
                lcd.setCursor(0, 1);
                if (Volume < 30){
                  lcd.print("Zvuk vypnut");//vypsání na druhý řádek LCD
                }
                else{
                lcd.print("Hlasitost: ");//vypsání na druhý řádek LCD
                lcd.print(mapVolume);
                lcd.print ("%");
                }
                //aktualizace hlasitosti
                predVolume = Volume;
            }

            else{
              lcd.setCursor(0, 1);
              if (Volume < 30){
                  lcd.print("Zvuk vypnut"); //vypsání na druhý řádek LCD
                }
                else{
                lcd.print("Hlasitost: ");//vypsání na druhý řádek LCD
                lcd.print(mapVolume);
                lcd.print ("%");
                }
            }

preferences.begin("radio", false); //ukládání do prefs
preferences.putInt("Volume", Volume);
preferences.putInt("ID_stanice", ID_stanice);
preferences.end();
}



if (!WiFi.isConnected()){ //V případě odpojení od WiFi
  Serial.println(WiFi.getMode());
  WiFi.mode(WIFI_STA);
  delay(1000);
   Serial.println(WiFi.getMode());
  WiFi.begin(ssid, pass);
  delay(10000);
  Serial.println(WiFi.localIP());

  // if (WiFi.isConnected()){
  //   ArduinoOTA.setHostname("ESP32");
  //   ArduinoOTA.begin();
  //   Serial.println("OTA aktivní");
  // }
}

ArduinoOTA.handle(); //Funkce OTA aktivní
Obnova=Obnova+1; //Přičtení obnovy LCD

stream_prehravac.setVolume(Volume);  
 switch(ID_stanice){              // Přepínání stanic - ID stanice
        case 1:
        
             nazev = "Hitradio Orion";       
             host = "ice.abradio.cz";         
             cesta= "/orion128.mp3";
             nar ="CZ";
            break;
        case 2:
        
           nazev = "Evropa 2"; 
           host= "ice.actve.net";          
           cesta= "/fm-evropa2-128";
           nar ="CZ";
           break;


        case 3:
        
           nazev = "Radio Blanik"; 
           host= "ice.abradio.cz";          
           cesta= "/blanikfm128.mp3";
           nar ="CZ";
           break;

        case 4:
          nazev = "Radio Kiss"; 
           host= "icecast4.play.cz";          
           cesta= "/kiss128.mp3";
           nar ="CZ";
           break;


        case 5:
          nazev = "Hitradio 90"; 
           host= "ice.abradio.cz";          
           cesta= "/hit90128.mp3";
           nar ="CZ";
  break;

  case 6:
          nazev = "Radio Cas"; 
           host= "icecast6.play.cz";          
           cesta= "/casradio128.mp3";
            nar ="CZ";
  break;

  case 7:
          nazev = "Country Radio"; 
           host= "icecast4.play.cz";          
           cesta= "/country128.mp3";
           nar ="CZ";
  break;

  case 8:
          nazev = "Frekvence 1"; 
           host= "ice.actve.net";          
           cesta= "/fm-frekvence1-128";
          nar ="CZ";
  break;
  
  case 9:
          nazev = "Radio Impuls"; 
           host= "icecast1.play.cz";          
           cesta= "/impuls128.mp3"; 
           nar ="CZ";
  break;
  case 10:
          nazev = "Fajn Radio"; 
           host= "ice.abradio.cz";          
           cesta= "/fajn128.mp3";
           nar ="CZ";
  break;
  case 11:
          nazev = "Rock Radio"; 
           host= "ice.abradio.cz";          
           cesta= "/rockradio128.mp3";
           nar ="CZ";
  break;

  case 12:
          nazev = "Radio Beat"; 
           host= "ice5.abradio.cz";          
           cesta= "/beat128.mp3";
nar ="CZ";
  break;

  case 13:
          nazev = "Hey Radio"; 
           host= "icecast3.play.cz";          
           cesta= "/hey-radio128.mp3";
nar ="CZ";
  break;

  case 14:
          nazev = "Hitradio FM Plus"; 
           host= "ice.abradio.cz";          
           cesta= "/fmplus128.mp3";
nar ="CZ";
  break;

  case 15:
          nazev = "Signal Radio"; 
           host= "icecast1.play.cz";          
           cesta= "/signal192.mp3";
  break;
nar ="CZ";
  case 16:
          nazev = "SRo 1 Radio Slovensko";
           host= "icecast.stv.livebox.sk";          
           cesta= "/slovensko_128.mp3";
nar ="SK";
  break;

  case 17:
          nazev = "Radio Devin (SRo 3)"; 
           host= "icecast.stv.livebox.sk";          
           cesta= "/devin_128.mp3";
nar ="SK";
  break;
  case 18:
          nazev = "Europa 2"; 
           host= "stream.bauermedia.sk";          
           cesta= "/europa2.mp3";
nar ="SK";
  break;
  case 19:
          nazev = "Radio Jemne"; 
           host= "stream.bauermedia.sk";          
           cesta= "/melody-hi.mp3";
nar ="SK";
  break;
  case 20:
          nazev = "Radio Pyramida"; 
           host= "icecast.stv.livebox.sk";          
           cesta= "/klasika_128.mp3";
nar ="SK";
  break;

  case 21:
          nazev = "Radio Tok FM"; 
           host= "radiostream.pl";          
           cesta= "/tuba10-1.mp3";
nar ="PL";

  break;

  case 22:
          nazev = "Radio Zlote Przeboje"; 
           host= "radiostream.pl";          
           cesta= "/tuba8936-1.mp3";
nar ="PL";

  break;

  case 23:
          nazev = "Nova FM"; 
           host= "live-bauerdk.sharp-stream.com";          
           cesta= "/nova128.mp3";
nar ="DK";
  break;
  case 24:
          nazev = "VLR"; 
           host= "webradio.vlr.dk";          
           cesta= "/vlrvejle";
nar ="DK";
  break;

case 25:
          nazev = "Hit Radio FFH"; 
           host= "mp3.ffh.de";          
           cesta= "/radioffh/hqlivestream-osthessen.mp3";
nar ="DE";
  break;
  case 26:
          nazev = "BB Radio"; 
           host= "irmedia.streamabc.net";          
           cesta= "/irm-bbradiolive-mp3-128-4531502";
nar ="DE";
  break;
  case 27:
          nazev = "Rock FM"; 
           host= "flucast-b02-06.flumotion.com";          
           cesta= "/cope/rockfm.mp3";
nar ="ES";
  break;
  case 28:
          nazev = "Euskadi Irratia"; 
           host= "mp3-eitb.stream.flumotion.com";          
           cesta= "/eitb/euskadiirratia.mp3"; 
nar ="ES";
  break;
  case 29:
          nazev = "MR 1 Kossuth Radio"; 
           host= "maxxima.mine.nu";          
           cesta= "/mr1.mp3";
nar ="HU";
  break;
  case 30:
          nazev = "Best FM"; 
           host= "icast.connectmedia.hu";          
           cesta= "/5113/live.mp3";
nar ="HU";
  break;
     }
            

    
  stream(); //funkce pro stream

  IROvladac(); //aktualizace IR

  Enkoder();//aktualizace enkódéru

  handleButton(); //aktualizace tlačítka
  
  Prikazy();//aktualizace příkázů
  
 BTbutton();//aktualizace BT tlačítka

 break;

//Funkce v BT režimu
case BT_REZIM:
 //Pokud není BT spuštěno, zapni jej
if (!a2dp_sink.is_connected()){
   startBluetooth();
}

//Refresh LCD v BT režimu
if (Obnova >= 20000){
       Obnova=0;
  bt_prehravac.setVolume(VolumeBT, VolumeBT); //Nastavení hlasitosti VS1053b

if (VolumeBT != VolumeBTKontrola){//Obnovení displeje v případě, že se změní hodnota hlasitosti
  VolumeBTKontrola = VolumeBT;
//Mapování hodnot hlasitosti v jiném pořadí a v jiném intervalu
int mapVolumeBT = map(VolumeBT, 1, 100, 100, 1);
if (mapVolumeBT == 0 || VolumeBT == 254){
    lcd.clear();
  lcd.setCursor(0, 0);
 lcd.print("BLUETOOTH");//Výpis na LCD displej
 lcd.setCursor(0,1); 
    lcd.print("Zvuk vypnut"); //Výpis na LCD displej            
}
else {
  lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("BLUETOOTH");//Výpis na LCD displej
 lcd.setCursor(0,1); 
 lcd.print("Hlasitost: ");//Výpis na LCD displej
 lcd.print (mapVolumeBT);
 lcd.print ("%");
}}
   
preferences.begin("radio", false); // Uložení hodnot do shared prefs
  preferences.putInt("VolumeBT", VolumeBT);
  preferences.end();

  
}

  

  handle_stream(); //aktualizace streamu

  IROvladac(); //aktualizace IR

  Enkoder();//aktualizace enkódéru

  handleButton(); //aktualizace tlačítka
  
  Prikazy();//aktualizace příkázů
  
  BTbutton();//aktualizace BT tlačítka

  Obnova++;
       

  break;
        
 }

  break;
//V případě server režimu
  case SERVER:

  if (!IPdone) {
  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("Server rezim");//Výpis na LCD displej
  lcd.setCursor(0, 1);
  lcd.print("IP: ");//Výpis na LCD displej
  String serverIP = WiFi.softAPIP().toString(); 
  lcd.print(serverIP);//Výpis IP serveru na LCD displej
  Serial.println("IP VYPSANA");
  IPdone = true; 
}

Prikazy(); //Detekce příkazů v server režimu
  break;
}
}



















