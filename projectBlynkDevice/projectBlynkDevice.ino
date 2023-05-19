/******************************************
projectBlynk.ino
created March 30, 2023
last modified April 3, 2023

author: Lucas Balog 20320245
code snippits from:
  

description: 
program runs devices as one of two MQTT clients in a set up.
This devices collects cammands via the blynk virtual pins 1-3.
it takes an integr value from the blynk server and sents it to
the other aruduino via MQTT on topic '178-c'.
It also recieves data via MQTT on topic '178-d' and vritually writes
it to blynk virtual pin 0 to be displayed in the cloud
*******************************************/

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
//#include <BlynkMultiClient.h>
#include <ArduinoMqttClient.h>

typedef struct dataNode {
  struct dataNode* next;
  unsigned long time;
  int speed;
} DataNode, *pDataNode;

typedef struct cmdNode {
  int device;
  int cmd;
  struct cmdNode* next;
} CmdNode, *pCmdNode;

pDataNode initDataNode(double time, double speed) {
  pDataNode n = (pDataNode)malloc(sizeof(struct dataNode));
  n->time = time;
  n->speed = speed;
  n->next = NULL;
  return n;
}

pCmdNode initCmdNode(int device, int cmd) {
  pCmdNode n = (pCmdNode)malloc(sizeof(struct cmdNode));
  n->device = device;
  n->cmd = cmd;
  n->next = NULL;
  return n;
}

pCmdNode cmdHead = NULL;
pCmdNode cmdTail = NULL;
pDataNode dataTail = NULL;
pDataNode dataHead = NULL;

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "the big sky";
char pass[] = "47%Chickens";
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "ubuntu";
int port = 1883;
const char topic[] = "178";

// removes head of linked list and sends as mqtt message
void sendCmd() {
  pCmdNode cmd;
  cmd = cmdHead;
  if (cmd != NULL) {
    cmdHead = cmdHead->next;
    if (cmdHead == NULL) cmdTail = NULL;
    mqttClient.beginMessage("178-c");
    mqttClient.print('c');
    mqttClient.print(cmd->device);
    mqttClient.print(cmd->cmd);
    mqttClient.print('E');
    mqttClient.endMessage();
    Serial.println("Message Sent");
    free(cmd);
  }   
}

//removes head of linked list and writes to blynk virtual pin.
void sendData() {
  pDataNode data;
  data = dataHead;
  if (data != NULL) {
    dataHead = dataHead->next;
    if (dataHead == NULL) dataTail = NULL;
    Blynk.virtualWrite(V0, data->speed);
    free(data);
  }  
}

//run task for core 1
void task1(void* pvParameters) {
  for (;;) {
    Blynk.run();
  }
}

//run task for core 0
void task0(void* pvParameters) {
  for (;;) {
    mqttClient.poll();
    sendCmd();
    sendData();
    delay(100);
  }
}

// handel data writen from blynk to V1
BLYNK_WRITE(V1) {
  pCmdNode n = initCmdNode(1, param.asInt());  // assigning incoming value from pin V1 to a variable
  // add node to tree
  if (cmdHead == NULL) {
    cmdHead = cmdTail = n;
  } else {
    if (cmdTail != NULL) cmdTail->next = n;
    cmdTail = n;
  }
}

// handel data writen from blynk to V2
BLYNK_WRITE(V2) {
  pCmdNode n = initCmdNode(2, param.asInt());  // assigning incoming value from pin V2 to a variable
  // add node to tree
  if (cmdHead == NULL) {
    cmdHead = cmdTail = n;
  } else {
    if (cmdTail != NULL) cmdTail->next = n;
    cmdTail = n;
  }
}

// handel data writen from blynk to V3
BLYNK_WRITE(V3) {
  pCmdNode n = initCmdNode(3, param.asInt());  // assigning incoming value from pin V3 to a variable
  if (cmdHead == NULL) {
    cmdHead = cmdTail = n;
  } else {
    if (cmdTail != NULL) cmdTail->next = n;
    cmdTail = n;
  }
}

void setup() {
  pinMode(A0, INPUT);
  Serial.begin(9600);

  //Blynk.begin("f54Lh5bDePoO33eI_Ho7M0ZO8QaBN57g", "the big sky", "47%Chickens");
  

  // connect to wifi
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    // failed, retry
    Serial.print(WiFi.status());
    delay(1000);
  }
  Serial.println();

  //print wifi info
  Serial.println("You're connected to the network");
  Serial.print("You are connected as ");
  Serial.println(WiFi.localIP());
  Serial.print(" : ");
  WiFi.hostname("ESP2");
  Serial.println(WiFi.getHostname());// for esp32
  //Serial.println(WiFi.hostname());//for esp8266

  //connect to blynk
  
  Blynk.begin("f54Lh5bDePoO33eI_Ho7M0ZO8QaBN57g", "the big sky", "47%Chickens");
  //Blynk.connectWiFi("the big sky", "47%Chickens");
  // Blynk.addClient("WiFi", wifiClient, 80);
  // //Blynk.connect();
  // Blynk.config("f54Lh5bDePoO33eI_Ho7M0ZO8QaBN57g");
  // Blynk.connect();
  Serial.println("CONNECTED" + String((bool)Blynk.connected()));
  Serial.print("Configed");
  // connect to mqtt broker
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
  Serial.println("178-d");

  // subscribe to a topic
  mqttClient.subscribe("178-d");

  //assigne and run core tasks
  xTaskCreatePinnedToCore(task1, "Blynk", 10000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task0, "Data", 10000, NULL, 1, NULL, 0);
  delay(500);
}

void loop() {

}

// handel recieved MQTT messages
void onMqttMessage(int messageSize) {
  char message[50];
  int pos = 0, pos2 =0;
  //parse message
  while (mqttClient.available()) {
    message[pos] = mqttClient.read();
    pos++;
  }
  //check if in qrite format and parse further
  if (message[0] == 'd') {
    int pos2 = 0;
    pos = 1;
    char time[10], speed[4];
    while (message[pos] != ':') {
      time[pos2] = message[pos];
      pos2++;
      pos++;
    }
    pos++;
    pos2 = 0;
    while (message[pos] != 'E') {
      speed[pos2] = message[pos];
      pos2++;
      pos++;
    }
    // add to data tree
    pDataNode n = initDataNode(String(time).toDouble(), String(speed).toInt());
    
    if (dataHead == NULL) {
      dataHead = dataTail = n;
    } else {
      dataTail->next = n;
      dataTail = n;
    }
  } else {
    Serial.println("ERROR: not data ");
    Serial.print(message);
  }
}

