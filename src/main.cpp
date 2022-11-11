#include <Arduino.h>
#include "ESP8266WiFi.h"
#include <WiFiUdp.h>          // Para el NTP
#include <TimeLib.h>
#include <ESP8266mDNS.h>      // Include the mDNS library
#include "LittleFS.h"         // To manage the file system where to store the web pages

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

// -----------------------------------------------------------------------------
// Variable Declarations
// -----------------------------------------------------------------------------
boolean logger = true;

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



void setup() {
  // put your setup code here, to run once:
  Serial.begin(SERIAL_BAUDRATE);

    // Wifi
    wifiSetup();
    //NTP
    configNTP();
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

    LittleFS.begin(); // Start the SPI Flash Files System
}

void loop() {
  // put your main code here, to run repeatedly:
  //mDNS
  MDNS.update();
  //Main program
  digitalClockDisplay();
  delay(100);
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