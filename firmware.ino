#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_camera.h>

// ================= CONFIG =================
const char* ssid = "";
const char* password = "";

const char* firmware_url = "http://192.168.0.108:8000/firmware";
const char* version_url  = "http://192.168.0.108:8000/version";
const char* log_url      = "http://192.168.0.108:8000/log";

#define FIRMWARE_VERSION "1.0.0" // Human detection

// ==========================================

// ================= CAMERA PINS =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= MOTION GLOBALS =================
uint8_t *prev_frame = NULL;
size_t prev_len = 0;

#define MOTION_THRESHOLD 105
#define SAMPLE_STEP 5

unsigned long lastTriggerTime = 0;
#define COOLDOWN 5000

int motionCounter = 0;
#define MOTION_CONFIRM 2

int baselineDiff = 0;
#define BASELINE_SMOOTH 20

int baseline = 0;

// ================= LOGGER =================
void logMsg(String msg) {
  Serial.println(msg);

  static unsigned long lastLog = 0;
  if (millis() - lastLog < 2000) return;

  lastLog = millis();

  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  http.begin(client, log_url);
  http.setTimeout(2000);
  http.addHeader("Content-Type", "text/plain");

  http.POST(msg);
  http.end();
}

// ================= CAMERA INIT (JPEG)=================
void initCamera() {
  logMsg("Camera Starting...");

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    logMsg("Camera init FAILED");
    return;
  }

  sensor_t * s = esp_camera_sensor_get();

  if (s) {
    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, 1);

    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_awb_gain(s, 1);

    s->set_whitebal(s, 1);

    s->set_gainceiling(s, (gainceiling_t)6);
    s->set_bpc(s, 1);
    s->set_wpc(s, 1);

    s->set_sharpness(s, 1);
    s->set_denoise(s, 1);

    s->set_aec2(s, 1);

    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
  }

  logMsg("Camera ready!");
}

// ================= MOTION DETECTION =================
bool detectMotion(camera_fb_t *fb) {
  if (!fb) return false;

  static int prevValues[200];
  static bool initialized = false;

  int diff = 0;
  int index = 0;

  int len = fb->len;

  // sample sparse points
  for (int i = 100; i < len - 100 && index < 200; i += 300) {

    int val = fb->buf[i];  // pseudo brightness

    if (!initialized) {
      prevValues[index] = val;
    } else {
      int d = abs(val - prevValues[index]);

      if (d > 20) {
        diff++;
      }

      // smooth update
      prevValues[index] = (prevValues[index] * 3 + val) / 4;
    }

    index++;
  }

  initialized = true;

  if (baseline == 0) baseline = diff;
  if (diff < baseline + 10){
    baseline = (baseline * 9 + diff) / 10;
  }

  Serial.println("Diff:" + String(diff));

  int threshold = baseline + 10;

  Serial.println("Threshold:" + String(threshold));

  if (diff > baseline + 30){
    Serial.println("Strong motion");
  }

  return diff > threshold;   
}


// ================= DOORBELL =================
void sendRing() {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, "http://192.168.0.108:8000/ring");
  int code = http.GET();

  Serial.println("Ring code: " + String(code));
  http.end();
}

// ================= OTA =================
String getServerVersion() {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, version_url);
  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    WiFiClient * stream = http.getStreamPtr();

    String version = "";
    unsigned long startTime = millis();

    while (millis() - startTime < 5000) {
      while (stream->available()) {
        char c = stream->read();
        version += c;
      }
      if (!stream->connected()) break;
      delay(10);
    }

    version.trim();
    http.end();
    return version;
  }

  http.end();
  return "";
}

void performOTA() {
  WiFiClient client;
  HTTPClient http;

  logMsg("Starting OTA...");

  http.begin(client, firmware_url);
  http.setTimeout(20000);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    
    logMsg("Firmware size:" + String(contentLength));

    if (!Update.begin(contentLength)){
      logMsg("Not enough space for OTA");
      http.end();
      return;
    }

    WiFiClient * stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    logMsg("Written bytes:" + String(written));

    if (written == contentLength && Update.end(true)) {
      logMsg("OTA Success!");
      logMsg("Rebooting...");
      delay(1000);
      ESP.restart();
    }
    else{
      logMsg("OTA Failed");
    }
  }
  else{
    logMsg("HTTP Error:" + String(httpCode));
  }

  http.end();
}

void checkForOTA() {
  String serverVersion = getServerVersion();
  
  logMsg("Checking OTA...");
  logMsg("Current Version:" + String(FIRMWARE_VERSION));

  if (serverVersion == "" || serverVersion == FIRMWARE_VERSION){
    logMsg("Already Latest Version");
    return;
  }

  logMsg("New Version detected!");
  performOTA();
}

//===============IMAGE==================
void sendImage(){
  camera_fb_t * fb = esp_camera_fb_get();

  if(!fb){
    Serial.println("Capture Failed");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  http.begin(client, "http://192.168.0.108:8000/upload");
  http.addHeader("Content-Type","image/jpeg");

  int code = http.POST(fb->buf,fb->len);

  Serial.println("Upload code:" + String(code));

  http.end();
  esp_camera_fb_return(fb);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  pinMode(4, OUTPUT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(4, HIGH); 
    delay(500);
    digitalWrite(4,LOW);
  }

  WiFi.setSleep(false);

  logMsg("ESP32 Connected: " + WiFi.localIP().toString());

  Serial.println("\nInitializing Camera...");
  delay(2000);
  initCamera();

  Serial.println("\nChecking for OTA update...");
  delay(2000);
  checkForOTA();
}

// ================= LOOP =================
void loop() {

  camera_fb_t *fb = esp_camera_fb_get();

  if(!fb) {
    delay(1000);
    return;
  }

  bool motion = detectMotion(fb);
  esp_camera_fb_return(fb);

  if (motion){
    motionCounter++;
  } else {
    motionCounter = 0;
  }


  if(motionCounter >= 5 && (millis() - lastTriggerTime > COOLDOWN)){
    lastTriggerTime = millis();
    motionCounter = 0;

    logMsg("Ringing...");
    //sendRing();

    logMsg("Doorcam");
    sendImage();
  }

  delay(1000);
}
