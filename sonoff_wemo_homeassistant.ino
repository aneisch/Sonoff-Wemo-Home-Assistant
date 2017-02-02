#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUDP.h>

//Majority of code is from https://github.com/torinnguyen/ESP8266Wemo

const char* ssid = "YOUR_SSID";                       // your network SSID (name)
const char* pass = "YOUR_PASSCODE";                       // your network password

String friendlyName = "Emulated Wemo";           // Alexa and/or Home Assistant will use this name to identify your device
const char* serialNumber = "221517K0101768";                  // anything will do
const char* uuid = "904bfa3c-1de2-11v2-8728-fd8eebaf492d";    // anything will do

// Multicast declarations for discovery
IPAddress ipMulti(239, 255, 255, 250);
const unsigned int portMulti = 1900;

// TCP port to listen on
const unsigned int webserverPort = 49153;

const int LED_PIN = 13;
const int RELAY_PIN = 12;
const int SWITCH_PIN = 0;
//initial switch (button) state
int switchState = 0;


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

//int status = WL_IDLE_STATUS;

WiFiUDP Udp;
byte packetBuffer[512]; //buffer to hold incoming and outgoing packets

//Start TCP server
ESP8266WebServer server(webserverPort);


//-----------------------------------------------------------------------
// UDP Multicast Server
//-----------------------------------------------------------------------

char* getDateString()
{
  //Doesn't matter which date & time, will work
  //Optional: replace with NTP Client implementation
  return "Wed, 29 Jun 2016 00:13:46 GMT";
}

void responseToSearchUdp(IPAddress& senderIP, unsigned int senderPort) 
{
  Serial.println("responseToSearchUdp");

  //This is absolutely neccessary as Udp.write cannot handle IPAddress or numbers correctly like Serial.print
  IPAddress myIP = WiFi.localIP();
  char ipChar[20];
  snprintf(ipChar, 20, "%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
  char portChar[7];
  snprintf(portChar, 7, ":%d", webserverPort);

  Udp.beginPacket(senderIP, senderPort);
  Udp.write("HTTP/1.1 200 OK\r\n");
  Udp.write("CACHE-CONTROL: max-age=86400\r\n");
  Udp.write("DATE: ");
  Udp.write(getDateString());
  Udp.write("\r\n");
  Udp.write("EXT:\r\n");
  Udp.write("LOCATION: ");
  Udp.write("http://");
  Udp.write(ipChar);
  Udp.write(portChar);
  Udp.write("/setup.xml\r\n");
  Udp.write("OPT: \"http://schemas.upnp.org/upnp/1/0/\"); ns=01\r\n");
  Udp.write("01-NLS: ");
  Udp.write(uuid);
  Udp.write("\r\n");
  Udp.write("SERVER: Unspecified, UPnP/1.0, Unspecified\r\n");
  Udp.write("X-User-Agent: redsonic\r\n");
  Udp.write("ST: upnp:rootdevice\r\n");
  Udp.write("USN: uuid:Socket-1_0-");
  Udp.write(serialNumber);
  Udp.write("upnp:rootdevice\r\n");
  Udp.write("\r\n");
  Udp.endPacket();
}

void UdpMulticastServerLoop()
{
  int numBytes = Udp.parsePacket();
  if (numBytes <= 0)
    return;

  IPAddress senderIP = Udp.remoteIP();
  unsigned int senderPort = Udp.remotePort();
  
  // read the packet into the buffer
  Udp.read(packetBuffer, numBytes); 

  // print out the received packet
  //Serial.write(packetBuffer, numBytes);

  // check if this is a M-SEARCH for WeMo device
  String request = String((char *)packetBuffer);
  int mSearchIndex = request.indexOf("M-SEARCH");
  if (mSearchIndex >= 0)
    //return;
    responseToSearchUdp(senderIP, senderPort);
}


//-----------------------------------------------------------------------
// HTTP Server
//-----------------------------------------------------------------------

void handleRoot() 
{
  Serial.println("handleRoot");

  server.send(200, "text/plain", "Tell Alexa to discover devices");
}

void handleEventXml()
{
  Serial.println("HandleEventXML");
    
  String eventservice_xml = "<scpd xmlns=\"urn:Belkin:service-1-0\">"
        "<actionList>"
          "<action>"
            "<name>SetBinaryState</name>"
            "<argumentList>"
              "<argument>"
                "<retval/>"
                "<name>BinaryState</name>"
                "<relatedStateVariable>BinaryState</relatedStateVariable>"
                "<direction>in</direction>"
                "</argument>"
            "</argumentList>"
          "</action>"
          "<action>"
            "<name>GetBinaryState</name>"
            "<argumentList>"
              "<argument>"
                "<retval/>"
                "<name>BinaryState</name>"
                "<relatedStateVariable>BinaryState</relatedStateVariable>"
                "<direction>out</direction>"
                "</argument>"
            "</argumentList>"
          "</action>"
      "</actionList>"
        "<serviceStateTable>"
          "<stateVariable sendEvents=\"yes\">"
            "<name>BinaryState</name>"
            "<dataType>Boolean</dataType>"
            "<defaultValue>0</defaultValue>"
           "</stateVariable>"
           "<stateVariable sendEvents=\"yes\">"
              "<name>level</name>"
              "<dataType>string</dataType>"
              "<defaultValue>0</defaultValue>"
           "</stateVariable>"
        "</serviceStateTable>"
        "</scpd>\r\n"
        "\r\n";

  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/xml\r\n";
  header += "Content-Length: ";
  header += eventservice_xml.length();
  header += "\r\n";
  header += "Date: ";
  header += getDateString();
  header += "\r\n";
  header += "LAST-MODIFIED: Sat, 01 Jan 2000 00:00:00 GMT\r\n";
  header += "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n";
  header += "X-User-Agent: redsonic\r\n";
  header += "connection: close\r\n";
  header += "\r\n";
  header += eventservice_xml;

  Serial.println(header);
  
  server.sendContent(header);
}

void handleSetupXml()
{
  Serial.println("handleSetupXml");
    
  String body = 
  "<?xml version=\"1.0\"?>\r\n"
  "<root xmlns=\"urn:Belkin:device-1-0\">\r\n"
  "<specVersion>\r\n"
    "<major>1</major>\r\n"
    "<minor>0</minor>\r\n"
        "</specVersion>\r\n"
        "<device>\r\n"
          "<deviceType>urn:Belkin:device:controllee:1</deviceType>\r\n"
          "<friendlyName>" + friendlyName + "</friendlyName>\r\n"
              "<manufacturer>Belkin International Inc.</manufacturer>\r\n"
              "<manufacturerURL>http://www.belkin.com</manufacturerURL>\r\n"
              "<modelDescription>Belkin Plugin Socket 1.0</modelDescription>\r\n"
              "<modelName>Socket</modelName>\r\n"
              "<modelNumber>1.0</modelNumber>\r\n"
              "<UDN>uuid:Socket-1_0-" + uuid + "</UDN>\r\n"
              "<modelURL>http://www.belkin.com/plugin/</modelURL>\r\n"
            "<serialNumber>" + serialNumber + "</serialNumber>\r\n"
            "<serviceList>\r\n"
              "<service>\r\n"
                "<serviceType>urn:Belkin:service:basicevent:1</serviceType>\r\n"
                "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>\r\n"
                "<controlURL>/upnp/control/basicevent1</controlURL>\r\n"
                "<eventSubURL>/upnp/event/basicevent1</eventSubURL>\r\n"
                "<SCPDURL>/eventservice.xml</SCPDURL>\r\n"
              "</service>\r\n"
            "</serviceList>\r\n"
          "</device>\r\n"
        "</root>\r\n"
        "\r\n";

  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/xml\r\n";
  header += "Content-Length: ";
  header += body.length();
  header += "\r\n";
  header += "Date: ";
  header += getDateString();
  header += "\r\n";
  header += "LAST-MODIFIED: Sat, 01 Jan 2000 00:00:00 GMT\r\n";
  header += "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n";
  header += "X-User-Agent: redsonic\r\n";
  header += "connection: close\r\n";
  header += "\r\n";
  header += body;

  Serial.println(header);
  
  server.sendContent(header);
}

void handleUpnpControl()
{
  Serial.println("handleUpnpControl");

  //Extract raw body
  //Because there is a '=' before "1.0", it will give the following:
  //"1.0" encoding="utf-8"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body><u:SetBinaryState xmlns:u="urn:Belkin:service:basicevent:1"><BinaryState>1</BinaryState></u:SetBinaryState></s:Body></s:Envelope>
  String body = server.arg(0);

  //Check valid request
  boolean isOn = body.indexOf("<BinaryState>1</BinaryState>") >= 0;
  boolean isOff = body.indexOf("<BinaryState>0</BinaryState>") >= 0;
  boolean isQuestion = body.indexOf("GetBinaryState") >= 0;
  Serial.println("body:");
  Serial.println(body);
  boolean isValid = isOn || isOff || isQuestion;
  if (!isValid) {
    Serial.println("Bad request from Amazon Echo");
    //Serial.println(body);
    server.send(400, "text/plain", "Bad request from Amazon Echo");
    return;
  }

  //On/Off Logic
  if (isOn) {
      digitalWrite(LED_PIN, 0);
      digitalWrite(RELAY_PIN, 1);
      Serial.println("Alexa is asking to turn ON a device");
      String body = 
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>1</BinaryState>\r\n"
      "</u:SetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";
      String header = "HTTP/1.1 200 OK\r\n";
      header += "Content-Length: ";
      header += body.length();
      header += "\r\n";
      header += "Content-Type: text/xml\r\n";
      header += "Date: ";
      header += getDateString();
      header += "\r\n";
      header += "EXT:\r\n";
      header += "SERVER: Linux/2.6.21, UPnP/1.0, Portable SDK for UPnP devices/1.6.18\r\n";
      header += "X-User-Agent: redsonic\r\n";
      header += "\r\n";
      header += body;
      server.sendContent(header);
  }
  else if (isOff) {
      digitalWrite(LED_PIN, 1);
      digitalWrite(RELAY_PIN, 0);
      Serial.println("Alexa is asking to turn OFF a device");
      String body = 
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:SetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>0</BinaryState>\r\n"
      "</u:SetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";
      String header = "HTTP/1.1 200 OK\r\n";
      header += "Content-Length: ";
      header += body.length();
      header += "\r\n";
      header += "Content-Type: text/xml\r\n";
      header += "Date: ";
      header += getDateString();
      header += "\r\n";
      header += "EXT:\r\n";
      header += "SERVER: Linux/2.6.21, UPnP/1.0, Portable SDK for UPnP devices/1.6.18\r\n";
      header += "X-User-Agent: redsonic\r\n";
      header += "\r\n";
      header += body;
      server.sendContent(header);
  }

  else if (isQuestion) {
      String body = 
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:GetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>" + String(digitalRead(12)) + "</BinaryState>\r\n"
      "</u:GetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>";
      String header = "HTTP/1.1 200 OK\r\n";
      header += "Content-Length: ";
      header += body.length();
      header += "\r\n";
      header += "Content-Type: text/xml\r\n";
      header += "Date: ";
      header += getDateString();
      header += "\r\n";
      header += "EXT:\r\n";
      header += "SERVER: Linux/2.6.21, UPnP/1.0, Portable SDK for UPnP devices/1.6.18\r\n";
      header += "X-User-Agent: redsonic\r\n";
      header += "\r\n";
      header += body;
      server.sendContent(header);
  }
}

void handleNotFound()
{
  Serial.println("handleNotFound()");
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  Serial.println();

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT);

  digitalWrite(LED_PIN, 1);
  digitalWrite(RELAY_PIN, 0);

  // setting up Station AP
  WiFi.begin(ssid, pass);

  // Wait for connect to AP
  Serial.print("[Connecting to ");
  Serial.print(ssid);
  Serial.print("]...");
  int tries=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
    tries++;
    if (tries > 50)
      break;
  }
  
  // print your WiFi info
  IPAddress ip = WiFi.localIP();
  Serial.println();
  Serial.print("Connected to ");
  Serial.print(WiFi.SSID());
  Serial.print(" with IP: ");
  Serial.println(ip);
  
  //UDP Server
  Udp.beginMulticast(WiFi.localIP(),  ipMulti, portMulti);
  Serial.print("Udp multicast server started at ");
  Serial.print(ipMulti);
  Serial.print(":");
  Serial.println(portMulti);
  
  //Web Server
  server.on("/", handleRoot);
  server.on("/setup.xml", handleSetupXml);
  server.on("/eventservice.xml", handleEventXml);
  server.on("/upnp/control/basicevent1", handleUpnpControl);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.print("HTTP server started on port ");
  Serial.println(webserverPort);
}

void loop(){


if (digitalRead(SWITCH_PIN)){
  delay(250);
  if (!digitalRead(SWITCH_PIN)){
  switchState = !switchState;
  Serial.print("Switch Pressed - Relay now "); 

// Show and change Relay State  
  if (digitalRead(13)){
    Serial.println("ON");
      digitalWrite(LED_PIN, 0);
      digitalWrite(RELAY_PIN, 1);  }
    else
    {
    Serial.println("OFF");
      digitalWrite(LED_PIN, 1);
      digitalWrite(RELAY_PIN, 0);
    }
  delay(500);
  }
  }

  
UdpMulticastServerLoop();   //UDP multicast receiver

server.handleClient();      //Webserver
}

