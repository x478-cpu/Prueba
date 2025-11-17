/*
 * ESP32 CAM - Servidor Web con Stream de Video MEJORADO
 * Versión estable para streaming en tiempo real
 * LED siempre encendido
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

// ===================
// Configuración Cámara
// ===================

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

// ===================
// Configuración WiFi
// ===================
const char* ssid = "ESP32-CAM-Robot";
const char* password = "12345678";

// ===================
// Variables Globales
// ===================
httpd_handle_t camera_server = NULL;
bool isStreaming = false;

// ===================
// Inicializar Cámara
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Configuración optimizada para streaming
  config.frame_size = FRAMESIZE_VGA;   // 640x480 - Más estable que SVGA
  config.jpeg_quality = 10;            // Calidad media (1-63, menor es mejor)
  config.fb_count = 2;                 // Dos buffers

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Ajustar configuración adicional para mejor rendimiento
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);        // Voltear verticalmente si es necesario
    s->set_hmirror(s, 1);      // Voltear horizontalmente si es necesario
    s->set_brightness(s, 1);   // Brillo ( -2 to 2)
    s->set_contrast(s, 1);     // Contraste (-2 to 2)
    s->set_saturation(s, 0);   // Saturación (-2 to 2)
  }
  
  Serial.println("Cámara inicializada correctamente");
}

// ===================
// Handler para Stream MJPG MEJORADO
// ===================
static esp_err_t stream_handler(httpd_req_t *req) {
  Serial.println("Cliente conectado al stream");
  isStreaming = true;
  
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  if(res != ESP_OK){
    Serial.println("Error setting response type");
    return res;
  }
  
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  
  uint32_t last_frame_time = millis();
  uint32_t frame_count = 0;
  
  while(true){
    // Verificar si el cliente sigue conectado
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
      Serial.println("Cliente desconectado");
      break;
    }
    
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }
    
    frame_count++;
    uint32_t current_time = millis();
    
    // Calcular y mostrar FPS cada 5 segundos
    if (current_time - last_frame_time > 5000) {
      float fps = (frame_count * 1000.0) / (current_time - last_frame_time);
      Serial.printf("Stream FPS: %.1f\n", fps);
      frame_count = 0;
      last_frame_time = current_time;
    }
    
    // Enviar boundary
    httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n"));
    httpd_resp_send_chunk(req, "Content-Type: image/jpeg\r\n", strlen("Content-Type: image/jpeg\r\n"));
    
    // Enviar tamaño de contenido
    char content_length[50];
    sprintf(content_length, "Content-Length: %u\r\n\r\n", fb->len);
    httpd_resp_send_chunk(req, content_length, strlen(content_length));
    
    // Enviar imagen
    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) {
      Serial.println("Error sending frame");
      esp_camera_fb_return(fb);
      break;
    }
    
    // Finalizar frame
    httpd_resp_send_chunk(req, "\r\n", strlen("\r\n"));
    
    esp_camera_fb_return(fb);
    fb = NULL;
    
    // Pequeña pausa para evitar sobrecarga
    delay(10);
  }
  
  isStreaming = false;
  Serial.println("Stream terminado");
  
  if (fb) {
    esp_camera_fb_return(fb);
  }
  
  return res;
}

// ===================
// Handler para Captura Simple
// ===================
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
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
  
  Serial.println("Captura individual enviada");
  return res;
}

// ===================
// Handler para Página Web MEJORADA
// ===================
static const char* MAIN_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Robot</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            text-align: center; 
            margin: 0; 
            padding: 20px; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        .container { 
            max-width: 800px; 
            margin: 0 auto; 
            background: white; 
            padding: 30px; 
            border-radius: 15px; 
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        h1 { 
            color: #333; 
            margin-bottom: 20px;
        }
        .video-container {
            margin: 20px 0;
            background: #000;
            border-radius: 10px;
            overflow: hidden;
            position: relative;
        }
        #streamImage {
            width: 100%;
            max-width: 640px;
            display: none;
        }
        .controls { 
            margin: 25px 0; 
        }
        button { 
            padding: 12px 25px; 
            margin: 8px; 
            font-size: 16px; 
            cursor: pointer; 
            background: #007bff; 
            color: white; 
            border: none; 
            border-radius: 8px; 
            transition: all 0.3s ease;
            box-shadow: 0 4px 6px rgba(0,123,255,0.3);
        }
        button:hover { 
            background: #0056b3; 
            transform: translateY(-2px);
            box-shadow: 0 6px 8px rgba(0,123,255,0.4);
        }
        button:active {
            transform: translateY(0);
        }
        .status {
            padding: 10px;
            margin: 10px 0;
            border-radius: 5px;
            font-weight: bold;
        }
        .loading {
            background: #fff3cd;
            color: #856404;
            display: none;
        }
        .success {
            background: #d1edff;
            color: #004085;
        }
        .stream-info {
            background: #f8f9fa;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-size: 14px;
        }
        .led-status {
            background: #d4edda;
            color: #155724;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 ESP32-CAM Robot - Seguidor de Líneas</h1>
        
        <div class="led-status">
            💡 LED de iluminación: <strong>ENCENDIDO</strong>
        </div>
        
        <div class="stream-info">
            <strong>Consejo:</strong> El stream puede tardar unos segundos en cargar. 
            Si no funciona, intenta recargar la página.
        </div>
        
        <div class="controls">
            <button onclick="startStream()">▶️ Ver Video en Tiempo Real</button>
            <button onclick="stopStream()">⏹️ Detener Video</button>
            <button onclick="capturePhoto()">📷 Capturar Foto</button>
        </div>
        
        <div id="loadingStatus" class="status loading">
            ⏳ Iniciando stream, por favor espera...
        </div>
        
        <div id="successStatus" class="status success" style="display:none;">
            ✅ Stream activo
        </div>
        
        <div class="video-container">
            <img id="streamImage" src="" alt="Video Stream">
        </div>
    </div>
    
    <script>
        let streamInterval;
        let isStreaming = false;
        
        function showLoading() {
            document.getElementById('loadingStatus').style.display = 'block';
            document.getElementById('successStatus').style.display = 'none';
        }
        
        function showSuccess() {
            document.getElementById('loadingStatus').style.display = 'none';
            document.getElementById('successStatus').style.display = 'block';
        }
        
        function hideStatus() {
            document.getElementById('loadingStatus').style.display = 'none';
            document.getElementById('successStatus').style.display = 'none';
        }
        
        function startStream() {
            if (isStreaming) return;
            
            showLoading();
            const img = document.getElementById('streamImage');
            img.style.display = 'block';
            isStreaming = true;
            
            // Usar un enfoque más simple - recargar la imagen periódicamente
            let frameCount = 0;
            streamInterval = setInterval(() => {
                img.src = '/capture?_t=' + new Date().getTime() + '&frame=' + frameCount++;
                img.onload = function() {
                    if (frameCount === 1) {
                        showSuccess();
                    }
                };
                img.onerror = function() {
                    console.log('Error loading frame');
                };
            }, 100); // 10 FPS máximo
            
            console.log('Stream started');
        }
        
        function stopStream() {
            if (!isStreaming) return;
            
            clearInterval(streamInterval);
            const img = document.getElementById('streamImage');
            img.style.display = 'none';
            isStreaming = false;
            hideStatus();
            
            console.log('Stream stopped');
        }
        
        function capturePhoto() {
            const img = document.getElementById('streamImage');
            stopStream(); // Detener stream para captura limpia
            
            img.src = '/capture?_t=' + new Date().getTime();
            img.style.display = 'block';
            img.onload = function() {
                // La imagen se muestra automáticamente
            };
            
            console.log('Photo captured');
        }
        
        // Iniciar stream automáticamente al cargar la página
        window.onload = function() {
            setTimeout(startStream, 1000);
        };
    </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, MAIN_page, strlen(MAIN_page));
}

// ===================
// Configurar Servidor HTTP
// ===================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 80;
  config.max_open_sockets = 3;  // Reducir sockets para mejor estabilidad
  config.lru_purge_enable = true;
  
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  
  httpd_uri_t capture_uri = {
    .uri       = "/capture",
    .method    = HTTP_GET,
    .handler   = capture_handler,
    .user_ctx  = NULL
  };
  
  Serial.println("Iniciando servidor web...");
  
  if (httpd_start(&camera_server, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_server, &index_uri);
    httpd_register_uri_handler(camera_server, &stream_uri);
    httpd_register_uri_handler(camera_server, &capture_uri);
    Serial.println("Servidor HTTP iniciado en puerto 80");
  } else {
    Serial.println("Error iniciando servidor HTTP");
  }
}

// ===================
// Setup Principal
// ===================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desactivar detector de brownout
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  Serial.println();
  Serial.println("=== Iniciando ESP32-CAM Robot ===");
  
  // Configurar LED flash - ENCENDER SIEMPRE
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);  // HIGH enciende el LED
  Serial.println("💡 LED encendido permanentemente");
  
  // Inicializar cámara
  setupCamera();
  
  // Configurar WiFi como punto de acceso
  WiFi.softAP(ssid, password);
  
  // Configurar red para mejor rendimiento
  WiFi.setSleep(false);
  
  IPAddress myIP = WiFi.softAPIP();
  
  Serial.println("==================================");
  Serial.print("Punto de acceso: ");
  Serial.println(ssid);
  Serial.print("Contraseña: ");
  Serial.println(password);
  Serial.print("Dirección IP: ");
  Serial.println(myIP);
  Serial.println("==================================");
  Serial.println("Conéctate al WiFi y ve a:");
  Serial.print("http://");
  Serial.println(myIP);
  Serial.println("==================================");
  
  // Iniciar servidor web
  startCameraServer();
  
  Serial.println("✅ Sistema listo! LED encendido");
}

void loop() {
  // El LED ya está encendido permanentemente, no necesitamos hacer nada más
  // Solo mantenemos el sistema funcionando
  
  // Mostrar estado periódicamente
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    Serial.printf("Sistema activo - Clientes conectados: %d\n", WiFi.softAPgetStationNum());
  }
  
  delay(100);
}