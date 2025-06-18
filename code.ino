#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>
#include <esp_wifi.h>
#include <SPIFFS.h> 

// EEPROM and login
#define EEPROM_SIZE 96
#define SSID_ADDR 0
#define PASS_ADDR 32
#define LOGIN_USER "admin"
#define LOGIN_PASS "admin"

WebServer server(80);

// LED pins: 5 for signal strength, 1 for online (green), 1 for offline (red)
const int ledPins[] = {17, 27, 26, 25, 33, 32, 12};  // 12 = offline, 32 = online
const int totalLEDs = 5;
int delayTime = 100;
bool isDisconnected = true;

// WiFi credentials
char ssid[32] = "SSID_NAME";
char pass[32] = "SSID_PASSWORD";

// Access Point credentials (static)
const char* apSSID = "NetMon";
const char* apPASS = "12345678";


String weatherApiKey = "WEATHER_API_KEY";//Signup at https://www.weatherapi.com/ to get your API key
String city = "Oye Ekiti";
String weatherData = "Fetching...";

// Time
String currentTime = "Getting time...";

// Auth
bool isAuthenticated = false;

void readCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 31; i++) {
    ssid[i] = EEPROM.read(SSID_ADDR + i);
    pass[i] = EEPROM.read(PASS_ADDR + i);
  }
  ssid[31] = 0; // null-terminate
  pass[31] = 0;
  EEPROM.end();
}

void writeCredentials(const char* newSsid, const char* newPass) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 32; i++) {
    EEPROM.write(SSID_ADDR + i, newSsid[i]);
    EEPROM.write(PASS_ADDR + i, newPass[i]);
    if (newSsid[i] == 0 && newPass[i] == 0) break;
  }
  EEPROM.commit();
  EEPROM.end();
}


// ---------- Time ----------
void setupTime() {
  configTime(3600, 0, "pool.ntp.org", "time.nist.gov");  // 3600 = 1 hour offset
}

String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "NTP error";
  char timeString[64];
  strftime(timeString, sizeof(timeString), "%I:%M:%S %p", &timeinfo);  // AM/PM format
  return String(timeString);
}

// ---------- Weather ----------
void fetchWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String cityEncoded = city;
    cityEncoded.replace(" ", "%20");
    String endpoint = "http://api.weatherapi.com/v1/current.json?key=" + weatherApiKey + "&q=" + cityEncoded;
    http.begin(endpoint);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        float temp_c = doc["current"]["temp_c"];
        const char* condition = doc["current"]["condition"]["text"];
        weatherData = String(condition) + ", " + String(temp_c) + "°C";
      } else {
        weatherData = "JSON parse error";
      }
    } else {
      weatherData = "Weather fetch failed!";
    }
    http.end();
  } else {
    weatherData = "No internet";
  }
}

void ledSearch() {
  for (int i = 0; i < totalLEDs; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(delayTime);
    digitalWrite(ledPins[i], LOW);
  }
  for (int i = totalLEDs - 1; i >= 0; i--) {
    digitalWrite(ledPins[i], HIGH);
    delay(delayTime);
    digitalWrite(ledPins[i], LOW);
  }
}


// ---------- Web Handlers ----------
String dashboardPage(String currentSSID, String currentPASS, String apSSID, String apPASS) {
  return R"rawliteral(<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>NetMon::Dashboard</title>
    <link rel="stylesheet" href="/milligram.min.css" />
    <style>
      body { background-color: #121212; color: #ffffff; margin: 0; }
      nav { position: fixed; z-index: 9; top: 0; left: 0; right: 0; background-color: #1e1e1e; padding: 1.5rem 2rem; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #333; }
      nav a { color: #ffffff; text-decoration: none; margin-left: 1rem; }
      nav a:hover { text-decoration: underline; }
      nav a.active { color: #9b4dca; font-weight: bold; }
      .main-content { padding: 2rem; }
      .card { background-color: #1e1e1e; border-radius: 8px; padding: 3rem; box-shadow: 0 0 10px rgba(0, 0, 0, 0.5); margin-bottom: 1rem; margin-top: 5rem; border: 1px solid #33333388; }
      input[type="text"], input[type="password"] { background-color: #2c2c2c; color: #ffffff; border: 1px solid #444; font-weight: 100; }
      h4 { font-weight: 500; }
      li { opacity: 0.8; }
      fieldset { border: 1px solid #333333; border-radius: 10px; padding: 2rem; }
      label, legend { font-weight: 300; }
    </style>
  </head>
  <body>
    <nav>
      <div class="container" style="display: flex; justify-content: space-between; align-items: center;">
        <span>Net<b style="color: #9b4dca">Mon</b></span>
        <div style="display:flex;justify-content:center;align-items:center;">
          <a href="#home" data-page="Home" onclick="loadPage('Home')">Home</a>
          <a href="#settings" data-page="Settings" onclick="loadPage('Settings')">Settings</a>
          <a href="#about" data-page="About This Device" onclick="loadPage('About This Device')">About</a>
          <a href="https://github.com/JohnPraise247/NetMon" target="_blank" title="View Source code" style="display:flex"><svg xmlns="http://www.w3.org/2000/svg"  viewBox="0 0 30 30" width="20px" height="20px" fill="#ffffff">    <path d="M15,3C8.373,3,3,8.373,3,15c0,5.623,3.872,10.328,9.092,11.63C12.036,26.468,12,26.28,12,26.047v-2.051 c-0.487,0-1.303,0-1.508,0c-0.821,0-1.551-0.353-1.905-1.009c-0.393-0.729-0.461-1.844-1.435-2.526 c-0.289-0.227-0.069-0.486,0.264-0.451c0.615,0.174,1.125,0.596,1.605,1.222c0.478,0.627,0.703,0.769,1.596,0.769 c0.433,0,1.081-0.025,1.691-0.121c0.328-0.833,0.895-1.6,1.588-1.962c-3.996-0.411-5.903-2.399-5.903-5.098 c0-1.162,0.495-2.286,1.336-3.233C9.053,10.647,8.706,8.73,9.435,8c1.798,0,2.885,1.166,3.146,1.481C13.477,9.174,14.461,9,15.495,9 c1.036,0,2.024,0.174,2.922,0.483C18.675,9.17,19.763,8,21.565,8c0.732,0.731,0.381,2.656,0.102,3.594 c0.836,0.945,1.328,2.066,1.328,3.226c0,2.697-1.904,4.684-5.894,5.097C18.199,20.49,19,22.1,19,23.313v2.734 c0,0.104-0.023,0.179-0.035,0.268C23.641,24.676,27,20.236,27,15C27,8.373,21.627,3,15,3z"/></svg></a>
        </div>
      </div>
    </nav>

    <div class="container main-content">
      <div class="card">
        <h4 id="content-title">Welcome</h4>
        <div id="content-body"><p>Loading...</p></div>
      </div>
    </div>

    <script>
      function loadPage(page) {
        const title = document.getElementById("content-title");
        const content = document.getElementById("content-body");
        const links = document.querySelectorAll("nav a");

        links.forEach(link => link.classList.toggle("active", link.getAttribute("data-page") === page));

        switch (page) {
          case "Home":
            title.textContent = "Home";
            content.innerHTML = `
              <small style="opacity:.7;">Internet status: {{INTERNET}}</small><br>
              <small style="opacity:.7;">Wifi Strength:</small><meter value="{{WIFISTRENGTH}}" low="2" high="5" max="5"></meter>
              <div class="row" style="margin-top:20px;">
                <fieldset class="column" style="margin-right:10px">
                  <legend>Current Time</legend>
                  <h2 id="time">{{TIME}}</h2>
                </fieldset>
                <fieldset class="column">
                  <legend>Weather</legend>
                  <h2>{{WEATHER}}</h2>
                </fieldset>
              </div>
              <div class="row" style="margin-top:20px;">
                <fieldset class="column">
                  <legend>Connected Users</legend>
                  <ol>{{CLIENTS}}</ol>
                </fieldset>
              </div>`;
            break;
          case "Settings":
            title.textContent = "Settings";
            content.innerHTML = `
             <form action="/weather" method='POST'>
              <fieldset>
                <legend>WeatherAPI config</legend>
                <input type="text" name='location' placeholder="Location" value="{{LOCATION}}"><br><br>
                <input type="password" name='apikey' placeholder="WeatherAPI Key" value="{{APIKEY}}" readonly><br><br>
                <a target="_blank" href="https://www.weatherapi.com/"><small>Learn more</small></a>
                <input type="submit" class="float-right" value="Save config">
              </fieldset>
            </form>
            <form action="/wifi" method='POST'>
              <fieldset>
                <legend>Network Autoconnect</legend>
                <input type="text" name='ssid' placeholder="SSID Name" value="{{SSID}}"><br><br>
                <input type="password" name='pass' placeholder="SSID Password" value="{{PASS}}"><br><br>
                <input type="submit" class="float-right" value="Save Wi-fi">
              </fieldset>
            </form>
            <fieldset>
              <legend>Access Point (Hotspot)</legend>
              <input type="text" id="apssid" placeholder="AP Name" value="{{APSSID}}" disabled><br><br>
              <input type="password" id="appass" placeholder="AP Password" value="{{APPASS}}" disabled><br><br>
            </fieldset>`;
            break;
          case "About This Device":
            title.textContent = "About";
            content.innerHTML = `
              <p>A simple ESP project created by Group 2 for CPE 502.</p>
              <h5><b>Group 2 Members</b></h5>
              <li>Praise Olatunji John</li>
              <li>Owotuyi Daniel Ayodeji</li>
              <li>Idoko Nelson Ehi</li>
              <li>Olumide Queen Ayanfeoluwa</li>
              <li>Ezekiel Ayodeji John</li>
              <li>Fadeyi Iyanuoluwa Samuel</li>
              <li>Dada David Anuoluwapo</li>
              <li>Akinoluwa Sharon Erioluwa</li>
              <li>Igbojionu Emmanuel Chibueze</li>
              <li>Obidere Micheal Oluwasegun</li>
            `;
            break;
          default:
            title.textContent = "Welcome";
            content.innerHTML = "<p>This is your ESP32 dashboard. Select a section to begin.</p>";
        }

        const hash = page.toLowerCase().replace(/\s/g, "");
        window.location.hash = hash;
      }

      setInterval(() => {
  fetch('/api/time')
    .then(res => res.text())
    .then(time => {
      const timeEl = document.getElementById('time');
      if (timeEl) {
        timeEl.textContent = time;
      }
    });
}, 1000); // update every second

setInterval(() => {
  fetch('/api/strength')
    .then(res => res.text())
    .then(strength => {
      const meter = document.querySelector('meter');
      if (meter) meter.value = strength;
    });
}, 2000);

      window.addEventListener("DOMContentLoaded", () => {
        const hash = window.location.hash.substring(1);
        const pageMap = { home: "Home", settings: "Settings", about: "About This Device" };
        const page = pageMap[hash] || "Home";
        loadPage(page);
      });
    </script>
  </body>
</html>)rawliteral";
}

int getWiFiStrengthLevel() {
  // if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -50) return 5;
  else if (rssi >= -60) return 4;
  else if (rssi >= -70) return 3;
  else if (rssi >= -80) return 2;
  else if (rssi >= -90) return 1;
  else return 0;
}

void handleLoginPage() {
  if (isAuthenticated) {
    server.sendHeader("Location", "/dashboard");
    server.send(302, "text/plain", "");
    return;
  }

  server.send(200, "text/html", R"rawliteral(<!DOCTYPE html><html><head><meta http-equiv="content-type" content="text/html; charset=utf-8" /><meta name="viewport" content="width=device-width, initial-scale=1.0" /><title>NetMon::Login</title><link rel="stylesheet" href="/milligram.min.css" /><style> body { background-color: #121212; color: #ffffff; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; } .container { max-width: 350px; width: 100%; padding: 2rem; background-color: #1e1e1e; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.5); border: 1px solid #33333388; } input[type="text"], input[type="password"] { background-color: #2c2c2c; color: #ffffff; border: 1px solid #444; font-weight:100; } input[type="submit"] { width:100%; } h3 { text-align: center; margin-bottom: 1.5rem; font-weight: 300; } form{ margin:0; } </style></head><body><div class="container"><h3>Sign In</h3><form action='/login' method='POST' autocomplete="off"><input type="text" name='user' placeholder="Username" /><input type="password" name='pass' placeholder="Password" /><input type="submit" value="Login" /></form></div></body></html>)rawliteral");
}

void handleLogin() {
  if (server.method() == HTTP_POST && server.hasArg("user") && server.hasArg("pass")) {
    if (server.arg("user") == LOGIN_USER && server.arg("pass") == LOGIN_PASS) {
      isAuthenticated = true;
      server.sendHeader("Location", "/dashboard");
      server.send(302, "text/plain", "");
      return;
    }
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

String getInternetStatus() {
  return (WiFi.status() == WL_CONNECTED) ? "Online" : "Offline";
}

String getConnectedClients() {
  wifi_sta_list_t stationList;
  esp_err_t result = esp_wifi_ap_get_sta_list(&stationList);
  String list = "";

  if (result == ESP_OK) {
    for (int i = 0; i < stationList.num; i++) {
      wifi_sta_info_t sta = stationList.sta[i];
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
              sta.mac[0], sta.mac[1], sta.mac[2],
              sta.mac[3], sta.mac[4], sta.mac[5]);
      list += "<li>" + String(macStr) + "</li>";
    }
  } else {
    list = "<li>Error fetching clients</li>";
  }

  return list == "" ? "<li>No other users</li>" : list;
}

void handleDashboard() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
    return;
  }

  currentTime = getFormattedTime();
  fetchWeather();

  String page = dashboardPage(ssid, pass, apSSID, apPASS);
  page.replace("{{TIME}}", currentTime);
  page.replace("{{WEATHER}}", weatherData);
  page.replace("{{INTERNET}}", getInternetStatus());
  page.replace("{{CLIENTS}}", getConnectedClients());
  page.replace("{{SSID}}", ssid);
  page.replace("{{PASS}}", pass);
  page.replace("{{APSSID}}", apSSID);
  page.replace("{{APPASS}}", apPASS);
  page.replace("{{WIFISTRENGTH}}", String(getWiFiStrengthLevel())); 
  page.replace("{{LOCATION}}", city);
  page.replace("{{APIKEY}}", weatherApiKey);

  server.send(200, "text/html", page);
}

void handleWiFiSave() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
    return;
  }

  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");

    if (newSsid.length() > 0 && newPass.length() < 32) {
      newSsid.toCharArray(ssid, 32);
      newPass.toCharArray(pass, 32);
      writeCredentials(ssid, pass);

       server.send(200, "text/html", R"rawliteral(<!DOCTYPE html><html><head><meta http-equiv="content-type" content="text/html; charset=utf-8" /><meta name="viewport" content="width=device-width, initial-scale=1.0" /><title>NetMon::Settings saved</title><link rel="stylesheet" href="/milligram.min.css" /><style> body { background-color: #121212; color: #ffffff; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; } .container { max-width: 350px; width: 100%; padding: 2rem; background-color: #1e1e1e; border-radius: 8px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.5); text-align:center; } h3 { text-align: center; margin-bottom: 1.5rem; font-weight: 300; } small{ opacity:.5; } </style></head><body><div class="container"><h3>Info</h3> Wi-Fi credentials saved. Reconnecting...<br><small>(Or just unplug and plug back in)</small></div></body></html>)rawliteral");

      delay(1000);
      ESP.restart();
    }
  }
}


void handleWeatherConfig() {
  if (!isAuthenticated) {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
    return;
  }

  if (server.hasArg("location")) {
    city = server.arg("location");
  }
  if (server.hasArg("apikey")) {
    weatherApiKey = server.arg("apikey");
  }

  server.sendHeader("Location", "/dashboard");
  server.send(302, "text/plain", "");
}

void handleMilligramCSS() {
  server.on("/milligram.min.css", HTTP_GET, []() {
    File f = SPIFFS.open("/milligram.min.css", "r");
    if (f) {
      server.streamFile(f, "text/css");
      f.close();
    } else {
      server.send(404, "text/plain", "CSS not found");
    }
  });
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);  

  // WiFi credentials
  strcpy(ssid, "SSID_NAME");
  strcpy(pass, "SSID_PASSWORD");
  readCredentials();
  
  for (int i = 0; i < totalLEDs; i++) pinMode(ledPins[i], OUTPUT);
  pinMode(ledPins[5], OUTPUT); // Online (green)
  pinMode(ledPins[6], OUTPUT); // Offline (red)
  digitalWrite(ledPins[6], HIGH);  // Assume offline initially

  // Enable AP + STA mode
  WiFi.mode(WIFI_AP_STA);

  // Start Access Point always
  WiFi.softAP(apSSID, apPASS);
  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());

  WiFi.begin(ssid, pass);
  Serial.println("Connecting to WiFi...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    ledSearch();
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    digitalWrite(ledPins[6], LOW);  // Offline OFF
  } else {
    Serial.println("\nWiFi STA failed. Still running as AP.");
  }

  // Start HTTP Server
  setupTime();
  SPIFFS.begin();
  handleMilligramCSS();
  server.on("/", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/dashboard", HTTP_GET, handleDashboard);
  server.on("/wifi", HTTP_POST, handleWiFiSave);
  server.on("/weather", HTTP_POST, handleWeatherConfig);
  server.on("/api/time", HTTP_GET, []() {
    server.send(200, "text/plain", getFormattedTime());
  });
  server.on("/api/strength", HTTP_GET, []() {
    server.send(200, "text/plain", String(getWiFiStrengthLevel()));
  });
  server.onNotFound([]() {
    Serial.println("Redirecting unknown path: " + server.uri());
    server.send(200, "text/html", "<meta http-equiv='refresh' content='0; url=/' />");
  });
  server.begin();
  Serial.println("Web server started.");
}

void checkInternetConnection() {
  HTTPClient http;
  http.setTimeout(3000);
  http.begin("http://example.com");

  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("HTTP OK: %d\n", httpCode);
    digitalWrite(ledPins[5], HIGH);   // Online LED ON
    digitalWrite(ledPins[6], LOW);    // Offline LED OFF
  } else {
    Serial.printf("HTTP Fail: %s\n", http.errorToString(httpCode).c_str());
    digitalWrite(ledPins[5], LOW);
    isDisconnected ? digitalWrite(ledPins[6], LOW) : digitalWrite(ledPins[6], HIGH);
  }

  http.end();
}

void loop() {
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    isDisconnected = false;
    long rssi = WiFi.RSSI();
    int ledsToLight = map(rssi, -100, -40, 0, totalLEDs);
    ledsToLight = constrain(ledsToLight, 0, totalLEDs);

    for (int i = 0; i < totalLEDs; i++) {
      digitalWrite(ledPins[i], i < ledsToLight ? HIGH : LOW);
    }
  } else {
    isDisconnected = true;
    digitalWrite(ledPins[6], LOW);  // Blink search
    ledSearch();
  }

  checkInternetConnection();
  delay(1000);
}
