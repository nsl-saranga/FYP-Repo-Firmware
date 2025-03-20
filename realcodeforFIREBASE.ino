#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include "time.h"


// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.

#include "addons/RTDBHelper.h"

#define DHTPIN 5
#define DHTPIN2 4
#define DHTTYPE DHT22


// #define GAS_READ_PIN 34
// Insert your network credentials
#define WIFI_SSID "SLT-Fiber-2.4G-66B0"
#define WIFI_PASSWORD "1e6a267ea!"

// Insert Firebase project API Key
#define API_KEY "AIzaSyDQJzcyH3CSvp1v_xTpsRufrMXnU7bHXv8"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "hesaranisal97@gmail.com"
#define USER_PASSWORD "hesaraandnisal"

// Insert RTDB URL
#define DATABASE_URL "https://dataset-5e91e-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Function prototypes
void initWiFi();
unsigned long getTime();

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
String outsideTempPath = "/outside_temperature";
String outsideHumPath = "/outside_humidity";
String insideTempPath = "/inside_temperature";
String insideHumPath = "/inside_humidity";
// String gasPath = "/air_quality";
String timePath = "/timestamp";

// Parent Node (to be updated in every loop)
String parentPath;

int timestamp;
FirebaseJson json;

const char* ntpServer = "pool.ntp.org";

float out_temperature;
float out_humidity;
float in_temperature;
float in_humidity;
// int air_quality;
unsigned long sendDataPrevMillis = 0;
const int timerDelay = 60000;  // Send data every 60 seconds

DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);


void setup() {
    Serial.begin(115200);
    dht.begin();
    dht2.begin();
    initWiFi();
    configTime(0, 0, ntpServer);
  
    // Assign the api key (required)
    config.api_key = API_KEY;
  
    // Assign the user sign in credentials
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
  
    // Assign the RTDB URL (required)
    config.database_url = DATABASE_URL;
  
    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(4096);
  
    // Assign the callback function for the long running token generation task
    config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
    // Assign the maximum retry of token generation
    config.max_token_generation_retry = 5;
  
    // Initialize the library with the Firebase authen and config
    Firebase.begin(&config, &auth);
  
    // Getting the user UID might take a few seconds
    Serial.println("Getting User UID");
    while ((auth.token.uid) == "") {
        Serial.print('.');
        delay(1000);
    }
  
    // Print user UID
    uid = auth.token.uid.c_str();
    Serial.print("User UID: ");
    Serial.println(uid);
  
    // Update database path
    databasePath = "/UsersData/" + uid + "/readings";
}

void loop() {
    // Check if it's time to read the sensors and send data
    if (Firebase.ready()) {
        // Read DHT sensor values
        out_humidity = dht.readHumidity();
        out_temperature = dht.readTemperature();
        in_humidity = dht2.readHumidity();
        in_temperature = dht2.readTemperature();
        // air_quality = analogRead(GAS_READ_PIN);
    
        
        // Get current timestamp
        timestamp = getTime();
        Serial.print("time: ");
        Serial.println(timestamp);

        parentPath = databasePath + "/" + String(timestamp);

        // Clear the JSON object before setting new values
        json.clear();

        // Set values in the JSON only if valid (skip NaN values)
        json.set(outsideTempPath.c_str(), String(out_temperature));
        json.set(outsideHumPath.c_str(), String(out_humidity));
        json.set(insideTempPath.c_str(), String(in_temperature));
        json.set(insideHumPath.c_str(), String(in_humidity));
        // json.set(gasPath.c_str(), String(air_quality));
        json.set(timePath.c_str(), String(timestamp));

        // Print the JSON string representation
        String jsonString;
        json.toString(jsonString);
        Serial.print("JSON Data: ");
        Serial.println(jsonString);

        // Send the JSON data to the database
        Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    }

    // Go to deep sleep for 60 seconds (60,000,000 microseconds)
    esp_deep_sleep(60 * 1000000);
}

void initWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(1000);
    }
    Serial.println(WiFi.localIP());
    Serial.println();
}

unsigned long getTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return(0);
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