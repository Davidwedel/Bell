#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

// WiFi credentials - CHANGE THESE
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Static IP - last octet only (will auto-detect network)
const int STATIC_IP_LAST_OCTET = 215;

// GPIO pins
const int BELL_PIN = 5;        // GPIO pin to control bell relay
const int BUTTON_PIN = 4;      // GPIO pin for physical button
const int LED_PIN = 2;         // Built-in LED on most ESP32 boards

// Bell control
const int BELL_DURATION = 1000; // Bell ring duration in ms
unsigned long bellStartTime = 0;
bool bellActive = false;

// Button debounce
unsigned long lastButtonPress = 0;
const int BUTTON_DEBOUNCE = 500;

// Timezone
String currentTimezone = "UTC0";

// Web server
AsyncWebServer server(80);

// Preferences for storing schedules
Preferences preferences;

// Schedule structure
struct Schedule {
  bool enabled;
  int dayOfWeek;  // 0=Sunday, 1=Monday, ..., 6=Saturday
  int hour;       // 0-23
  int minute;     // 0-59
  bool triggered; // To prevent multiple triggers in same minute
};

const int MAX_SCHEDULES = 20;
Schedule schedules[MAX_SCHEDULES];
int scheduleCount = 0;

// Function prototypes
void ringBell();
void checkSchedules();
void loadSchedules();
void saveSchedules();
void handleButton();

void setup() {
  Serial.begin(115200);

  // Setup GPIO
  pinMode(BELL_PIN, OUTPUT);
  digitalWrite(BELL_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Connect to WiFi with DHCP first to discover network
  Serial.print("Connecting to WiFi (DHCP)");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected via DHCP!");

  // Get network configuration from DHCP
  IPAddress dhcpIP = WiFi.localIP();
  IPAddress gateway = WiFi.gatewayIP();
  IPAddress subnet = WiFi.subnetMask();
  IPAddress dns = WiFi.dnsIP();

  Serial.print("DHCP IP: ");
  Serial.println(dhcpIP);

  // Build static IP using same network but with custom last octet
  IPAddress staticIP(dhcpIP[0], dhcpIP[1], dhcpIP[2], STATIC_IP_LAST_OCTET);

  // Disconnect and reconnect with static IP
  WiFi.disconnect();
  delay(100);

  Serial.print("Reconnecting with static IP: ");
  Serial.println(staticIP);

  if (!WiFi.config(staticIP, gateway, subnet, dns)) {
    Serial.println("Static IP configuration failed!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected with static IP!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Load preferences
  preferences.begin("bell", false);
  currentTimezone = preferences.getString("timezone", "UTC0");

  // Configure time with NTP and timezone
  configTzTime(currentTimezone.c_str(), "pool.ntp.org", "time.nist.gov");

  Serial.print("Timezone set to: ");
  Serial.println(currentTimezone);

  // Load schedules from preferences
  loadSchedules();

  // Setup web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Bell Controller</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 800px;
      margin: 0 auto;
      padding: 20px;
      background: #f0f0f0;
    }
    .container {
      background: white;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
      margin-bottom: 20px;
    }
    h1 { color: #333; margin-top: 0; }
    h2 { color: #555; border-bottom: 2px solid #4CAF50; padding-bottom: 10px; }
    .time-display {
      font-size: 24px;
      font-weight: bold;
      color: #4CAF50;
      margin: 10px 0;
    }
    button {
      background: #4CAF50;
      color: white;
      border: none;
      padding: 10px 20px;
      font-size: 16px;
      border-radius: 5px;
      cursor: pointer;
      margin: 5px;
    }
    button:hover { background: #45a049; }
    button.delete { background: #f44336; }
    button.delete:hover { background: #da190b; }
    .ring-btn {
      background: #ff9800;
      font-size: 20px;
      padding: 15px 30px;
    }
    .ring-btn:hover { background: #e68900; }
    input, select {
      padding: 8px;
      margin: 5px;
      border: 1px solid #ddd;
      border-radius: 4px;
    }
    .schedule-item {
      background: #f9f9f9;
      padding: 10px;
      margin: 10px 0;
      border-radius: 5px;
      border-left: 4px solid #4CAF50;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .schedule-info { flex-grow: 1; }
    .form-group {
      margin: 15px 0;
    }
    label {
      display: inline-block;
      width: 120px;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔔 Bell Controller</h1>
    <div class="time-display" id="currentTime">Loading time...</div>
    <button class="ring-btn" onclick="ringNow()">🔔 Ring Now!</button>
  </div>

  <div class="container">
    <h2>Time Zone Configuration</h2>
    <div class="form-group">
      <label>Time Zone:</label>
      <select id="timezone" style="width: 300px;">
        <option value="UTC0">UTC</option>
        <option value="EST5EDT,M3.2.0,M11.1.0">US Eastern</option>
        <option value="CST6CDT,M3.2.0,M11.1.0">US Central</option>
        <option value="MST7MDT,M3.2.0,M11.1.0">US Mountain</option>
        <option value="PST8PDT,M3.2.0,M11.1.0">US Pacific</option>
        <option value="AKST9AKDT,M3.2.0,M11.1.0">US Alaska</option>
        <option value="HST10">US Hawaii</option>
        <option value="GMT0BST,M3.5.0/1,M10.5.0">UK</option>
        <option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe</option>
        <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe</option>
        <option value="JST-9">Japan</option>
        <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia East</option>
        <option value="NZST-12NZDT,M9.5.0,M4.1.0/3">New Zealand</option>
      </select>
      <button onclick="setTimezone()">Set Timezone</button>
    </div>
    <p style="color: #666; font-size: 14px; margin: 5px 0;">
      Automatically handles Daylight Saving Time (DST) transitions
    </p>
  </div>

  <div class="container">
    <h2>Add New Schedule</h2>
    <div class="form-group">
      <label>Day of Week:</label>
      <select id="dayOfWeek">
        <option value="0">Sunday</option>
        <option value="1">Monday</option>
        <option value="2">Tuesday</option>
        <option value="3">Wednesday</option>
        <option value="4">Thursday</option>
        <option value="5">Friday</option>
        <option value="6">Saturday</option>
      </select>
    </div>
    <div class="form-group">
      <label>Time:</label>
      <input type="number" id="hour" min="0" max="23" value="12" style="width: 60px;"> :
      <input type="number" id="minute" min="0" max="59" value="0" style="width: 60px;">
    </div>
    <button onclick="addSchedule()">Add Schedule</button>
  </div>

  <div class="container">
    <h2>Scheduled Ring Times</h2>
    <div id="scheduleList">Loading schedules...</div>
  </div>

  <script>
    const days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];

    function updateTime() {
      fetch('/time')
        .then(r => r.json())
        .then(data => {
          const day = days[data.dayOfWeek];
          const time = String(data.hour).padStart(2, '0') + ':' +
                       String(data.minute).padStart(2, '0') + ':' +
                       String(data.second).padStart(2, '0');
          document.getElementById('currentTime').textContent = day + ' ' + time;
        });
    }

    function loadSchedules() {
      fetch('/schedules')
        .then(r => r.json())
        .then(data => {
          const list = document.getElementById('scheduleList');
          if (data.schedules.length === 0) {
            list.innerHTML = '<p>No schedules configured.</p>';
            return;
          }
          list.innerHTML = data.schedules.map((s, i) => `
            <div class="schedule-item">
              <div class="schedule-info">
                <strong>${days[s.dayOfWeek]}</strong> at
                ${String(s.hour).padStart(2, '0')}:${String(s.minute).padStart(2, '0')}
              </div>
              <button class="delete" onclick="deleteSchedule(${i})">Delete</button>
            </div>
          `).join('');
        });
    }

    function ringNow() {
      fetch('/ring', { method: 'POST' })
        .then(() => alert('Bell ringing!'));
    }

    function addSchedule() {
      const data = {
        dayOfWeek: parseInt(document.getElementById('dayOfWeek').value),
        hour: parseInt(document.getElementById('hour').value),
        minute: parseInt(document.getElementById('minute').value)
      };

      fetch('/schedule', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
      })
      .then(r => r.json())
      .then(result => {
        if (result.success) {
          loadSchedules();
          alert('Schedule added!');
        } else {
          alert('Error: ' + result.message);
        }
      });
    }

    function deleteSchedule(index) {
      fetch('/schedule/' + index, { method: 'DELETE' })
        .then(r => r.json())
        .then(result => {
          if (result.success) {
            loadSchedules();
          } else {
            alert('Error deleting schedule');
          }
        });
    }

    function loadTimezone() {
      fetch('/timezone')
        .then(r => r.json())
        .then(data => {
          document.getElementById('timezone').value = data.timezone;
        });
    }

    function setTimezone() {
      const timezone = document.getElementById('timezone').value;

      fetch('/timezone', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ timezone: timezone })
      })
      .then(r => r.json())
      .then(result => {
        if (result.success) {
          alert('Timezone updated! Device will restart to apply changes.');
        } else {
          alert('Error updating timezone');
        }
      });
    }

    // Update time and schedules periodically
    updateTime();
    loadTimezone();
    loadSchedules();
    setInterval(updateTime, 1000);
    setInterval(loadSchedules, 5000);
  </script>
</body>
</html>
)rawliteral");
  });

  // Get current time
  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request){
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      request->send(500, "application/json", "{\"error\":\"Failed to obtain time\"}");
      return;
    }

    JsonDocument doc;
    doc["hour"] = timeinfo.tm_hour;
    doc["minute"] = timeinfo.tm_min;
    doc["second"] = timeinfo.tm_sec;
    doc["dayOfWeek"] = timeinfo.tm_wday;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Get all schedules
  server.on("/schedules", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    JsonArray array = doc["schedules"].to<JsonArray>();

    for (int i = 0; i < scheduleCount; i++) {
      if (schedules[i].enabled) {
        JsonObject obj = array.add<JsonObject>();
        obj["dayOfWeek"] = schedules[i].dayOfWeek;
        obj["hour"] = schedules[i].hour;
        obj["minute"] = schedules[i].minute;
      }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Add schedule
  server.on("/schedule", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);

      JsonDocument response;

      if (scheduleCount >= MAX_SCHEDULES) {
        response["success"] = false;
        response["message"] = "Maximum schedules reached";
      } else {
        schedules[scheduleCount].enabled = true;
        schedules[scheduleCount].dayOfWeek = doc["dayOfWeek"];
        schedules[scheduleCount].hour = doc["hour"];
        schedules[scheduleCount].minute = doc["minute"];
        schedules[scheduleCount].triggered = false;
        scheduleCount++;
        saveSchedules();

        response["success"] = true;
      }

      String responseStr;
      serializeJson(response, responseStr);
      request->send(200, "application/json", responseStr);
  });

  // Delete schedule
  server.on("^\\/schedule\\/(\\d+)$", HTTP_DELETE, [](AsyncWebServerRequest *request){
    String indexStr = request->pathArg(0);
    int index = indexStr.toInt();

    JsonDocument doc;

    if (index >= 0 && index < scheduleCount) {
      // Shift remaining schedules
      for (int i = index; i < scheduleCount - 1; i++) {
        schedules[i] = schedules[i + 1];
      }
      scheduleCount--;
      saveSchedules();
      doc["success"] = true;
    } else {
      doc["success"] = false;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Ring bell endpoint
  server.on("/ring", HTTP_POST, [](AsyncWebServerRequest *request){
    ringBell();
    request->send(200, "application/json", "{\"success\":true}");
  });

  // Get timezone
  server.on("/timezone", HTTP_GET, [](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["timezone"] = currentTimezone;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Set timezone
  server.on("/timezone", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);

      String newTimezone = doc["timezone"].as<String>();
      preferences.putString("timezone", newTimezone);

      JsonDocument response;
      response["success"] = true;

      String responseStr;
      serializeJson(response, responseStr);
      request->send(200, "application/json", responseStr);

      // Restart ESP32 to apply timezone change
      delay(500);
      ESP.restart();
  });

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  checkSchedules();
  handleButton();

  // Handle bell timing
  if (bellActive && (millis() - bellStartTime >= BELL_DURATION)) {
    digitalWrite(BELL_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    bellActive = false;
    Serial.println("Bell stopped");
  }

  delay(100);
}

void ringBell() {
  if (!bellActive) {
    digitalWrite(BELL_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    bellActive = true;
    bellStartTime = millis();
    Serial.println("Bell ringing!");
  }
}

void handleButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastButtonPress > BUTTON_DEBOUNCE) {
      ringBell();
      lastButtonPress = millis();
    }
  }
}

void checkSchedules() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return; // Time not yet synced
  }

  int currentDay = timeinfo.tm_wday;
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;

  for (int i = 0; i < scheduleCount; i++) {
    if (schedules[i].enabled &&
        schedules[i].dayOfWeek == currentDay &&
        schedules[i].hour == currentHour &&
        schedules[i].minute == currentMinute) {

      if (!schedules[i].triggered) {
        ringBell();
        schedules[i].triggered = true;
        Serial.printf("Schedule %d triggered\n", i);
      }
    } else {
      // Reset trigger flag when time passes
      schedules[i].triggered = false;
    }
  }
}

void loadSchedules() {
  scheduleCount = preferences.getInt("count", 0);
  if (scheduleCount > MAX_SCHEDULES) scheduleCount = 0;

  for (int i = 0; i < scheduleCount; i++) {
    String prefix = "s" + String(i) + "_";
    schedules[i].enabled = preferences.getBool((prefix + "en").c_str(), false);
    schedules[i].dayOfWeek = preferences.getInt((prefix + "day").c_str(), 0);
    schedules[i].hour = preferences.getInt((prefix + "hr").c_str(), 0);
    schedules[i].minute = preferences.getInt((prefix + "min").c_str(), 0);
    schedules[i].triggered = false;
  }

  Serial.printf("Loaded %d schedules\n", scheduleCount);
}

void saveSchedules() {
  preferences.putInt("count", scheduleCount);

  for (int i = 0; i < scheduleCount; i++) {
    String prefix = "s" + String(i) + "_";
    preferences.putBool((prefix + "en").c_str(), schedules[i].enabled);
    preferences.putInt((prefix + "day").c_str(), schedules[i].dayOfWeek);
    preferences.putInt((prefix + "hr").c_str(), schedules[i].hour);
    preferences.putInt((prefix + "min").c_str(), schedules[i].minute);
  }

  Serial.printf("Saved %d schedules\n", scheduleCount);
}
