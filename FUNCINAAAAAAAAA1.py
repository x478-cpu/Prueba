import cv2
import numpy as np
import serial
import time

# Configuración serial
try:
    arduino = serial.Serial('COM4', 115200, timeout=1)
    time.sleep(2)
    print("✅ Arduino conectado")
except Exception as e:
    print(f"❌ Error Arduino: {e}")
    exit()

cap = cv2.VideoCapture(1)
if not cap.isOpened():
    print("❌ No se pudo abrir la cámara")
    exit()

print("🚗 INICIANDO SEGUIDOR CON COMUNICACIÓN MEJORADA")
print("Presiona 'q' para salir")


def send_command_improved(command, arduino_obj):
    """Envía comando con terminación para asegurar la lectura"""
    try:
        # Enviar comando con newline para que Arduino sepa que es el final
        full_command = command + '\n'
        arduino_obj.write(full_command.encode())
        arduino_obj.flush()
        print(f"📤 ENVIADO: '{command}'")
        return True
    except Exception as e:
        print(f"❌ Error enviando '{command}': {e}")
        return False


# Enviar comando de paro inicial
send_command_improved("S", arduino)

try:
    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # Procesamiento básico de imagen
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        _, thresh = cv2.threshold(gray, 100, 255, cv2.THRESH_BINARY_INV)

        height, width = thresh.shape
        roi = thresh[height // 2:, :]

        # Encontrar contornos
        contours, _ = cv2.findContours(roi, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        cx = width // 2  # Valor por defecto (centro)

        if contours:
            largest = max(contours, key=cv2.contourArea)
            if cv2.contourArea(largest) > 500:  # Área mínima
                M = cv2.moments(largest)
                if M["m00"] != 0:
                    cx = int(M["m10"] / M["m00"])
                    cy = int(M["m01"] / M["m00"]) + height // 2
                    cv2.circle(frame, (cx, cy), 8, (255, 0, 0), -1)

        # Calcular desviación
        center_x = width // 2
        deviation = cx - center_x

        # Lógica de control simple
        if abs(deviation) < 50:
            command = "FS"
        elif deviation < -100:
            command = "L2"
        elif deviation < 0:
            command = "L1"
        elif deviation > 100:
            command = "R2"
        else:
            command = "R1"

        # Enviar comando
        send_command_improved(command, arduino)

        # Visualización
        cv2.line(frame, (center_x, 0), (center_x, height), (255, 255, 255), 2)
        cv2.putText(frame, f"CMD: {command}", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)
        cv2.putText(frame, f"DEV: {deviation}", (20, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)

        cv2.imshow('Seguidor de Linea', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except Exception as e:
    print(f"💥 ERROR: {e}")

finally:
    print("🧹 Limpiando...")
    send_command_improved("S", arduino)
    time.sleep(0.5)
    arduino.close()
    cap.release()
    cv2.destroyAllWindows()
    print("✅ Programa terminado")