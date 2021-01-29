#include "Arduino.h"
#include "Adafruit_ADS1015.h"
#include "DFRobot_ESP_EC.h"
#include <EEPROM.h>
#include <WiFi.h> // Include the Wi-Fi library
#include "DFRobot_ESP_PH_WITH_ADC.h"
#include <Wire.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <cString>
#include <SHT2x.h>
#include <HTTPClient.h>
#include <aREST.h>
#define OUTPUT_BUFFER_SIZE 5000
#define NUMBER_VARIABLES 20
#define NUMBER_FUNCTIONS 20

LiquidCrystal_I2C lcd(0x27, 20, 4);
BH1750 lightMeter(0x23);

// Update these with values suitable for your network.

const char *ssid = "Your SSID";
const char *password = "Your Password";
String sn = "2020110001", tanggal = "20165-165-165", waktu = "45:165:85";
char c;

/************************* EC  and PH Initialization **************************/
DFRobot_ESP_EC ec;
DFRobot_ESP_PH_WITH_ADC ph;
Adafruit_ADS1115 ads;
/************************* End Initialization *************************/

/********************** JSN Initialization *****************************/
#include <NewPing.h>
#define trigPin 2
#define echoPin 4
// Define maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500 cm:
#define MAX_DISTANCE 400
// NewPing setup of pins and maximum distance.
NewPing sonar = NewPing(trigPin, echoPin, MAX_DISTANCE);
/**************************** End Initialization *******************************/

#define VREF 3.3 // analog reference voltage(Volt) of the ADC

SHT2x SHT2x;

WiFiClient espClient;
WiFiServer server(80);

float phValue = 0, lastpH = 0, phValueAvg, reservoir_temp, tdsValue, hum, vol, vol2 = 0, ecValue, ecValueAvg, lastEc, voltage, voltage1, temperature, temp = 25;
int pHrelaypin = 33, TDSrelaypin = 25, samplingrelay = 26, pomparelay = 33, i = 1, lux;

//Temperature chip i/o
#define ONE_WIRE_BUS 15
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//rtc & lcd
#define SDA_PIN 21
#define SCL_PIN 22
#define DS3231_I2C_ADDRESS 0x68
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

// Create aREST instance
aREST rest = aREST();

void setup()
{
  Serial.begin(115200);
  lcd.init(); // initialize the lcd
  lcd.backlight();

  setup_wifi();

  pinMode(pHrelaypin, OUTPUT);
  pinMode(TDSrelaypin, OUTPUT);
  pinMode(samplingrelay, OUTPUT);
  pinMode(pomparelay, OUTPUT);
  //turn off all relays
  relay(1, 1, 1);
  delay(1000);
  waktu1();
  EEPROM.begin(32); //needed EEPROM.begin to store calibration k in eeprom
  ph.begin();
  ec.begin();
  //by default lib store calibration k since 10 change it by set ec.begin(30); to start from 30
  ads.setGain(GAIN_ONE);

  // coba kasi address pada i2c ads.begin() dicari pake i2c scanner
  ads.begin();

  sensors.begin(); //DS18B20 start

  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else
  {
    Serial.println(F("Error initialising BH1750"));
  }

  // coba kasi address pada i2c SHT2x.begin() dicari pake i2c scanner
  SHT2x.begin();

  // Init variables and expose them to REST API
  rest.variable("cahaya", &lux);
  rest.variable("temperature", &temp);
  rest.variable("humidity", &hum);
  rest.variable("distance", &vol);
  rest.variable("TDS", &tdsValue);
  rest.variable("reservoir_temp", &reservoir_temp);
  rest.variable("pH", &phValue);

  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("1");
  rest.set_name("HabBandungan");
  Serial.println("REST ID = 1 & NAME = HabBandungan");
  // Start the server
  server.begin();
  Serial.println("Server started");
  lcd.clear();
}

void loop()
{
  displayLcd();
  delay(1000);
  for (i = 1; i < 5; i++)
  {
    read_BH();
    delay(250);
  }

  readSHT();
  delay(1000);
  i = 1;
  for (i = 1; i < 9; i++)
  {
    read_JSN();
    delay(250);
  }
  delay(1000);

  sampling();
  delay(1000);

  read_temp();
  delay(1000);

  relay(1, 0, 1);
  delay(1000);
  i = 1;
  for (i = 1; i < 41; i++)
  {
    static unsigned long timepoint = millis();
    if (millis() - timepoint > 1000U) //time interval: 1s
    {

      timepoint = millis();
      voltage1 = ads.readADC_SingleEnded(0) / 1.50;
      Serial.print("voltage:");
      Serial.println(voltage1, 0);

      //temperature = readTemperature();  // read your temperature sensor to execute temperature compensation
      Serial.print("temperature:");
      Serial.print(temp, 1);
      Serial.println("^C");

      // coba ganti temp dengan reservoir_temp
      ecValue = ec.readEC(voltage1, reservoir_temp); // convert voltage to EC with temperature compensation
      ecValue = (ecValue * 500) / 2;
      if (ecValue >= 3000 || ecValue <= 500)
      {
        ecValue = lastEc;
      }
      else
      {
        lastEc = ecValue;
      }
      //get average pH Value from 25th to 40th measurement
      if (i >= 25)
      {
        ecValueAvg += ecValue;
        ecValue = ecValueAvg / (i - 24);
        if (ecValue <= 3000 && ecValue >= 500)
        {
          lastEc = ecValue;
        }
      }
      Serial.print("EC:");
      Serial.print(ecValue, 4);
      Serial.println("us/cm");
      displayLcd();
    }

    ec.calibration(voltage1, temp); // calibration process by Serail CMD
    delay(1000);
  }

  delay(1000);
  relay(1, 1, 1);
  delay(20000);

  //PH
  relay(0, 1, 1);
  delay(1000);
  i = 1;
  for (i = 1; i < 41; i++)
  {
    static unsigned long timepoint = millis();
    if (millis() - timepoint > 1000U) //time interval: 1s
    {
      timepoint = millis();
      /**
       * index 0 for adc's pin A0
       * index 1 for adc's pin A1
       * index 2 for adc's pin A2
       * index 3 for adc's pin A3
      */
      voltage = ads.readADC_SingleEnded(1) / 10; // read the voltage
      Serial.print("voltage:");
      Serial.println(voltage, 0);

      // coba ganti temp dengan reservoir_temp
      phValue = ph.readPH(voltage, temp); // convert voltage to pH with temperature compensation
                                          //      phValue = phValue + 0.8;
      if (phValue >= 8 || phValue <= 4)
      {
        phValue = lastpH;
      }
      else
      {
        lastpH = phValue;
      }
      //get average pH Value from 25th to 40th measurement
      if (i >= 25)
      {
        phValueAvg += phValue;
        phValue = phValueAvg / (i - 24);
        if (phValue <= 8 && phValue >= 4)
        {
          lastpH = phValue;
        }
      }
      Serial.print("pH:");
      Serial.println(phValue, 4);
      displayLcd();
    }
    ph.calibration(voltage, temp); // calibration process by Serail CMD
    delay(1000);
  }

  delay(1000);
  relay(1, 1, 1);
  delay(1000);

  phValueAvg = 0;
  ecValueAvg = 0;
  timestamp();
  delay(100);
  String sensor1 = "cahaya";
  String sensor2 = "temperature";
  String sensor3 = "humidity";
  String sensor4 = "distance";
  String sensor5 = "TDS";
  String sensor6 = "reservoir_temp";
  String sensor7 = "pH";

  // Here is my API, use your own API
  String postData = (String) "&sn=" + sn.c_str() + "&dgw=" + tanggal.c_str() + "&tgw=" + waktu.c_str() + "&sensor=" + sensor1 + "x" + sensor2 + "x" + sensor3 + "x" + sensor4 + "x" + sensor5 + "x" + sensor6 + "x" + sensor7 + "&nilai=" + lux + "x" + temperature + "x" + hum + "x" + vol + "x" + ecValue + "x" + reservoir_temp + "x" + phValue;

  HTTPClient http;

  // Insert your http link below
  http.begin("your url here" + postData);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  auto httpCode = http.POST(postData);
  Serial.println(postData);
  http.end();

  //Handle client rest, taruh di loop paling atas
  WiFiClient client = server.available();
  if (!client)
  {
    return;
  }
  while (!client.available())
  {
    delay(1);
  }
  rest.handle(client);
}

/***************** sampling state ****************************************/
void sampling()
{
  relay(1, 1, 0);
  //    Serial.print("Pick sample water....");
  //    Serial.println(n);
  delay(10000);
  relay(1, 1, 1);
  delay(1000);
}

//----------------------start of RTC DS3231 function----------------------//
// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ((val / 10 * 16) + (val % 10));
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return ((val / 16 * 10) + (val % 16));
}
void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);                    // set next input to start at the seconds register
  Wire.write(decToBcd(second));     // set seconds
  Wire.write(decToBcd(minute));     // set minutes
  Wire.write(decToBcd(hour));       // set hours
  Wire.write(decToBcd(dayOfWeek));  // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month));      // set month
  Wire.write(decToBcd(year));       // set year (0 to 99)
  Wire.endTransmission();
}
void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}
//----------------------end of RTC DS3231 function----------------------//

//-----------------------RTC--------------------------//
void waktu1()
{
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);

  Serial.print(dayOfMonth, DEC);
  Serial.print("-");
  Serial.print(month, DEC);
  Serial.print("-");
  Serial.print("20");
  Serial.print(year, DEC);
  Serial.print(" ");

  Serial.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute < 10)
  {
    Serial.print("0");
  }
  Serial.print(minute, DEC);
  Serial.print(":");
  if (second < 10)
  {
    Serial.print("0");
  }
  Serial.println(second, DEC);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("      PT. HAB      ");
  lcd.setCursor(0, 1);
  lcd.print("   Mon. Hidroponik  ");
  lcd.setCursor(0, 2);
  lcd.print("   ");
  lcd.print(dayOfMonth, DEC);
  lcd.print("-");
  lcd.print(month, DEC);
  lcd.print("-");
  lcd.print("20");
  lcd.print(year, DEC);
  lcd.print(" ");
  lcd.print(hour, DEC);
  lcd.print(":");
  if (minute < 10)
    lcd.print("0");
  lcd.print(minute, DEC);
  lcd.print("  ");
}
//------------------END of RTC----------------------//

/******************** Read SHT value ***********************/
void readSHT()
{

  hum = SHT2x.GetHumidity();
  if (hum >= 90)
  {
    hum = 90;
  }
  temperature = SHT2x.GetTemperature();

  Serial.print("Humidity(%RH): ");
  Serial.print(hum);
  Serial.print("\tTemperature(C): ");
  Serial.println(temperature);
  displayLcd();
}
/********************* End SHT Rading **************************/

/********************** Relay Value ****************************/
void relay(int pH_state, int TDS_state, int sampling_state)
{
  digitalWrite(pHrelaypin, pH_state);
  digitalWrite(TDSrelaypin, TDS_state);
  digitalWrite(samplingrelay, sampling_state);
}
/********************** End Relay **********************/

// --------------- Reservoir Temperature --------------- //
void read_temp()
{
  sensors.requestTemperatures();
  delay(500);
  reservoir_temp = sensors.getTempCByIndex(0);
  Serial.print("reservoir temperature: ");
  Serial.println(reservoir_temp);
  displayLcd();
  delay(1000);
}
// ----- End Of Measurement ------ //

/******************************* BH1750 Reading *******************************/
void read_BH()
{
  lux = lightMeter.readLightLevel();
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");
  displayLcd();
  delay(1000);
}
/************************** End BH Reading ********************************/

/****************************** Read JSN *********************************/
void read_JSN()
{
  // Measure distance and print to the Serial Monitor:
  Serial.print("Distance = ");
  // Send ping, get distance in cm and print result (0 = outside set distance range):
  vol = sonar.ping_cm();
  vol = 120 - vol;
  if (vol >= 120 || vol <= 40)
  {
    vol = vol2;
  }
  else
  {
    vol2 = vol;
  }
  Serial.print(vol);
  Serial.println(" cm");
  displayLcd();
  delay(500);
}
/***************************** End JSN Reading ******************************/

//------------------- Time Stamps -------------------//
void timestamp()
{
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);

  String hari = String(dayOfMonth, DEC);
  String bulan = String(month, DEC);
  String tahun = String(year, DEC);
  String jam = String(hour, DEC);
  String menit = String(minute, DEC);
  String detik = String(second, DEC);

  if (month < 10)
  {
    tanggal = "20" + tahun + "-" + "0" + bulan + "-" + hari;
  }
  else if (month >= 10)
  {
    tanggal = "20" + tahun + "-" + bulan + "-" + hari;
  }

  if (hour < 10 && minute < 10 && second < 10)
  {
    waktu = "0" + jam + ":" + "0" + menit + ":" + "0" + detik;
  }
  else if (hour < 10 && minute < 10 && second >= 10)
  {
    waktu = "0" + jam + ":" + "0" + menit + ":" + detik;
  }
  else if (hour < 10 && minute >= 10 && second >= 10)
  {
    waktu = "0" + jam + ":" + menit + ":" + detik;
  }
  else if (hour < 10 && minute >= 10 && second < 10)
  {
    waktu = "0" + jam + ":" + menit + ":" + "0" + detik;
  }
  else if (hour < 10 && minute >= 10 && second >= 10)
  {
    waktu = "0" + jam + ":" + menit + ":" + detik;
  }
  else if (hour >= 10 && minute < 10 && second < 10)
  {
    waktu = jam + ":" + "0" + menit + ":" + "0" + detik;
  }
  else if (hour >= 10 && minute >= 10 && second < 10)
  {
    waktu = jam + ":" + menit + ":" + "0" + detik;
  }
  else if (hour >= 10 && minute < 10 && second >= 10)
  {
    waktu = jam + ":" + "0" + menit + ":" + detik;
  }
  else
    waktu = jam + ":" + menit + ":" + detik;

  Serial.print(tanggal);
  Serial.print("   ");
  Serial.print(waktu);
  Serial.print("   ");
}
// ------------- End Of Timestamps --------------- //

/********************* Sensor Display to LCD ***************************/
void displayLcd()
{
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  lcd.setCursor(0, 0);
  lcd.print("   ");
  lcd.print(dayOfMonth, DEC);
  lcd.print("-");
  lcd.print(month, DEC);
  lcd.print("-");
  lcd.print("20");
  lcd.print(year, DEC);
  lcd.print(" ");
  lcd.print(hour, DEC);
  lcd.print(":");
  if (minute < 10)
    lcd.print("0");
  lcd.print(minute, DEC);
  lcd.print("  ");
  lcd.setCursor(0, 1);
  lcd.print("T/H:");
  lcd.print(temperature, 0);
  lcd.print("/");
  lcd.print(hum, 0);
  lcd.setCursor(0, 2);
  lcd.print("pH :");
  lcd.print(phValue, 1);
  lcd.print(" ");
  lcd.setCursor(0, 3);
  lcd.print("TDS:");
  lcd.print(ecValue, 0);
  lcd.print(" ");
  lcd.setCursor(10, 1);
  lcd.print("L :");
  lcd.print(lux);
  lcd.setCursor(10, 2);
  lcd.print("BL:");
  lcd.print(vol, 0);
  lcd.print("  ");
  lcd.setCursor(18, 2);
  lcd.print("cm");
  lcd.setCursor(10, 3);
  lcd.print("WT:");
  lcd.print(reservoir_temp, 1);
  lcd.print((char)223);
  lcd.print("C");
  // delay(5000);
  //  lcd.clear();
}
/************************ End of Display ***************************/

/************************* Setup Wifi *****************************/
void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  lcd.clear();
  waktu1();
  lcd.setCursor(0, 3);
  lcd.print("Connect");
  lcd.setCursor(9, 3);
  lcd.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  lcd.clear();
  waktu1();
  lcd.setCursor(0, 3);
  lcd.print("Connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(3000);
  lcd.clear();
}
/************************** End Setup Wifi **********************/
