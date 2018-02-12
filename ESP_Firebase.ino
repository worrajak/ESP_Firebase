#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <PZEM004T.h>
#include <time.h>
#include <DHT.h>

#define DHTTYPE DHT22
#define DHTPIN  12

// Config WiFi
#define WIFI_SSID     "HUAWEIJAK"
#define WIFI_PASSWORD "12345678"

// Config Firebase
#define FIREBASE_HOST "energypv-monitoring.firebaseio.com"
#define FIREBASE_AUTH "zS6pijifRb7Qv9nSwV9leZmK0pWvBM8Wr108blnK"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

float calBill(float Unit, float ft, bool over_150_Unit_per_month = false) ;

// Config time
int timezone = 7;

// Config bill
#define FIX_FT -15.90

char ntp_server1[20] = "ntp.ku.ac.th";
char ntp_server2[20] = "fw.eng.ku.ac.th";
char ntp_server3[20] = "time.uni.net.th";

int dst = 0;
unsigned long lastUpdateEnergy = 0, lastUpdateFirebase = 0;
float Volt, Amp, Power;
unsigned long Energy = 0;

PZEM004T pzem(12, 14);  // (RX,TX) connect to TX,RX of PZEM
IPAddress ip(192, 168, 43, 100);

DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266

void setup() {
  Serial.begin(115200);
/*
  Serial.println();
  while (!pzem.setAddress(ip)) {
    Serial.println("Connecting to PZEM...");
    delay(500);
  }
*/
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  configTime(timezone * 3600, dst, ntp_server1, ntp_server2, ntp_server3);
  Serial.println("Waiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Now: " + getTime());

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

void loop() {
  if ((millis() - lastUpdateEnergy) >= 1000) {
    lastUpdateEnergy = millis();

    unsigned long startTime = millis();
    Volt = pzem.voltage(ip);
    Volt = Volt > 0 ? Volt : 0;
    Amp = pzem.current(ip);
    Amp = Amp > 0 ? Amp : 0;
    Power = Volt * Amp;
    Energy += Power;
    /*Serial.print("Load time: ");
    Serial.print(millis() - startTime);
    Serial.println("ms");
    Serial.println("Voltage: " + String(Volt, 2));
    Serial.println("Current: " + String(Amp, 2));
    Serial.println("Power: " + String(Power, 2));
    Serial.println("Energy: " + String(Energy));
    Serial.println("--------------------");
    */
  }

  time_t now = time(nullptr);
  struct tm* nowTime = localtime(&now);
  if ((nowTime->tm_sec % 30) == 0) {
    lastUpdateFirebase = millis();

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["volt"] = Volt;
    root["amp"] = Amp;
    root["time"] = getTime();

    String name = Firebase.push("/logPower", root);
    // handle error
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    Serial.print("pushed: /logPower/");
    Serial.println(name);

    Firebase.setInt("/energy", Energy);
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }

    Firebase.setFloat("/amount", calBill(Energy / 1000, FIX_FT, false));
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
  }

  delay(0); // Disable WDT
}

String getTime() {
  time_t now = time(nullptr);
  struct tm* newtime = localtime(&now);

  String tmpNow = "";
  tmpNow += String(newtime->tm_year + 1900);
  tmpNow += "-";
  tmpNow += String(newtime->tm_mon + 1);
  tmpNow += "-";
  tmpNow += String(newtime->tm_mday);
  tmpNow += " ";
  tmpNow += String(newtime->tm_hour);
  tmpNow += ":";
  tmpNow += String(newtime->tm_min);
  tmpNow += ":";
  tmpNow += String(newtime->tm_sec);
  return tmpNow;
}

float calBill(float Unit, float ft, bool over_150_Unit_per_month) {
  float Service = over_150_Unit_per_month ? 38.22 : 8.19;

  float total = 0;

  if (!over_150_Unit_per_month) {
    float Rate15 = 2.3488;
    float Rate25 = 2.9882;
    float Rate35 = 3.2405;
    float Rate100 = 3.6237;
    float Rate150 = 3.7171;
    float Rate400 = 4.2218;
    float RateMore400 = 4.4217;

    if (Unit >= 6) total += min(Unit, 15) * Rate15;
    if (Unit >= 16) total += min(Unit - 15, 10) * Rate25;
    if (Unit >= 26) total += min(Unit - 25, 10) * Rate35;
    if (Unit >= 36) total += min(Unit - 35, 65) * Rate100;
    if (Unit >= 101) total += min(Unit - 100, 50) * Rate150;
    if (Unit >= 151) total += min(Unit - 150, 250) * Rate400;
    if (Unit >= 401) total += (Unit - 400) * RateMore400;
  } else {
    float Rate150 = 3.2484;
    float Rate400 = 4.2218;
    float RateMore400 = 4.4217;

    total += min(Unit, 150) * Rate150;
    if (Unit >= 151) total += min(Unit - 150, 150) * Rate400;
    if (Unit >= 401) total += (Unit - 400) * RateMore400;
  }

  total += Service;
  total += Unit * (ft / 100);
  total += total * 7 / 100;

  return total;
}
