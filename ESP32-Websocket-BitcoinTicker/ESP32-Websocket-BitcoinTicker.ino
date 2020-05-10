
#include <ArduinoWebsockets.h>
#include <WiFi.h> 
#include <ArduinoJson.h>

#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <Button2.h>


#include <TimeLib.h>

#include <WiFiUdp.h>
#include <NTPClient.h>


#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL          4   // Display backlight control pin
#define ADC_EN          14  //ADC_EN is the ADC detection enable port
#define ADC_PIN         34
#define BUTTON_1        35
#define BUTTON_2        0

using namespace websockets;

WebsocketsClient client;

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

const long utcOffsetInSeconds =  -4 * 60 * 60;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

float precioAnterior=0;
float precioMenor=0;
float precioMayor=0;
float precio=0;
long lastsync=0;
String lastSync_str;
String horaActual_str;

char lastsync_dt[19]="" ;
char horaActual[8] = "";

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{
    //this section is commented since sleeping the ESP32 breaks the socket connection
    /*
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
    */
    delay(ms);
}


void button_init()
{
    btn1.setLongClickHandler([](Button2 & b) {
        Serial.println("long right click..");
    });
    btn1.setPressedHandler([](Button2 & b) {
        Serial.println("right click..");   
    });

    btn2.setLongClickHandler([](Button2 & b) {
        Serial.println("long left click..");
    });
    btn2.setPressedHandler([](Button2 & b) {
        Serial.println("left click..");
    });
}

void button_loop()
{
    btn1.loop();
    btn2.loop();
}

const char* ssid = "your_ssid";
const char* password = "your_password";

void wifi_init()
{
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("Connecting to the wifi network...",0,tft.height() / 2 - 16);
    WiFi.begin(ssid, password);
    int x=0;
    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        tft.drawString(".",0,tft.height() / 2 - 16);
        espDelay(1000);
        x++;
        if(x>10)
        {
          Serial.print("Retrying...");
          WiFi.begin(ssid, password);
          espDelay(500);
          x=0;
        }
    }
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connected!",0,tft.height() / 2 - 16);
    espDelay(1000);    
    tft.fillScreen(TFT_BLACK);
}

void actualizarDisplay()
{   
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
   
    tft.setTextSize(1);
    tft.drawString(horaActual_str, 220, 0);
        
    if(precioAnterior>precio)
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.fillTriangle(200, 47, 195, 42, 205, 42, TFT_RED);
      tft.fillTriangle(200, 33, 195, 38, 205, 38, TFT_BLACK);//this deletes the green triangle
    }
    else
    {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.fillTriangle(200, 47, 195, 42, 205, 42, TFT_BLACK);//this deletes the red triangle
      tft.fillTriangle(200, 33, 195, 38, 205, 38, TFT_GREEN);
    }

    tft.setTextSize(3);
    tft.drawString( String(precio, 2), tft.width() / 2, tft.height() / 2 -25);
       
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString( String(precioMenor, 2), tft.width() / 2 -60, tft.height() / 2 );
  
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString( String(precioMayor, 2), tft.width() / 2 +60, tft.height() / 2 );

    precioAnterior=precio;
    
    if(precio<precioMenor) 
    {
      precioMenor=precio;
    }
    if(precioMayor<precio)
    {
      precioMayor=precio;
    }

    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    String lu_txt = "Last Update:"+lastSync_str;
    tft.drawString(lu_txt, tft.width() / 2,125);
}

const char* websockets_connection_string = "wss://ws.bitstamp.net/"; 

// This certificate was updated 07-05-2020
const char echo_org_ssl_ca_cert[] PROGMEM = \
    "-----BEGIN CERTIFICATE-----\n" \
"MIIElDCCA3ygAwIBAgIQAf2j627KdciIQ4tyS8+8kTANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaME0xCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg\n" \
"U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" \
"ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83\n" \
"nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd\n" \
"KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f\n" \
"/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX\n" \
"kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0\n" \
"/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8C\n" \
"AQAwDgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYY\n" \
"aHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6\n" \
"Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1\n" \
"oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RD\n" \
"QS5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8v\n" \
"d3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFA+AYRyCMWHVLyjnjUY4tCzh\n" \
"xtniMB8GA1UdIwQYMBaAFAPeUDVW0Uy7ZvCj4hsbw5eyPdFVMA0GCSqGSIb3DQEB\n" \
"CwUAA4IBAQAjPt9L0jFCpbZ+QlwaRMxp0Wi0XUvgBCFsS+JtzLHgl4+mUwnNqipl\n" \
"5TlPHoOlblyYoiQm5vuh7ZPHLgLGTUq/sELfeNqzqPlt/yGFUzZgTHbO7Djc1lGA\n" \
"8MXW5dRNJ2Srm8c+cftIl7gzbckTB+6WohsYFfZcTEDts8Ls/3HB40f/1LkAtDdC\n" \
"2iDJ6m6K7hQGrn2iWZiIqBtvLfTyyRRfJs8sjX7tN8Cp1Tm5gr8ZDOo0rwAhaPit\n" \
"c+LJMto4JQtV05od8GiG7S5BNO98pVAdvzr508EIDObtHopYJeS4d60tbvVS3bR0\n" \
"j6tJLp07kzQoH3jOlOrHvdPJbRzeXDLz\n" \
    "-----END CERTIFICATE-----\n";



void onMessageCallback(WebsocketsMessage message) {
     //Serial.print("Got Message: ");
    //Serial.println(message.data());
    StaticJsonDocument<500> doc;
    String msg = message.data();
    //Serial.println(msg);
    deserializeJson(doc, msg);

    String eventType =doc["event"].as<char*>();
    if(eventType=="trade")
    {
      precio=doc["data"]["price"].as<float>();
      
      if(precioMenor==0)
      {
        precioMenor=precio;
      }
      
      lastsync = doc["data"]["timestamp"].as<long>()+utcOffsetInSeconds;

      char buff[19]="" ;
      sprintf(lastsync_dt, "%02d/%02d/%02d %02d:%02d:%02d", day(lastsync), month(lastsync), year(lastsync), hour(lastsync), minute(lastsync), second(lastsync));

      lastSync_str=String(lastsync_dt);
      actualizarDisplay();
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
   Serial.println("event callback!");
   Serial.println(data);
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");     
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
        Serial.println("Responding Pong!");
        client.pong();
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

void connectWS()
{
   tft.setCursor(0, 0);
  tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("Opening socket connection...",0,tft.height() / 2 - 16);
    
  bool conn = false;
    while(!conn)
    {
      conn = client.connect(websockets_connection_string);
      if(conn) {
          Serial.println("Connected!");
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Socket open, subscribing to channel...",0,tft.height() / 2 - 16);
          espDelay(1000);
          tft.fillScreen(TFT_BLACK);
          
          // Send a ping
          client.ping();
          
          StaticJsonDocument<200> doc;
          doc["event"] = "bts:subscribe";
          JsonArray data = doc.createNestedArray("data");
          StaticJsonDocument<200> doc2;
          doc2["channel"] = "live_trades_btcusd";
          doc["data"] = doc2; 
          String ret="";
          serializeJson(doc, ret);
          //Serial.println(ret);
          client.send(ret);
      
      } else {
          Serial.println("Not Connected!");
          tft.fillScreen(TFT_BLACK);
          tft.drawString("Could not open socket, trying again...",0,tft.height() / 2 - 16);
          espDelay(1000);
          tft.fillScreen(TFT_BLACK);
      }
      
    }
}

void updateTime()
{
  timeClient.update();
  long epochNow = timeClient.getEpochTime();
  sprintf(horaActual, "%02d:%02d:%02d", hour(epochNow), minute(epochNow), second(epochNow));
  horaActual_str = String(horaActual); 
}

void setup()
{
    if (TFT_BL > 0) { // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
        pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
        digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
    }

    Serial.begin(115200);
    Serial.println("Start");

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    
    wifi_init();  

    button_init();

     // run callback when messages are received
    client.onMessage(onMessageCallback);
    
    // run callback when events are occuring
    client.onEvent(onEventsCallback);

    // Before connecting, set the ssl fingerprint of the server
    client.setCACert(echo_org_ssl_ca_cert);

    // Connect to server
     connectWS();

  
}

long lastcheck=0;
void loop()
{  

    
    button_loop();
    
    if(client.available()) 
    {
        client.poll();
    }
    else
    {
        connectWS();
    }
    

    if(millis() - lastcheck > 1000) 
    {
      updateTime();
      actualizarDisplay();
      lastcheck=millis();
    }

    
}
