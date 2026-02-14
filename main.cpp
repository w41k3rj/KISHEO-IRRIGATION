#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/* ================= WEB PAGE ================= */

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Smart Tank</title>
<style>
body{font-family:Arial;text-align:center;background:#0a192f;color:white;}
.card{background:#112240;padding:20px;margin:10px;border-radius:10px;}
</style>
</head>
<body>
<h2>SMART WATER TANK</h2>

<div class="card">Level: <span id="level"></span>%</div>
<div class="card">Tank Status: <span id="tank"></span></div>
<div class="card">Pump: <span id="pump"></span></div>
<div class="card">Last Filled: <span id="last"></span></div>
<div class="card">Monthly Fills: <span id="monthly"></span></div>

<script>
setInterval(()=>{
fetch("/data")
.then(res=>res.json())
.then(data=>{
document.getElementById("level").innerHTML=data.level.toFixed(1);
document.getElementById("tank").innerHTML=data.tank;
document.getElementById("pump").innerHTML=data.pump;
document.getElementById("last").innerHTML=data.last;
document.getElementById("monthly").innerHTML=data.monthly;
});
},2000);
</script>

</body>
</html>
)rawliteral";

/* ================= PINS ================= */

#define TRIG 5
#define ECHO 18
#define RELAY 23
#define BUZZER 19

/* ================= WIFI ================= */

const char* ssid = "w41k3rj";
const char* password = "123456789";

/* ================= OBJECTS ================= */

AsyncWebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

/* ================= VARIABLES ================= */

float tankHeight = 10.0;   
float waterLevelPercent = 0;

String tankStatus = "UNKNOWN";
String pumpStatus = "OFF";
String lastFilled = "Never";

int monthlyFills = 0;
bool alreadyCounted = false;

/* ================= FUNCTIONS ================= */

long readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  return pulseIn(ECHO, HIGH, 30000) * 0.034 / 2;
}

/* ================= SETUP ================= */

void setup() {

  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 STARTING...");


  Serial.println("\n==============================");
  Serial.println(" SMART WATER TANK SYSTEM ");
  Serial.println("==============================");

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY, HIGH);
  digitalWrite(BUZZER, LOW);

  /* ===== WIFI CONNECT ===== */

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n------------------------------");
  Serial.println(" WIFI CONNECTED SUCCESSFULLY ");
  Serial.print(" IP ADDRESS: ");
  Serial.println(WiFi.localIP());
  Serial.println("------------------------------");

  timeClient.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"level\":" + String(waterLevelPercent) + ",";
    json += "\"tank\":\"" + tankStatus + "\",";
    json += "\"pump\":\"" + pumpStatus + "\",";
    json += "\"last\":\"" + lastFilled + "\",";
    json += "\"monthly\":" + String(monthlyFills);
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

/* ================= LOOP ================= */

void loop() {

  timeClient.update();

  long distance = readDistance();

  if(distance <= 0 || distance > tankHeight){
    Serial.println("⚠ Ultrasonic Sensor Error!");
    delay(2000);
    return;
  }

  float waterHeight = tankHeight - distance;
  waterLevelPercent = (waterHeight / tankHeight) * 100;
  waterLevelPercent = constrain(waterLevelPercent, 0, 100);

  if (waterLevelPercent <= 20) {

    digitalWrite(RELAY, LOW);
    pumpStatus = "ON";
    tankStatus = "FILLING";
    alreadyCounted = false;
  }

  else if (waterLevelPercent >= 90 && !alreadyCounted) {

    digitalWrite(RELAY, HIGH);
    pumpStatus = "OFF";
    tankStatus = "FULL";

    digitalWrite(BUZZER, HIGH);
    delay(500);
    digitalWrite(BUZZER, LOW);

    lastFilled = timeClient.getFormattedTime();
    monthlyFills++;
    alreadyCounted = true;
  }

  else {
    tankStatus = "NORMAL";
  }

  /* ===== SERIAL MONITOR OUTPUT ===== */

  Serial.println("====================================");
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  Serial.print("Water Level: ");
  Serial.print(waterLevelPercent);
  Serial.println(" %");

  Serial.print("Pump Status: ");
  Serial.println(pumpStatus);

  Serial.println("====================================\n");

  delay(2000);
}
