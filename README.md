# Watering System using ESP8266

This project implements a watering system, using ESP8266 with a Relay Module, serving a WebPage to configure and program the watering. The relay acts on a washing machine electro-valve to open and close the water flow.
Project is managed and compiled using [PlatformIO](https://platformio.org/) (By far the best way to interact with this devices).

I’m using Arduino’s built in `ESP8266WiFi.h` library to manage the WiFi connection, check project’s initial commit on main.

In Branch `ntp` you can follow the implementation of NTP time retrieval in order to manage the programming of the watering system. Here I use [PaulStoffregen/Time](https://github.com/PaulStoffregen/Time) library for time management and `WiFiUdp.h` for the NTP UDP packet management.

In Branch `mDNS`you can follow the implementation of mDNS in the ESP in order to connect via hostname. Used `ESP8266mDNS.h` integrated library in order to implement it.

In Branch `Webserver&websockets` you can see the implementation of the webserver that servers html+css+js content to the connected browser, and manages the websocket communication with the ESP in order to manually manage the watering and automatic programming of it.
Basically you have an `index.html` page that allows to manually start and stop the watering, calling a js function with a button component. From there you can move to another page called `Riego1` that allows the automatic programming of the watering via a form that calls a js function on submission in order to communicate with the ESP. There could be several more `RiegoX` pages in case several valves were to be managed (As an example with a nodeMCU rather than ESP8266 as in this example), also new buttons for the index page should be made for controlling the extra valves.
The pages are made in plain html+js (No framework) and CSS of the page was made with Tailwindcss (Just because I wanted to play with tailwind with no framework). Basically you need to build the css from the page directory (Automatically with `pnpm dev` command, as in `page/package.json` - then gzip html files, js files and css files - Then copy them to the data directory where platformio copies them to the littleFS in ESP)
The communication protocol consist on a text string sent via Websockets.
First:

```c++
char EstadoRiego[28] = "<#RIE#R1-0R2-0R3-0R4-0RT-0>";
```

Controls the On/Off status of the valve (Not yet implemented in this branch, just the communication). Basically this is prepared for 5 valves (`R#`) and it’s state `0` or `1` .
Second:

```c++
char *Programacion = "<#PRG#R1E-1R1H-12:00R1T-05R1D-1234567>";
```

Controls the programing state (`RxE-#`), the hour of activation (`RxH-##:##`), duration of the watering (`RxT-##`) and the days that it will be active (`RxD-#######`). This values are sent from the ESP to the page on load and saved/read from the EEPROM in ESP (With `EEPROM.h` library)
In this implementation only `R1`is being used as it is an ESP8266 with one relay.

In Branch `watering-management` the logic to activate/deactivate the manual and programmed watering is developed. In the main loop, according to the value of the valve number (In this example I am using only 1 but could be extended to more) in `EstadoRiego` variable, the GPIO Pin or Serial Code is sent to the ESP Relay in order to activate the electro-valve. This value is updated manually via de web interface, or programmed via it and stored as explained before in the EEPROM and on memory on `Programacion` variable. In the main loop, function `rutinaProgramacion()` is called to check when the time of running it comes, as well as the turn off time for the valve. Also there is a watchdog mechanism implemented for the manual activation with `MAXRIEGO` definition, in order to shut down the watering after those seconds in case you activate and then forgot to turn it off.

In Branch `event-notification`, push event notification was added using [GitHub - binwiederhier/ntfy: Send push notifications to your phone or desktop using PUT/POST](https://github.com/binwiederhier/ntfy) library.
