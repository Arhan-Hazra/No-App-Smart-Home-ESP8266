#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

ESP8266WebServer server(80);
DNSServer dnsServer;

// --- PIN CONFIGURATION ---

// RELAYS (Make sure your physical wiring matches this exact order)
// Index 0 (Room Fan)   -> D1 (5)
// Index 1 (Tube Light) -> D2 (4)
// Index 2 (Room Bulb)  -> D5 (14)
// Index 3 (Dining Fan) -> D0 (16)
const int relayPins[] = {5, 4, 14, 16};   

// SWITCHES (Exactly as requested)
// Index 0 (Room Fan)   -> D7 (13)
// Index 1 (Tube Light) -> TX (1)
// Index 2 (Room Bulb)  -> RX (3)
// Index 3 (Dining Fan) -> D4 (2)
const int switchPins[] = {13, 1, 3, 2};  

bool states[] = {false, false, false, false};
int lastSwitch[] = {HIGH, HIGH, HIGH, HIGH};
const char* names[] = {"Room Fan", "Tube Light", "Room Bulb", "Dining Fan"};

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>";
  html += "body{text-align:center;font-family:sans-serif;background:#121212;color:white;user-select:none;}";
  html += ".btn{display:block;width:85%;padding:25px;margin:15px auto;border-radius:15px;border:none;font-size:24px;color:white;font-weight:bold;transition:background 0.1s;}";
  html += ".on{background:#2ecc71;box-shadow:0 4px #1b914d;} .off{background:#333;box-shadow:0 4px #111;}";
  html += "</style></head><body><h1>Smart Control</h1><div id='btns'>";
  
  for(int i=0; i<4; i++) {
    String cls = states[i] ? "on" : "off";
    String lbl = states[i] ? " (ON)" : " (OFF)";
    html += "<button onclick='t(" + String(i) + ")' id='b" + String(i) + "' class='btn " + cls + "'>" + String(names[i]) + lbl + "</button>";
  }
  
  html += "</div><script>const n=['Room Fan','Tube Light','Room Bulb','Dining Fan'];";
  // OPTIMISTIC TOGGLE
  html += "function t(id){ let b=document.getElementById('b'+id); let s=b.className.includes('off');";
  html += "b.className='btn '+(s?'on':'off'); b.innerHTML=n[id]+(s?' (ON)':' (OFF)'); fetch('/toggle?id='+id); }";
  
  // BACKGROUND SYNC
  html += "setInterval(function(){ fetch('/status').then(r=>r.json()).then(d=>{";
  html += "for(let i=0;i<4;i++){ let b=document.getElementById('b'+i); b.className='btn '+(d[i]?'on':'off');";
  html += "b.innerHTML=n[i]+(d[i]?' (ON)':' (OFF)'); } }); }, 1200);</script></body></html>";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  String out = "[";
  for(int i=0; i<4; i++) { out += (states[i] ? "true" : "false"); if(i<3) out += ","; }
  out += "]";
  server.send(200, "application/json", out);
}

void handleToggle() {
  int id = server.arg("id").toInt();
  states[id] = !states[id];
  digitalWrite(relayPins[id], states[id] ? LOW : HIGH);
  server.send(200, "text/plain", "ok");
}

void setup() {
  // Serial is disabled because RX and TX are being used as physical switch inputs.
  
  for(int i=0; i<4; i++) {
    pinMode(relayPins[i], OUTPUT); 
    digitalWrite(relayPins[i], HIGH); // Relays OFF by default (Active-Low)
    
    // Uses internal resistor. Switch connects the pin to Ground.
    pinMode(switchPins[i], INPUT_PULLUP); 
    lastSwitch[i] = digitalRead(switchPins[i]);
  }

  // ACT AS ROUTER
  WiFi.softAP("My Smart Room", "yourpassword");
  dnsServer.start(53, "*", WiFi.softAPIP()); // For the Auto-Popup
  
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/status", handleStatus);
  server.onNotFound(handleRoot); // Captive Portal redirect
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // PHYSICAL SWITCH LOGIC
  for (int i = 0; i < 4; i++) {
    int cur = digitalRead(switchPins[i]);
    if (cur != lastSwitch[i]) {
      states[i] = !states[i]; // Flip the internal logical state
      digitalWrite(relayPins[i], states[i] ? LOW : HIGH); // Update the relay
      lastSwitch[i] = cur;
      delay(50); // Software debounce to prevent rapid flickering
    }
  }
}