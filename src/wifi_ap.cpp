#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include "config.h"
#include "chat_store.h"

// ─── Forward declaration for send callback ────────────────────────────────
typedef void (*OnSendRequestCb)(const char* text);
static OnSendRequestCb g_send_cb = nullptr;

static AsyncWebServer  g_server(80);
static AsyncWebSocket  g_ws("/ws");
static DNSServer       g_dns;
static const char*     g_callsign = nullptr;
static uint8_t         g_clients  = 0;

// ─── Embedded chat UI ─────────────────────────────────────────────────────
// Single-page app served directly from PROGMEM.
// WebSocket at /ws — real-time bidirectional messaging.
static const char CHAT_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>CIPHER</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0d0d0d;--surface:#1a1a1a;--border:#2a2a2a;
  --accent:#00ff88;--accent-dim:#00cc6a;
  --text:#e0e0e0;--dim:#888;
  --own:#004d29;--other:#1e1e1e;
  --hdr:52px;--inp:64px
}
html,body{height:100%;height:100dvh}
body{
  background:var(--bg);color:var(--text);
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
  display:flex;flex-direction:column;overflow:hidden;
  height:100dvh
}
header{
  height:var(--hdr);background:var(--surface);
  border-bottom:1px solid var(--border);
  display:flex;align-items:center;padding:0 1rem;gap:.6rem;flex-shrink:0
}
.logo{color:var(--accent);font-weight:700;font-size:1rem;letter-spacing:3px}
.node{color:var(--dim);font-size:.8rem}
.spacer{flex:1}
.rssi{color:var(--dim);font-size:.7rem}
.dot{width:9px;height:9px;border-radius:50%;background:var(--accent);transition:background .4s}
.dot.off{background:#333}
#msgs{
  flex:1;overflow-y:auto;padding:.75rem;
  display:flex;flex-direction:column;gap:.5rem
}
#msgs:empty::after{
  content:'No messages yet.';color:var(--dim);
  font-size:.85rem;margin:auto;text-align:center
}
.bw{display:flex;flex-direction:column;max-width:80%}
.bw.own{align-self:flex-end;align-items:flex-end}
.bw.other{align-self:flex-start;align-items:flex-start}
.sender{font-size:.65rem;color:var(--dim);margin-bottom:2px;padding:0 .35rem}
.bubble{
  padding:.55rem .9rem;border-radius:18px;
  font-size:.9rem;line-height:1.45;word-break:break-word
}
.bubble.own{
  background:var(--own);color:#b0ffd0;
  border-bottom-right-radius:4px
}
.bubble.other{
  background:var(--other);color:var(--text);
  border:1px solid var(--border);border-bottom-left-radius:4px
}
.meta{font-size:.62rem;color:var(--dim);margin-top:2px;padding:0 .35rem}
#bar{
  height:var(--inp);background:var(--surface);
  border-top:1px solid var(--border);
  display:flex;align-items:center;padding:0 .75rem;gap:.5rem;flex-shrink:0
}
#inp{
  flex:1;background:var(--bg);border:1px solid var(--border);
  color:var(--text);padding:.6rem 1rem;border-radius:24px;
  font-size:.9rem;outline:none
}
#inp:focus{border-color:var(--accent)}
#inp::placeholder{color:var(--dim)}
#btn{
  width:42px;height:42px;background:var(--accent);border:none;
  border-radius:50%;cursor:pointer;display:flex;
  align-items:center;justify-content:center;flex-shrink:0;
  transition:background .15s
}
#btn:hover{background:var(--accent-dim)}
#btn:disabled{background:#2a2a2a;cursor:default}
#btn svg{fill:#000}
#btn:disabled svg{fill:#555}
.toast{
  position:fixed;bottom:calc(var(--inp) + 10px);
  left:50%;transform:translateX(-50%);
  background:#222;color:var(--dim);font-size:.75rem;
  padding:.3rem .9rem;border-radius:12px;
  opacity:0;transition:opacity .3s;pointer-events:none;white-space:nowrap
}
.toast.show{opacity:1}
</style>
</head>
<body>
<header>
  <span class="logo">CIPHER</span>
  <span class="node" id="nname">connecting...</span>
  <span class="spacer"></span>
  <span class="rssi" id="rssi"></span>
  <span class="dot off" id="dot"></span>
</header>

<div id="msgs"></div>

<div id="bar">
  <input id="inp" type="text" placeholder="Type a message..." maxlength="187"
         autocomplete="off" autocorrect="off" autocapitalize="sentences">
  <button id="btn" disabled>
    <svg width="18" height="18" viewBox="0 0 24 24">
      <path d="M2 21l21-9L2 3v7l15 2-15 2z"/>
    </svg>
  </button>
</div>

<div class="toast" id="toast"></div>

<script>
const ME="%NODE%";
const msgs=document.getElementById("msgs");
const inp=document.getElementById("inp");
const btn=document.getElementById("btn");
const dot=document.getElementById("dot");
const nname=document.getElementById("nname");
const rssiEl=document.getElementById("rssi");
const toastEl=document.getElementById("toast");
let ws,rt,toastTimer;

function pad(n){return n<10?"0"+n:""+n}
function fmt(ms){
  const d=new Date();
  return pad(d.getHours())+":"+pad(d.getMinutes());
}
function toast(msg,ms=2500){
  clearTimeout(toastTimer);
  toastEl.textContent=msg;
  toastEl.classList.add("show");
  toastTimer=setTimeout(()=>toastEl.classList.remove("show"),ms);
}
function addBubble(from,text,rssi,own){
  const wrap=document.createElement("div");
  wrap.className="bw "+(own?"own":"other");
  if(!own){
    const s=document.createElement("div");
    s.className="sender";s.textContent=from;wrap.appendChild(s);
  }
  const b=document.createElement("div");
  b.className="bubble "+(own?"own":"other");
  b.textContent=text;wrap.appendChild(b);
  const m=document.createElement("div");
  m.className="meta";
  m.textContent=fmt()+(rssi&&rssi!==-1?"  ● "+rssi+" dBm":"");
  wrap.appendChild(m);
  msgs.appendChild(wrap);
  msgs.scrollTop=msgs.scrollHeight;
}
function send(){
  const t=inp.value.trim();
  if(!t||!ws||ws.readyState!==1)return;
  ws.send(JSON.stringify({type:"send",text:t}));
  inp.value="";inp.focus();
}
function connect(){
  const proto=location.protocol==="https:"?"wss:":"ws:";
  ws=new WebSocket(proto+"//"+location.host+"/ws");
  ws.onopen=()=>{
    btn.disabled=false;
    dot.classList.remove("off");
    toast("Connected");
  };
  ws.onclose=()=>{
    btn.disabled=true;
    dot.classList.add("off");
    nname.textContent="reconnecting...";
    rssiEl.textContent="";
    clearTimeout(rt);rt=setTimeout(connect,3000);
  };
  ws.onerror=()=>ws.close();
  ws.onmessage=(e)=>{
    let d;try{d=JSON.parse(e.data)}catch{return}
    if(d.type==="status"){
      nname.textContent=d.node;
      if(d.rssi)rssiEl.textContent=d.rssi+" dBm";
    }else if(d.type==="msg"){
      addBubble(d.from,d.text,d.rssi,d.own);
    }else if(d.type==="history"){
      msgs.innerHTML="";
      (d.messages||[]).forEach(m=>addBubble(m.from,m.text,m.rssi,m.own));
    }
  };
}
btn.addEventListener("click",send);
inp.addEventListener("keydown",e=>{if(e.key==="Enter")send()});
connect();
</script>
</body>
</html>)rawhtml";

// ─── WebSocket helpers ────────────────────────────────────────────────────

// Build a JSON status message
static String buildStatus(int16_t rssi) {
    JsonDocument doc;
    doc["type"] = "status";
    doc["node"] = g_callsign;
    if (rssi != 0) doc["rssi"] = rssi;
    doc["clients"] = g_clients;
    String out;
    serializeJson(doc, out);
    return out;
}

// Build a JSON history message from the chat store
static String buildHistory() {
    JsonDocument doc;
    doc["type"] = "history";
    JsonArray arr = doc["messages"].to<JsonArray>();

    uint8_t count = chatStoreCount();
    for (uint8_t i = 0; i < count; i++) {
        const CipherMessage* m = chatStoreGet(i);
        if (!m) continue;
        JsonObject obj = arr.add<JsonObject>();
        obj["from"] = m->from;
        obj["text"] = m->text;
        obj["rssi"] = m->rssi;
        obj["own"]  = m->own;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

// Broadcast a new message to all connected WebSocket clients
void wifiApBroadcast(const char* from, const char* text,
                     int16_t rssi, bool own) {
    JsonDocument doc;
    doc["type"] = "msg";
    doc["from"] = from;
    doc["text"] = text;
    doc["rssi"] = rssi;
    doc["own"]  = own;
    String out;
    serializeJson(doc, out);
    g_ws.textAll(out);
}

uint8_t wifiApClientCount() { return g_clients; }

// ─── WebSocket event handler ──────────────────────────────────────────────
static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        g_clients++;
        Serial.printf("[WS] Client %u connected (%u total)\n",
                      client->id(), g_clients);
        // Send history then status
        client->text(buildHistory());
        client->text(buildStatus(0));

    } else if (type == WS_EVT_DISCONNECT) {
        if (g_clients > 0) g_clients--;
        Serial.printf("[WS] Client %u disconnected (%u total)\n",
                      client->id(), g_clients);

    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->opcode != WS_TEXT) return;

        // Null-terminate incoming text
        char buf[256];
        size_t copy_len = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
        memcpy(buf, data, copy_len);
        buf[copy_len] = '\0';

        JsonDocument doc;
        if (deserializeJson(doc, buf) != DeserializationError::Ok) return;

        const char* type_str = doc["type"] | "";
        if (strcmp(type_str, "send") == 0) {
            const char* text = doc["text"] | "";
            if (strlen(text) > 0 && g_send_cb) {
                g_send_cb(text);
            }
        }
    } else if (type == WS_EVT_ERROR) {
        Serial.printf("[WS] Error on client %u\n", client->id());
    }
}

// ─── Public init ──────────────────────────────────────────────────────────
void wifiApInit(const char* callsign, OnSendRequestCb send_cb) {
    g_callsign = callsign;
    g_send_cb  = send_cb;

    // Build SSID: "CIPHER-ALPHA" etc.
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "CIPHER-%s", callsign);

    // Start AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS);
    Serial.printf("[WiFi] AP started: %s  IP: %s\n",
                  ssid, WiFi.softAPIP().toString().c_str());

    // DNS — resolve all domains to 192.168.4.1 so phones auto-open browser
    g_dns.start(53, "*", WiFi.softAPIP());

    // WebSocket
    g_ws.onEvent(onWsEvent);
    g_server.addHandler(&g_ws);

    // Chat page — served at / and common captive portal detection URLs
    auto serveChat = [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", CHAT_HTML);
    };

    g_server.on("/", HTTP_GET, serveChat);
    g_server.on("/index.html", HTTP_GET, serveChat);

    // iOS / Android captive portal detection — redirect into full browser
    g_server.on("/hotspot-detect.html",    HTTP_GET, [](AsyncWebServerRequest* r){
        r->redirect("/");
    });
    g_server.on("/generate_204",           HTTP_GET, [](AsyncWebServerRequest* r){
        r->redirect("/");
    });
    g_server.on("/connecttest.txt",        HTTP_GET, [](AsyncWebServerRequest* r){
        r->redirect("/");
    });
    g_server.on("/redirect",               HTTP_GET, [](AsyncWebServerRequest* r){
        r->redirect("/");
    });

    // Catch-all
    g_server.onNotFound([](AsyncWebServerRequest* req) {
        req->redirect("/");
    });

    g_server.begin();
    Serial.println("[Web] Server started on port 80");
}

// Call from loop() to process DNS
void wifiApTick() {
    g_dns.processNextRequest();
    g_ws.cleanupClients();
}
