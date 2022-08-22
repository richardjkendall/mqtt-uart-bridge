#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define MAX_COMMAND 50
#define MAX_COMMAND_FIELDS 10
#define MAX_SUB_TOPICS 20
#define MAX_WIFI_WAIT 20
#define STATUS_DELAY 10000
#define DEBUG 1

/*
const char* mqttServer   = "10.152.183.246";
const int   mqttPort     = 1883;
*/
long lastReconnectAttempt = 0;
long lastStatusMessage = 0;
long lastTimeSend = 0;
bool gotWifiDetails = false;
char* wifiSsid;
char* wifiPassword;
bool gotMqttDetails = false;
bool mqttLoginNeeded = false;
char* mqttBrokerIp;
char* mqttBrokerPort;
char* mqttClientName;
char* mqttBrokerUsername;
char* mqttBrokerPassword;
int mqttSubCount = 0;
char* mqttTopics[MAX_SUB_TOPICS];

char serialBuffer[MAX_COMMAND] = {0};
int serialBufferPos = 0;
String error;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void pubSubCallback(char* topic, byte* payload, unsigned int length) {
  // need to get the payload, the pointer to the byte array may contain more data so we need to use the length
  char* message = (char*) malloc(length + 1);
  memcpy(message, payload, length*sizeof(char));
  message[length] = '\0';  // null terminate the string
    
  if(DEBUG) {
    writeDebug("message-in", message);
  }

  Serial.print("$MESSAGE,");
  Serial.print(topic);
  Serial.print(",");
  Serial.println(message);

  free(message);
}

void writeDebug(String module, String message) {
  Serial.print("$DEBUG,");
  Serial.print(module);
  Serial.print(",");
  Serial.println(message);
}

void writeStatus(String module, String message) {
  Serial.print("$INFO,");
  Serial.print(module);
  Serial.print(",");
  Serial.println(message);
}

void writeError(String module, String message) {
  Serial.print("$ERROR,");
  Serial.print(module);
  Serial.print(",");
  Serial.println(message);
}

int parse_comma_delimited_str(char *string, char **fields, int max_fields) {
  int i = 0;
  fields[i++] = string;
  while ((i < max_fields) && NULL != (string = strchr(string, ','))) {
    *string = '\0';
    fields[i++] = ++string;
  }
  return --i;
}

bool initWifiStation() {
  int loopCount = 0;
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(wifiSsid, wifiPassword);    
  writeStatus("wifi", "connecting");
  while (WiFi.status() != WL_CONNECTED && loopCount < MAX_WIFI_WAIT) {
     delay(1000);
     writeStatus("wifi", "still connecting");
     loopCount++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    writeStatus("wifi", "connected");
    return true;
  } else {
    writeError("wifi", "connection timed out");
    return false;
  }
}

void subscribeToTopic(char* topic) {
  if(mqttClient.subscribe(topic)) {
    char message[100];
    sprintf(message, "subscribed to %s", topic);
    writeStatus("mqtt", message);
  } else {
    char message[100];
    sprintf(message, "failed to subscribe to %s", topic);
    writeError("mqtt", message);
  }
  
}

void initMQTTClient() {
  // Connecting to MQTT server
  mqttClient.setServer(mqttBrokerIp, atoi(mqttBrokerPort));
  if(DEBUG) {
    writeDebug("mqtt", mqttBrokerIp);
    writeDebug("mqtt", mqttBrokerPort);
  }
  mqttClient.setCallback(pubSubCallback);
  if(mqttLoginNeeded) {
    writeStatus("mqtt", "attempting to connect with credentials");
    if(mqttClient.connect(mqttClientName, mqttBrokerUsername, mqttBrokerPassword)) {
      writeStatus("mqtt", "connected");
    } else {
      writeError("mqtt", "failed to connect");
    }
  } else {
    writeStatus("mqtt", "attempting to connect without credentials");
    if(mqttClient.connect(mqttClientName)) {
      writeStatus("mqtt", "connected");
    } else {
      writeError("mqtt", "failed to connect");
    }
  }
  // if we get here are are connected then we can subscribe to our list of topics, this is useful for reconnect scenarios
  if(mqttClient.connected()) {
    subscribeToTopic(mqttClientName);
    writeStatus("mqtt", "subscribing to any saved topics...");
    for(int i = 0; i < mqttSubCount; i++) {
      subscribeToTopic(mqttTopics[i]);
    }
  }
}

void copyString(char **dest, char **src) {
  *dest = (char*) malloc((strlen(*src)+1) * sizeof(char));
  strcpy(*dest, *src);
}

bool processCommand() {
  if(DEBUG) {
    writeDebug("serial in", serialBuffer);
  }
  char *field[MAX_COMMAND_FIELDS];
  int token_count = parse_comma_delimited_str(serialBuffer, field, MAX_COMMAND_FIELDS);
  
  /* WIFI */
  if(strcmp(field[0], "$WIFI") == 0) {
    // this is the WIFI command, check we have enough fields (expect 2)
    if(token_count == 2) {
      if(WiFi.status() == WL_CONNECTED) {
        // we are already connected, so disconnect
        writeStatus("wifi", "disconnecting");
        WiFi.disconnect();
      }
      // store the details
      //wifiSsid = (char*) malloc((strlen(field[1])+1) * sizeof(char));
      //strcpy(wifiSsid, field[1]);
      //wifiSsid = field[1];
      //wifiPassword = field[2];
      copyString(&wifiSsid, &field[1]);
      copyString(&wifiPassword, &field[2]);
      gotWifiDetails = true;
      // trigger the connect
      initWifiStation();
    } else {
      writeError("wifi","command expects 2 parameters: ssid/password");
    }
  }

  /* MQTT Broker connect */
  if(strcmp(field[0], "$BROKER") == 0) {
    // this is the MQTT broker command, we need to check how many fields we have - if three then no username/password is needed
    // we need to be connected to wifi first
    if(WiFi.status() == WL_CONNECTED) {
      if(token_count == 3) {
        writeStatus("mqtt", "no login details for MQTT broker");
        gotMqttDetails = true;
        mqttLoginNeeded = false;
        copyString(&mqttBrokerIp, &field[1]);
        copyString(&mqttBrokerPort, &field[2]);
        copyString(&mqttClientName, &field[3]);
        initMQTTClient();
      } else {
        if(token_count == 5) {
          writeStatus("mqtt", "login details for the MQTT broker provided");
          gotMqttDetails = true;
          mqttLoginNeeded = true;
          copyString(&mqttBrokerIp, &field[1]);
          copyString(&mqttBrokerPort, &field[2]);
          copyString(&mqttClientName, &field[3]);
          copyString(&mqttBrokerUsername, &field[4]);
          copyString(&mqttBrokerPassword, &field[5]);
          initMQTTClient();
        } else {
          writeError("mqtt", "command expects either 3 or 5 parameters");
        }
      }
    } else {
      writeError("mqtt", "cannot connect to broker when not connected to wifi");
    }
  }

  /* MQTT publish */
  if(strcmp(field[0], "$PUB") == 0) {
    // this is the publish command, it should have two fields, the target topic and the message to send
    // we need to be connected to the broker and wifi
    if(WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
      if(token_count == 2) {
        mqttClient.publish(field[1], field[2]);
        writeStatus("pub", "message sent");
      } else {
        writeError("pub", "command expects 2 parameters");
      }
    } else {
      writeError("pub", "cannot send to a topic if not connected to wifi and/or the MQTT broker");
    }
  }

  /* MQTT subscribe */
  if(strcmp(field[0], "$SUB") == 0) {
    // this is the subscribe command, it should have one field, the target topic
    // we need to be connected to the broker and wifi
    if(WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
      if(token_count == 1) {
        // check if we have any spare topics to subscribe to
        if(mqttSubCount == MAX_SUB_TOPICS) {
          writeError("sub", "you have no spare subscription slots available");
        } else {
          //mqttTopics[mqttSubCount] = field[1];
          copyString(&mqttTopics[mqttSubCount], &field[1]);
          mqttSubCount++;
          subscribeToTopic(field[1]);
        }
      } else {
        writeError("sub", "command expects a single parameter, the topic to be subscribed to");
      }
    } else {
      writeError("sub", "cannot subscribe to a topic if not connected to wifi and/or the MQTT broker");
    }
  }
  
  return true;
}

void loop() {
  /*if(!mqttClient.connected()) {
    long now = millis();
    if(now - lastReconnectAttempt > 5000) {
      Serial.println("Attempting to reconnect...");
      lastReconnectAttempt = now;
      if(initMQTTClient()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // check how long we've been up for?
    long now = millis();
    if(now - lastTimeSend > 5000) {
      lastTimeSend = now;
      mqttClient.publish(PUB_GATE_UPTIME, );
    }
    mqttClient.loop();
  }*/
  if(Serial.available() > 0) {
    char rec = Serial.read();
    if (rec == '\n') {
      serialBuffer[serialBufferPos] = '\0';
      serialBufferPos = 0;
      processCommand();
    } else {
      if(serialBufferPos > MAX_COMMAND - 1) {
        serialBufferPos = 0;
      }
      serialBuffer[serialBufferPos] = rec;
      serialBufferPos++;
    }
  } else {
    // if not serial available then we should check other things are still good and send the status message
    // we should check the client is still connected
    long now = millis();
    if(mqttClient.connected()) {
      mqttClient.loop();
    } else {
      // if we are not connected, we should attempt a reconnect if we've waited long enough
      // we only do this if we've got the details to connect
      if(gotWifiDetails && gotMqttDetails) {
        if(now - lastReconnectAttempt > 5000) {
          writeStatus("mqtt", "attempting to connect again");
          lastReconnectAttempt = now;
          // if wifi has disconnected, we should try that first
          if(WiFi.status() != WL_CONNECTED) {
            writeStatus("wifi", "attempting reconnect");
            if(initWifiStation()) {
              writeStatus("mqtt", "attempting reconnect");
              initMQTTClient();
            }
          } else {
            writeStatus("mqtt", "attempting reconnect");
            initMQTTClient();
          }
        }
      }
    }
    // we should emit a status message every now and again
    if(now - lastStatusMessage > STATUS_DELAY || now - lastStatusMessage < 0) {
      lastStatusMessage = now;
      Serial.print("$STATUS,");
      Serial.print(WiFi.status());
      Serial.print(",");
      Serial.print(WiFi.localIP());
      Serial.print(",");
      Serial.println(mqttClient.connected());
    }
  }
}

void setup() {
  // turn on serial comms
  // baud rate 19.2kbps
  Serial.begin(19200);

  // set last reconnect and status times to 0
  lastReconnectAttempt = 0;
  lastStatusMessage = 0;
}
