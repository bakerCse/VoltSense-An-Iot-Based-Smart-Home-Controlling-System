#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <LiquidCrystal_I2C.h>

// Enter your Wi-Fi credentials here
const char* ssid = "Use your own (e.g. bakerCSE)";
const char* password = "Set your password(e.g. ******)";

ESP8266WebServer server(80);
Adafruit_INA219 ina219;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ESP8266 Relay Pins (Using raw GPIO numbers)
const int relay1 = 14; // D5
const int relay2 = 12; // D6
const int relay3 = 13; // D7
const int relay4 = 0;  // D3

bool r1State = false, r2State = false, r3State = false, r4State = false;

// Hardware Failsafe Flags
bool inaFound = false;
bool lcdFound = false;

unsigned long previousMillis = 0;
const long displayInterval = 3000;
bool showIPScreen = false;

// A string return function to generate the HTML page (feel free to go wild)
String generateHTML() {
  float busvoltage = 0.0;
  float current_mA = 0.0;
  float power_mW = 0.0;

  if (inaFound) {
    busvoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
  }

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='3'>"; 
  html += "<style>body{font-family:Arial; text-align:center; margin-top:30px; background-color:#222; color:white;}";
  html += ".btn{display:inline-block; padding:15px 25px; font-size:20px; margin:10px; cursor:pointer; text-decoration:none; color:white; border-radius:5px;}";
  html += ".on{background-color:#4CAF50;} .off{background-color:#f44336;}";
  html += ".data-box{background-color:#333; padding:15px; margin:15px auto; width:90%; max-width:400px; border-radius:10px;}</style></head><body>";
  
  html += "<h2>IoT Based Smart Inverter System</h2>";
  
  html += "<div class='data-box'>";
  if (inaFound) {
    html += "<h3>Voltage: " + String(busvoltage, 2) + " V</h3>";
    html += "<h3>Current: " + String(current_mA, 1) + " mA</h3>";
    html += "<h3>Power: " + String(power_mW, 1) + " mW</h3>";
  } else {
    html += "<h3 style='color:#f44336;'>Sensor Offline!</h3>";
    html += "<h4>Check INA219 Wiring</h4>";
  }
  html += "</div>";

  if(r1State) html += "<a href='/r1/off' class='btn off'>Relay 1 OFF</a><br>";
  else html += "<a href='/r1/on' class='btn on'>Relay 1 ON</a><br>";

  if(r2State) html += "<a href='/r2/off' class='btn off'>Relay 2 OFF</a><br>";
  else html += "<a href='/r2/on' class='btn on'>Relay 2 ON</a><br>";

  if(r3State) html += "<a href='/r3/off' class='btn off'>Relay 3 OFF</a><br>";
  else html += "<a href='/r3/on' class='btn on'>Relay 3 ON</a><br>";

  if(r4State) html += "<a href='/r4/off' class='btn off'>Relay 4 OFF</a><br>";
  else html += "<a href='/r4/on' class='btn on'>Relay 4 ON</a><br>";

  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", generateHTML());
}

void setup() {
  Serial.begin(115200);

  // 1. Setup Relays First (Immediate safe state)
  pinMode(relay1, OUTPUT); pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT); pinMode(relay4, OUTPUT);
  digitalWrite(relay1, HIGH); digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH); digitalWrite(relay4, HIGH);

  // 2. FORCE I2C PINS: SDA = 4 (D2), SCL = 5 (D1)
  Wire.begin(4, 5);
  
  // 3. Failsafe Ping for LCD
  Wire.beginTransmission(0x27); //you may need to use different memory location if required
  if (Wire.endTransmission() == 0) {
    lcdFound = true;
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Starting WiFi...");
  } else {
    Serial.println("LCD not found at 0x27. Skipping LCD setup.");
  }

  // 4. (Optional) Failsafe Check for INA219
  if (!ina219.begin()) {
    Serial.println("INA219 NOT FOUND. Skipping Sensor setup.");
    inaFound = false;
  } else {
    Serial.println("INA219 Found.");
    inaFound = true;
  }

  // 5. Connect to Wi-Fi
  Serial.println("\nConnecting to Wi-Fi..."); //use your own loading note
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Update LCD only if it exists
  if (lcdFound) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
  }
  
  Serial.println("\nWiFi connected. IP: ");
  Serial.println(WiFi.localIP());

  // 6. Setup Web Server
  server.on("/", handleRoot);
  server.on("/r1/on", [](){ digitalWrite(relay1, LOW); r1State = true; handleRoot(); });
  server.on("/r1/off", [](){ digitalWrite(relay1, HIGH); r1State = false; handleRoot(); });
  server.on("/r2/on", [](){ digitalWrite(relay2, LOW); r2State = true; handleRoot(); });
  server.on("/r2/off", [](){ digitalWrite(relay2, HIGH); r2State = false; handleRoot(); });
  server.on("/r3/on", [](){ digitalWrite(relay3, LOW); r3State = true; handleRoot(); });
  server.on("/r3/off", [](){ digitalWrite(relay3, HIGH); r3State = false; handleRoot(); });
  server.on("/r4/on", [](){ digitalWrite(relay4, LOW); r4State = true; handleRoot(); });
  server.on("/r4/off", [](){ digitalWrite(relay4, HIGH); r4State = false; handleRoot(); });

  server.begin();
}

void loop() {
  server.handleClient(); // Always run web server

  // Update LCD only if it was successfully found
  if (lcdFound) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= displayInterval) {
      previousMillis = currentMillis;

      int activeLoads = r1State + r2State + r3State + r4State;
      lcd.clear(); 
      
      if (showIPScreen) {
        lcd.setCursor(0, 0);
        lcd.print("Web Control IP:");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
      } else {
        lcd.setCursor(0, 0);
        
        if (inaFound) {
          float busvoltage = ina219.getBusVoltage_V();
          float current_mA = ina219.getCurrent_mA();
          lcd.print(busvoltage, 1);
          lcd.print("V ");
          lcd.print(current_mA, 0);
          lcd.print("mA");
        } else {
          lcd.print("Sensor Offline!");
        }

        lcd.setCursor(0, 1);
        lcd.print("Active Loads: ");
        lcd.print(activeLoads);
      }
      
      showIPScreen = !showIPScreen;
    }
  }
}