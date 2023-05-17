#include <Arduino.h>
#include "ESP8266WiFi.h"
#include <ESP8266mDNS.h>      // Include the mDNS library
#include "LittleFS.h"         // To manage the file system where to store the web pages
#include <ESP8266WebServer.h> // Include the WebServer library
#include <WebSocketsServer.h> // Include to manage Websockets for web page communication to ESP
#include <EEPROM.h>
#include "ESP8266HTTPClient.h" // To manage the PUT requests for event notifications
#include "secrets.h"
// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include "RTClib.h"

// -----------------------------------------------------------------------------
// Constant Declarations
// -----------------------------------------------------------------------------

#define SERIAL_BAUDRATE 9600
#define MAXRIEGO 3600L // Safety meassure to turn off watering in case you forgot that you turned it on. In Seconds (This is 30 minutes: 1800s)
uint8_t Riego_Pin = D0;

RTC_DS3231 rtc;

// -----------------------------------------------------------------------------
// Funtion Declarations
// -----------------------------------------------------------------------------
void wifiSetup();
void digitalClockDisplay();
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleRoot();                      // function prototypes for HTTP handlers
void handleNotFound();
void configWebSocket();
const char *LeerProgramacion(int addr);    // Read Programing sent from page
void GrabarProgramacion(char payload[39]); // Save Programming on memory
void rutinaProgramacion();                 // Watering programing routine
// void sendEvent(int state);                 // Send Events to ntfy.sh
// -----------------------------------------------------------------------------
// Variable Declarations
// -----------------------------------------------------------------------------
boolean logger = true;
// Websocket Watering Variables
char EstadoRiego[28] = "<#RIE#R1-0R2-0R3-0R4-0RT-0>";
char CodigoEnvio[6] = "#RIE#";
char *Programacion = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
DateTime HoraEncendido = DateTime(); // store the current time in time variable t
char RiegoP[5] = "1111";             // Variable to manage the day change with the programing (In this case example for 4 watering lines, only using the first one in this project)
int state = 0;
// Case used only when ESP (eg NodeMCU) uses several watering lines and what a general disabled state for all
// char Activado[9] = "<#ACT#0>";

// -----------------------------------------------------------------------------
// WebServer declarations
// -----------------------------------------------------------------------------
ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
WebSocketsServer webSocket(81); // create a websocket server on port 81

// -----------------------------------------------------------------------------
// Pinout configuration for relays
// -----------------------------------------------------------------------------
// For ESP+Relay modules that manage the relay via the serial port (Instead of GPIO), in my case this happened for a lighting system (Not this project)
// byte relON[] = {0xA0, 0x01, 0x01, 0xA2};  //Hex command to send to onboard serial microprocessor for open relay
// byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1}; //Hex command to send to serial for close relay
// For ESP+Relay modules that manage the relay via GPIO port, see below in setup

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(SERIAL_BAUDRATE);

  // Wifi
  wifiSetup();
  // RTC
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1)
      delay(10);
  }
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  //  configNTP();
  HoraEncendido = rtc.now();
  digitalClockDisplay();
  // mDNS
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
  LittleFS.begin();                                     // Start the SPI Flash Files System
                                                        // Websockets
  configWebSocket();                                    // Start a WebSocket server
                                                        // Web Server
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });
  // EEPROM Initialization
  EEPROM.begin(61);
  // GrabarProgramacion("<#PRG#R1E-1R1H-18:00R1T-05R1D-1234567>");
  server.begin(); // Actually start the server

  // GPIO configuration for ESP+relays modules that manage activation via GPIO (ej watering system at my department)
  pinMode(Riego_Pin, OUTPUT);   // 8266 Pin to the relay
  digitalWrite(Riego_Pin, LOW); // Turn of 8266 relay
  int j = 1;                    // Number of watering lines for the loop, in this project only one
  int i;
  for (i = 1; i <= j; i++)
  {
    // PRINCIPIO RUTINA PROGRAMACION RIEGO
    LeerProgramacion(i); // LeerProgramacion stores the particular watering line values on to Programacion
    if (logger)
      Serial.printf("Programacion Leida: %s", Programacion);
    // char *Programacion = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
  }
}

void loop()
{
  // put your main code here, to run repeatedly:
  // mDNS
  MDNS.update();
  webSocket.loop();
  server.handleClient(); // Listen for HTTP requests from clients
  // Main program
  //  For cases with several watering valves
  //  if (EstadoRiego[9] == '0' && EstadoRiego[13] == '0' && EstadoRiego[17] == '0' && EstadoRiego[21] == '0' && EstadoRiego[25] == '0')
  if (EstadoRiego[9] == '0')
  {
    if (state == 1)
    {
      // sendEvent(0);
      state = 0;
    }

    digitalWrite(Riego_Pin, LOW); // Turn of 8266 relay
  }
  // Turn on first valve (Should repeat for other watering valves, adding to turn off the rest in the if clause)
  if (EstadoRiego[9] == '1')
  {
    if (state == 0)
    {
      // sendEvent(1);
      state = 1;
    }
    digitalWrite(Riego_Pin, HIGH); // Turn on 8266 relay
  }
  // Verify timer safety check with MAXRIEGO variable (To turn off watering in case you forgot after MAXRIEGO seconds)
  // for cases with several valves:
  // if (EstadoRiego[9] == '1' || EstadoRiego[13] == '1' || EstadoRiego[17] == '1' || EstadoRiego[21] == '1' || EstadoRiego[25] == '1')
  if (EstadoRiego[9] == '1')
  {

    if (rtc.now() > HoraEncendido + MAXRIEGO)
    {
      strcpy(EstadoRiego, "<#RIE#R1-0R2-0R3-0R4-0RT-0>");
      if (logger)
        Serial.println(EstadoRiego);
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

void wifiSetup()
{

  // Set WIFI module to STA mode
  // WiFi.mode(WIFI_STA);

  // Connect as Client
  // Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  // WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Set WIFI module to SOFT_AP mode
  WiFi.mode(WIFI_AP);

  // Create SOFT_AP
  WiFi.softAP(LOCAL_WIFI_SSID, LOCAL_WIFI_PASS);

  // Wait
  // while (WiFi.status() != WL_CONNECTED) {
  //     Serial.print(".");
  //     delay(100);
  // }
  // Serial.println();

  // Connected!
  Serial.printf("[WIFI] SOFT AP Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.softAPIP().toString().c_str());
}

void printDigits(int digits)
{
  // utility for digital clock display: prints  leading 0
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay()
{
  DateTime now = rtc.now();
  // digital clock display of the time
  Serial.print(now.hour());
  printDigits(now.minute());
  printDigits(now.second());
  Serial.print(" ");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.month());
  Serial.print(" ");
  Serial.print(now.year());
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
      // if ((EstadoRiego[9] == '0' && EstadoRiego[13] == '0' && EstadoRiego[17] == '0' && EstadoRiego[21] == '0') && (payload[9] == '1' || payload[13] == '1' || payload[17] == '1' || payload[21] == '1'))
      if (EstadoRiego[9] == '0' && payload[9] == '1')
      {
        HoraEncendido = rtc.now(); // Timer for the watering start, in order to check in main loop for the safety meassure
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
    else if (payload[2] == 'T')
    {
      // Ajust time from connected browser
      // <#TIM#20230516140241#>
      if (logger)
        Serial.printf("Hora a configurar: %s\n", payload);
      u_int16_t year_num = (payload[6] - '0') * 1000 + (payload[7] - '0') * 100 + (payload[8] - '0') * 10 + (payload[9] - '0');
      u_int8_t month_num = (payload[10] - '0') * 10 + (payload[11] - '0');
      u_int8_t day_num = (payload[12] - '0') * 10 + (payload[13] - '0');
      u_int8_t hour_num = (payload[14] - '0') * 10 + (payload[15] - '0');
      u_int8_t minute_num = (payload[16] - '0') * 10 + (payload[17] - '0');
      u_int8_t second_num = (payload[18] - '0') * 10 + (payload[19] - '0');
      if (logger)
        Serial.printf("%d-%d-%d %d:%d:%d\n", year_num, month_num, day_num, hour_num, minute_num, second_num);
      rtc.adjust(DateTime(year_num, month_num, day_num, hour_num, minute_num, second_num));

      digitalClockDisplay();
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
  // Riego1Hora [6] = "12:00";
  Programacion[12] = riego + 48;
  for (j = 0; j < 5; j++)
  {
    EEPROM.get(addr, Programacion[15 + j]);
    addr++;
  }

  // Riego1Duracion [3] = "05";
  Programacion[21] = riego + 48;
  for (j = 0; j < 2; j++)
  {
    EEPROM.get(addr, Programacion[24 + j]);
    addr++;
  } //+9

  // Riego1Dias[8] = "0000000";
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

  if (logger)
    Serial.printf("Direccion a guardar: %i", addr);

  EEPROM.put(addr, payload[10]);
  addr++;
  // RiegoHora [6] = "12:00";

  for (j = 0; j < 5; j++)
  {
    EEPROM.put(addr, payload[15 + j]);
    addr++;
  }

  // RiegoDuracion [3] = "05";
  for (j = 0; j < 2; j++)
  {
    EEPROM.put(addr, payload[24 + j]);
    addr++;
  } //+9

  // RiegoDias[8] = "0000000";
  for (j = 0; j < 7; j++)
  {
    EEPROM.put(addr, payload[30 + j]);
    addr++;
  } //+6
  if (EEPROM.commit())
  {
    Serial.println("EEPROM successfully committed");
    LeerProgramacion(payload[7] - 48);
  }
  else
  {
    Serial.println("ERROR! EEPROM commit failed");
  }
  if (logger)
  {
    Serial.printf("Programacion Riego: %s", Programacion);
  }
}

void rutinaProgramacion()
{
  DateTime datetime = rtc.now();
  // USed only in case of a global enabled state, not used in this project
  // EEPROM.get(0, Activado[6]);
  //  Serial.println(Activado[6]);
  // if (Activado[6] == '1')
  //{
  int j = 1; // Number of watering lines for the loop, in this project only one
  int i;
  for (i = 1; i <= j; i++)
  {
    // PRINCIPIO RUTINA PROGRAMACION RIEGO
    // LeerProgramacion(i); // LeerProgramacion stores the particular watering line values on to Programacion
    // if (logger)
    //   Serial.printf("Programacion Leida: %s", Programacion);
    // char *Programacion = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
    if (Programacion[10] == '1') // Watering Line enabled
    {
      if ((Programacion[datetime.dayOfTheWeek() + 30] - 48) == datetime.dayOfTheWeek() + 1) // if we are in an active day of the week for the program
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

        DateTime horariego = DateTime(datetime.year(), datetime.month(), datetime.day(), (byte)atoi(HoraRiego), (byte)atoi(MinutoRiego), 0);

        if (datetime < horariego && RiegoP[i - 1] == '1')
        {
          RiegoP[i - 1] = '0'; // Changed day, turn the programed watering to 0
        }
        // Check if we reached a time to water for the particular line, and if it is enabled
        if (datetime > horariego && (datetime < horariego + TimeSpan(atoi(RiegoDuracion) * 60)) && EstadoRiego[(i - 1) * 4 + 9] == '0' && RiegoP[i - 1] == '0')
        {
          if (logger)
            Serial.printf("Enciendo Riego%i Hora de Regar", 1);
          // sendEvent(2);
          // ImprimirHora(t);
          strcpy(EstadoRiego, "<#RIE#R1-0R2-0R3-0R4-0RT-0>");
          EstadoRiego[(i - 1) * 4 + 9] = '1';
          Serial.printf("%s\n", EstadoRiego);
          webSocket.broadcastTXT(EstadoRiego);
          HoraEncendido = datetime;
        }
        // Check if watering time has ended
        if (datetime > horariego + TimeSpan(atoi(RiegoDuracion) * 60) && RiegoP[i - 1] == '0')
        {
          if (logger)
            Serial.printf("Apago Riego%i Hora de Regar", 1);
          // sendEvent(3);
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

// Send watering event to ntfy.sh... state can be 0 (OFF), 1 (ON) and the 2 for programing start and 3 for programming end coudl extend to notify several watering systems with and extra input variable
// void sendEvent(int state)
// {
//   if ((WiFi.status() == WL_CONNECTED))
//   {

//     WiFiClient client;
//     HTTPClient http;

//     Serial.print("[HTTP] begin...\n");
//     // configure traged server and url
//     http.begin(client, "http://ntfy.sh/" HOST); // HTTP
//     http.addHeader("Content-Type", "application/json");
//     http.addHeader("X-Title", "Evento de Riego");
//     int httpCode;
//     Serial.print("[HTTP] POST...\n");
//     // start connection and send HTTP header and body
//     switch (state)
//     {
//     case 0:
//       httpCode = http.POST("El riego se ha apagado");
//       break;
//     case 1:
//       httpCode = http.POST("El riego se ha encendido");
//       break;
//     case 2:
//       httpCode = http.POST("Iniciando programacion");
//       break;
//     case 3:
//       httpCode = http.POST("Finalizando programacion");
//       break;
//     }

//     // httpCode will be negative on error
//     if (httpCode > 0)
//     {
//       // HTTP header has been send and Server response header has been handled
//       Serial.printf("[HTTP] POST... code: %d\n", httpCode);

//       // file found at server
//       if (httpCode == HTTP_CODE_OK)
//       {
//         const String &payload = http.getString();
//         Serial.println("received payload:\n<<");
//         Serial.println(payload);
//         Serial.println(">>");
//       }
//     }
//     else
//     {
//       Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
//     }

//     http.end();
//   }
// }