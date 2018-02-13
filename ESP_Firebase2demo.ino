#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <time.h>
#include <DHT.h>
#include <WiFiUdp.h>

#define DHTTYPE DHT22
#define DHTPIN  12

// Config WiFi
#define WIFI_SSID     " "
#define WIFI_PASSWORD " "

// Config Firebase
#define FIREBASE_HOST "monitoring.firebaseio.com"
#define FIREBASE_AUTH "AAApijifRb7Qv9nSwV9leZmK0pWvBM8Wr108blnN"
#define datLog "/logTempHumidPQLab"


#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

int timezone = 7;
int dst = 0;

unsigned long lastUpdateEnergy = 0, lastUpdateFirebase = 0;

//IPAddress ip(192, 168, 43, 100);

DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266


void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());
  configTime(timezone * 3600, dst * 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}
int Log_flag=0;
long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 1.0;

time_t prevDisplay = 0;

void loop() {
  long now_t = millis();
  if (now_t - lastMsg > 5000) {
    lastMsg = now_t;

    float newTemp = dht.readTemperature();
    float newHum = dht.readHumidity();

    if (checkBound(newTemp, temp, diff)) {
      temp = newTemp;
      Serial.print("New temperature:");
      Serial.println(String(temp).c_str());
    }

    if (checkBound(newHum, hum, diff)) {
      hum = newHum;
      Serial.print("New humidity:");
      Serial.println(String(hum).c_str());
    }
  }
  time_t now = time(nullptr);
  struct tm* nowTime = localtime(&now);
  if (((nowTime->tm_min % 5) == 0)&&(Log_flag == 0)) {
    Log_flag=1;
    lastUpdateFirebase = millis();
    Serial.print("pushing databased ");
    Serial.println(getTime());

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["Temp"] = temp;
    root["Humid"] = hum;
    root["time"] = getTime();

    String name = Firebase.push(datLog, root);
    // handle error
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    Serial.print("pushed: ");
    Serial.print(datLog);
    Serial.print(" ");
    Serial.println(name);
    
    }
    if((nowTime->tm_min % 5)!=0){
      Log_flag=0;
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
