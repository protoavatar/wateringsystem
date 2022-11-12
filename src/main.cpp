#include <Arduino.h>
#include "ESP8266WiFi.h"
#include <WiFiUdp.h>          // Para el NTP
#include <TimeLib.h>
#include <ESP8266mDNS.h>      // Include the mDNS library
#include "LittleFS.h"        // To manage the file system where to store the web pages
#include <ESP8266WebServer.h> // Include the WebServer library
#include <WebSocketsServer.h> // Include to manage Websockets for web page communication to ESP
#include <EEPROM.h>

// -----------------------------------------------------------------------------
// Constant Declarations
// -----------------------------------------------------------------------------
#define WIFI_SSID "Jacobiano"  //Cambiar por tu WIFI SSID
#define WIFI_PASS "galgogalgo"  //Cambiar por tu WIFI password
#define HOST "RIEGOTEST"  //Cambiar por tu WIFI password
#define SERIAL_BAUDRATE 9600

// -----------------------------------------------------------------------------
// Funtion Declarations
// -----------------------------------------------------------------------------
void wifiSetup();
void digitalClockDisplay();
void sendNTPpacket(IPAddress &address);
void configNTP();
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleRoot(); // function prototypes for HTTP handlers
void handleNotFound();
void configWebSocket();
const char *LeerProgramacion(int addr); // Read Programing sent from page
void GrabarProgramacion(char payload[39]); // Save Programming on memory
// -----------------------------------------------------------------------------
// Variable Declarations
// -----------------------------------------------------------------------------
boolean logger = true;
// Websocket Watering Variables
char EstadoRiego[28] = "<#RIE#R1-0R2-0R3-0R4-0RT-0>";
char CodigoEnvio[6] = "#RIE#";
char *Programacion = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
// Case used only when ESP (eg NodeMCU) uses several watering lines and what a general disabled state for all
//char Activado[9] = "<#ACT#0>";

// -----------------------------------------------------------------------------
// NTP declarations
// -----------------------------------------------------------------------------
// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

const int timeZone = -3; // Central European Time

boolean ntp = false;

WiFiUDP Udp;
unsigned int localPort = 8888; // local port to listen for UDP packets

time_t getNtpTime();

// -----------------------------------------------------------------------------
// WebServer declarations
// -----------------------------------------------------------------------------
ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
WebSocketsServer webSocket(81); // create a websocket server on port 81


void setup() {
  // put your setup code here, to run once:
  Serial.begin(SERIAL_BAUDRATE);

    // Wifi
    wifiSetup();
    //NTP
    configNTP();
    digitalClockDisplay();
    //mDNS
      if (MDNS.begin(HOST))
  { // Start the mDNS responder for esp8266.local
    if (logger)
      Serial.println("mDNS responder started");
  }
  else
  {
    if (logger)
      Serial.println("Error setting up MDNS responder!");
  }
  // LittleFS
    LittleFS.begin(); // Start the SPI Flash Files System
  // Websockets
   configWebSocket(); // Start a WebSocket server
   // Web Server
      server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });
  // EEPROM Initialization
  EEPROM.begin(61);
  server.begin(); // Actually start the server
}

void loop() {
  // put your main code here, to run repeatedly:
  //mDNS
  MDNS.update();
  webSocket.loop();
  server.handleClient(); // Listen for HTTP requests from clients
  //Main program

}


// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup() {

    // Set WIFI module to STA mode
    WiFi.mode(WIFI_STA);

    // Connect
    Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Wait
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }
    Serial.println();

    // Connected!
    Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

}


// -----------------------------------------------------------------------------
// NTP
// -----------------------------------------------------------------------------
void configNTP()
{
  if (logger)
    Serial.println("Starting UDP");
  Udp.begin(localPort);
  if (logger)
    Serial.print("Local port: ");
  if (logger)
    Serial.println(Udp.localPort());
  if (logger)
    Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);
}



/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  if (logger)
    Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  if (logger)
    Serial.print(ntpServerName);
  if (logger)
    Serial.print(": ");
  if (logger)
    Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      if (logger)
        Serial.println("Receive NTP Response");
      ntp = true;
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  if (logger)
    Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints  leading 0
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}

// -----------------------------------------------------------------------------
// WebSockets
// -----------------------------------------------------------------------------
void webSocketEvent(byte num, WStype_t type, byte *payload, size_t lenght)
{ // When a WebSocket message is received
  switch (type)
  {
  case WStype_DISCONNECTED: // if the websocket is disconnected
    if (logger)
      Serial.printf("[%u] Disconnected!\n", num);
    break;
  case WStype_CONNECTED:
  { // if a new websocket connection is established
    IPAddress ip = webSocket.remoteIP(num);
    if (logger)
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
  }
  break;
  case WStype_TEXT: // if new text data is received
    if (strcmp((char *)payload, "Index") == 0)
    {
      webSocket.broadcastTXT(EstadoRiego);
      // Case used only when ESP (eg NodeMCU) uses several watering lines and what a general disabled state for all
      // EEPROM.get(0, Activado[6]);
      // // Serial.println(Activado[6]);
      // webSocket.broadcastTXT(Activado);
      if (timeStatus() != timeNotSet)
      {
        webSocket.broadcastTXT("<#NTP#Sincronizado#>");
      }
      else
      {
        webSocket.broadcastTXT("<#NTP#NOsincronizado#>");
      }
    }
    else if (strcmp((char *)payload, "Riego1") == 0)
    {
      webSocket.broadcastTXT(LeerProgramacion(1));
    }
    // In case there are several watering lines (eg with nodeMCU)
    // else if (strcmp((char *)payload, "Riego2") == 0)
    // {
    //   webSocket.broadcastTXT(LeerProgramacion(2));
    // }
    // else if (strcmp((char *)payload, "Riego3") == 0)
    // {
    //   webSocket.broadcastTXT(LeerProgramacion(3));
    // }
    // else if (strcmp((char *)payload, "Riego4") == 0)
    // {
    //   webSocket.broadcastTXT(LeerProgramacion(4));
    // }
    else if (payload[2] == 'R')
    {
      if (EstadoRiego[25] == '0') // Solo si no esta prendido el tanque
      {
        Serial.printf("%s\n", payload);
        if ((EstadoRiego[9] == '0' && EstadoRiego[13] == '0' && EstadoRiego[17] == '0' && EstadoRiego[21] == '0') && (payload[9] == '1' || payload[13] == '1' || payload[17] == '1' || payload[21] == '1'))
        {
          // HoraEncendido = now();
          if (logger)
            Serial.printf("Empieza el timer");
        }
        strcpy(EstadoRiego, (char *)payload);
        webSocket.broadcastTXT(payload);
      }
    }
    else if (payload[2] == 'P')
    {
      GrabarProgramacion((char *)payload);
    }
    break;
    // Case used only when ESP (eg NodeMCU) uses several watering lines and what a general disabled state for all
    // else if (payload[2] == 'A')
    // {
    //   //char Activado[9] = "<#ACT#0>";
    //   EEPROM.get(0, Activado[6]);
    //   // Serial.println(Activado[6]);
    //   if (Activado[6] == '0')
    //   { // Es 0
    //     // Serial.println("Era 0");
    //     Activado[6] = '1';
    //   }
    //   else // Es 1
    //   {
    //     // Serial.println("Era 1");
    //     Activado[6] = '0';
    //   }
    //   Serial.print(Activado);
    //   EEPROM.put(0, Activado[6]);
    //   EEPROM.commit();
    //   webSocket.broadcastTXT(Activado);
    // }

    // break;

}
}

void configWebSocket()
{                                    // Start a WebSocket server
  webSocket.begin();                 // start the websocket server
  webSocket.onEvent(webSocketEvent); // if there's an incomming websocket message, go to function 'webSocketEvent'
  if (logger)
    Serial.println("WebSocket server started.");
}

// -----------------------------------------------------------------------------
// WebServer
// -----------------------------------------------------------------------------
String getContentType(String filename)
{
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";                    // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  String pathWithGz = path + ".gz";
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path))
  {                                       // If the file exists, either as a compressed archive, or normal
    if (LittleFS.exists(pathWithGz))      // If there's a compressed version available
      path += ".gz";                      // Use the compressed version
    File file = LittleFS.open(path, "r"); // Open the file
    Serial.println(file);
    size_t sent = server.streamFile(file, contentType); // Send it to the client
    file.close();                                       // Close the file again
    if (logger)
      Serial.println(String("\tSent file: ") + path);
    return true;
  }
  if (logger)
    Serial.println(String("\tFile Not Found: ") + path);
  return false; // If the file doesn't exist, return false
}

// char Programacion[39] = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";

const char *LeerProgramacion(int riego)
{
  int addr = 15 * (riego - 1) + 1;
  int j = 0;
  Programacion[7] = riego + 48;
  EEPROM.get(addr, Programacion[10]);
  addr++;
  //Riego1Hora [6] = "12:00";
  Programacion[12] = riego + 48;
  for (j = 0; j < 5; j++)
  {
    EEPROM.get(addr, Programacion[15 + j]);
    addr++;
  }

  //Riego1Duracion [3] = "05";
  Programacion[21] = riego + 48;
  for (j = 0; j < 2; j++)
  {
    EEPROM.get(addr, Programacion[24 + j]);
    addr++;
  } //+9

  //Riego1Dias[8] = "0000000";
  Programacion[27] = riego + 48;
  for (j = 0; j < 7; j++)
  {
    EEPROM.get(addr, Programacion[30 + j]);
    addr++;
  } //+6

  if (logger)
  {
    Serial.printf("Programacion Riego %i:", riego);
    Serial.println(Programacion);
  }
  return Programacion;
  // for debugging in order to push any time to test
  // switch (riego)
  // {
  // case 1:
  //   return "<#PRG#R1E-1R1H-13:20R1T-01R1D-1234567>";
  //   break;
  // case 2:
  //   return "<#PRG#R2E-1R2H-13:22R2T-01R2D-1234567>";
  //   break;
  // case 3:
  //   return "<#PRG#R3E-1R3H-13:24R3T-01R3D-1234567>";
  //   break;
  // case 4:
  //   return "<#PRG#R4E-1R4H-13:26R4T-01R4D-1234567>";
  //   break;
  // }
}

// char Programacion[39] = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
void GrabarProgramacion(char payload[39])
{

  int j = 0;
  int addr = 15 * (payload[7] - 48 - 1) + 1;
  Serial.println(addr);
  if (logger)
    Serial.printf("Direccion a guardar: %i", addr);

  EEPROM.put(addr, payload[10]);
  addr++;
  //RiegoHora [6] = "12:00";

  for (j = 0; j < 5; j++)
  {
    EEPROM.put(addr, payload[15 + j]);
    addr++;
  }

  //RiegoDuracion [3] = "05";
  for (j = 0; j < 2; j++)
  {
    EEPROM.put(addr, payload[24 + j]);
    addr++;
  } //+9

  //RiegoDias[8] = "0000000";
  for (j = 0; j < 7; j++)
  {
    EEPROM.put(addr, payload[30 + j]);
    addr++;
  } //+6
 if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      Serial.println("ERROR! EEPROM commit failed");
    }
}