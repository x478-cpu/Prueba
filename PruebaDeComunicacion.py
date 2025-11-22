import serial
import time
import sys


def test_comunicacion_directa():
    """🧪 PRUEBA DIRECTA DE COMUNICACIÓN"""
    print("🧪 INICIANDO PRUEBA DEFINITIVA DE COMUNICACIÓN")
    print("🔧 Conecta la ESP32 y CIERRA Arduino IDE")

    try:
        # Intentar conexión directa
        arduino = serial.Serial('COM9', 115200, timeout=1)
        print("✅ Puerto COM9 abierto")

        # Esperar un poco
        time.sleep(2)

        # Leer cualquier dato inicial
        print("📖 Leyendo datos iniciales...")
        time.sleep(1)
        while arduino.in_waiting:
            linea = arduino.readline().decode('utf-8', errors='ignore').strip()
            print(f"📨 ESP32: {linea}")

        # Probar cada comando
        comandos = ['F', 'L', 'R', 'S', 'X']

        for comando in comandos:
            print(f"\n🎯 Probando comando: {comando}")

            # Limpiar buffer
            arduino.reset_input_buffer()

            # Enviar comando
            print(f"📤 Enviando: {comando}")
            arduino.write(comando.encode('utf-8'))
            arduino.flush()

            # Esperar respuesta
            time.sleep(0.5)

            # Leer todas las respuestas disponibles
            respuestas = []
            while arduino.in_waiting:
                linea = arduino.readline().decode('utf-8', errors='ignore').strip()
                if linea:
                    respuestas.append(linea)

            if respuestas:
                print(f"✅ Respuestas: {respuestas}")
            else:
                print("❌ NO HUBO RESPUESTA")

            time.sleep(1)  # Esperar entre comandos

        arduino.close()
        print("\n🎊 Prueba completada")

    except Exception as e:
        print(f"❌ Error: {e}")
        print("💡 Posibles soluciones:")
        print("   1. CIERRA Arduino IDE completamente")
        print("   2. Verifica que COM9 sea el puerto correcto")
        print("   3. Revisa los drivers CH340")
        print("   4. Prueba otro cable USB")


# Ejecutar prueba
if __name__ == "__main__":
    test_comunicacion_directa()