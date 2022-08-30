# mqtt-uart-bridge
This implements a 'bridge' which runs on an ESP8266 and allows a device which can only talk on Serial to connect to WiFi and an MQTT broker and then to communicate with that broker, publishing messages and subscribing to topics.

The code implements a basic language which allows commands to be sent on the serial port to control actions such as connecting to the network, connecting to an MQTT broker and then interacting with that broker.

## Language

### Commands

Each command is a comma separated list of values, starting with a $ character.  The first word adter the $ is a keyword and the fields which come after it are the values that keyword needs e.g.

```
$WIFI,ssid,password
```

| Command | Fields | Description |
| --- | --- | --- |
| `$WIFI`| ssid,password | Connects to a WiFi network using the provided ssid and password |
| `$BROKER` | ip,port,name[,username,password] | Connects to an MQTT broker.  Username and password is optional.  Client name is set to the value provided in the `name` field |
| `$PUB` | topic,payload | Sends the message in payload to the specified topic |
| `$SUB` | topic | Subscribe to the specified topic |

### Responses

The bridge will write to the Serial port at different times.  Each of these messages will be prefixed in a specific way.

| Prefix | Structure | Meaning |
| --- | --- | --- | 
| `$MESSAGE` | topic,payload | Emitted when `message` is receieved on `topic` |
| `$INFO` | module,log | Emitted during normal operations when something happens which the consumer needs to be aware of |
| `$DEBUG` | module,log | Emitted when debug logging is enabled |
| `$ERROR` | module,log | Emitted when an error condition occurs |
| `$STATUS` | wifi status,ip address,mqtt connected | Emitted periodically so that consumer can be aware of the current state |

## Behaviour

The client will attempt to remain connected to the WiFi network and MQTT broker.  It will reconnect to the broker if disconnected and attempt to resubscribe to previously subscribed topics if the connection is lost.

## Configuration

Some of the configuration is defined in the code

```c
#define MAX_COMMAND 50
#define MAX_COMMAND_FIELDS 10
#define MAX_SUB_TOPICS 20
#define MAX_WIFI_WAIT 20
#define STATUS_DELAY 10000
#define DEBUG 1
```

| Setting | Use | 
| --- | --- |
| `MAX_COMMAND` | Maximum length of the command string |
| `MAX_COMMAND_FIELDS` | Maximum number of fields in a command |
| `MAX_SUB_TOPICS` | Maximum number of MQTT topics which can be subscribed to |
| `MAX_WIFI_WAIT` | How many cycles should we wait until the WiFi connects |
| `STATUS_DELAY` | How many milliseconds between status updates |
| `DEBUG` | Should we write debug log messages? |
