#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DHT.h>
#include <time.h>

// =====================================================
// ARNOLD ACTIVITY 4 - ESP32 DHT11 FIREBASE MONITOR
// =====================================================

// =====================================================
// DHT11
// =====================================================

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// =====================================================
// ARNOLD FIREBASE CONFIG
// =====================================================

#define API_KEY "AIzaSyCZEBATYLvyFzTZlrBfQm7Eg_AfsK6JNrw"

#define DATABASE_URL "https://arnie-8ccb5-default-rtdb.europe-west1.firebasedatabase.app"

// IMPORTANT:
// Replace these two values with the Firebase Authentication
// email and password used by Arnold's ESP32 project.
#define USER_EMAIL "arnoldeusala910@gmail.com"
#define USER_PASSWORD "Astrologo23"

// =====================================================
// FIREBASE OBJECTS 
// =====================================================

UserAuth user_auth(
    API_KEY,
    USER_EMAIL,
    USER_PASSWORD,
    3000
);

FirebaseApp app;
WiFiClientSecure ssl_client;
AsyncClientClass aClient(ssl_client);
RealtimeDatabase Database;

// =====================================================
// WEB SERVER
// =====================================================

AsyncWebServer server(80);

// =====================================================
// WIFI MANAGER
// =====================================================

const char *AP_SSID = "ESP-WIFI-MANAGER";
bool wifiManagerMode = false;

// =====================================================
// SENSOR INTERVAL
// =====================================================

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 10000;

// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

void processFirebase(AsyncResult &aResult);
bool connectToSavedWiFi();
void startWiFiManager();
void startMainWebServer();
void setupFirebase();
void sendSensorData();

String readFile(const char *path);
bool writeFile(const char *path, const String &data);
void deleteWiFiFiles();

String getDateString();
String getTimeString();

// =====================================================
// WIFI MANAGER HTML
// =====================================================

const char WIFI_MANAGER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CORE // WiFi Manager</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{
    min-height:100vh;
    background:#080d14;
    color:#dbeafe;
    font-family:"Segoe UI",Arial,sans-serif;
    display:flex;
    justify-content:center;
    align-items:center;
    padding:20px;
}
.grid{
    position:fixed;
    inset:0;
    z-index:-1;
    background-image:
        linear-gradient(rgba(14,165,233,.035) 1px,transparent 1px),
        linear-gradient(90deg,rgba(14,165,233,.035) 1px,transparent 1px);
    background-size:40px 40px;
}
.card{
    width:100%;
    max-width:520px;
    background:#111923;
    border:1px solid #172033;
    padding:35px;
    box-shadow:0 0 50px rgba(0,0,0,.35);
}
.brand{
    color:#22d3ee;
    font-family:monospace;
    font-size:12px;
    letter-spacing:3px;
    margin-bottom:10px;
}
h1{color:#f8fafc;font-size:32px;margin-bottom:8px}
.subtitle{
    color:#64748b;
    font-family:monospace;
    font-size:10px;
    letter-spacing:2px;
    margin-bottom:30px;
}
label{
    display:block;
    color:#64748b;
    font-family:monospace;
    font-size:10px;
    letter-spacing:2px;
    margin-bottom:8px;
}
input{
    width:100%;
    padding:14px;
    margin-bottom:20px;
    background:#050a10;
    color:#f8fafc;
    border:1px solid #263449;
    outline:none;
    font-family:monospace;
}
input:focus{
    border-color:#22d3ee;
    box-shadow:0 0 0 1px #22d3ee;
}
button{
    width:100%;
    padding:15px;
    background:#06202a;
    color:#f4fafb;
    border:1px solid #22d3ee;
    font-family:monospace;
    font-weight:bold;
    letter-spacing:2px;
    cursor:pointer;
}
button:hover{background:#083746}
.note{
    margin-top:20px;
    color:#475569;
    font-family:monospace;
    font-size:9px;
    line-height:1.7;
}
</style>
</head>
<body>
<div class="grid"></div>
<div class="card">
    <div class="brand">CORE // ESP32</div>
    <h1>WiFi Manager</h1>
    <div class="subtitle">ARNOLD EUSALA ENVIRONMENT MONITOR</div>

    <form method="POST" action="/">
        <label>WIFI SSID</label>
        <input type="text" name="ssid" placeholder="Enter WiFi name" required>

        <label>WIFI PASSWORD</label>
        <input type="password" name="pass" placeholder="Enter WiFi password">

        <label>STATIC IP</label>
        <input type="text" name="ip" placeholder="192.168.1.100">

        <label>GATEWAY</label>
        <input type="text" name="gateway" placeholder="192.168.1.1">

        <button type="submit">SAVE &amp; RESTART</button>
    </form>

    <div class="note">
        Leave STATIC IP and GATEWAY empty if you want the ESP32
        to use DHCP.
    </div>
</div>
</body>
</html>
)rawliteral";

// =====================================================
// FILE FUNCTIONS
// =====================================================

String readFile(const char *path)
{
    if (!LittleFS.exists(path)) return "";

    File file = LittleFS.open(path, "r");
    if (!file) return "";

    String data = file.readString();
    file.close();
    data.trim();

    return data;
}

bool writeFile(const char *path, const String &data)
{
    File file = LittleFS.open(path, "w");

    if (!file) {
        Serial.print("Failed to open file: ");
        Serial.println(path);
        return false;
    }

    file.print(data);
    file.close();

    return true;
}

void deleteWiFiFiles()
{
    LittleFS.remove("/ssid.txt");
    LittleFS.remove("/pass.txt");
    LittleFS.remove("/ip.txt");
    LittleFS.remove("/gateway.txt");
}

// =====================================================
// CONNECT SAVED WIFI
// =====================================================

bool connectToSavedWiFi()
{
    String ssid = readFile("/ssid.txt");
    String pass = readFile("/pass.txt");
    String ip = readFile("/ip.txt");
    String gateway = readFile("/gateway.txt");

    if (ssid.length() == 0) {
        Serial.println("No saved WiFi credentials.");
        return false;
    }

    WiFi.mode(WIFI_STA);
    delay(300);

    if (ip.length() > 0 && gateway.length() > 0) {
        IPAddress local_IP;
        IPAddress gateway_IP;

        if (local_IP.fromString(ip) &&
            gateway_IP.fromString(gateway)) {

            IPAddress subnet(255,255,255,0);

            WiFi.config(
                local_IP,
                gateway_IP,
                subnet
            );
        }
    }

    WiFi.begin(
        ssid.c_str(),
        pass.c_str()
    );

    Serial.print("Connecting to WiFi");

    unsigned long startTime = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < 20000
    ) {
        Serial.print(".");
        delay(500);
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WIFI CONNECTED");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("Failed to connect to saved WiFi.");

    WiFi.disconnect(true);
    delay(500);

    return false;
}

// =====================================================
// START WIFI MANAGER
// =====================================================

void startWiFiManager()
{
    wifiManagerMode = true;

    WiFi.mode(WIFI_AP);
    delay(300);

    WiFi.softAP(AP_SSID);
    delay(500);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       WIFI MANAGER MODE");
    Serial.println("=================================");
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request) {
            request->send(
                200,
                "text/html",
                WIFI_MANAGER_HTML
            );
        }
    );

    server.on(
        "/",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {

            String ssid = "";
            String pass = "";
            String ip = "";
            String gateway = "";

            if (request->hasParam("ssid", true))
                ssid = request->getParam("ssid", true)->value();

            if (request->hasParam("pass", true))
                pass = request->getParam("pass", true)->value();

            if (request->hasParam("ip", true))
                ip = request->getParam("ip", true)->value();

            if (request->hasParam("gateway", true))
                gateway = request->getParam("gateway", true)->value();

            ssid.trim();
            pass.trim();
            ip.trim();
            gateway.trim();

            if (ssid.length() == 0) {
                request->send(
                    400,
                    "text/plain",
                    "WiFi SSID is required."
                );
                return;
            }

            writeFile("/ssid.txt", ssid);
            writeFile("/pass.txt", pass);
            writeFile("/ip.txt", ip);
            writeFile("/gateway.txt", gateway);

            request->send(
                200,
                "text/html",
                "<html><body style='font-family:Arial;background:#080d14;color:#dbeafe;text-align:center;padding:50px'>"
                "<h1 style='color:#22d3ee'>WIFI SAVED</h1>"
                "<p>The ESP32 will restart and connect to the saved WiFi.</p>"
                "<p>Please wait...</p>"
                "</body></html>"
            );

            delay(1200);
            ESP.restart();
        }
    );

    server.begin();
}

// =====================================================
// MAIN WEB SERVER
// =====================================================

void startMainWebServer()
{
    wifiManagerMode = false;

    server.on(
        "/",
        HTTP_GET,
        [](AsyncWebServerRequest *request) {

            if (LittleFS.exists("/index.html")) {
                request->send(
                    LittleFS,
                    "/index.html",
                    "text/html"
                );
            } else {
                request->send(
                    404,
                    "text/plain",
                    "index.html not found."
                );
            }
        }
    );

    server.on(
        "/change-wifi",
        HTTP_GET,
        [](AsyncWebServerRequest *request) {

            request->send(
                200,
                "text/html",
                "<html><body style='font-family:Arial;background:#080d14;color:#dbeafe;text-align:center;padding:50px'>"
                "<h1 style='color:#22d3ee'>CHANGING WIFI...</h1>"
                "<p>WiFi settings will be cleared.</p>"
                "<p>The ESP32 will restart.</p>"
                "</body></html>"
            );

            delay(1000);
            deleteWiFiFiles();
            ESP.restart();
        }
    );

    server.serveStatic(
        "/",
        LittleFS,
        "/"
    );

    server.begin();

    Serial.println();
    Serial.println("=================================");
    Serial.println("      MAIN WEB SERVER READY");
    Serial.println("=================================");
    Serial.print("Website: http://");
    Serial.println(WiFi.localIP());
}

// =====================================================
// FIREBASE CALLBACK
// =====================================================

void processFirebase(AsyncResult &aResult)
{
    if (!aResult.isResult()) return;

    if (aResult.isError()) {
        Firebase.printf(
            "Firebase Error: %s, code: %d\n",
            aResult.error().message().c_str(),
            aResult.error().code()
        );
    }
}

// =====================================================
// FIREBASE SETUP
// =====================================================

void setupFirebase()
{
    ssl_client.setInsecure();

    initializeApp(
        aClient,
        app,
        getAuth(user_auth),
        processFirebase,
        "authTask"
    );

    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
}

// =====================================================
// DATE / TIME
// =====================================================

String getDateString()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {
        return "1970-01-01";
    }

    char buffer[20];

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d",
        &timeinfo
    );

    return String(buffer);
}

String getTimeString()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {
        return "00:00:00";
    }

    char buffer[20];

    strftime(
        buffer,
        sizeof(buffer),
        "%H:%M:%S",
        &timeinfo
    );

    return String(buffer);
}

// =====================================================
// SEND SENSOR DATA
// =====================================================

void sendSensorData()
{
    if (!app.ready()) {
        return;
    }

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("DHT11 read failed.");
        return;
    }

    String date = getDateString();
    String time = getTimeString();

    String basePath =
        "/ESP32_Data/" +
        date +
        "/" +
        time;

    String temperaturePath =
        basePath + "/temperature";

    String humidityPath =
        basePath + "/humidity";

    Serial.print("Temperature: ");
    Serial.print(temperature, 1);
    Serial.println(" °C");

    Serial.print("Humidity: ");
    Serial.print(humidity, 1);
    Serial.println(" %");

    Database.set<float>(
        aClient,
        temperaturePath,
        temperature,
        processFirebase,
        "temperatureTask"
    );

    Database.set<float>(
        aClient,
        humidityPath,
        humidity,
        processFirebase,
        "humidityTask"
    );
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println(" ARNOLD ACTIVITY 4 DHT11 MONITOR");
    Serial.println("=================================");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
        while (true) delay(1000);
    }

    dht.begin();

    bool connected = connectToSavedWiFi();

    if (!connected) {
        startWiFiManager();
        return;
    }

    configTime(
        8 * 3600,
        0,
        "pool.ntp.org",
        "time.nist.gov",
        "time.google.com"
    );

    struct tm timeinfo;
    int retry = 0;

    while (
        !getLocalTime(&timeinfo) &&
        retry < 20
    ) {
        delay(500);
        retry++;
    }

    setupFirebase();
    startMainWebServer();

    Serial.println("SYSTEM READY");
    Serial.print("Website: http://");
    Serial.println(WiFi.localIP());
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    if (!wifiManagerMode) {
        app.loop();
    }

    if (
        !wifiManagerMode &&
        millis() - lastSensorRead >= SENSOR_INTERVAL
    ) {
        lastSensorRead = millis();
        sendSensorData();
    }

    delay(10);
}
