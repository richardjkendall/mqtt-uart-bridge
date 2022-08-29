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
