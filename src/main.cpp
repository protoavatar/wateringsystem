#include <Arduino.h>
#include "ESP8266WiFi.h"
#include <WiFiUdp.h>          // Para el NTP
#include <TimeLib.h>
#include <ESP8266mDNS.h>      // Include the mDNS library
#include "LittleFS.h"        // To manage the file system where to store the web pages
#include <ESP8266WebServer.h> // Include the WebServer library
#include <WebSocketsServer.h> // Include to manage Websockets for web page communication to ESP
#include <EEPROM.h>
#include "ESP8266HTTPClient.h" // To manage the PUT requests for event notifications

// -----------------------------------------------------------------------------
// Constant Declarations
// -----------------------------------------------------------------------------
#define WIFI_SSID "Jacobiano"  //Cambiar por tu WIFI SSID
#define WIFI_PASS "galgogalgo"  //Cambiar por tu WIFI password
#define HOST "RIEGOTEST"  //Cambiar por tu WIFI password
#define SERIAL_BAUDRATE 9600
#define MAXRIEGO 1800 // Safety meassure to turn off watering in case you forgot that you turned it on. In Seconds (This is 30 minutes: 1800s)

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
void rutinaProgramacion(); // Watering programing routine
time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss); // helper function to change from ASCII to time
void sendEvent(int state); // Send Events to ntfy.sh
// -----------------------------------------------------------------------------
// Variable Declarations
// -----------------------------------------------------------------------------
boolean logger = true;
// Websocket Watering Variables
char EstadoRiego[28] = "<#RIE#R1-0R2-0R3-0R4-0RT-0>";
char CodigoEnvio[6] = "#RIE#";
char *Programacion = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
time_t HoraEncendido = now(); // store the current time in time variable t
char RiegoP[5] = "1111"; // Variable to manage the day change with the programing (In this case example for 4 watering lines, only using the first one in this project)
int state = 0;
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

// -----------------------------------------------------------------------------
// Pinout configuration for relays
// -----------------------------------------------------------------------------
// For ESP+Relay modules that manage the relay via the serial port (Instead of GPIO), in my case this happened for a lighting system (Not this project)
//byte relON[] = {0xA0, 0x01, 0x01, 0xA2};  //Hex command to send to onboard serial microprocessor for open relay
//byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1}; //Hex command to send to serial for close relay
// For ESP+Relay modules that manage the relay via GPIO port, see below in setup


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

  // GPIO configuration for ESP+relays modules that manage activation via GPIO (ej watering system at my department)
    pinMode(0, OUTPUT); // 8266 Pin to the relay
    digitalWrite(8, HIGH);    // Turn of 8266 relay
}

void loop() {
  // put your main code here, to run repeatedly:
  //mDNS
  MDNS.update();
  webSocket.loop();
  server.handleClient(); // Listen for HTTP requests from clients
  //Main program
  // For cases with several watering valves
  // if (EstadoRiego[9] == '0' && EstadoRiego[13] == '0' && EstadoRiego[17] == '0' && EstadoRiego[21] == '0' && EstadoRiego[25] == '0')
  if (EstadoRiego[9] == '0')
  {
    if (state == 1) {
			sendEvent(0);
			state = 0;
			}

		digitalWrite(8, HIGH);    // Turn of 8266 relay
  }
  // Turn on first valve (Should repeat for other watering valves, adding to turn off the rest in the if clause)
  if (EstadoRiego[9] == '1')
  {
		    if (state == 0) {
			sendEvent(1);
			state = 1;
			}
    digitalWrite(8, LOW);    // Turn on 8266 relay
  }
  // Verify timer safety check with MAXRIEGO variable (To turn off watering in case you forgot after MAXRIEGO seconds)
  // for cases with several valves:
  //if (EstadoRiego[9] == '1' || EstadoRiego[13] == '1' || EstadoRiego[17] == '1' || EstadoRiego[21] == '1' || EstadoRiego[25] == '1')
  if (EstadoRiego[9] == '1')
  {

    if (now() > HoraEncendido + MAXRIEGO)
    {
      strcpy(EstadoRiego, "<#RIE#R1-0R2-0R3-0R4-0RT-0>");
      if (logger) Serial.println(EstadoRiego);
      webSocket.broadcastTXT(EstadoRiego);
      if (logger)
        Serial.println(F("APAGO RIEGOS - TIEMPO MAXIMO CUMPLIDO"));
    }
  }
  rutinaProgramacion();
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
              Serial.printf("%s\n", payload);
        // In case there are several watering valves
        //if ((EstadoRiego[9] == '0' && EstadoRiego[13] == '0' && EstadoRiego[17] == '0' && EstadoRiego[21] == '0') && (payload[9] == '1' || payload[13] == '1' || payload[17] == '1' || payload[21] == '1'))
        if (EstadoRiego[9] == '0' && payload[9] == '1')
        {
          HoraEncendido = now(); // Timer for the watering start, in order to check in main loop for the safety meassure
          if (logger)
            Serial.printf("Empieza el timer");
        }
        strcpy(EstadoRiego, (char *)payload);
        webSocket.broadcastTXT(payload);

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

  // if (logger)
  // {
  //   Serial.printf("Programacion Riego %i:", riego);
  //   Serial.println(Programacion);
  // }
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

void rutinaProgramacion()
{
    // USed only in case of a global enabled state, not used in this project
  //EEPROM.get(0, Activado[6]);
  // Serial.println(Activado[6]);
  //if (Activado[6] == '1')
  //{
    int j=1;// Number of watering lines for the loop, in this project only one
    int i;
    for (i = 1; i <= j; i++)
    {
      // PRINCIPIO RUTINA PROGRAMACION RIEGO
      LeerProgramacion(i); // LeerProgramacion stores the particular watering line values on to Programacion
      // if (logger)
      //   Serial.printf("Programacion Leida: %s", Programacion);
      //char *Programacion = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
      if (Programacion[10] == '1') // Watering Line enabled
      {
        if ((Programacion[weekday(now()) - 1 + 30] - 48) == weekday(now())) // if we are in an active day of the week for the program
        {
          char HoraRiego[3] = "00";
          char MinutoRiego[3] = "00";
          char RiegoDuracion[3] = "00";

          uint8_t j = 0;
          for (j = 0; j < 2; j++)
          {
            HoraRiego[j] = Programacion[j + 15];
            MinutoRiego[j] = Programacion[j + 18];
            RiegoDuracion[j] = Programacion[j + 24];
          }
          HoraRiego[2] = '\0';
          MinutoRiego[2] = '\0';
          RiegoDuracion[2] = '\0';

          if (now() < tmConvert_t(year(now()), month(now()), day(now()), (byte)atoi(HoraRiego), (byte)atoi(MinutoRiego), 0) && RiegoP[i - 1] == '1')
          {
            RiegoP[i - 1] = '0'; //Changed day, turn the programed watering to 0
          }
          // Check if we reached a time to water for the particular line, and if it is enabled
          if ((now() > tmConvert_t(year(now()), month(now()), day(now()), (byte)atoi(HoraRiego), (byte)atoi(MinutoRiego), 0)) && (now() < (atoi(RiegoDuracion) * 60) + tmConvert_t(year(now()), month(now()), day(now()), (byte)atoi(HoraRiego), (byte)atoi(MinutoRiego), 0)) && EstadoRiego[(i - 1) * 4 + 9] == '0' && RiegoP[i - 1] == '0')
          {
            if (logger)
              Serial.printf("Enciendo Riego%i Hora de Regar", 1);
						sendEvent(2);
            // ImprimirHora(t);
            strcpy(EstadoRiego, "<#RIE#R1-0R2-0R3-0R4-0RT-0>");
            EstadoRiego[(i - 1) * 4 + 9] = '1';
            Serial.printf("%s\n", EstadoRiego);
            webSocket.broadcastTXT(EstadoRiego);
            HoraEncendido = now();
          }
          // Check if watering time has ended
          if (now() > (atoi(RiegoDuracion) * 60) + tmConvert_t(year(now()), month(now()), day(now()), (byte)atoi(HoraRiego), (byte)atoi(MinutoRiego), 0) && RiegoP[i - 1] == '0')
          {
            if (logger)
              Serial.printf("Apago Riego%i Hora de Regar", 1);
						sendEvent(3);
            // ImprimirHora(t);
            strcpy(EstadoRiego, "<#RIE#R1-0R2-0R3-0R4-0RT-0>");
            Serial.printf("%s\n", EstadoRiego);
            webSocket.broadcastTXT(EstadoRiego);
            RiegoP[i - 1] = '1'; // Ya paso el Riego del Dia
          }
        }
      }
      // FIN RUTINA PROGRAMACION RIEGO
    }
  //}
}

time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss)
{
  tmElements_t tmSet;
  tmSet.Year = YYYY - 1970;
  tmSet.Month = MM;
  tmSet.Day = DD;
  tmSet.Hour = hh;
  tmSet.Minute = mm;
  tmSet.Second = ss;
  return makeTime(tmSet); //convert to time_t
}

// Send watering event to ntfy.sh... state can be 0 (OFF), 1 (ON) and the 2 for programing start and 3 for programming end coudl extend to notify several watering systems with and extra input variable
void sendEvent(int state) {
  if ((WiFi.status() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    // configure traged server and url
    http.begin(client, "http://ntfy.sh/" HOST); //HTTP
    http.addHeader("Content-Type", "application/json");
		http.addHeader("X-Title", "Evento de Riego");
		int httpCode;
    Serial.print("[HTTP] POST...\n");
    // start connection and send HTTP header and body
		switch(state)
	{
		case 0:
		httpCode = http.POST("El riego se ha apagado");
			break;
		case 1:
		httpCode = http.POST("El riego se ha encendido");
			break;
		case 2:
				httpCode = http.POST("Iniciando programacion");
			break;
		case 3:
		httpCode = http.POST("Finalizando programacion");
			break;
	}

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        const String& payload = http.getString();
        Serial.println("received payload:\n<<");
        Serial.println(payload);
        Serial.println(">>");
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}