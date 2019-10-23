/*******************************************************/
#include <WiFi.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <H3LIS331DL.h>


/*********************************************************
  LED Stimulation
*/
#define led_1 18
#define led_2 25
#define led_3 32
#define led_4 33
bool led_state = false;


/**************************************************************
  Accelerometer
*/
H3LIS331DL h3lis;
byte error;
int8_t address;
int acc_total;
int accX , accY, accZ;
unsigned int sensitivity = 1000;
bool accState = true;
//Time
unsigned long startTime;
unsigned long lastTime;
unsigned int period = 10000;        // 10 second
//punchFrequency
unsigned int punchFrequency;
unsigned int punchCount;
//reactionTime
unsigned int reactionTime;



/**************************************************************
  Web Server
*/
// Constants
const char *ssid = "ESP32-AP";
const char *password =  "123456789";
const char *msg_toggle_led = "toggleLED";
const char *msg_get_led = "getLEDState";
const int dns_port = 53;
const int http_port = 80;
const int ws_port = 1337;

// Globals
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(1337);
char msg_buf[30];


/**************************************************************
  Functions
*/
// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num, WStype_t type, uint8_t * payload, size_t length) {
  // Figure out the type of WebSocket event
  switch (type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", client_num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(client_num);
        Serial.printf("[%u] Connection from ", client_num);
        Serial.println(ip.toString());
      }
      break;

    // Handle text messages from client
    case WStype_TEXT:
      // Print out raw message
      Serial.printf("[%u] Received text: %s\n", client_num, payload);

      if ( strcmp((char *)payload, "frequency") == 0 ) {
        punchCount = 0;
        accState = true;
        ledState(true);
        startTime = millis();
        while (accState) {
          Wire.beginTransmission(address);
          error = Wire.endTransmission();
          if (error == 0)
          { lastTime = millis();
            //Serial.println(lastTime - startTime);
            if ((lastTime - startTime) >= period ) {
              ledState(false);
              punchFrequency = (punchCount * 1000) / period;
              sprintf(msg_buf, "%s %u", "Mode_1 : ", punchFrequency);
              Serial.printf(" %s  punch/s \n", msg_buf);
              webSocket.sendTXT(client_num, msg_buf);
              accState = !accState;
            }
            // Display the results for Linear Acceleration values
            h3lis.Measure_Accelerometer();

            // Output data to screen
            accX = h3lis.h3lis_accelData.X;
            accY = h3lis.h3lis_accelData.Y;
            accZ = h3lis.h3lis_accelData.Z;
            acc_total = sqrt(pow(accX, 2) + pow(accY, 2) + pow(accZ, 2));
            //Serial.printf("Acceleration X =  %d\n", accX);
            //Serial.printf("Acceleration Y =  %d\n", accY);
            //Serial.printf("Acceleration Z =  %d\n", accZ);
            //Serial.printf("Acceleration total =  %u\n", acc_total);

            if (acc_total >= sensitivity) {
              punchCount++;
              //Serial.println( punchCount);
            }
          }
          else {
            Serial.println("H3LIS331DL Disconnected!");
          }
        }
      }
      else if ( strcmp((char *)payload, "reaction") == 0 ) {
        accState = true;
        ledState(true);
        startTime = millis();
        while (accState) {
          Wire.beginTransmission(address);
          error = Wire.endTransmission();
          if (error == 0)
          { // Display the results for Linear Acceleration values
            h3lis.Measure_Accelerometer();

            // Output data to screen
            accX = h3lis.h3lis_accelData.X;
            accY = h3lis.h3lis_accelData.Y;
            accZ = h3lis.h3lis_accelData.Z;
            acc_total = sqrt(pow(accX, 2) + pow(accY, 2) + pow(accZ, 2));
            //Serial.printf("Acceleration X =  %d\n", accX);
            //Serial.printf("Acceleration Y =  %d\n", accY);
            //Serial.printf("Acceleration Z =  %d\n", accZ);
            //Serial.printf("Acceleration total =  %u\n", acc_total);
            if (acc_total >= sensitivity ) {
              lastTime = millis();
              reactionTime = (lastTime - startTime);
              sprintf(msg_buf, "%s %u", "Mode_2 : ", reactionTime);
              Serial.printf(" %s  ms \n", msg_buf);
              webSocket.sendTXT(client_num, msg_buf);
              ledState(false);
              accState = !accState;
            }
          }
          else {
            Serial.println("H3LIS331DL Disconnected!");
          }
        }
      }
      else if ( strcmp((char *)payload, "Hello MCU") == 0 ) {
        sprintf(msg_buf, "%s", "Hello PC");
        Serial.printf("[%u] Sending text : %s\n", client_num, msg_buf);
        webSocket.sendTXT(client_num, msg_buf);
      }
      else {
        Serial.println("Message not recognized");
      }
      break;

    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}
void ledState(bool state) {
  led_state = state;
  digitalWrite(led_1, led_state);
  digitalWrite(led_2, led_state);
  digitalWrite(led_3, led_state);
  digitalWrite(led_4, led_state);
}

// Callback: send homepage
void onIndexRequest(AsyncWebServerRequest * request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +  "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/index.html", "text/html");
}

// Callback: send style sheet
void onCSSRequest(AsyncWebServerRequest * request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +  "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/style.css", "text/css");
}

// Callback: send 404 if requested file does not exist
void onPageNotFound(AsyncWebServerRequest * request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +  "] HTTP GET request of " + request->url());
  request->send(404, "text/plain", "Not found");
}


/**************************************************************
  Main
*/
void setup() {
  Serial.begin(115200);
  pinMode(led_1, OUTPUT);
  pinMode(led_2, OUTPUT);
  pinMode(led_3, OUTPUT);
  pinMode(led_4, OUTPUT);
  digitalWrite(led_1, led_state);
  digitalWrite(led_2, led_state);
  digitalWrite(led_3, led_state);
  digitalWrite(led_4, led_state);
  // The address can be changed making the option of connecting multiple devices
  h3lis.getAddr_H3LIS331DL(H3LIS331DL_ADDRESS_UPDATED);     // 0x19
  address = h3lis.h3lis_i2cAddress;

  // The Acceleration Data Rate Selection and Acceleration Full-Scale Selection
  h3lis.setAccelDataRate(ACCEL_DATARATE_1000HZ);           // AODR (Hz): 1000
  h3lis.setAccelRange(ACCEL_RANGE_400G);                   // เธขเธ‘400 G
  h3lis.begin();
  delay(500);
  if ( !SPIFFS.begin()) {
    Serial.println("Error mounting SPIFFS");
    while (1);
  }
  // Start access point
  WiFi.softAP(ssid, password);
  Serial.println();
  Serial.println("AP running");
  Serial.print("My IP address: ");
  Serial.println(WiFi.softAPIP());

  // On HTTP request for root, provide index.html file
  server.on("/", HTTP_GET, onIndexRequest);

  // On HTTP request for style sheet, provide style.css
  server.on("/style.css", HTTP_GET, onCSSRequest);

  // Handle requests for pages that do not exist
  server.onNotFound(onPageNotFound);

  // Start web server
  server.begin();

  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

}


void loop() {
  webSocket.loop();
}


