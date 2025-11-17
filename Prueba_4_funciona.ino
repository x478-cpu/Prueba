/*
 * ESP32 CAM - Servidor Web OPTIMIZADO para MÁXIMO FPS
 */

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp32-hal-psram.h"
#include <WiFi.h>

// Configuración de pines para AI-Thinker ESP32-CAM
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

// Configuración WiFi
const char* ssid = "ESP32-CAM-Robot";
const char* password = "12345678";

// Variables Globales
httpd_handle_t camera_server = NULL;

// ===================
// Configuración Cámara OPTIMIZADA
// ===================
void setupCamera() {
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
  config.xclk_freq_hz = 20000000;  // 20MHz - estándar
  
  // 🚀 CONFIGURACIÓN PARA MÁXIMO FPS 🚀
  config.frame_size = FRAMESIZE_QVGA;    // 320x240 - MÁS RÁPIDO que VGA
  config.jpeg_quality = 15;              // Calidad media-baja (más rápido)
  config.fb_count = 1;                   // SOLO 1 BUFFER - MÁXIMA VELOCIDAD
  config.pixel_format = PIXFORMAT_JPEG;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Optimizar sensor para velocidad
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
    s->set_brightness(s, 0);    // Neutral
    s->set_contrast(s, 0);      // Neutral  
    s->set_saturation(s, 0);    // Neutral
    s->set_special_effect(s, 0); // Sin efectos
    s->set_whitebal(s, 1);      // Auto white balance ON
    s->set_awb_gain(s, 1);      // Auto gain ON
    s->set_wb_mode(s, 0);       // Auto mode
    s->set_exposure_ctrl(s, 1); // Auto exposure ON
    s->set_aec2(s, 0);          // AEC2 OFF para más velocidad
    s->set_ae_level(s, 0);      // Neutral
    s->set_aec_value(s, 300);   // Valor de exposición fijo
    s->set_gain_ctrl(s, 1);     // Auto gain ON
    s->set_agc_gain(s, 0);      // Ganancia automática
    s->set_gainceiling(s, (gainceiling_t)0); // Sin límite de ganancia
    s->set_bpc(s, 0);           // BPC OFF
    s->set_wpc(s, 1);           // WPC ON
    s->set_raw_gma(s, 1);       // Gamma ON
    s->set_lenc(s, 1);          // Lensa correction ON
    s->set_dcw(s, 1);           // DCW ON
  }
  
  Serial.println("✅ Cámara optimizada para MÁXIMO FPS");
}

// ===================
// Stream Handler ULTRA RÁPIDO
// ===================
static esp_err_t stream_handler(httpd_req_t *req) {
  Serial.println("🎬 Cliente conectado - Stream ULTRA RÁPIDO");
  
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char * part_buf[64];
  
  // Headers para stream rápido
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "X-Accel-Buffering", "no"); // Importante para velocidad
  
  if(res != ESP_OK) return res;

  uint32_t last_frame_time = millis();
  uint32_t frame_count = 0;
  uint32_t total_size = 0;
  
  while(true) {
    // Verificar si cliente sigue conectado
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) break;
    
    // 🚀 CAPTURA ULTRA RÁPIDA - sin procesamiento extra
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Capture failed");
      break;
    }
    
    frame_count++;
    total_size += fb->len;
    
    // Enviar frame MUY RÁPIDO
    httpd_resp_send_chunk(req, "--frame\r\n", 10);
    httpd_resp_send_chunk(req, "Content-Type: image/jpeg\r\n", 25);
    
    char content_length[32];
    int clen = snprintf(content_length, 32, "Content-Length: %u\r\n\r\n", fb->len);
    httpd_resp_send_chunk(req, content_length, clen);
    
    // Enviar imagen directamente del buffer
    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) break;
    
    httpd_resp_send_chunk(req, "\r\n", 2);
    
    // 🚀 LIBERAR INMEDIATAMENTE para siguiente frame
    esp_camera_fb_return(fb);
    fb = NULL;
    
    // Mostrar estadísticas cada ~100 frames
    if (frame_count % 100 == 0) {
      uint32_t current_time = millis();
      float fps = (frame_count * 1000.0) / (current_time - last_frame_time);
      float avg_size = total_size / frame_count;
      Serial.printf("📊 FPS: %.1f | Avg Size: %.0f bytes\n", fps, avg_size);
    }
    
    // 🚀 DELAY MÍNIMO para máximo FPS
    delay(5); // Reducido al mínimo
  }
  
  Serial.printf("📈 Stream terminado: %u frames\n", frame_count);
  if (fb) esp_camera_fb_return(fb);
  return res;
}

// ===================
// Handler para Captura Simple (RÁPIDO)
// ===================
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  return ESP_OK;
}

// Página web simple (igual que antes)
static const char* MAIN_page = R"rawliteral(
<!DOCTYPE html><html><head>
<title>ESP32-CAM MAX FPS</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; text-align: center; margin: 20px; background: #f0f0f0; }
.container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
button { padding: 12px 25px; margin: 8px; font-size: 16px; cursor: pointer; background: #007bff; color: white; border: none; border-radius: 8px; }
button:hover { background: #0056b3; }
.video-container { margin: 20px 0; background: #000; border-radius: 10px; overflow: hidden; }
#streamImage { width: 100%; max-width: 320px; display: none; }
</style>
</head>
<body>
<div class="container">
    <h1>🚀 ESP32-CAM - MÁXIMO FPS</h1>
    <div style="background:#e7f3ff;padding:10px;border-radius:5px;margin:10px 0;">
        <strong>Configuración:</strong> QVGA (320x240) | Calidad: 15 | 1 Buffer
    </div>
    <button onclick="startStream()">▶️ Ver Video ULTRA RÁPIDO</button>
    <button onclick="stopStream()">⏹️ Detener</button>
    <button onclick="capturePhoto()">📷 Capturar Foto</button>
    <div class="video-container">
        <img id="streamImage" src="" alt="Video Stream">
    </div>
</div>
<script>
let streamInterval;
function startStream() {
    const img = document.getElementById('streamImage');
    img.style.display = 'block';
    let frameCount = 0;
    streamInterval = setInterval(() => {
        img.src = '/capture?_t=' + Date.now() + '&frame=' + frameCount++;
    }, 50); // 20 FPS máximo en modo captura
}
function stopStream() {
    clearInterval(streamInterval);
    document.getElementById('streamImage').style.display = 'none';
}
function capturePhoto() {
    const img = document.getElementById('streamImage');
    stopStream();
    img.src = '/capture?_t=' + Date.now();
    img.style.display = 'block';
}
</script>
</body></html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, MAIN_page, strlen(MAIN_page));
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_open_sockets = 2;  // Mínimo para mejor rendimiento
  config.lru_purge_enable = true;
  
  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };
  httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler };
  
  if (httpd_start(&camera_server, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_server, &index_uri);
    httpd_register_uri_handler(camera_server, &stream_uri);
    httpd_register_uri_handler(camera_server, &capture_uri);
    Serial.println("✅ Servidor HTTP optimizado para FPS");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.println("\n🚀 ESP32-CAM - MODO MÁXIMO FPS");
  
  // LED siempre encendido
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  
  // Inicializar cámara optimizada
  setupCamera();
  
  // WiFi optimizado
  WiFi.softAP(ssid, password);
  WiFi.setSleep(false);
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("📡 IP: ");
  Serial.println(myIP);
  Serial.println("⚡ Sistema listo - MÁXIMA VELOCIDAD");
  
  startCameraServer();
}

void loop() {
  delay(1000);
}