#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <DHT.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Authentication Configuration
const char* www_username = "admin";
const char* www_password = "admin123";

// DHT Configuration
#define DHTPIN D4      // GPIO2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Web Server
ESP8266WebServer server(80);

// Data Logging
const int MAX_LOG_ENTRIES = 1440; // 24 hours * 60 minutes
float temperatureLog[MAX_LOG_ENTRIES];
float humidityLog[MAX_LOG_ENTRIES];
unsigned long timeLog[MAX_LOG_ENTRIES];
int logIndex = 0;
bool logInitialized = false;

// System Monitoring Variables
unsigned long startTime = 0;
unsigned long totalRequests = 0;
float minTemp = 1000;
float maxTemp = -1000;
float avgTemp = 0;
float tempSum = 0;
int tempReadings = 0;

// Function Prototypes
bool checkAuth();
void handleLogin();
void handleRoot();
void handleData();
void handleChartData();
void handleSystemInfo();
void handleLogout();
void handleNotFound();
String getContentType(String filename);
bool handleFileRead(String path);
void readDHTData();
void updateStatistics(float temp, float hum);

void setup() {
  Serial.begin(115200);
  
  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  
  // Initialize DHT sensor
  dht.begin();
  delay(1000);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize MDNS
  if (!MDNS.begin("esp8266-dht")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("MDNS responder started: esp8266-dht.local");
  }
  
  // Initialize arrays
  for (int i = 0; i < MAX_LOG_ENTRIES; i++) {
    temperatureLog[i] = 0;
    humidityLog[i] = 0;
    timeLog[i] = 0;
  }
  
  startTime = millis();
  
  // Setup server routes
  server.on("/", HTTP_GET, []() {
    if (!checkAuth()) {
      server.sendHeader("Location", "/login");
      server.send(302, "text/plain", "");
      return;
    }
    handleRoot();
  });
  
  server.on("/login", HTTP_GET, []() {
    handleLogin();
  });
  
  server.on("/login", HTTP_POST, []() {
    if (server.hasArg("username") && server.hasArg("password")) {
      if (server.arg("username") == www_username && server.arg("password") == www_password) {
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "Login successful");
        return;
      }
    }
    server.send(401, "text/plain", "Invalid credentials");
  });
  
  server.on("/logout", []() {
    server.sendHeader("Location", "/login");
    server.send(302, "text/plain", "Logged out");
  });
  
  server.on("/data", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(401, "text/plain", "Authentication required");
      return;
    }
    handleData();
  });
  
  server.on("/chart-data", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(401, "text/plain", "Authentication required");
      return;
    }
    handleChartData();
  });
  
  server.on("/system", HTTP_GET, []() {
    if (!checkAuth()) {
      server.send(401, "text/plain", "Authentication required");
      return;
    }
    handleSystemInfo();
  });
  
  server.onNotFound([]() {
    if (!checkAuth()) {
      server.sendHeader("Location", "/login");
      server.send(302, "text/plain", "");
      return;
    }
    if (!handleFileRead(server.uri())) {
      handleNotFound();
    }
  });
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Read initial data
  readDHTData();
}

void loop() {
  server.handleClient();
  MDNS.update();
  
  static unsigned long lastRead = 0;
  static unsigned long lastSave = 0;
  unsigned long currentMillis = millis();
  
  // Read sensor every 60 seconds (adjust as needed)
  if (currentMillis - lastRead >= 60000) {
    lastRead = currentMillis;
    readDHTData();
  }
  
  // Save data to SPIFFS every 10 minutes
  if (currentMillis - lastSave >= 600000) {
    lastSave = currentMillis;
    saveDataToSPIFFS();
  }
}

bool checkAuth() {
  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      return true;
    }
  }
  return false;
}

void handleLogin() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
    }
    .login-container {
      background: white;
      padding: 40px;
      border-radius: 10px;
      box-shadow: 0 15px 35px rgba(0,0,0,0.2);
      width: 300px;
    }
    h2 {
      text-align: center;
      color: #333;
      margin-bottom: 30px;
    }
    .input-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      margin-bottom: 5px;
      color: #666;
    }
    input {
      width: 100%;
      padding: 10px;
      border: 1px solid #ddd;
      border-radius: 5px;
      box-sizing: border-box;
    }
    button {
      width: 100%;
      padding: 12px;
      background: #667eea;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 16px;
      transition: background 0.3s;
    }
    button:hover {
      background: #5a67d8;
    }
    .error {
      color: #e53e3e;
      text-align: center;
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="login-container">
    <h2>ESP8266 DHT Monitor</h2>
    <form method="POST" action="/login">
      <div class="input-group">
        <label for="username">Username:</label>
        <input type="text" id="username" name="username" required>
      </div>
      <div class="input-group">
        <label for="password">Password:</label>
        <input type="password" id="password" name="password" required>
      </div>
      <button type="submit">Login</button>
    </form>
    <div class="error" id="error"></div>
  </div>
  <script>
    const urlParams = new URLSearchParams(window.location.search);
    if (urlParams.get('error')) {
      document.getElementById('error').textContent = 'Invalid credentials';
    }
  </script>
</body>
</html>
)=====";
  
  server.send(200, "text/html", html);
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DHT11 Monitor</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
      min-height: 100vh;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    .header h1 {
      font-size: 24px;
      font-weight: 300;
    }
    .nav {
      display: flex;
      gap: 20px;
    }
    .nav a {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 20px;
      transition: background 0.3s;
    }
    .nav a:hover, .nav a.active {
      background: rgba(255,255,255,0.2);
    }
    .container {
      max-width: 1400px;
      margin: 20px auto;
      padding: 20px;
    }
    .dashboard {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 20px;
      margin-bottom: 30px;
    }
    .card {
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.08);
      transition: transform 0.3s, box-shadow 0.3s;
    }
    .card:hover {
      transform: translateY(-5px);
      box-shadow: 0 10px 20px rgba(0,0,0,0.12);
    }
    .card h3 {
      color: #667eea;
      margin-bottom: 15px;
      font-size: 18px;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .card h3 i {
      font-size: 20px;
    }
    .value {
      font-size: 36px;
      font-weight: bold;
      color: #333;
      margin-bottom: 10px;
    }
    .unit {
      font-size: 18px;
      color: #666;
    }
    .chart-container {
      background: white;
      border-radius: 10px;
      padding: 25px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.08);
      margin-top: 20px;
    }
    .chart-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    .chart-header h2 {
      color: #667eea;
    }
    .chart-controls {
      display: flex;
      gap: 10px;
    }
    button {
      padding: 8px 16px;
      background: #667eea;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      transition: background 0.3s;
    }
    button:hover {
      background: #5a67d8;
    }
    .time-filter {
      background: #edf2f7;
      padding: 5px 15px;
      border-radius: 20px;
      font-size: 14px;
    }
    .stats {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 15px;
      margin-top: 15px;
    }
    .stat-item {
      display: flex;
      justify-content: space-between;
      padding: 10px 0;
      border-bottom: 1px solid #eee;
    }
    .stat-label {
      color: #666;
    }
    .stat-value {
      font-weight: bold;
      color: #333;
    }
    .temperature-high { color: #e53e3e; }
    .temperature-low { color: #3182ce; }
    .temperature-avg { color: #38a169; }
    @media (max-width: 768px) {
      .container {
        padding: 10px;
      }
      .dashboard {
        grid-template-columns: 1fr;
      }
      .value {
        font-size: 28px;
      }
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>🌡️ DHT11 Environment Monitor</h1>
    <div class="nav">
      <a href="/" class="active">Dashboard</a>
      <a href="/system">System Info</a>
      <a href="/logout">Logout</a>
    </div>
  </div>
  
  <div class="container">
    <!-- Current Readings -->
    <div class="dashboard">
      <div class="card">
        <h3>🌡️ Current Temperature</h3>
        <div class="value" id="current-temp">--</div>
        <div class="unit">°C</div>
        <div class="stats">
          <div class="stat-item">
            <span class="stat-label">Min:</span>
            <span class="stat-value temperature-low" id="min-temp">--</span>
          </div>
          <div class="stat-item">
            <span class="stat-label">Max:</span>
            <span class="stat-value temperature-high" id="max-temp">--</span>
          </div>
          <div class="stat-item">
            <span class="stat-label">Avg:</span>
            <span class="stat-value temperature-avg" id="avg-temp">--</span>
          </div>
        </div>
      </div>
      
      <div class="card">
        <h3>💧 Current Humidity</h3>
        <div class="value" id="current-hum">--</div>
        <div class="unit">%</div>
        <div class="stats">
          <div class="stat-item">
            <span class="stat-label">Min:</span>
            <span class="stat-value temperature-low" id="min-hum">--</span>
          </div>
          <div class="stat-item">
            <span class="stat-label">Max:</span>
            <span class="stat-value temperature-high" id="max-hum">--</span>
          </div>
          <div class="stat-item">
            <span class="stat-label">Avg:</span>
            <span class="stat-value temperature-avg" id="avg-hum">--</span>
          </div>
        </div>
      </div>
      
      <div class="card">
        <h3>📊 System Status</h3>
        <div class="stats">
          <div class="stat-item">
            <span class="stat-label">Uptime:</span>
            <span class="stat-value" id="uptime">--</span>
          </div>
          <div class="stat-item">
            <span class="stat-label">WiFi Signal:</span>
            <span class="stat-value" id="wifi-signal">--</span>
          </div>
          <div class="stat-item">
            <span class="stat-label">Last Update:</span>
            <span class="stat-value" id="last-update">--</span>
          </div>
          <div class="stat-item">
            <span class="stat-label">Data Points:</span>
            <span class="stat-value" id="data-points">--</span>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Charts -->
    <div class="chart-container">
      <div class="chart-header">
        <h2>24-Hour Temperature & Humidity Trend</h2>
        <div class="chart-controls">
          <div class="time-filter">Last 24 hours</div>
          <button onclick="refreshData()">🔄 Refresh</button>
        </div>
      </div>
      <canvas id="temperatureChart"></canvas>
    </div>
    
    <div class="chart-container">
      <h2>Humidity Chart</h2>
      <canvas id="humidityChart"></canvas>
    </div>
  </div>
  
  <script>
    // Global variables
    let tempChart, humidityChart;
    let currentData = {};
    
    // DOM elements
    const currentTempEl = document.getElementById('current-temp');
    const currentHumEl = document.getElementById('current-hum');
    const minTempEl = document.getElementById('min-temp');
    const maxTempEl = document.getElementById('max-temp');
    const avgTempEl = document.getElementById('avg-temp');
    const minHumEl = document.getElementById('min-hum');
    const maxHumEl = document.getElementById('max-hum');
    const avgHumEl = document.getElementById('avg-hum');
    const uptimeEl = document.getElementById('uptime');
    const wifiSignalEl = document.getElementById('wifi-signal');
    const lastUpdateEl = document.getElementById('last-update');
    const dataPointsEl = document.getElementById('data-points');
    
    // Initialize charts
    function initCharts() {
      const tempCtx = document.getElementById('temperatureChart').getContext('2d');
      const humCtx = document.getElementById('humidityChart').getContext('2d');
      
      tempChart = new Chart(tempCtx, {
        type: 'line',
        data: {
          labels: [],
          datasets: [{
            label: 'Temperature (°C)',
            data: [],
            borderColor: '#e53e3e',
            backgroundColor: 'rgba(229, 62, 62, 0.1)',
            borderWidth: 2,
            fill: true,
            tension: 0.4
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          height: 300,
          scales: {
            x: {
              title: {
                display: true,
                text: 'Time'
              }
            },
            y: {
              title: {
                display: true,
                text: 'Temperature (°C)'
              },
              beginAtZero: false
            }
          }
        }
      });
      
      humidityChart = new Chart(humCtx, {
        type: 'line',
        data: {
          labels: [],
          datasets: [{
            label: 'Humidity (%)',
            data: [],
            borderColor: '#3182ce',
            backgroundColor: 'rgba(49, 130, 206, 0.1)',
            borderWidth: 2,
            fill: true,
            tension: 0.4
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          height: 300,
          scales: {
            x: {
              title: {
                display: true,
                text: 'Time'
              }
            },
            y: {
              title: {
                display: true,
                text: 'Humidity (%)'
              },
              beginAtZero: true,
              max: 100
            }
          }
        }
      });
    }
    
    // Format time
    function formatTime(timestamp) {
      const date = new Date(timestamp * 1000);
      return date.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'});
    }
    
    // Format duration
    function formatDuration(ms) {
      const seconds = Math.floor(ms / 1000);
      const hours = Math.floor(seconds / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      const secs = seconds % 60;
      return `${hours}h ${minutes}m ${secs}s`;
    }
    
    // Update current readings
    function updateCurrentReadings(data) {
      currentTempEl.textContent = data.temperature.toFixed(1);
      currentHumEl.textContent = data.humidity.toFixed(1);
      
      minTempEl.textContent = data.minTemp.toFixed(1);
      maxTempEl.textContent = data.maxTemp.toFixed(1);
      avgTempEl.textContent = data.avgTemp.toFixed(1);
      
      minHumEl.textContent = data.minHum.toFixed(1);
      maxHumEl.textContent = data.maxHum.toFixed(1);
      avgHumEl.textContent = data.avgHum.toFixed(1);
      
      uptimeEl.textContent = formatDuration(data.uptime);
      wifiSignalEl.textContent = `${data.wifiRSSI} dBm`;
      lastUpdateEl.textContent = new Date().toLocaleTimeString();
      dataPointsEl.textContent = data.dataPoints;
    }
    
    // Update charts with historical data
    function updateCharts(chartData) {
      const labels = chartData.map(item => formatTime(item.timestamp));
      const temps = chartData.map(item => item.temperature);
      const hums = chartData.map(item => item.humidity);
      
      tempChart.data.labels = labels;
      tempChart.data.datasets[0].data = temps;
      tempChart.update();
      
      humidityChart.data.labels = labels;
      humidityChart.data.datasets[0].data = hums;
      humidityChart.update();
    }
    
    // Fetch current data
    async function fetchCurrentData() {
      try {
        const response = await fetch('/data');
        if (!response.ok) throw new Error('Network error');
        const data = await response.json();
        currentData = data;
        updateCurrentReadings(data);
      } catch (error) {
        console.error('Error fetching current data:', error);
      }
    }
    
    // Fetch chart data
    async function fetchChartData() {
      try {
        const response = await fetch('/chart-data');
        if (!response.ok) throw new Error('Network error');
        const data = await response.json();
        updateCharts(data);
      } catch (error) {
        console.error('Error fetching chart data:', error);
      }
    }
    
    // Refresh all data
    function refreshData() {
      fetchCurrentData();
      fetchChartData();
    }
    
    // Initialize everything
    window.onload = function() {
      initCharts();
      refreshData();
      // Auto-refresh every 30 seconds
      setInterval(fetchCurrentData, 30000);
      // Auto-refresh chart every 5 minutes
      setInterval(fetchChartData, 300000);
    };
  </script>
</body>
</html>
)=====";
  
  server.send(200, "text/html", html);
}

void handleData() {
  StaticJsonDocument<512> doc;
  
  // Get current readings
  float currentTemp = temperatureLog[logIndex > 0 ? logIndex - 1 : 0];
  float currentHum = humidityLog[logIndex > 0 ? logIndex - 1 : 0];
  
  // Calculate statistics
  float minTempVal = minTemp;
  float maxTempVal = maxTemp;
  float avgTempVal = tempReadings > 0 ? tempSum / tempReadings : 0;
  
  // For humidity, calculate min/max
  float minHum = 1000, maxHum = -1000, sumHum = 0;
  int validHumReadings = 0;
  for (int i = 0; i < logIndex; i++) {
    if (humidityLog[i] > 0) {
      if (humidityLog[i] < minHum) minHum = humidityLog[i];
      if (humidityLog[i] > maxHum) maxHum = humidityLog[i];
      sumHum += humidityLog[i];
      validHumReadings++;
    }
  }
  float avgHum = validHumReadings > 0 ? sumHum / validHumReadings : 0;
  
  doc["temperature"] = currentTemp;
  doc["humidity"] = currentHum;
  doc["minTemp"] = minTempVal;
  doc["maxTemp"] = maxTempVal;
  doc["avgTemp"] = avgTempVal;
  doc["minHum"] = minHum;
  doc["maxHum"] = maxHum;
  doc["avgHum"] = avgHum;
  doc["uptime"] = millis();
  doc["wifiRSSI"] = WiFi.RSSI();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["dataPoints"] = logIndex;
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  
  server.send(200, "application/json", jsonResponse);
}

void handleChartData() {
  StaticJsonDocument<8192> doc;
  JsonArray data = doc.to<JsonArray>();
  
  // Get the last 24 hours of data (or whatever is available)
  int startIdx = logIndex - 1440;
  if (startIdx < 0) startIdx = 0;
  
  for (int i = startIdx; i < logIndex; i++) {
    if (temperatureLog[i] != 0 || humidityLog[i] != 0) {
      JsonObject entry = data.createNestedObject();
      entry["timestamp"] = timeLog[i];
      entry["temperature"] = temperatureLog[i];
      entry["humidity"] = humidityLog[i];
    }
  }
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  
  server.send(200, "application/json", jsonResponse);
}

void handleSystemInfo() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>System Information</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
      min-height: 100vh;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    .header h1 {
      font-size: 24px;
      font-weight: 300;
    }
    .nav {
      display: flex;
      gap: 20px;
    }
    .nav a {
      color: white;
      text-decoration: none;
      padding: 8px 16px;
      border-radius: 20px;
      transition: background 0.3s;
    }
    .nav a:hover, .nav a.active {
      background: rgba(255,255,255,0.2);
    }
    .container {
      max-width: 1200px;
      margin: 30px auto;
      padding: 0 20px;
    }
    .card {
      background: white;
      border-radius: 10px;
      padding: 30px;
      box-shadow: 0 5px 15px rgba(0,0,0,0.08);
      margin-bottom: 30px;
    }
    .card h2 {
      color: #667eea;
      margin-bottom: 25px;
      padding-bottom: 10px;
      border-bottom: 2px solid #f0f0f0;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .card h2 i {
      font-size: 24px;
    }
    .info-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 20px;
    }
    .info-item {
      display: flex;
      justify-content: space-between;
      padding: 15px 0;
      border-bottom: 1px solid #f0f0f0;
    }
    .info-item:last-child {
      border-bottom: none;
    }
    .info-label {
      color: #666;
      font-weight: 500;
    }
    .info-value {
      font-weight: bold;
      color: #333;
    }
    .progress-bar {
      height: 10px;
      background: #edf2f7;
      border-radius: 5px;
      margin-top: 5px;
      overflow: hidden;
    }
    .progress-fill {
      height: 100%;
      background: linear-gradient(90deg, #667eea, #764ba2);
      border-radius: 5px;
    }
    .status-good { color: #38a169; }
    .status-warning { color: #d69e2e; }
    .status-critical { color: #e53e3e; }
    .action-buttons {
      display: flex;
      gap: 10px;
      margin-top: 20px;
    }
    .btn {
      padding: 10px 20px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-weight: 500;
      transition: all 0.3s;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-primary:hover {
      background: #5a67d8;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .btn-secondary {
      background: #edf2f7;
      color: #4a5568;
    }
    .btn-secondary:hover {
      background: #e2e8f0;
    }
    .btn-danger {
      background: #fc8181;
      color: white;
    }
    .btn-danger:hover {
      background: #f56565;
    }
    @media (max-width: 768px) {
      .container {
        padding: 10px;
      }
      .info-grid {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>🖥️ System Information</h1>
    <div class="nav">
      <a href="/">Dashboard</a>
      <a href="/system" class="active">System Info</a>
      <a href="/logout">Logout</a>
    </div>
  </div>
  
  <div class="container">
    <!-- System Status Card -->
    <div class="card">
      <h2>📊 System Status</h2>
      <div class="info-grid">
        <div>
          <div class="info-item">
            <span class="info-label">Chip ID:</span>
            <span class="info-value" id="chip-id">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">Chip Model:</span>
            <span class="info-value" id="chip-model">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">CPU Frequency:</span>
            <span class="info-value" id="cpu-freq">-- MHz</span>
          </div>
          <div class="info-item">
            <span class="info-label">SDK Version:</span>
            <span class="info-value" id="sdk-version">--</span>
          </div>
        </div>
        
        <div>
          <div class="info-item">
            <span class="info-label">System Uptime:</span>
            <span class="info-value" id="system-uptime">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">Total Requests:</span>
            <span class="info-value" id="total-requests">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">Last Reset:</span>
            <span class="info-value" id="last-reset">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">System Health:</span>
            <span class="info-value status-good" id="system-health">Good</span>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Memory & Storage Card -->
    <div class="card">
      <h2>💾 Memory & Storage</h2>
      <div class="info-grid">
        <div>
          <div class="info-item">
            <span class="info-label">Free Heap:</span>
            <span class="info-value" id="free-heap">-- bytes</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" id="heap-bar" style="width: 0%"></div>
          </div>
          
          <div class="info-item">
            <span class="info-label">Max Heap:</span>
            <span class="info-value" id="max-heap">-- bytes</span>
          </div>
          <div class="info-item">
            <span class="info-label">Heap Fragmentation:</span>
            <span class="info-value" id="heap-frag">--%</span>
          </div>
        </div>
        
        <div>
          <div class="info-item">
            <span class="info-label">Flash Chip Size:</span>
            <span class="info-value" id="flash-size">-- MB</span>
          </div>
          <div class="info-item">
            <span class="info-label">SPIFFS Total:</span>
            <span class="info-value" id="spiffs-total">-- bytes</span>
          </div>
          <div class="info-item">
            <span class="info-label">SPIFFS Used:</span>
            <span class="info-value" id="spiffs-used">-- bytes</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" id="spiffs-bar" style="width: 0%"></div>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Network Information Card -->
    <div class="card">
      <h2>🌐 Network Information</h2>
      <div class="info-grid">
        <div>
          <div class="info-item">
            <span class="info-label">WiFi SSID:</span>
            <span class="info-value" id="wifi-ssid">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">IP Address:</span>
            <span class="info-value" id="ip-address">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">MAC Address:</span>
            <span class="info-value" id="mac-address">--</span>
          </div>
        </div>
        
        <div>
          <div class="info-item">
            <span class="info-label">WiFi Signal:</span>
            <span class="info-value" id="wifi-signal">-- dBm</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" id="signal-bar" style="width: 0%"></div>
          </div>
          
          <div class="info-item">
            <span class="info-label">WiFi Channel:</span>
            <span class="info-value" id="wifi-channel">--</span>
          </div>
          <div class="info-item">
            <span class="info-label">Connection Status:</span>
            <span class="info-value status-good" id="wifi-status">Connected</span>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Sensor Information Card -->
    <div class="card">
      <h2>🌡️ Sensor Information</h2>
      <div class="info-grid">
        <div>
          <div class="info-item">
            <span class="info-label">Sensor Type:</span>
            <span class="info-value">DHT11</span>
          </div>
          <div class="info-item">
            <span class="info-label">Data Pin:</span>
            <span class="info-value">GPIO2 (D4)</span>
          </div>
          <div class="info-item">
            <span class="info-label">Total Readings:</span>
            <span class="info-value" id="total-readings">--</span>
          </div>
        </div>
        
        <div>
          <div class="info-item">
            <span class="info-label">Update Interval:</span>
            <span class="info-value">60 seconds</span>
          </div>
          <div class="info-item">
            <span class="info-label">Data Retention:</span>
            <span class="info-value">24 hours</span>
          </div>
          <div class="info-item">
            <span class="info-label">Last Successful Read:</span>
            <span class="info-value" id="last-read">--</span>
          </div>
        </div>
      </div>
    </div>
    
    <!-- System Actions Card -->
    <div class="card">
      <h2>⚙️ System Actions</h2>
      <div class="action-buttons">
        <button class="btn btn-primary" onclick="refreshSystemInfo()">
          🔄 Refresh Information
        </button>
        <button class="btn btn-secondary" onclick="restartESP()">
          🔄 Restart ESP
        </button>
        <button class="btn btn-danger" onclick="clearData()">
          🗑️ Clear Logged Data
        </button>
      </div>
    </div>
  </div>
  
  <script>
    // Format bytes to human readable
    function formatBytes(bytes) {
      if (bytes === 0) return '0 Bytes';
      const k = 1024;
      const sizes = ['Bytes', 'KB', 'MB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
    
    // Format duration
    function formatDuration(ms) {
      const seconds = Math.floor(ms / 1000);
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      const secs = seconds % 60;
      
      if (days > 0) return `${days}d ${hours}h ${minutes}m`;
      if (hours > 0) return `${hours}h ${minutes}m ${secs}s`;
      return `${minutes}m ${secs}s`;
    }
    
    // Calculate WiFi signal percentage (RSSI to percentage)
    function rssiToPercent(rssi) {
      if (rssi >= -50) return 100;
      if (rssi <= -100) return 0;
      return 2 * (rssi + 100);
    }
    
    // Fetch system information
    async function fetchSystemInfo() {
      try {
        const response = await fetch('/data');
        if (!response.ok) throw new Error('Network error');
        const data = await response.json();
        
        // Update system status
        document.getElementById('system-uptime').textContent = formatDuration(data.uptime);
        document.getElementById('total-requests').textContent = data.totalRequests || 0;
        document.getElementById('last-reset').textContent = new Date().toLocaleString();
        
        // Update memory info
        const freeHeap = data.freeHeap || 0;
        const maxHeap = 81920; // Typical for ESP8266
        const heapUsedPercent = ((maxHeap - freeHeap) / maxHeap * 100).toFixed(1);
        
        document.getElementById('free-heap').textContent = formatBytes(freeHeap);
        document.getElementById('max-heap').textContent = formatBytes(maxHeap);
        document.getElementById('heap-frag').textContent = data.heapFrag || '0';
        document.getElementById('heap-bar').style.width = heapUsedPercent + '%';
        
        // Update network info
        document.getElementById('wifi-signal').textContent = data.wifiRSSI + ' dBm';
        const signalPercent = rssiToPercent(data.wifiRSSI);
        document.getElementById('signal-bar').style.width = signalPercent + '%';
        
        // Update sensor info
        document.getElementById('total-readings').textContent = data.dataPoints || 0;
        document.getElementById('last-read').textContent = new Date().toLocaleTimeString();
        
        // Static system info (would need additional endpoints for full info)
        document.getElementById('chip-id').textContent = 'ESP8266';
        document.getElementById('chip-model').textContent = 'ESP-12E/F';
        document.getElementById('cpu-freq').textContent = '80';
        document.getElementById('sdk-version').textContent = '3.0.0';
        document.getElementById('wifi-ssid').textContent = 'Your Network';
        document.getElementById('ip-address').textContent = window.location.hostname;
        document.getElementById('mac-address').textContent = '--:--:--:--:--:--';
        document.getElementById('wifi-channel').textContent = '--';
        
        // Storage info (static for now)
        document.getElementById('flash-size').textContent = '4';
        document.getElementById('spiffs-total').textContent = formatBytes(1048576);
        document.getElementById('spiffs-used').textContent = formatBytes(524288);
        document.getElementById('spiffs-bar').style.width = '50%';
        
      } catch (error) {
        console.error('Error fetching system info:', error);
      }
    }
    
    // System actions
    function refreshSystemInfo() {
      fetchSystemInfo();
      showNotification('System information refreshed', 'success');
    }
    
    function restartESP() {
      if (confirm('Are you sure you want to restart the ESP8266?')) {
        fetch('/restart', { method: 'POST' })
          .then(() => {
            showNotification('ESP8266 is restarting...', 'info');
            setTimeout(() => {
              window.location.reload();
            }, 5000);
          })
          .catch(error => {
            console.error('Error:', error);
            showNotification('Failed to restart', 'error');
          });
      }
    }
    
    function clearData() {
      if (confirm('Are you sure you want to clear all logged data?')) {
        fetch('/clear-data', { method: 'POST' })
          .then(response => {
            if (response.ok) {
              showNotification('Data cleared successfully', 'success');
              setTimeout(() => {
                window.location.reload();
              }, 2000);
            }
          })
          .catch(error => {
            console.error('Error:', error);
            showNotification('Failed to clear data', 'error');
          });
      }
    }
    
    // Notification system
    function showNotification(message, type) {
      // Remove existing notification
      const existingNotification = document.querySelector('.notification');
      if (existingNotification) {
        existingNotification.remove();
      }
      
      // Create new notification
      const notification = document.createElement('div');
      notification.className = `notification notification-${type}`;
      notification.textContent = message;
      
      // Add styles
      notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 15px 25px;
        border-radius: 5px;
        color: white;
        font-weight: 500;
        z-index: 1000;
        animation: slideIn 0.3s ease-out;
        box-shadow: 0 5px 15px rgba(0,0,0,0.1);
      `;
      
      if (type === 'success') {
        notification.style.background = 'linear-gradient(135deg, #38a169, #2f855a)';
      } else if (type === 'error') {
        notification.style.background = 'linear-gradient(135deg, #e53e3e, #c53030)';
      } else {
        notification.style.background = 'linear-gradient(135deg, #667eea, #764ba2)';
      }
      
      document.body.appendChild(notification);
      
      // Remove after 3 seconds
      setTimeout(() => {
        notification.style.animation = 'slideOut 0.3s ease-in forwards';
        setTimeout(() => notification.remove(), 300);
      }, 3000);
      
      // Add keyframe animations
      if (!document.querySelector('#notification-styles')) {
        const style = document.createElement('style');
        style.id = 'notification-styles';
        style.textContent = `
          @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
          }
          @keyframes slideOut {
            from { transform: translateX(0); opacity: 1; }
            to { transform: translateX(100%); opacity: 0; }
          }
        `;
        document.head.appendChild(style);
      }
    }
    
    // Initialize
    window.onload = function() {
      fetchSystemInfo();
      // Auto-refresh every 10 seconds
      setInterval(fetchSystemInfo, 10000);
    };
  </script>
</body>
</html>
)=====";
  
  server.send(200, "text/html", html);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) {
    path += "index.html";
  }
  
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) {
      path = pathWithGz;
    }
    
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void readDHTData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  Serial.printf("Temperature: %.1f°C, Humidity: %.1f%%\n", temperature, humidity);
  
  // Store data in circular buffer
  temperatureLog[logIndex] = temperature;
  humidityLog[logIndex] = humidity;
  timeLog[logIndex] = millis() / 1000; // Store in seconds
  
  // Update statistics
  updateStatistics(temperature, humidity);
  
  // Increment index with wrap-around
  logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
  
  if (!logInitialized && logIndex == 0) {
    logInitialized = true;
  }
}

void updateStatistics(float temp, float hum) {
  // Update temperature statistics
  if (temp < minTemp) minTemp = temp;
  if (temp > maxTemp) maxTemp = temp;
  
  tempSum += temp;
  tempReadings++;
}

void saveDataToSPIFFS() {
  File file = SPIFFS.open("/data_log.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  
  StaticJsonDocument<4096> doc;
  JsonArray tempArray = doc.createNestedArray("temperature");
  JsonArray humArray = doc.createNestedArray("humidity");
  JsonArray timeArray = doc.createNestedArray("time");
  
  for (int i = 0; i < logIndex; i++) {
    tempArray.add(temperatureLog[i]);
    humArray.add(humidityLog[i]);
    timeArray.add(timeLog[i]);
  }
  
  serializeJson(doc, file);
  file.close();
  
  Serial.println("Data saved to SPIFFS");
}