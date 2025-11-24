// Pines L298N
const int IN1 = 12;
const int IN2 = 14;  
const int IN3 = 26;
const int IN4 = 27;
const int ENA = 13;
const int ENB = 25;

// Velocidades
int speedSlow = 130;
int speedNormal = 160;
int speedFast = 190;

// Buffer para comandos
String commandBuffer = "";
const unsigned long COMMAND_TIMEOUT = 100; // Tiempo máximo entre caracteres (ms)
unsigned long lastCharTime = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  
  stopCar();
  Serial.println("🔧 ARDUINO INICIADO - LECTURA MEJORADA");
  Serial.println("💡 Comandos: FS, L1, L2, R1, R2, S");
}

void loop() {
  // Leer caracteres disponibles
  while (Serial.available()) {
    char c = Serial.read();
    
    // Si es un carácter de control (newline, carriage return) o el buffer está lleno, procesar comando
    if (c == '\n' || c == '\r' || commandBuffer.length() >= 5) {
      if (commandBuffer.length() > 0) {
        processCommand(commandBuffer);
        commandBuffer = "";
      }
    } else {
      // Agregar carácter al buffer
      commandBuffer += c;
      lastCharTime = millis();
    }
  }
  
  // Timeout: si ha pasado mucho tiempo desde el último carácter, procesar el buffer
  if (commandBuffer.length() > 0 && (millis() - lastCharTime > COMMAND_TIMEOUT)) {
    processCommand(commandBuffer);
    commandBuffer = "";
  }
  
  delay(10);
}

void processCommand(String command) {
  command.trim();
  
  Serial.print("📥 COMANDO RECIBIDO: '");
  Serial.print(command);
  Serial.println("'");
  
  // Procesar comando
  if (command == "FS") {
    moveForward(speedSlow);
    Serial.println("🚗 ADELANTE SUAVE");
  } 
  else if (command == "F") {
    moveForward(speedNormal);
    Serial.println("🚗 ADELANTE NORMAL");
  }
  else if (command == "L1") {
    turnLeft(speedSlow, speedNormal);
    Serial.println("↩️  IZQUIERDA SUAVE");
  } 
  else if (command == "L2") {
    turnLeft(speedFast, speedNormal);
    Serial.println("↩️  IZQUIERDA FUERTE");
  } 
  else if (command == "R1") {
    turnRight(speedNormal, speedSlow);
    Serial.println("↪️  DERECHA SUAVE");
  } 
  else if (command == "R2") {
    turnRight(speedNormal, speedFast);
    Serial.println("↪️  DERECHA FUERTE");
  } 
  else if (command == "S") {
    stopCar();
    Serial.println("🛑 DETENIDO");
  }
  else {
    Serial.print("❌ COMANDO DESCONOCIDO: '");
    Serial.print(command);
    Serial.println("'");
  }
}

void moveForward(int speed) {
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnLeft(int speedLeft, int speedRight) {
  analogWrite(ENA, speedLeft);
  analogWrite(ENB, speedRight);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight(int speedLeft, int speedRight) {
  analogWrite(ENA, speedLeft);
  analogWrite(ENB, speedRight);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopCar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}