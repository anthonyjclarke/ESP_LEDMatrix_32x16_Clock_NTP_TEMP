// Anthony Clarke @anthonyjclarke
//
// Updated to source time and date from NTP Server & DST Handling
//
// Credit to @neptune https://github.com/neptune2/simpleDSTadjust & 
// Pawel A. Hernik - https://www.youtube.com/watch?v=2wJOdi0xzas&t=1s @cbm80amiga Youtube Channel
//
// Updates 3rd March 2018
// v1.0 Removed time / date collection from google.com method to NTP server read
// v1.1 Modified Mode 0 Display to Display Temp - DHT22 Sensor
/*
 * (c)20016-18 Pawel A. Hernik
 * 
  ESP-01 pinout from top:

  GND    GP2 GP0 RX/GP3
  TX/GP1 CH  RST VCC

  MAX7219 5 wires to ESP-1 from pin side:
  Re Br Or Ye
  Gr -- -- --
  capacitor between VCC and GND
  resistor 1k between CH and VCC

  USB to Serial programming
  ESP-1 from pin side:
  FF(GP0) to GND, RR(CH_PD) to GND before upload
  Gr FF -- Bl
  Wh -- RR Vi

  ------------------------
  NodeMCU 1.0/D1 mini pinout:

  D8 - DataIn
  D7 - LOAD/CS
  D6 - CLK

*/
// 
// Set Configuration below based on Location
//
// DHT22 Sensor - Data Pin D5

#include <ESP8266WiFi.h>
#include <time.h>
#include <simpleDSTadjust.h>
#include <DHT.h>

#define DHTPIN D5     // what digital pin we're connected to

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#define NUM_MAX 8
#define LINE_WIDTH 32
#define ROTATE  90

// for ESP-01 module
//#define DIN_PIN 2 // D4
//#define CS_PIN  3 // D9/RX
//#define CLK_PIN 0 // D3

// for NodeMCU 1.0/D1 mini
#define DIN_PIN 15  // D8
#define CS_PIN  13  // D7
#define CLK_PIN 12  // D6

#define DEBUG(x) x
//#define DEBUG(x) x

#include "max7219.h"
#include "fonts.h"

// =======================================================================
// Your config below!
// =======================================================================
const char* ssid     = "xxxxxxxxxx";     // SSID of local network
const char* password = "yyyyyyyyyy";   // Password on network

// Maximum of 3 servers
#define NTP_SERVERS "us.pool.ntp.org", "pool.ntp.org", "time.nist.gov"

// Daylight Saving Time (DST) rule configuration
// Rules work for most contries that observe DST - see https://en.wikipedia.org/wiki/Daylight_saving_time_by_country for details and exceptions
// See http://www.timeanddate.com/time/zones/ for standard abbreviations and additional information
// Caution: DST rules may change in the future
//
#if 0
//US Eastern Time Zone (New York, Boston)
#define timezone -5 // US Eastern Time Zone
struct dstRule StartRule = {"EDT", Second, Sun, Mar, 2, 3600};    // Daylight time = UTC/GMT -4 hours
struct dstRule EndRule = {"EST", First, Sun, Nov, 2, 0};       // Standard time = UTC/GMT -5 hour
#else
//Australia Eastern Time Zone (Sydney)
#define timezone +10 // Australian Eastern Time Zone
struct dstRule StartRule = {"AEDT", First, Sun, Oct, 2, 3600};    // Daylight time = UTC/GMT +11 hours
struct dstRule EndRule = {"AEST", First, Sun, Apr, 2, 0};      // Standard time = UTC/GMT +10 hour
#endif

// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

// =======================================================================

int h, m, s, day, month, year, dayOfWeek;
String date;
String buf="";

int xPos=0, yPos=0;

int temp, humid;            // No room for decimals!

void setup() 
{
  buf.reserve(500);
  Serial.begin(115200);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN, 1);
  sendCmdAll(CMD_INTENSITY, 0);

  dht.begin();
  
  DEBUG(Serial.print("\n\nConnecting to WiFi ");)
  WiFi.begin(ssid, password);
  clr();
  xPos=0;
  printString("CONNECT..", font3x7);
  refreshAll();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); DEBUG(Serial.print("."));
  }
  clr();
  xPos=0;
  DEBUG(Serial.println(""); Serial.print("MyIP: "); Serial.println(WiFi.localIP());)
  printString((WiFi.localIP().toString()).c_str(), font3x7);
  refreshAll();

  delay(1000);
  
  clr();
  xPos=0;
  printString("INITALISE TIME....", font3x7);
  refreshAll();DEBUG(Serial.println("Reading Initial Time\n");)
  updateNTP(); // Init the NTP time
  delay(2000);

  updateTemp();
}

// =======================================================================

unsigned int curTime,updTime=0;
int dots,mode;

void loop()
{
  curTime = millis();
  if(curTime-updTime>600000) {
    updTime = curTime;
    updateNTP();                    // Update NTP Time every 600s=10m
    updateTime(0);
    updateTemp();
   }
  dots = (curTime % 1000)<500;     // blink 2 times/sec
  mode = (curTime % 60000)/20000;  // change mode every 20s
  updateTime(0);
  if(mode==0) drawTime0(); else
  if(mode==1) drawTime1(); else drawTime2();
  refreshAll();
  delay(100);
}

// =======================================================================

//char* monthNames[] = {"STY","LUT","MAR","KWI","MAJ","CZE","LIP","SIE","WRZ","PAZ","LIS","GRU"};
const char* monthNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
char txt[30];

void drawTime0()          // Updated to display Time in Top Line and Temp in bottom
{
clr();
  yPos = 0;
  xPos = (h>9) ? 0 : 2;
  sprintf(txt,"%d",h);
  printString(txt, digits5x8rn);
  if(dots) printCharX(':', digits5x8rn, xPos);
  xPos+=(h>=22 || h==20)?1:2;
  sprintf(txt,"%02d",m);
  printString(txt, digits5x8rn);
  sprintf(txt,"%02d",s);
  printString(txt, digits3x5);
  yPos = 1;
  xPos = 1;
  sprintf(txt,"T%d H%d",temp, humid);
  printString(txt, font3x7);
  for(int i=0;i<LINE_WIDTH;i++) scr[LINE_WIDTH+i]<<=1;
}

void drawTime1()
{
  clr();
  yPos = 0;
  xPos = (h>9) ? 0 : 3;
  sprintf(txt,"%d",h);
  printString(txt, digits5x16rn);
  if(dots) printCharX(':', digits5x16rn, xPos);
  xPos+=(h>=22 || h==20)?1:2;
  sprintf(txt,"%02d",m);
  printString(txt, digits5x16rn);
  sprintf(txt,"%02d",s);
  printString(txt, font3x7);
}

void drawTime2()
{
  clr();
  yPos = 0;
  xPos = (h>9) ? 0 : 2;
  sprintf(txt,"%d",h);
  printString(txt, digits5x8rn);
  if(dots) printCharX(':', digits5x8rn, xPos);
  xPos+=(h>=22 || h==20)?1:2;
  sprintf(txt,"%02d",m);
  printString(txt, digits5x8rn);
  sprintf(txt,"%02d",s);
  printString(txt, digits3x5);
  yPos = 1;
  xPos = 1;
  sprintf(txt,"%d&%s&%d",day,monthNames[month-1],year-2000);
  printString(txt, font3x7);
  for(int i=0;i<LINE_WIDTH;i++) scr[LINE_WIDTH+i]<<=1;
}

// =======================================================================

int charWidth(char c, const uint8_t *font)
{
  int fwd = pgm_read_byte(font);
  int fht = pgm_read_byte(font+1);
  int offs = pgm_read_byte(font+2);
  int last = pgm_read_byte(font+3);
  if(c<offs || c>last) return 0;
  c -= offs;
  int len = pgm_read_byte(font+4);
  return pgm_read_byte(font + 5 + c * len);
}

// =======================================================================

int stringWidth(const char *s, const uint8_t *font)
{
  int wd=0;
  while(*s) wd += 1+charWidth(*s++, font);
  return wd-1;
}

// =======================================================================

int stringWidth(String str, const uint8_t *font)
{
  return stringWidth(str.c_str(), font);
}

// =======================================================================

int printCharX(char ch, const uint8_t *font, int x)
{
  int fwd = pgm_read_byte(font);
  int fht = pgm_read_byte(font+1);
  int offs = pgm_read_byte(font+2);
  int last = pgm_read_byte(font+3);
  if(ch<offs || ch>last) return 0;
  ch -= offs;
  int fht8 = (fht+7)/8;
  font+=4+ch*(fht8*fwd+1);
  int j,i,w = pgm_read_byte(font);
  for(j = 0; j < fht8; j++) {
    for(i = 0; i < w; i++) scr[x+LINE_WIDTH*(j+yPos)+i] = pgm_read_byte(font+1+fht8*i+j);
    if(x+i<LINE_WIDTH) scr[x+LINE_WIDTH*(j+yPos)+i]=0;
  }
  return w;
}

// =======================================================================

void printChar(unsigned char c, const uint8_t *font)
{
  if(xPos>NUM_MAX*8) return;
  int w = printCharX(c, font, xPos);
  xPos+=w+1;
}

// =======================================================================

void printString(const char *s, const uint8_t *font)
{
  while(*s) printChar(*s++, font);
  //refreshAll();
}

void printString(String str, const uint8_t *font)
{
  printString(str.c_str(), font);
}

// =======================================================================
// MON, TUE, WED, THU, FRI, SAT, SUN
// JAN FEB MAR APR MAY JUN JUL AUG SEP OCT NOV DEC
// Thu, 19 Nov 2015
// decodes: day, month(1..12), dayOfWeek(1-Mon,7-Sun), year
void decodeDate(String date)
{
  switch(date.charAt(0)) {
    case 'M': dayOfWeek=1; break;
    case 'T': dayOfWeek=(date.charAt(1)=='U')?2:4; break;
    case 'W': dayOfWeek=3; break;
    case 'F': dayOfWeek=5; break;
    case 'S': dayOfWeek=(date.charAt(1)=='A')?6:7; break;
  }
  int midx = 6;
  if(isdigit(date.charAt(midx))) midx++;
  midx++;
  switch(date.charAt(midx)) {
    case 'F': month = 2; break;
    case 'M': month = (date.charAt(midx+2)=='R') ? 3 : 5; break;
    case 'A': month = (date.charAt(midx+1)=='P') ? 4 : 8; break;
    case 'J': month = (date.charAt(midx+1)=='A') ? 1 : ((date.charAt(midx+2)=='N') ? 6 : 7); break;
    case 'S': month = 9; break;
    case 'O': month = 10; break;
    case 'N': month = 11; break;
    case 'D': month = 12; break;
  }
  day = date.substring(5, midx-1).toInt();
  year = date.substring(midx+4, midx+9).toInt();
  return;
}

//----------------------- Functions -------------------------------

void updateNTP() {
  
  configTime(timezone * 3600, 0, NTP_SERVERS);

  delay(500);
  while (!time(nullptr)) {
    Serial.print("#");
    delay(1000);
  }
  DEBUG(Serial.println("Getting Time from NTP");)
  updateTime(0);
  DEBUG(Serial.println(String(h) + ":" + String(m) + ":" + String(s)+"   Date: "+day+"."+month+"."+year+" ["+dayOfWeek+"] ");)
}


void updateTime(time_t offset)
{
  char buf[30];
  char *dstAbbrev;
  time_t t = dstAdjusted.time(&dstAbbrev)+offset;
  struct tm *timeinfo = localtime (&t);
  
  h = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
  m = timeinfo->tm_min;
  s = timeinfo->tm_sec;
  day = timeinfo->tm_mday;
  month = timeinfo->tm_mon+1;
  year = timeinfo->tm_year+1900;

  // not setting dayofweek and not sure if used!?

}

// =======================================================================

void updateTemp() {

  float t,h;
  
  t = dht.readTemperature();
  h = dht.readHumidity();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    Serial.println("Using last values");
    return;
  }
    temp = t;
    humid = h;
    DEBUG(Serial.println("Temp = "+String(temp) + "oC : Humid = " + String(humid) + "%\n");)
}

