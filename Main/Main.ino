// ESP32 Gemma3 chat with 1 MB rolling memory (LittleFS)
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

const char* SSID = "YOUR_SSID";
const char* PASS = "YOUR_PASSWORD";
const char* URL  = "http://localhost:11434/v1/chat/completions";

const char* LOG  = "/chat.log";              // chat history file
const size_t MAX = 1024 * 1024;               // 1 MB max size

// --- helper: append a line & trim file if it grows past MAX ---
void logMsg(const char* role, const String& text) {
  File f = LittleFS.open(LOG, FILE_APPEND);
  f.printf("%s|%s\n", role, text.c_str());
  f.close();

  // shrink file to ~80 % if above MAX
  if (LittleFS.open(LOG).size() > MAX) {
    File in = LittleFS.open(LOG, FILE_READ);
    String keep;
    in.seek(in.size() - (MAX * 8 / 10));          // jump near end
    in.readStringUntil('\n');                     // align to next full line
    keep = in.readString();                        // read rest
    in.close();

    File out = LittleFS.open(LOG, FILE_WRITE);
    out.print(keep);
    out.close();
  }
}

// --- helper: return chat history as JSON array string ---
String historyJSON() {
  DynamicJsonDocument doc(256 * 1024);            // adjust if needed
  JsonArray arr = doc.to<JsonArray>();
  File f = LittleFS.open(LOG, FILE_READ);
  while (f && f.available()) {
    String line = f.readStringUntil('\n');
    int p = line.indexOf('|');
    if (p > 0) {
      JsonObject m = arr.createNestedObject();
      m["role"]    = line.substring(0, p);
      m["content"] = line.substring(p + 1);
    }
  }
  f.close();
  String out; serializeJson(arr, out); return out;
}

// --- send chat ---
void sendChat(const String& user) {
  logMsg("user", user);
  String payload = String("{\"model\":\"gemma3:1b\",\"messages\":") + historyJSON() + ",\"stream\":false}";

  HTTPClient http;
  http.begin(URL);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);

  if (code > 0) {
    DynamicJsonDocument res(32 * 1024);
    deserializeJson(res, http.getString());
    String ans = res["choices"][0]["message"]["content"].as<String>();
    Serial.println(ans);
    logMsg("assistant", ans);
  } else {
    Serial.printf("HTTP error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  LittleFS.begin(true);                           // auto‑format on first run
  sendChat("How do I set up a smart garden system?");
}

void loop() {
  // call sendChat("your message") whenever you need
}
