#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>
#include <FS.h> 
#include <TaskScheduler.h>

// ----- Variáveis globais -----
ESP8266WiFiMulti wifiMulti; 
ESP8266WebServer server(80);
Scheduler runner;
int pressure = 73;
int maxPressure = 140;
int minPressure = 80;
bool pressurizing = false;
bool releasing = false;
bool running = false;
int targetPressure = 140;
String logBuffer = "";

const int AIA = D1; 
const int AIB = D2;
const int BIA = D6;  
const int BIB = D7;  

void handle_start_page();
void handle_getPressure();
void handle_setPressure();
void handle_setOn();
void handle_setOff();
void handle_getLog();
void updatePressure();
void updatePumpStatus();
void handleHTTP();
void addLog();
int getPressure();

Task taskUpdatePressure(20, TASK_FOREVER, &updatePressure);
Task taskUpdatePumpStatus(20, TASK_FOREVER, &updatePumpStatus);
Task taskHandleHTTP(1, TASK_FOREVER, &handleHTTP);

void setup() {
  // put your setup code here, to run once:
  pinMode(AIA, OUTPUT); // set pins to output
  pinMode(AIB, OUTPUT);
  pinMode(BIA, OUTPUT);
  pinMode(BIB, OUTPUT);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Serial comm started.");

  if (!SPIFFS.begin()) {
      Serial.println("An Error has occurred while mounting SPIFFS");
  }

  char bufferName[64];
  char bufferPass[64];
  if (SPIFFS.exists("/aps.lst")) {
    Serial.println("Found AP list, loading.");
    File file = SPIFFS.open("/aps.lst", "r");
    while (file.available()) {
      int l = file.readBytesUntil(',', bufferName, sizeof(bufferName));
      bufferName[l] = 0;
      if (l == 0)
        break;
      Serial.println(String(l));
      l = file.readBytesUntil('\n', bufferPass, sizeof(bufferPass));
      if (l == 0)
        break;
      bufferPass[l-1] = 0;
      Serial.println("[" + String(bufferName) + "]");
      Serial.println("[" + String(bufferPass) + "]");
      if (wifiMulti.addAP(bufferName, bufferPass)) {
        Serial.println("AddAP returned true.");
      } else {
        Serial.println("AddAP returned FALSE.");
      }
    }
    file.close();
  } else {
    Serial.println("AP list not found.");
  }
  Serial.println("Conecting to wifi...");
  int wS = wifiMulti.run();
  while (wS != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(250);
    Serial.println("wS: " + String(wS));
    wS = wifiMulti.run();
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());   

  server.on("/", handle_start_page);
  server.on("/getPressure", handle_getPressure);
  server.on("/setPressure", handle_setPressure);
  server.on("/setOn", handle_setOn);
  server.on("/setOff", handle_setOff);
  server.on("/getLog", handle_getLog);

  // Inicializa o servidor
  server.begin();
  Serial.println("Web Server started.");

  runner.addTask(taskUpdatePressure);
  runner.addTask(taskUpdatePumpStatus);
  runner.addTask(taskHandleHTTP);

  taskUpdatePressure.enable();
  taskHandleHTTP.enable();

  addLog("Startup complete.");
}


void pumpOn() {
  digitalWrite(BIB,HIGH);
  digitalWrite(BIA,LOW);
}

void pumpOff() {
  digitalWrite(BIA,LOW);
  digitalWrite(BIB,LOW);
}

void releaseOn() {
  digitalWrite(AIA,HIGH);
  digitalWrite(AIB,LOW);
}

void releaseOff() {
  digitalWrite(AIA,LOW);
  digitalWrite(AIB,LOW);
}

void updateTargetPressure() {
  if (pressure >= maxPressure) {
    targetPressure = minPressure;
  } else if (pressure <= minPressure) {
    targetPressure = maxPressure;
  }
}

int getPressure() {
  return (1114 - analogRead(A0)) * 0.819;
}

void updatePressure() {
  pressure = getPressure();
}

void updatePumpStatus() {
  updateTargetPressure();
  if (pressure < targetPressure) {
    if (releasing) {
      releaseOff();
      releasing = false;
    }
    if (!pressurizing) {
      pumpOn();
      pressurizing = true;
    }
  } else if (pressure >= targetPressure) {
    if (pressurizing) {
      pumpOff();
      pressurizing = false;
    }
    if (!releasing) {
      releaseOn();
      releasing = true;
    }
  }
}

void handleHTTP() {
  server.handleClient();
}

void loop() {
  // put your main code here, to run repeatedly:
  runner.execute();
}

void handle_start_page()
{
  File file = SPIFFS.open("/start-page.htm", "r");                 // Open it
  size_t sent = server.streamFile(file, "text/html"); // And send it to the client
  file.close();                                       // Then close the file again
}

void handle_getPressure()
{
  server.send(200, "text/html", String(pressure));
}

void handle_setOn()
{
  pressurizing = false;
  releasing = true;
  taskUpdatePumpStatus.enable();
  pumpOff();
  releaseOn();
  addLog("Pumping/release system ACTIVATED.");
  server.send(200, "text/html", "OK");
}

void handle_setOff()
{
  pressurizing = false;
  releasing = false;
  taskUpdatePumpStatus.disable();
  pumpOff();
  releaseOff();
  addLog("Pumping/release system DEACTIVATED.");
  server.send(200, "text/html", "OK");
}

void handle_setPressure()
{
  maxPressure = server.arg("max").toInt();
  minPressure = server.arg("min").toInt();
  targetPressure = minPressure;
  
  addLog("Setting minPressure to [" + String(minPressure,10) + "] and maxPressure to [" + String(maxPressure,10) + "].");

  server.send(200, "text/html", "OK");
}

void handle_getLog()
{
  server.send(200, "text/html", logBuffer);
  logBuffer = "";
}

void addLog(String l) {
  float ms = (millis() / 1000.0);
  logBuffer += "[" + String(ms, 2) + "] " + l + "\n";
}
