#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include "time.h"
#include "HX711.h"
#include <RunningMedian.h>

// Firebase includes
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Pin definitions
#define DHTPIN_OUTSIDE 5       // DHT22 for outside measurements
#define DHTPIN_INSIDE 4        // DHT22 for inside measurements
#define DHTTYPE DHT22
#define HX711_DOUT_PIN 18      // HX711 data pin
#define HX711_SCK_PIN 19       // HX711 clock pin

// Network credentials
#define WIFI_SSID "SLT-Fiber-2.4G-66B0"
#define WIFI_PASSWORD "1e6a267ea!"

// Firebase credentials
#define API_KEY "AIzaSyDQJzcyH3CSvp1v_xTpsRufrMXnU7bHXv8"
#define USER_EMAIL "hesaranisal97@gmail.com"
#define USER_PASSWORD "hesaraandnisal"
#define DATABASE_URL "https://dataset-5e91e-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Data collection configuration
const int DATA_SEND_INTERVAL = 60000;     // Send data every 60 seconds
const int SENSOR_READ_COUNT = 10;         // Number of readings to average
const unsigned long SENSOR_READ_DELAY = 100; // Delay between sensor readings in ms
const int DEEP_SLEEP_DURATION = 60;       // Deep sleep duration in seconds

// HX711 calibration values
const float HX711_CALIBRATION_FACTOR = 21000.0; // Replace with your calibration factor
const float HX711_KNOWN_WEIGHT = 100.0;         // Known weight in grams used for calibration

// Global objects
DHT dhtOutside(DHTPIN_OUTSIDE, DHTTYPE);
DHT dhtInside(DHTPIN_INSIDE, DHTTYPE);
HX711 scale;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
RunningMedian tempOutsideMedian(SENSOR_READ_COUNT);
RunningMedian humOutsideMedian(SENSOR_READ_COUNT);
RunningMedian tempInsideMedian(SENSOR_READ_COUNT);
RunningMedian humInsideMedian(SENSOR_READ_COUNT);
RunningMedian weightMedian(SENSOR_READ_COUNT);

// Firebase paths
String uid;
String databasePath;
String outsideTempPath = "/outside_temperature";
String outsideHumPath = "/outside_humidity";
String insideTempPath = "/inside_temperature";
String insideHumPath = "/inside_humidity";
String weightPath = "/weight";
String timePath = "/timestamp";
String parentPath;

// Global variables for sensor readings
float out_temperature = 0.0;
float out_humidity = 0.0;
float in_temperature = 0.0;
float in_humidity = 0.0;
float weight = 0.0;
int timestamp = 0;
FirebaseJson json;

// NTP server for timestamp
const char* ntpServer = "pool.ntp.org";

// Function prototypes
void initWiFi();
unsigned long getTime();
void initHX711();
void collectSensorData();
float processWeight(float rawWeight);
float processDHTTemperature(float rawTemp);
float processDHTHumidity(float rawHumidity);
void initFirebase();
bool sendDataToFirebase();
void printSensorData();

void setup() {
    Serial.begin(115200);
    delay(1000); // Short delay for serial port to initialize
    
    Serial.println("\n===============================");
    Serial.println("Starting ESP32 Sensor Monitor");
    Serial.println("===============================");
    
    // Initialize sensors
    dhtOutside.begin();
    dhtInside.begin();
    initHX711();
    
    // Initialize WiFi and Firebase
    initWiFi();
    configTime(0, 0, ntpServer);
    initFirebase();
    
    // Collect and send data
    collectSensorData();
    if (sendDataToFirebase()) {
        Serial.println("Data sent successfully");
    } else {
        Serial.println("Failed to send data");
    }
    
    // Going to deep sleep
    Serial.printf("Going to deep sleep for %d seconds\n", DEEP_SLEEP_DURATION);
    esp_deep_sleep(DEEP_SLEEP_DURATION * 1000000);
}

void loop() {
    // This won't be executed due to deep sleep
}

void initWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi. Restarting...");
        ESP.restart();
    }
}

void initHX711() {
    Serial.println("Initializing HX711 scale...");
    scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    
    if (!scale.is_ready()) {
        Serial.println("HX711 not found. Check wiring!");
    } else {
        Serial.println("HX711 initialized");
        scale.set_scale(HX711_CALIBRATION_FACTOR);
        scale.tare(); // Reset scale to zero
        Serial.println("Scale tared");
    }
}

void initFirebase() {
    Serial.println("Initializing Firebase...");
    
    // Configure Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    
    // Connection settings
    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(4096);
    config.token_status_callback = tokenStatusCallback;
    config.max_token_generation_retry = 5;
    
    // Initialize Firebase
    Firebase.begin(&config, &auth);
    
    // Wait for authentication
    Serial.print("Authenticating with Firebase...");
    int timeout = 30; // 30 seconds timeout
    while (auth.token.uid == "" && timeout > 0) {
        Serial.print(".");
        delay(1000);
        timeout--;
    }
    
    if (auth.token.uid == "") {
        Serial.println("\nFailed to authenticate with Firebase");
        ESP.restart();
    }
    
    // Success - set paths
    uid = auth.token.uid.c_str();
    Serial.printf("\nAuthenticated as user: %s\n", uid.c_str());
    databasePath = "/UsersData/" + uid + "/readings";
}

void collectSensorData() {
    Serial.println("Collecting sensor data...");
    
    // Clear previous data
    tempOutsideMedian.clear();
    humOutsideMedian.clear();
    tempInsideMedian.clear();
    humInsideMedian.clear();
    weightMedian.clear();
    
    // Take multiple readings for each sensor and add to median filters
    for (int i = 0; i < SENSOR_READ_COUNT; i++) {
        Serial.printf("Reading #%d\n", i+1);
        
        // Read DHT sensors
        float tempOutside = dhtOutside.readTemperature();
        float humOutside = dhtOutside.readHumidity();
        float tempInside = dhtInside.readTemperature();
        float humInside = dhtInside.readHumidity();
        
        // Read scale
        float rawWeight = scale.get_units(5); // Average of 5 readings
        
        // Add readings to median filters if they are valid
        if (!isnan(tempOutside)) tempOutsideMedian.add(tempOutside);
        if (!isnan(humOutside)) humOutsideMedian.add(humOutside);
        if (!isnan(tempInside)) tempInsideMedian.add(tempInside);
        if (!isnan(humInside)) humInsideMedian.add(humInside);
        if (!isnan(rawWeight)) humInsideMedian.add(rawWeight);
  
        
        delay(SENSOR_READ_DELAY);
    }
    
    // Process the collected data
    out_temperature = processDHTTemperature(tempOutsideMedian.getMedian());
    out_humidity = processDHTHumidity(humOutsideMedian.getMedian());
    in_temperature = processDHTTemperature(tempInsideMedian.getMedian());
    in_humidity = processDHTHumidity(humInsideMedian.getMedian());
    weight = processWeight(weightMedian.getMedian());
    
    // Get timestamp
    timestamp = getTime();
    
    // Print the processed values
    printSensorData();
}

float processDHTTemperature(float rawTemp) {
    // Apply temperature compensation and filtering
    if (isnan(rawTemp) || rawTemp < -40 || rawTemp > 80) {
        return 0.0; // Invalid reading
    }
    
    // Round to 1 decimal place
    return round(rawTemp * 100) / 100.0;
}

float processDHTHumidity(float rawHumidity) {
    // Process humidity data
    if (isnan(rawHumidity) || rawHumidity < 0 || rawHumidity > 100) {
        return 0.0; // Invalid reading
    }
    
    
    // Clip to valid range
    if (rawHumidity > 100) rawHumidity= 100;
    
    // Round to whole number
    return round(rawHumidity * 100)/100.0;
}

float processWeight(float rawWeight) {
    // Process weight data
    if (rawWeight < -1000 || rawWeight > 5000) { // Adjust these thresholds based on your application
        return 0.0; // Invalid reading
    }
    
    
    // Round to 2 decimal place
    return round(rawWeight * 100) / 100.0;
}

bool sendDataToFirebase() {
    if (!Firebase.ready()) {
        Serial.println("Firebase not ready");
        return false;
    }
    
    // Create parent path with timestamp
    parentPath = databasePath + "/" + String(timestamp);
    
    // Clear the JSON object
    json.clear();
    
    // Add sensor data to JSON
    json.set(outsideTempPath.c_str(), String(out_temperature));
    json.set(outsideHumPath.c_str(), String(out_humidity));
    json.set(insideTempPath.c_str(), String(in_temperature));
    json.set(insideHumPath.c_str(), String(in_humidity));
    json.set(weightPath.c_str(), String(weight));
    json.set(timePath.c_str(), String(timestamp));
    
    // Print JSON data for debugging
    String jsonString;
    json.toString(jsonString);
    Serial.println("Sending data to Firebase:");
    Serial.println(jsonString);
    
    // Send data to Firebase
    if (Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json)) {
        Serial.println("Firebase data sent successfully");
        return true;
    } else {
        Serial.printf("Firebase error: %s\n", fbdo.errorReason().c_str());
        return false;
    }
}

void printSensorData() {
    Serial.println("\n--- Processed Sensor Data ---");
    Serial.printf("Outside Temperature: %.1f°C\n", out_temperature);
    Serial.printf("Outside Humidity: %.1f%%\n", out_humidity);
    Serial.printf("Inside Temperature: %.1f°C\n", in_temperature);
    Serial.printf("Inside Humidity: %.1f%%\n", in_humidity);
    Serial.printf("Weight: %.1fg\n", weight);
    Serial.printf("Timestamp: %d\n", timestamp);
    Serial.println("----------------------------\n");
}

unsigned long getTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return 0;
    }
    time(&now);
    return now;
}











































// #include <Arduino.h>
// #include <WiFi.h>
// #include <Firebase_ESP_Client.h>
// #include <Adafruit_Sensor.h>
// #include "DHT.h"
// #include "time.h"


// // Provide the token generation process info.
// #include "addons/TokenHelper.h"
// // Provide the RTDB payload printing info and other helper functions.

// #include "addons/RTDBHelper.h"

// #define DHTPIN 5
// #define DHTPIN2 4
// #define DHTTYPE DHT22


// // #define GAS_READ_PIN 34
// // Insert your network credentials
// #define WIFI_SSID "SLT-Fiber-2.4G-66B0"
// #define WIFI_PASSWORD "1e6a267ea!"

// // Insert Firebase project API Key
// #define API_KEY "AIzaSyDQJzcyH3CSvp1v_xTpsRufrMXnU7bHXv8"

// // Insert Authorized Email and Corresponding Password
// #define USER_EMAIL "hesaranisal97@gmail.com"
// #define USER_PASSWORD "hesaraandnisal"

// // Insert RTDB URL
// #define DATABASE_URL "https://dataset-5e91e-default-rtdb.asia-southeast1.firebasedatabase.app/"

// // Function prototypes
// void initWiFi();
// unsigned long getTime();

// FirebaseData fbdo;
// FirebaseAuth auth;
// FirebaseConfig config;

// // Variable to save USER UID
// String uid;

// // Database main path (to be updated in setup with the user UID)
// String databasePath;
// // Database child nodes
// String outsideTempPath = "/outside_temperature";
// String outsideHumPath = "/outside_humidity";
// String insideTempPath = "/inside_temperature";
// String insideHumPath = "/inside_humidity";
// // String gasPath = "/air_quality";
// String timePath = "/timestamp";

// // Parent Node (to be updated in every loop)
// String parentPath;

// int timestamp;
// FirebaseJson json;

// const char* ntpServer = "pool.ntp.org";

// float out_temperature;
// float out_humidity;
// float in_temperature;
// float in_humidity;
// // int air_quality;
// unsigned long sendDataPrevMillis = 0;
// const int timerDelay = 60000;  // Send data every 60 seconds

// DHT dht(DHTPIN, DHTTYPE);
// DHT dht2(DHTPIN2, DHTTYPE);


// void setup() {
//     Serial.begin(115200);
//     dht.begin();
//     dht2.begin();
//     initWiFi();
//     configTime(0, 0, ntpServer);
  
//     // Assign the api key (required)
//     config.api_key = API_KEY;
  
//     // Assign the user sign in credentials
//     auth.user.email = USER_EMAIL;
//     auth.user.password = USER_PASSWORD;
  
//     // Assign the RTDB URL (required)
//     config.database_url = DATABASE_URL;
  
//     Firebase.reconnectWiFi(true);
//     fbdo.setResponseSize(4096);
  
//     // Assign the callback function for the long running token generation task
//     config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
//     // Assign the maximum retry of token generation
//     config.max_token_generation_retry = 5;
  
//     // Initialize the library with the Firebase authen and config
//     Firebase.begin(&config, &auth);
  
//     // Getting the user UID might take a few seconds
//     Serial.println("Getting User UID");
//     while ((auth.token.uid) == "") {
//         Serial.print('.');
//         delay(1000);
//     }
  
//     // Print user UID
//     uid = auth.token.uid.c_str();
//     Serial.print("User UID: ");
//     Serial.println(uid);
  
//     // Update database path
//     databasePath = "/UsersData/" + uid + "/readings";
// }

// void loop() {
//     // Check if it's time to read the sensors and send data
//     if (Firebase.ready()) {
//         // Read DHT sensor values
//         out_humidity = dht.readHumidity();
//         out_temperature = dht.readTemperature();
//         in_humidity = dht2.readHumidity();
//         in_temperature = dht2.readTemperature();
//         // air_quality = analogRead(GAS_READ_PIN);
    
        
//         // Get current timestamp
//         timestamp = getTime();
//         Serial.print("time: ");
//         Serial.println(timestamp);

//         parentPath = databasePath + "/" + String(timestamp);

//         // Clear the JSON object before setting new values
//         json.clear();

//         // Set values in the JSON only if valid (skip NaN values)
//         json.set(outsideTempPath.c_str(), String(out_temperature));
//         json.set(outsideHumPath.c_str(), String(out_humidity));
//         json.set(insideTempPath.c_str(), String(in_temperature));
//         json.set(insideHumPath.c_str(), String(in_humidity));
//         // json.set(gasPath.c_str(), String(air_quality));
//         json.set(timePath.c_str(), String(timestamp));

//         // Print the JSON string representation
//         String jsonString;
//         json.toString(jsonString);
//         Serial.print("JSON Data: ");
//         Serial.println(jsonString);

//         // Send the JSON data to the database
//         Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
//     }

//     // Go to deep sleep for 60 seconds (60,000,000 microseconds)
//     esp_deep_sleep(60 * 1000000);
// }

// void initWiFi() {
//     WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//     Serial.print("Connecting to WiFi ..");
//     while (WiFi.status() != WL_CONNECTED) {
//       Serial.print('.');
//       delay(1000);
//     }
//     Serial.println(WiFi.localIP());
//     Serial.println();
// }

// unsigned long getTime() {
//     time_t now;
//     struct tm timeinfo;
//     if (!getLocalTime(&timeinfo)) {
//         return(0);
//     }
//     time(&now);
//     return now;
// }


// #include <Arduino.h>
// #include <WiFi.h>
// #include <Firebase_ESP_Client.h>
// #include <Adafruit_Sensor.h>
// #include "DHT.h"
// #include "time.h"
// #include "HX711.h"  // Include the HX711 library

// // Provide the token generation process info.
// #include "addons/TokenHelper.h"
// // Provide the RTDB payload printing info and other helper functions.
// #include "addons/RTDBHelper.h"

// // DHT sensor pins
// #define DHTPIN 5
// #define DHTPIN2 4
// #define DHTTYPE DHT22

// // HX711 circuit wiring
// #define LOADCELL_DOUT_PIN 16
// #define LOADCELL_SCK_PIN 17

// // Insert your network credentials
// #define WIFI_SSID "SLT-Fiber-2.4G-66B0"
// #define WIFI_PASSWORD "1e6a267ea!"

// // Insert Firebase project API Key
// #define API_KEY "AIzaSyDQJzcyH3CSvp1v_xTpsRufrMXnU7bHXv8"

// // Insert Authorized Email and Corresponding Password
// #define USER_EMAIL "hesaranisal97@gmail.com"
// #define USER_PASSWORD "hesaraandnisal"

// // Insert RTDB URL
// #define DATABASE_URL "https://dataset-5e91e-default-rtdb.asia-southeast1.firebasedatabase.app/"

// // Function prototypes
// void initWiFi();
// unsigned long getTime();
// void setupHX711();
// float getWeight();

// FirebaseData fbdo;
// FirebaseAuth auth;
// FirebaseConfig config;

// // HX711 scale
// HX711 scale;

// // Calibration factor for the scale (you need to determine this for your specific load cell)
// float calibration_factor = -7050.0; // This value will need to be adjusted based on your calibration

// // Variable to save USER UID
// String uid;

// // Database main path (to be updated in setup with the user UID)
// String databasePath;
// // Database child nodes
// String outsideTempPath = "/outside_temperature";
// String outsideHumPath = "/outside_humidity";
// String insideTempPath = "/inside_temperature";
// String insideHumPath = "/inside_humidity";
// String weightPath = "/weight";  // Add path for weight data
// String timePath = "/timestamp";

// // Parent Node (to be updated in every loop)
// String parentPath;

// int timestamp;
// FirebaseJson json;

// const char* ntpServer = "pool.ntp.org";

// float out_temperature;
// float out_humidity;
// float in_temperature;
// float in_humidity;
// float weight;  // Variable to store weight readings

// const int timerDelay = 60000;  // Send data every 60 seconds

// DHT dht(DHTPIN, DHTTYPE);
// DHT dht2(DHTPIN2, DHTTYPE);

// void setup() {
//     Serial.begin(115200);
    
//     // Initialize sensors
//     dht.begin();
//     dht2.begin();
//     setupHX711();  // Initialize HX711 and load cell
    
//     initWiFi();
//     configTime(0, 0, ntpServer);
  
//     // Assign the api key (required)
//     config.api_key = API_KEY;
  
//     // Assign the user sign in credentials
//     auth.user.email = USER_EMAIL;
//     auth.user.password = USER_PASSWORD;
  
//     // Assign the RTDB URL (required)
//     config.database_url = DATABASE_URL;
  
//     Firebase.reconnectWiFi(true);
//     fbdo.setResponseSize(4096);
  
//     // Assign the callback function for the long running token generation task
//     config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
//     // Assign the maximum retry of token generation
//     config.max_token_generation_retry = 5;
  
//     // Initialize the library with the Firebase authen and config
//     Firebase.begin(&config, &auth);
  
//     // Getting the user UID might take a few seconds
//     Serial.println("Getting User UID");
//     while ((auth.token.uid) == "") {
//         Serial.print('.');
//         delay(1000);
//     }
  
//     // Print user UID
//     uid = auth.token.uid.c_str();
//     Serial.print("User UID: ");
//     Serial.println(uid);
  
//     // Update database path
//     databasePath = "/UsersData/" + uid + "/readings";
// }

// void loop() {
//     // Check if it's time to read the sensors and send data
//     if (Firebase.ready()) {
//         // Read DHT sensor values
//         out_humidity = dht.readHumidity();
//         out_temperature = dht.readTemperature();
//         in_humidity = dht2.readHumidity();
//         in_temperature = dht2.readTemperature();
        
//         // Read weight from load cell
//         weight = getWeight();
//         Serial.print("Weight: ");
//         Serial.print(weight);
//         Serial.println(" kg");
        
//         // Get current timestamp
//         timestamp = getTime();
//         Serial.print("time: ");
//         Serial.println(timestamp);

//         parentPath = databasePath + "/" + String(timestamp);

//         // Clear the JSON object before setting new values
//         json.clear();

//         // Set values in the JSON
//         json.set(outsideTempPath.c_str(), String(out_temperature));
//         json.set(outsideHumPath.c_str(), String(out_humidity));
//         json.set(insideTempPath.c_str(), String(in_temperature));
//         json.set(insideHumPath.c_str(), String(in_humidity));
//         json.set(weightPath.c_str(), String(weight));  // Add weight data
//         json.set(timePath.c_str(), String(timestamp));

//         // Print the JSON string representation
//         String jsonString;
//         json.toString(jsonString);
//         Serial.print("JSON Data: ");
//         Serial.println(jsonString);

//         // Send the JSON data to the database
//         Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
//     }

//     // Go to deep sleep for 60 seconds (60,000,000 microseconds)
//     esp_deep_sleep(60 * 1000000);
// }

// // Initialize HX711 for weight measurements
// void setupHX711() {
//     scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
//     scale.set_scale(calibration_factor);  // Apply calibration factor
//     scale.tare();  // Reset scale to 0
//     Serial.println("HX711 initialized");
// }

// // Function to get weight reading from HX711
// float getWeight() {
//     // Take multiple readings and average them for stability
//     const int numReadings = 10;
//     float total = 0;
    
//     for (int i = 0; i < numReadings; i++) {
//         total += scale.get_units();
//         delay(10);
//     }
    
//     // Return the average reading
//     return total / numReadings;
// }

// void initWiFi() {
//     WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//     Serial.print("Connecting to WiFi ..");
//     while (WiFi.status() != WL_CONNECTED) {
//       Serial.print('.');
//       delay(1000);
//     }
//     Serial.println(WiFi.localIP());
//     Serial.println();
// }

// unsigned long getTime() {
//     time_t now;
//     struct tm timeinfo;
//     if (!getLocalTime(&timeinfo)) {
//         return(0);
//     }
//     time(&now);
//     return now;
// }