/**********************************************************************************
 *  TITLE: Smart Home Security Camera (ESP32-CAM + PIR + Buzzer + LED + Relay)
 *  Created by: Kiran Gorajanal (2025)
 *  FIXED VERSION - With Your Credentials
 **********************************************************************************/

#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID   "TMPL123456"
#define BLYNK_TEMPLATE_NAME "Smart Home Security 2"
#define BLYNK_AUTH_TOKEN    "-6LDL85qlQhv8nIUbbMG7l0Ei4RD2OOO"

// --- Libraries ---
#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// --- Wi-Fi Credentials ---
const char* ssid = "***Kiran";
const char* password = "kiran127";

// --- Pin Definitions ---
#define PIR_PIN     13   // PIR motion sensor input
#define LED_PIN      4   // On-board flash LED
#define BUZZER_PIN  12   // Active buzzer output
#define RELAY_PIN   15   // Relay module output
#define RED_LED_PIN 14   // External red LED with 220Ω resistor

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// --- Global Variables ---
String local_IP;
BlynkTimer timer;
bool motionActive = false;
bool systemArmed = true;
bool buzzerEnabled = true;
bool relayEnabled = true;
unsigned long lastMotionTime = 0;
const unsigned long MOTION_COOLDOWN = 5000;

// Camera server
httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// ======================================================================================
// 📸 CAMERA WEB SERVER - CAPTURE HANDLER
// ======================================================================================
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ======================================================================================
// 📸 CAMERA WEB SERVER - STREAM HANDLER
// ======================================================================================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len;
  uint8_t *_jpg_buf;
  char *part_buf[64];

  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }

    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!jpeg_converted) {
        Serial.println("JPEG compression failed");
        res = ESP_FAIL;
      }
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }

    if (res != ESP_OK) {
      break;
    }
  }

  return res;
}

// ======================================================================================
// 🌐 START CAMERA WEB SERVER
// ======================================================================================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  Serial.println("Starting web server on port 80");
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
  }

  config.server_port = 81;
  config.ctrl_port = 81;
  Serial.println("Starting stream server on port 81");
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

// ======================================================================================
// 🔴 BLINK RED LED
// ======================================================================================
void blinkRedLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_LED_PIN, LOW);
    delay(200);
  }
}

// ======================================================================================
// 📸 CAPTURE PHOTO AND SEND TO BLYNK
// ======================================================================================
void takePhoto() {
  Serial.println("📷 Taking photo...");
  
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);
  
  if (buzzerEnabled) {
    digitalWrite(BUZZER_PIN, HIGH);
  }
  
  if (relayEnabled) {
    digitalWrite(RELAY_PIN, HIGH);
  }
  
  delay(300);

  uint32_t randomNum = random(50000);
  String photoURL = "http://" + local_IP + "/capture?_cb=" + String(randomNum);

  Serial.println("📷 Photo URL: " + photoURL);
  Blynk.virtualWrite(V2, photoURL);
  Blynk.logEvent("motion_detected", String("⚠ Motion Detected at ") + local_IP);

  delay(1000);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  
  blinkRedLED(3);
}

// ======================================================================================
// 📸 CAMERA INITIALIZATION
// ======================================================================================
void initCamera() {
  Serial.println("🎥 Initializing camera...");
  
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    Serial.println("✅ PSRAM found - High quality mode");
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("⚠ PSRAM not found - Standard quality mode");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed with error 0x%x\n", err);
    blinkRedLED(10);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  s->set_framesize(s, FRAMESIZE_QVGA);
  Serial.println("✅ Camera initialized successfully!");
  blinkRedLED(2);
}

// ======================================================================================
// 🚨 MOTION DETECTION LOGIC
// ======================================================================================
void checkMotion() {
  if (!systemArmed) {
    digitalWrite(RED_LED_PIN, LOW);
    return;
  }

  int motion = digitalRead(PIR_PIN);
  unsigned long currentTime = millis();

  if (motion == HIGH && !motionActive) {
    if (currentTime - lastMotionTime >= MOTION_COOLDOWN) {
      motionActive = true;
      lastMotionTime = currentTime;
      
      Serial.println("⚠️ MOTION DETECTED!");
      Blynk.virtualWrite(V1, 1);
      takePhoto();
    }
  } 
  else if (motion == LOW && motionActive) {
    motionActive = false;
    Serial.println("✅ Motion Cleared - System Monitoring");
    Blynk.virtualWrite(V1, 0);
    digitalWrite(RED_LED_PIN, HIGH);
  }
  
  if (systemArmed && !motionActive) {
    digitalWrite(RED_LED_PIN, HIGH);
  }
}

// ======================================================================================
// 📲 BLYNK VIRTUAL PIN FUNCTIONS
// ======================================================================================

// V3 - Manual Photo Capture
BLYNK_WRITE(V3) {
  int state = param.asInt();
  if (state == 1) {
    Serial.println("📷 Manual photo capture triggered");
    takePhoto();
    Blynk.virtualWrite(V3, 0);
  }
}

// V4 - Arm/Disarm System
BLYNK_WRITE(V4) {
  systemArmed = param.asInt();
  if (systemArmed) {
    Serial.println("🛡️ System ARMED");
    Blynk.virtualWrite(V7, "🛡️ ARMED - Monitoring");
    digitalWrite(RED_LED_PIN, HIGH);
  } else {
    Serial.println("🔓 System DISARMED");
    Blynk.virtualWrite(V7, "🔓 DISARMED");
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
    motionActive = false;
  }
}

// V5 - Buzzer Enable/Disable
BLYNK_WRITE(V5) {
  buzzerEnabled = param.asInt();
  Serial.println(buzzerEnabled ? "🔊 Buzzer ENABLED" : "🔇 Buzzer DISABLED");
}

// V6 - Relay Enable/Disable
BLYNK_WRITE(V6) {
  relayEnabled = param.asInt();
  Serial.println(relayEnabled ? "🔌 Relay ENABLED" : "⚫ Relay DISABLED");
}

// Blynk Connected
BLYNK_CONNECTED() {
  Serial.println("✅ Connected to Blynk!");
  Blynk.virtualWrite(V4, systemArmed);
  Blynk.virtualWrite(V5, buzzerEnabled);
  Blynk.virtualWrite(V6, relayEnabled);
  Blynk.virtualWrite(V7, systemArmed ? "🛡️ ARMED - Monitoring" : "🔓 DISARMED");
  Blynk.virtualWrite(V1, 0);
}

// ======================================================================================
// ⚙️ SETUP
// ======================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n🚀 ========================================");
  Serial.println("🚀 Smart Home Security System Starting...");
  Serial.println("🚀 ========================================\n");

  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  // Startup LED sequence
  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(150);
    digitalWrite(RED_LED_PIN, LOW);
    delay(150);
  }

  initCamera();

  Serial.print("📡 Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    Serial.print(".");
    digitalWrite(RED_LED_PIN, !digitalRead(RED_LED_PIN));
    delay(500);
    wifiAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📍 IP Address: ");
    Serial.println(WiFi.localIP());
    local_IP = WiFi.localIP().toString();
    
    digitalWrite(RED_LED_PIN, HIGH);
    delay(2000);
  } else {
    Serial.println("\n❌ WiFi Connection Failed!");
    blinkRedLED(10);
    return;
  }

  startCameraServer();
  Serial.print("🌐 Camera Server: http://");
  Serial.println(WiFi.localIP());
  Serial.print("📹 Live Stream: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream");

  Serial.println("📲 Connecting to Blynk...");
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  timer.setInterval(500L, checkMotion);

  Serial.println("\n✅ ========================================");
  Serial.println("✅ System Ready - 24/7 Surveillance Active");
  Serial.println("✅ ========================================\n");
  
  blinkRedLED(3);
}

// ======================================================================================
// 🔁 MAIN LOOP
// ======================================================================================
void loop() {
  Blynk.run();
  timer.run();
}