/*
 * ESP32 CAM - Seguidor de Líneas con Control de Motores
 * Servidor web con stream de video y control de motores
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
// Configuración Motores
// ===================
#define MOTOR_A_IN1 12
#define MOTOR_A_IN2 13
#define MOTOR_B_IN1 14
#define MOTOR_B_IN2 15

// ===================
// Configuración WiFi
// ===================
const char* ssid = "ESP32-CAM-Robot";
const char* password = "12345678";

// ===================
// Variables Globales
// ===================
httpd_handle_t camera_server = NULL;

// ===================
// Configuración Cámara
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
  config.frame_size = FRAMESIZE_QVGA;   // 320x240 - Más rápido
  config.jpeg_quality = 12;             // Calidad media
  config.fb_count = 1;                  // Un buffer para máxima velocidad

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Ajustar configuración adicional para mejor rendimiento
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1);        // Voltear verticalmente
    s->set_hmirror(s, 1);      // Voltear horizontalmente
    s->set_brightness(s, 0);   // Brillo neutral
    s->set_contrast(s, 0);     // Contraste neutral
    s->set_saturation(s, 0);   // Saturación neutral
  }
  
  Serial.println("✅ Cámara inicializada correctamente");
}

// ===================
// Configuración Motores
// ===================
void setupMotors() {
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  
  // Inicialmente apagar motores
  stopMotors();
  
  Serial.println("✅ Motores inicializados");
}

void setMotorSpeeds(int leftSpeed, int rightSpeed) {
  // Limitar velocidades entre -255 y 255
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);
  
  // Motor izquierdo (A)
  if (leftSpeed > 0) {
    // Adelante
    digitalWrite(MOTOR_A_IN1, HIGH);
    digitalWrite(MOTOR_A_IN2, LOW);
  } else if (leftSpeed < 0) {
    // Atrás
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, HIGH);
  } else {
    // Parar
    digitalWrite(MOTOR_A_IN1, LOW);
    digitalWrite(MOTOR_A_IN2, LOW);
  }
  
  // Motor derecho (B)
  if (rightSpeed > 0) {
    // Adelante
    digitalWrite(MOTOR_B_IN1, HIGH);
    digitalWrite(MOTOR_B_IN2, LOW);
  } else if (rightSpeed < 0) {
    // Atrás
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, HIGH);
  } else {
    // Parar
    digitalWrite(MOTOR_B_IN1, LOW);
    digitalWrite(MOTOR_B_IN2, LOW);
  }
  
  // Si tu driver de motores soporta PWM, puedes usar:
  // analogWrite(MOTOR_A_PWM, abs(leftSpeed));
  // analogWrite(MOTOR_B_PWM, abs(rightSpeed));
  
  Serial.printf("🎯 Motores: L=%d R=%d\n", leftSpeed, rightSpeed);
}

void stopMotors() {
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, LOW);
  digitalWrite(MOTOR_B_IN2, LOW);
  Serial.println("🛑 Motores detenidos");
}

// ===================
// Handler para Stream MJPG
// ===================
static esp_err_t stream_handler(httpd_req_t *req) {
  Serial.println("📹 Cliente conectado al stream");
  
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  
  // Configurar headers para stream
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  
  if(res != ESP_OK) return res;

  uint32_t frame_count = 0;
  uint32_t last_frame_time = millis();
  
  while(true) {
    // Verificar si cliente sigue conectado
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
      Serial.println("📹 Cliente desconectado del stream");
      break;
    }
    
    // Capturar frame
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Error capturando frame");
      res = ESP_FAIL;
      break;
    }
    
    frame_count++;
    
    // Enviar frame
    httpd_resp_send_chunk(req, "--frame\r\n", 10);
    httpd_resp_send_chunk(req, "Content-Type: image/jpeg\r\n", 25);
    
    char content_length[32];
    int clen = snprintf(content_length, 32, "Content-Length: %u\r\n\r\n", fb->len);
    httpd_resp_send_chunk(req, content_length, clen);
    
    // Enviar imagen
    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) {
      Serial.println("❌ Error enviando frame");
      esp_camera_fb_return(fb);
      break;
    }
    
    httpd_resp_send_chunk(req, "\r\n", 2);
    
    // Liberar frame
    esp_camera_fb_return(fb);
    fb = NULL;
    
    // Mostrar estadísticas cada 100 frames
    if (frame_count % 100 == 0) {
      uint32_t current_time = millis();
      float fps = (frame_count * 1000.0) / (current_time - last_frame_time);
      Serial.printf("📊 Stream FPS: %.1f\n", fps);
      frame_count = 0;
      last_frame_time = current_time;
    }
    
    // Pequeña pausa
    delay(10);
  }
  
  if (fb) {
    esp_camera_fb_return(fb);
  }
  
  Serial.println("📹 Stream terminado");
  return res;
}

// ===================
// Handler para Captura Simple
// ===================
static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Error en captura individual");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  Serial.println("📸 Captura individual enviada");
  return res;
}

// ===================
// Handler para Control de Motores
// ===================
static esp_err_t motor_handler(httpd_req_t *req) {
  char buffer[100];
  int ret = httpd_req_get_url_query_str(req, buffer, sizeof(buffer));
  
  if (ret == ESP_OK) {
    char left_str[10], right_str[10];
    
    // Obtener parámetros left y right de la URL
    if (httpd_query_key_value(buffer, "left", left_str, sizeof(left_str)) == ESP_OK &&
        httpd_query_key_value(buffer, "right", right_str, sizeof(right_str)) == ESP_OK) {
      
      int left_speed = atoi(left_str);
      int right_speed = atoi(right_str);
      
      // Controlar motores
      setMotorSpeeds(left_speed, right_speed);
      
      // Respuesta de confirmación
      String response = "Motores: L=" + String(left_speed) + " R=" + String(right_speed);
      httpd_resp_set_type(req, "text/plain");
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
      httpd_resp_send(req, response.c_str(), response.length());
      
      Serial.printf("🚗 Comando motores: L=%d R=%d\n", left_speed, right_speed);
      return ESP_OK;
    }
  }
  
  // Si hay error, enviar respuesta de error
  httpd_resp_send_500(req);
  return ESP_FAIL;
}

// ===================
// Handler para Página Web
// ===================
static const char* MAIN_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Robot - Seguidor de Lineas</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            text-align: center; 
            margin: 20px; 
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
        .status-box {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 10px;
            margin: 15px 0;
            border-left: 5px solid #007bff;
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
        .video-container {
            margin: 20px 0;
            background: #000;
            border-radius: 10px;
            overflow: hidden;
            position: relative;
        }
        #streamImage {
            width: 100%;
            max-width: 320px;
            display: none;
        }
        .motor-control {
            background: #e7f3ff;
            padding: 15px;
            border-radius: 10px;
            margin: 15px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🤖 ESP32-CAM - Seguidor de Líneas</h1>
        
        <div class="status-box">
            <strong>Estado del Sistema:</strong>
            <div>✅ Cámara funcionando</div>
            <div>✅ Motores listos</div>
            <div>🎯 Control desde Python activo</div>
        </div>
        
        <div class="motor-control">
            <h3>🚗 Control de Motores</h3>
            <p>Los motores se controlan automáticamente desde Python</p>
            <p><strong>URL API:</strong> <code>/motors?left=VALOR&right=VALOR</code></p>
        </div>
        
        <div class="controls">
            <button onclick="startStream()">▶️ Ver Video</button>
            <button onclick="stopStream()">⏹️ Detener Video</button>
            <button onclick="capturePhoto()">📷 Capturar Foto</button>
            <button onclick="stopMotors()">🛑 Parar Motores</button>
        </div>
        
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
            }, 100);
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
        
        function stopMotors() {
            fetch('/motors?left=0&right=0')
                .then(response => response.text())
                .then(data => {
                    console.log('Motores detenidos:', data);
                    alert('Motores detenidos');
                })
                .catch(error => {
                    console.error('Error:', error);
                    alert('Error al detener motores');
                });
        }
        
        // Iniciar stream automáticamente
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
  config.max_open_sockets = 3;
  config.lru_purge_enable = true;
  
  // Definir URIs
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
  
  httpd_uri_t motor_uri = {
    .uri       = "/motors",
    .method    = HTTP_GET,
    .handler   = motor_handler,
    .user_ctx  = NULL
  };
  
  Serial.println("🌐 Iniciando servidor web...");
  
  if (httpd_start(&camera_server, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_server, &index_uri);
    httpd_register_uri_handler(camera_server, &stream_uri);
    httpd_register_uri_handler(camera_server, &capture_uri);
    httpd_register_uri_handler(camera_server, &motor_uri);
    Serial.println("✅ Servidor HTTP iniciado en puerto 80");
    Serial.println("📡 Endpoints disponibles:");
    Serial.println("   • /         - Página web");
    Serial.println("   • /stream   - Video MJPEG");
    Serial.println("   • /capture  - Captura individual");
    Serial.println("   • /motors   - Control de motores");
  } else {
    Serial.println("❌ Error iniciando servidor HTTP");
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
  Serial.println("🚀 ESP32-CAM - SEGUIDOR DE LÍNEAS INICIANDO");
  Serial.println("===========================================");
  
  // Configurar LED flash - SIEMPRE ENCENDIDO
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  Serial.println("💡 LED flash encendido");
  
  // Inicializar componentes
  setupCamera();
  setupMotors();
  
  // Configurar WiFi como punto de acceso
  WiFi.softAP(ssid, password);
  WiFi.setSleep(false); // Mejor rendimiento
  
  IPAddress myIP = WiFi.softAPIP();
  
  Serial.println("===========================================");
  Serial.print("📶 Punto de acceso: ");
  Serial.println(ssid);
  Serial.print("🔐 Contraseña: ");
  Serial.println(password);
  Serial.print("🌐 Dirección IP: ");
  Serial.println(myIP);
  Serial.println("===========================================");
  Serial.println("💻 Conéctate al WiFi y ve a:");
  Serial.print("   http://");
  Serial.println(myIP);
  Serial.println("===========================================");
  
  // Iniciar servidor web
  startCameraServer();
  
  Serial.println("✅ Sistema listo para seguidor de líneas!");
  Serial.println("🤖 Los motores responden a: /motors?left=VALOR&right=VALOR");
}

void loop() {
  // El servidor web maneja las solicitudes automáticamente
  // Solo mantenemos el sistema activo
  
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus > 10000) { // Cada 10 segundos
    lastStatus = millis();
    int clients = WiFi.softAPgetStationNum();
    Serial.printf("📊 Estado: %d cliente(s) conectado(s)\n", clients);
  }
  
  delay(1000);
}