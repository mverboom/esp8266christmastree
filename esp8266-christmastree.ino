#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h> 
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>

#define PIN D1
#define NUM_PIXELS  10
#define COLOR_RANDOM -2
#define COLOR_GET -1
#define COLOR_STATE -3
#define UPDATE_GET -1
#define UPDATE_RANDOM -2
#define UPDATE_STATIC -3
#define UPDATE_STATE -4
#define UPDATE_LEN -5
#define EFFECT_STATE -1
//#define vel 100 // Velocity in milliseconds

const int effects = 5;
const char * const effectlist[] = { "Blink", "Loop", "Fill", "Double loop", "Fade"};

// LED layout
//    9
// 6  7  8
// 5  4  3
// 0  1  2

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, PIN, NEO_RGB + NEO_KHZ400);
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>Christmas Tree</title>
<style>
div.circle { width: 50px; height: 50px; background: black; -moz-border-radius: 50px; -webkit-border-radius: 50px; border-radius: 50px; }
body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }
</style>
<script>
var websock;
function start() {
  ws = new WebSocket('ws://' + window.location.hostname + ':81/');
  ws.onopen = function(event) { console.log('websocket opened'); };
  ws.onclose = function(event) { console.log('websocket closed'); };
  ws.onerror = function(event) { console.log(event); };
  ws.onmessage = function(event) {
    var i, color, col, x;
    //console.log(event.data);
    switch (event.data.substr(0,1)) {
       case 'e':
          console.log(event.data);
          var x = document.getElementById("effect");
          var option = document.createElement("option");
          option.text = event.data.substr(1);
          x.add(option);
       break;
       case 'd':
          document.getElementById("effect").selectedIndex = event.data.substr(1);
       break;
       case 'l':
          for (i = 0; i*7 < (event.data.length - 1); i++) {
             color = event.data.substr(1+i*7,7);
             x = document.getElementById(i);
             x.style.backgroundColor = color;
          }
       break;
       case 'c':
          document.getElementById("color").value = event.data.substr(1,7);
          document.getElementById("colorrandom").checked = false;
          document.getElementById("colorcheck").checked = true;
       break;
       case 'r':
          document.getElementById("colorrandom").checked = true;
          document.getElementById("colorcheck").checked = false;
       break;
       case 'u':
           console.log(event.data);
          document.getElementById("intstatic").checked = true;
          document.getElementById("intrandom").checked = false;
          document.getElementById("intstatval").value = event.data.substr(1);
       break;
       case 'v':
          console.log(event.data);
          document.getElementById("intstatic").checked = false;
          document.getElementById("intrandom").checked = true;
          document.getElementById("intrandval").value = event.data.substr(1);
       break;
    }
  };
}
function setcolor(event) {
   console.log(event);
   var msg="s" + document.getElementById("color").value;
   ws.send(msg);
}
function setcolorrandom(event) {
  if (document.getElementById("colorrandom").checked == true)
     ws.send("r");
  else {
     var msg="s" + document.getElementById("color").value;
     ws.send(msg);
  }
}
function setintstatic(event) {
   var msg="u" + document.getElementById("intstatval").value;
   ws.send(msg);
}
function setintrandom(event) {
   var msg="v" + document.getElementById("intrandval").value;
   ws.send(msg);
}
function seteffect(event) {
   var msg="d" + document.getElementById("effect").selectedIndex;
   ws.send(msg);
}
window.onload = start;
</script>
</head>
<body>
<h1>Christmas Tree</h1>
<div style="position: relative; width:500px; height:370px;">
<div style="position: absolute; top:  10px; left: 200px;" class=circle id=9></div>

<div style="position: absolute; top: 100px; left: 135px;" class=circle id=6></div>
<div style="position: absolute; top: 165px; left: 200px;" class=circle id=7></div>
<div style="position: absolute; top:  90px; left: 255px;" class=circle id=8></div>

<div style="position: absolute; top: 200px; left:  95px;" class=circle id=5></div>
<div style="position: absolute; top: 250px; left: 200px;" class=circle id=4></div>
<div style="position: absolute; top: 200px; left: 300px;" class=circle id=3></div>

<div style="position: absolute; top: 290px; left:  50px;" class=circle id=0></div>
<div style="position: absolute; top: 330px; left: 200px;" class=circle id=1></div>
<div style="position: absolute; top: 295px; left: 340px;" class=circle id=2></div>
</div>
<b>Effect</b><br>
<select id="effect" onChange=seteffect(event);>
</select><br>
<b>Color</b><br>
<input type="checkbox" id=colorcheck  onClick=setcolor(event);><input type="color" id="color" value="#ff0000" onChange=setcolor(event);><br>
<input type="checkbox" id=colorrandom onClick=setcolorrandom(event);> Random<br>
<b>Interval</b><br>
<input type="checkbox" id=intstatic   onClick=setintstatic(event);>Static <input type="number" id="intstatval" min=1 max=9999 value=1000 onChange=setintstatic(event);>ms<br>
<input type="checkbox" id=intrandom   onClick=setintrandom(event);>Random, max <input type="number" id="intrandval" min=1 max=9999 value=1000 onChange=setintrandom(event);>ms<br>
</td></tr></table>
</body>
</html>
)rawliteral";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
   long int curcolor;
   int interval;
   char msg[10];
   int count;
   int state;

   Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
   switch(type) {
      case WStype_DISCONNECTED:
         Serial.printf("[%u] Disconnected!\r\n", num);
         break;
      case WStype_CONNECTED: {
         IPAddress ip = webSocket.remoteIP(num);
         Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
         for (count = 0; count < effects; count++) {
            sprintf(msg, "e%s", effectlist[count]);
            webSocket.sendTXT(num, msg);
         }
         state = effect(EFFECT_STATE);
         sprintf(msg, "d%d", state);
         webSocket.sendTXT(num,msg);
         curcolor = color(COLOR_STATE);
         switch (curcolor) {
            case COLOR_RANDOM:
               webSocket.sendTXT(num, "r");
            break;
            default:
               sprintf(msg, "c#%02x%02x%02x", (uint8_t) ((curcolor >> 16) & 0xff), (uint8_t) ((curcolor >> 8) & 0xff),
(uint8_t) ((curcolor) & 0xff));
               webSocket.sendTXT(num, msg);
            break;
         }
         interval = update(UPDATE_STATE,0);
         switch (interval) {
            case UPDATE_RANDOM:
               sprintf(msg, "v%d", update(UPDATE_LEN,0));
               webSocket.sendTXT(num, msg);
            break;
            default:
               sprintf(msg, "u%d", update(UPDATE_LEN,0));
               webSocket.sendTXT(num, msg);
            break;
         }
      }
      break;
      case WStype_TEXT:
         Serial.printf("[%u] get Text: %s\r\n", num, payload);
         switch (payload[0]) {
            case 'd':
               effect(strtol((char *) &payload[1], NULL, 10));
               setstrip(0);
               webSocket.broadcastTXT(payload);
            break;
            case 's':
               color(strtol((char *) &payload[2], NULL, 16));
               payload[0] = 'c';
               webSocket.broadcastTXT(payload, 8);
            break;
            case 'r':
               color(COLOR_RANDOM);
               webSocket.broadcastTXT(payload, length);
            break;
            case 'u':
               update(UPDATE_STATIC, strtol((char *) &payload[1], NULL, 10));
               webSocket.broadcastTXT(payload);
            break;
            case 'v':
               update(UPDATE_RANDOM, strtol((char *) &payload[1], NULL, 10));
               webSocket.broadcastTXT(payload);
            break;
         }     
      break;
      case WStype_BIN:
         Serial.printf("[%u] get binary length: %u\r\n", num, length);
         hexdump(payload, length);
      break;
      default:
         Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
   }
}

void web_root() {
   server.send(200, "text/html", INDEX_HTML);
}

void web_notfound() {
   server.send(404, "text/plain", "Not found");
}

void setup() {
  int i;

  Serial.begin(115200);
  Serial.println("Starting christmastree");
  strip.begin();
  setstrip(0);
  strip.setPixelColor(9,255,0,0);
  strip.show();

  WiFiManager wifiManager;
  wifiManager.autoConnect("ChristmasTree");

  Serial.println("Wifi setup done.");
  strip.setPixelColor(9,0,255,0);
  strip.show();
  if ( MDNS.begin ( "tree" ) ) {
     Serial.println ( "MDNS responder started" );
    MDNS.addService("http", "tcp", 80);
  } else
     Serial.println("MDNS responder failed");
  server.on("/", web_root);
  server.onNotFound(web_notfound);
  server.begin();
  Serial.println("HTTP server started");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Websocket server started");
  
  setstrip(0);
}

void setstrip(long int color) {
   int i;
   for (i=0; i<NUM_PIXELS; i++)
      strip.setPixelColor(i, color);
   stripshow();
}


int update(int updatetype, int setlength) {
   static int update = UPDATE_STATIC;
   static int length = 1000;
   int temp;

   switch (updatetype) {
      case UPDATE_STATE:
         return(update);
      break;
      case UPDATE_LEN:
         return(length);
      break;
      case UPDATE_GET:
         if (update == UPDATE_RANDOM) {
            return(random(length));
         } else
            return(length);
      break;
      case UPDATE_RANDOM:
         update = UPDATE_RANDOM;
         length = setlength;
         return(random(length));
      break;
      case UPDATE_STATIC:
         update = UPDATE_STATIC;
         length = setlength;
         return(length);
      break;
   }
}

long int color(long int setcolor) {
   static long int color = COLOR_RANDOM;
   long int temp;

   switch (setcolor) {
      case COLOR_STATE:
         return(color);
      break;
      case COLOR_GET:
         if (color == COLOR_RANDOM) {
            temp = random(0xffffff);
            return(temp);
         }
         else {
             return(color);
         }
      break;
      case COLOR_RANDOM:
         color = COLOR_RANDOM;
         return(random(0xffffff));
      break;
      default:
         color = setcolor;
         return(color);
   }
}

int effect(int seteffect) {
   static int effect = 4;

   switch (seteffect) {
      case EFFECT_STATE:
         return(effect);
      break;
      default:
         effect = seteffect;
         return(effect);
   }
}

void stripshow() {
   uint8_t status[NUM_PIXELS * 3];
   char colorstring[NUM_PIXELS * 10];
   int i;
   long color;
   strip.show();
   sprintf(colorstring, "l\0");
   for (i = 0; i < NUM_PIXELS; i++) {
      color = strip.getPixelColor(i);
      sprintf(colorstring, "%s#%02x%02x%02x", colorstring, (uint8_t) ((color >> 16) & 0xff), (uint8_t) ((color >> 8) & 0xff),
(uint8_t) ((color) & 0xff));
   }
   //sprintf(colorstring, "%s\0", colorstring);
   //Serial.println(colorstring);
   webSocket.broadcastTXT(colorstring, strlen(colorstring));
   //webSocket.broadcastBIN(status,3);
}

void led_blink(void) {
   static unsigned long last_time;
   static int col = 0;
   unsigned long time = millis();
   static int interval = update(UPDATE_GET,0);

   if (time > last_time + interval) {
      if (col == 0)
         col = color(COLOR_GET);
      else
         col = 0;
      setstrip(col);
      last_time = time;
      interval = update(UPDATE_GET,0);
   }
}

void led_loop(int pat, const int leds[]) {
   static unsigned long last_time;
   static int pos = 0;
   unsigned long time = millis();
   static int interval = update(UPDATE_GET,0);

   if (time > last_time + interval) {
      strip.setPixelColor(leds[pos], 0, 0, 0);
      pos++;
      if (pos >= pat)
         pos = 0;
      strip.setPixelColor(leds[pos], color(COLOR_GET));
      stripshow();
      last_time = time;
      interval = update(UPDATE_GET,0);
   }
}

long int rgb(long int color, int perc) {
   unsigned int r;
   unsigned int g;
   unsigned int b;
   unsigned long int col;

   r = ((color  >> 16) & 0xff) * perc / 100;
   g = ((color >> 8) & 0xff)  * perc / 100;
   b = ((color     ) &0xff)  * perc /100;
   col =  ((long int) r) << 16 | ((long int) g) << 8 | ((long int) b);
   return(col);
}

// Divide timesteps by led steps to get same interval
// Optional only fade in or out?
// Pattern fades?
int led_fade(int led, int step) {
   static int led_fade[NUM_PIXELS];
   static int led_steps[NUM_PIXELS];
   static int led_step[NUM_PIXELS];
   static long int led_color[NUM_PIXELS];
   
   static unsigned long last_time[NUM_PIXELS];
   unsigned long time = millis();
   static int interval[NUM_PIXELS];

   int count;
   int active = 0;
   int updated = 0;

   if (led != -1 && led_fade[led] == 0) {
      led_steps[led] = step;
      led_step[led] = 0;
      led_color[led] = color(COLOR_GET);
      led_fade[led] = 1;
      interval[led] = update(UPDATE_GET,0) / step;
      last_time[led] = 0;
      //Serial.printf("Added led %d, steps %d\n", led, step);
   }

   for (count = 0; count < NUM_PIXELS; count++) {
      if (led_fade[count] != 0) {
         active++;
         if (time > last_time[count] + interval[count]) {
            updated=1;
            //Serial.printf("Updated led %d\n", count);
            if (led_fade[count] == 1) {
               led_step[count]++;
               if (led_step[count] == led_steps[count])
                  led_fade[count] = -1; 
            } else {
               led_step[count]--;
               if (led_step[count] < 0) {
                  led_step[count] = 0;
                  led_fade[count] = 0;
               }
            }
            strip.setPixelColor(count, rgb(led_color[count], (led_step[count] *100)/ led_steps[count]));
           last_time[count] = time;
            interval[count] = update(UPDATE_GET,0) / led_steps[count];
         }
      }
   }
   if (updated > 0) {
      stripshow();
      //Serial.printf("update, active: %d\n", active);
      return(active);
   }
   if (active == 0)
      return(0);
   else
      return(-1);
}

const int pat_fill[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
const int pat_loop[] = {9, 8, 3, 2, 1, 0, 5, 6};
const int pat_dblloop[] = {1, 4, 7, 9, 6, 5, 0, 1, 4, 7, 9, 8, 3, 2};

void loop() {
   int ret;

   while (1) {
      yield();
      server.handleClient();
      webSocket.loop();
      switch (effect(EFFECT_STATE)) {
         case 0: led_blink();
         break;
         case 1: led_loop(8, pat_loop);
         break;
         case 2: led_loop(10, pat_fill);
         break;
         case 3: led_loop(14, pat_dblloop);
         break;
         case 4: ret = led_fade(-1,0);
                 if (ret != -1 && ret < 6 && random(10) == 5) {
                    led_fade(random(NUM_PIXELS),random(4,10));
                 }
         break;
      }
   }
}
