# Temperature_Logger
ESP32/8266 Temperature Logger and graphs it in a web page.






2. Uploading Files to SPIFFS

You need to upload the HTML files to SPIFFS. First, install the ESP8266 Sketch Data Upload tool in Arduino IDE:

    Go to Tools → ESP8266 Sketch Data Upload

    Create a data folder in your sketch directory

    Place all HTML/CSS/JS files there

    Run the data upload tool

3. Required Libraries

Install these libraries via Arduino Library Manager:

    ESP8266WiFi (comes with ESP8266 board package)

    ESP8266WebServer (comes with ESP8266 board package)

    ESP8266mDNS (comes with ESP8266 board package)

    FS (comes with ESP8266 board package)

    DHT sensor library by Adafruit

    ArduinoJson by Benoit Blanchon

4. Configuration

Before uploading:

    Change WiFi credentials:
    cpp

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

Change authentication credentials (optional but recommended):
cpp

const char* www_username = "admin";
const char* www_password = "admin123";

5. Features

This complete system includes:

✅ User Authentication - Login page with username/password
✅ Real-time Monitoring - Current temperature/humidity display
✅ 24-Hour Graphing - Interactive charts with Chart.js
✅ Statistics - Min, max, average values
✅ System Monitoring Page - Resource usage, WiFi info, memory stats
✅ Responsive Design - Works on mobile and desktop
✅ Data Logging - Stores readings for 24 hours
✅ Auto-refresh - Updates every 30 seconds
✅ Beautiful UI - Modern gradient design with cards
✅ Error Handling - Robust sensor reading validation
✅ SPIFFS Storage - Files served from flash memory
6. Usage

    Upload the sketch to your ESP8266

    Upload the data files to SPIFFS

    Connect to the ESP8266 IP address

    Login with username: admin, password: admin123

    View real-time graphs and statistics

    Click "System Info" for resource monitoring

7. Future Enhancements

You can easily extend this by:

    Adding data export to CSV

    Implementing MQTT for cloud logging

    Adding OTA updates

    Creating mobile notifications

    Adding more sensors (DHT22 when you get it)

    Implementing timezone support

    Adding data backup to SD card

The code is modular and well-organized, making it easy to modify and extend. When you get the DHT22, simply change #define DHTTYPE DHT11 to #define DHTTYPE DHT22 and the system will work with the more accurate sensor!