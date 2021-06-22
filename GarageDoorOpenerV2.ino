#include <SimpleTimer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>       //mqtt
#include <ArduinoOTA.h>


/*************************** START USER CONFIG SECTION ***************************/

#define USER_SSID                 "YOUR_SSID"
#define USER_PASSWORD             "WIFI_PASSWORD"
#define USER_MQTT_SERVER          "MQTT_SERVER_ADDRESS"
#define USER_MQTT_PORT            1883
#define USER_MQTT_USER_NAME       "MQTT_USER"
#define USER_MQTT_PASSWORD        "MQTT_PASSWORD"
#define USER_MQTT_CLIENT_NAME     "GarageDoors"

#define DOORS                     2                   // how many doors to monitor
#define SENSOR_PINS               14,12               // remember arrays start at 0
#define OUTPUT_PINS               5,4

/*************************** END USER CONFIG SECTION ****************************/

WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;

// global Variables //
bool boot = true;
char charTopic[50];
char charPayload[50];
const int door_count = DOORS;
static const uint8_t sense_pins[] = {SENSOR_PINS};    // makes an array of input pins
static const uint8_t switch_pins[] = {OUTPUT_PINS};   // makes an array of output pins
byte door_state[door_count];                          // create array for door status
byte door_state_last[door_count];                     // create array for last state for doors
byte x=0;
byte refresh[door_count];
byte r=0;

// functions //
void setup_wifi() {
  WiFi.begin(USER_SSID, USER_PASSWORD);
}

// connect or reconnect to mqtt server
void reconnect() {
  int retries = 0;
  while (!client.connected()) {
    if (retries < 150) {
      if (client.connect(USER_MQTT_CLIENT_NAME, USER_MQTT_USER_NAME, USER_MQTT_PASSWORD)) {
        if (boot == false) { client.publish(USER_MQTT_CLIENT_NAME"/checkIn","Reconnected" ); }
        if (boot == true) { client.publish(USER_MQTT_CLIENT_NAME"/checkIn","Rebooted" ); }
        client.subscribe(USER_MQTT_CLIENT_NAME"/doorCommand");
      } else {
        retries++;
        delay(5000);
      }
    }
    if (retries > 149) { ESP.restart(); }
  }
}

// Check door sensors
void doorCheck() {
  if (x < door_count) {
    door_state[x] = digitalRead(sense_pins[x]);
    if (door_state[x] != door_state_last[x] || refresh[x] == 1) {
      String doorNum = String(x + 1); // convert int to string and add 1 so human readable
      String doorClose = String(USER_MQTT_CLIENT_NAME) + "/doorStatus/door"  + String(doorNum); // concat message for mqtt topic and then convert to char array
      doorClose.toCharArray(charTopic, doorClose.length() + 1);
      if (door_state[x] == LOW) {
        client.publish(charTopic, "closed", true);
        Serial.print("door ");
        Serial.print(doorNum);
        Serial.println(" Closed");
      } else {
        client.publish(charTopic, "open", true);
        Serial.print("door ");
        Serial.print(doorNum);
        Serial.println(" Open");
      }
      delay(50);
    }
    door_state_last[x] = door_state[x];
    if (refresh[x] == 1) { refresh[x] = 0; }
  }
  x = x+1;
  // reset door count to 0 after set count reached
  if (x > door_count) {x = 0;}
}

// control output pins to toggle doors with mqtt
void doorControl(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  int intPayload = newPayload.toInt();
  Serial.println(newPayload);
  Serial.println();
  newPayload.toCharArray(charPayload, newPayload.length() + 1);

  // mqtt topic of CLIENT_NAME/doorControl will trigger appropiate pin from payload (in array, so door 0 will be a payload of 0)
  if (newTopic == USER_MQTT_CLIENT_NAME"/doorCommand") {
    digitalWrite(switch_pins[intPayload - 1], LOW);
    delay(500);
    digitalWrite(switch_pins[intPayload - 1], HIGH);
  }
}

void checkIn() {
  for(int i=0;i<door_count;i++){ refresh[i] = 1; }
}

void setup() {
  // call wifi setup function
  WiFi.mode(WIFI_STA);
  setup_wifi();

  // setup mqtt server / client stuff
  client.setServer(USER_MQTT_SERVER, USER_MQTT_PORT);
  client.setCallback(doorControl);

  // setup sensor pins for input
  for (byte y = 0; y < door_count; y++) {
    pinMode(sense_pins[y], INPUT); 
  }

  // setup switch pins for output, and default to high
  for (byte y = 0; y < door_count; y++) {
    pinMode(switch_pins[y], OUTPUT); 
    digitalWrite(switch_pins[y], HIGH);
  }

  // setup arduino ota update
  ArduinoOTA.setHostname(USER_MQTT_CLIENT_NAME);
  ArduinoOTA.begin();

  // setup timer for refreshing door sensors
  timer.setInterval(60000, checkIn);
  
  Serial.begin(9600);
}

void loop() {
  doorCheck();
  if (!client.connected()) { reconnect(); }
  client.loop();
  ArduinoOTA.handle();
  timer.run();
}
