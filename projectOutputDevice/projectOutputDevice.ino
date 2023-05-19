/******************************************
projectOutputDevice.ino
created March 20, 2023
last modified April 3, 2023

author: Lucas Balog 20320245
code snippits from:
https://howtomechatronics.com/tutorials/arduino/how-to-control-ws2812b-individually-addressable-leds-using-arduino/
https://examples.blynk.cc/?board=ESP8266&shield=ESP8266%20WiFi&example=GettingStarted%2FGetData
https://docs.arduino.cc/
https://docs.arduino.cc/tutorials/uno-wifi-rev2/uno-wifi-r2-mqtt-device-to-device

description: 
program runs devices as one of two MQTT clients in a set up.
This devices collects cammands from the other arduino and runs the
appropriate leds as described in the runState() function. 
This device also collects speed data as an analog value from A0, packages
it with a time stamp and then sends it via MQTT on topic '178-d' to the
other arduino
*******************************************/

#include <ArduinoMqttClient.h>
#include <FastLED.h>
#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
#include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
#include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_GIGA)
#include <WiFi.h>
#endif

#define ssid "the big sky"
#define pass "47%Chickens"
#define LED_PIN D1
#define NUM_LEDS 7
CRGB leds[NUM_LEDS];

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "ubuntu";
int port = 1883;
const char topic[] = "178";

// define nodes of linked lists
typedef struct dataNode {
  struct dataNode *next;
  unsigned long time;
  int speed;
} DataNode, *pDataNode;

typedef struct cmdNode {
  int device;
  int cmd;
  struct cmdNode *next;
} CmdNode, *pCmdNode;

int pos = 0;
char message[4];

pCmdNode cmdHead = NULL;
pCmdNode cmdTail = NULL;
pDataNode dataTail = NULL;
pDataNode dataHead = NULL;

bool led1State = LOW;
bool led2State = LOW;
bool led3State = 1;
int led1cmd = 1;
int led2cmd = 1;
int led3cmd = 1;
uint8_t led1 = 4;
uint8_t led2 = 5;

// create new data list node
pDataNode initDataNode(double speed) {
  pDataNode n = (pDataNode)malloc(sizeof(struct dataNode));
  n->time = millis();
  n->speed = speed;
  n->next = NULL;
  return n;
}

//create new command list node
pCmdNode initCmdNode(int device, int cmd) {
  pCmdNode n = (pCmdNode)malloc(sizeof(struct cmdNode));
  n->device = device;
  n->cmd = cmd;
  n->next = NULL;
  return n;
}

// get data from A0 and add to tree;
void collectData() {
  pDataNode n = (pDataNode)initDataNode(analogRead(A0));
  if (dataHead == NULL) {
    dataHead = dataTail = n;
  } else {
    dataTail->next = n;
    dataTail = n;
  }
}

// remove head of data tree and sen over MQTT
void sendData() {
  pDataNode data;
  data = dataHead;
  String packet;
  if (data != NULL) {
    dataHead = dataHead->next;
    if (dataHead == NULL) dataTail = NULL;
    mqttClient.beginMessage("178-d");
    packet = "d" + String(data->time) + ":" + String(data->speed) + "E";
    mqttClient.print(packet);
    mqttClient.endMessage();
  }
  free(data);
}

// remove head of command tree and set led states from command
void setState() {
  pCmdNode cmd;
  cmd = cmdHead;
  if (cmdHead == NULL) {
    cmdTail = NULL;
  } else {
    Serial.print(cmd->device);
    Serial.println(cmd->cmd);
    cmdHead = cmd->next;
    if (cmd->device == 1) {
      led1cmd = cmd->cmd;
      Serial.print("Here" + String(led1cmd));
    } else if (cmd->device == 2) {
      led2cmd = cmd->cmd;
    } else if (cmd->device == 3) {
      led3cmd = cmd->cmd;
    }
  }
  free(cmd);
}

// run leds based on last assigned or defult states
void runState() {
  if (led1cmd == 1)
    led1State = LOW;
  else if (led1cmd == 2)
    led1State = HIGH;
  else if (led1cmd == 3)
    led1State = !led1State;
  if (led2cmd == 1)
    led2State = LOW;
  else if (led2cmd == 2)
    led2State = HIGH;
  else if (led2cmd == 3)
    led2State = !led2State;

  if (led3cmd == 1) {
    digitalWrite(16, HIGH);
    for (int i = 0; i < 7; i++)
      leds[i] = CRGB(0, 0, 0);
  } else if (led3cmd == 2) {
    digitalWrite(16, LOW);
    for (int i = 0; i < 7; i++)
      leds[i] = CRGB(255, 0, 0);
  } else if (led3cmd == 3) {
    for (int i = 0; i < 7; i++)
      leds[i] = CRGB(0, 255, 0);
  } else if (led3cmd == 4) {
    for (int i = 0; i < 7; i++)
      leds[i] = CRGB(0, 0, 255);
  } else if (led3cmd == 5) {
    for (int i = 0; i < 7; i++)
      leds[i] = CRGB(255, 255, 255);
  }
  FastLED.show();
  digitalWrite(D4, led1State);
  digitalWrite(D5, led2State);
}


void setup() {

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  pinMode(A0, INPUT);
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  pinMode(16, OUTPUT);
  digitalWrite(D4, HIGH);
  digitalWrite(D5, HIGH);
  delay(500);
  digitalWrite(D4, LOW);
  digitalWrite(D5, LOW);
  Serial.begin(9600);

  // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    // failed, retry
    Serial.print(WiFi.status());
    delay(1000);
  }
  Serial.println();

  // display wifi information
  Serial.println("You're connected to the network");
  Serial.print("You are connected as ");
  Serial.println(WiFi.localIP());
  Serial.print(" : ");
  WiFi.hostname("ESP1");
  Serial.println(WiFi.getHostname());  //for esp8266
  //Serial.println(WiFi.hostname()); // for esp32

  // connect to MQTT broker
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println("ubuntu");

  while (!mqttClient.connect("ubuntu", port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    delay(500);
  }
  Serial.println("You're connected to the MQTT broker!");
  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);
  Serial.print("Subscribing to topic: ");
  Serial.println(mqttClient.subscribe("178-c"));  // subscribe to topic
  Serial.print("Waiting for messages on topic: ");
  Serial.println("178-c");
}

void loop() {
  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();
  if (message[pos] == 'E') {

    pos = 0;
  }
  collectData();
  sendData();
  setState();
  runState();
  delay(150);
}

void onMqttMessage(int messageSize) {
  Serial.println("Message Received");
  //parse message
  for (pos = 0; pos < 4; pos++) {
    message[pos] = mqttClient.read();
  }
  //check is write starting format
  if (message[0] == 'c') {
    //add to list
    pCmdNode n = initCmdNode(message[1] - 48, message[2] - 48);
    if (cmdHead == NULL) {
      cmdHead = cmdTail = n;
    } else {
      cmdTail->next = n;
      cmdTail = n;
    }
  } else if (message[0] != 'E') {
    Serial.println("ERROR: Code from esp not 'c': ");
    Serial.print(String(message));
  }
}