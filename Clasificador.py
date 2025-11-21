import cv2
import numpy as np

# --- Captura de video ---
cap = cv2.VideoCapture(0)


def clasificar_forma(contorno):
    """Clasifica la forma geométrica basándose en el contorno"""
    try:
        # Aproximar el contorno
        perimetro = cv2.arcLength(contorno, True)
        approx = cv2.approxPolyDP(contorno, 0.03 * perimetro, True)
        vertices = len(approx)

        # Calcular área
        area = cv2.contourArea(contorno)
        if area < 500:  # Filtrar áreas muy pequeñas
            return "Muy pequeno", vertices

        # Calcular circularidad
        circularidad = (4 * np.pi * area) / (perimetro * perimetro) if perimetro > 0 else 0

        # Encontrar el rectángulo delimitador
        x, y, w, h = cv2.boundingRect(contorno)
        relacion_aspecto = float(w) / h if h > 0 else 0

        # Clasificar por número de vértices
        if vertices == 3:
            return "Triangulo", vertices
        elif vertices == 4:
            # Calcular qué tan rectangular es el contorno
            area_rect = w * h
            extension = area / area_rect if area_rect > 0 else 0

            if 0.9 <= relacion_aspecto <= 1.1 and extension > 0.85:
                return "Cuadrado", vertices
            elif extension > 0.75:
                return "Rectangulo", vertices
            else:
                return "Cuadrilatero", vertices
        elif vertices == 5:
            return "Pentagono", vertices
        elif vertices == 6:
            return "Hexagono", vertices
        elif vertices > 6:
            if circularidad > 0.75:
                return "Circulo", vertices
            elif circularidad > 0.6:
                return "Ovalo", vertices
            else:
                return f"Poligono({vertices})", vertices
        else:
            return "Desconocido", vertices

    except Exception as e:
        return "Error", 0


while True:
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.resize(frame, (640, 480))

    # --- RECORTAR ZONA CENTRAL ---
    altura, ancho = frame.shape[:2]
    margen_vertical = altura // 4
    margen_horizontal = ancho // 3

    inicio_x = margen_horizontal
    fin_x = ancho - margen_horizontal
    inicio_y = margen_vertical
    fin_y = altura - margen_vertical

    # Recortar la región central
    frame_recortado = frame[inicio_y:fin_y, inicio_x:fin_x]

    # --- PREPROCESAMIENTO SIMPLIFICADO PERO EFECTIVO ---
    gris = cv2.cvtColor(frame_recortado, cv2.COLOR_BGR2GRAY)

    # Método 1: Umbral adaptativo (funciona mejor con iluminación variable)
    thresh_adaptativo = cv2.adaptiveThreshold(
        gris, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY_INV, 11, 2
    )

    # Método 2: Umbral simple (para objetos con buen contraste)
    _, thresh_simple = cv2.threshold(gris, 100, 255, cv2.THRESH_BINARY_INV)

    # Combinar ambos métodos
    thresh_combinado = cv2.bitwise_or(thresh_adaptativo, thresh_simple)

    # Operaciones morfológicas para limpiar
    kernel = np.ones((3, 3), np.uint8)
    thresh_combinado = cv2.morphologyEx(thresh_combinado, cv2.MORPH_OPEN, kernel)
    thresh_combinado = cv2.morphologyEx(thresh_combinado, cv2.MORPH_CLOSE, kernel)

    # Encontrar contornos en la imagen procesada
    contornos_formas, _ = cv2.findContours(
        thresh_combinado, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE
    )

    # --- DETECCIÓN DE COLORES ---
    hsv = cv2.cvtColor(frame_recortado, cv2.COLOR_BGR2HSV)

    # Rangos HSV ajustados
    rojo_bajo1 = np.array([0, 100, 100])
    rojo_alto1 = np.array([10, 255, 255])
    rojo_bajo2 = np.array([160, 100, 100])
    rojo_alto2 = np.array([180, 255, 255])

    verde_bajo = np.array([40, 50, 50])
    verde_alto = np.array([90, 255, 255])

    azul_bajo = np.array([90, 50, 50])
    azul_alto = np.array([130, 255, 255])

    # Máscaras de color
    mask_rojo1 = cv2.inRange(hsv, rojo_bajo1, rojo_alto1)
    mask_rojo2 = cv2.inRange(hsv, rojo_bajo2, rojo_alto2)
    mask_rojo = cv2.add(mask_rojo1, mask_rojo2)

    mask_verde = cv2.inRange(hsv, verde_bajo, verde_alto)
    mask_azul = cv2.inRange(hsv, azul_bajo, azul_alto)

    # Variables para colores
    rojo_detectado = False
    verde_detectado = False
    azul_detectado = False

    # Detectar colores
    colores_info = [
        (mask_rojo, 'Rojo', (0, 0, 255)),
        (mask_verde, 'Verde', (0, 255, 0)),
        (mask_azul, 'Azul', (255, 0, 0))
    ]

    for mask, nombre, color in colores_info:
        contornos_color, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        for c in contornos_color:
            area = cv2.contourArea(c)
            if area > 500:
                x, y, w, h = cv2.boundingRect(c)
                x_orig = x + inicio_x
                y_orig = y + inicio_y

                cv2.rectangle(frame, (x_orig, y_orig), (x_orig + w, y_orig + h), color, 2)
                cv2.putText(frame, nombre, (x_orig, y_orig - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

                if nombre == 'Rojo':
                    rojo_detectado = True
                elif nombre == 'Verde':
                    verde_detectado = True
                elif nombre == 'Azul':
                    azul_detectado = True

    # --- DETECCIÓN Y CLASIFICACIÓN DE FORMAS ---
    forma_principal = "No geometrico"
    vertices_principal = 0

    # Ordenar contornos por área (mayor primero)
    contornos_formas = sorted(contornos_formas, key=cv2.contourArea, reverse=True)

    for contorno in contornos_formas:
        area = cv2.contourArea(contorno)
        if area > 1000:  # Solo objetos significativos
            forma, vertices = clasificar_forma(contorno)

            if forma != "Muy pequeno" and forma != "Error":
                forma_principal = forma
                vertices_principal = vertices

                # Dibujar el contorno y información
                x, y, w, h = cv2.boundingRect(contorno)
                x_orig = x + inicio_x
                y_orig = y + inicio_y

                # Dibujar contorno
                cv2.drawContours(frame, [contorno + np.array([inicio_x, inicio_y])],
                                 -1, (0, 255, 255), 3)

                # Dibujar bounding box
                cv2.rectangle(frame, (x_orig, y_orig), (x_orig + w, y_orig + h),
                              (255, 0, 0), 2)

                # Mostrar información de la forma
                info_forma = f"{forma} ({vertices} vertices)"
                cv2.putText(frame, info_forma, (x_orig, y_orig - 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
                cv2.putText(frame, info_forma, (x_orig, y_orig - 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 1)
                break  # Solo el objeto más grande

    # --- DETERMINAR COLOR COMBINADO ---
    if rojo_detectado and azul_detectado:
        color_detectado = 'Rojo+Azul'
    elif rojo_detectado and verde_detectado:
        color_detectado = 'Rojo+Verde'
    elif rojo_detectado:
        color_detectado = 'Rojo'
    elif verde_detectado:
        color_detectado = 'Verde'
    elif azul_detectado:
        color_detectado = 'Azul'
    else:
        color_detectado = 'Sin color'

    # --- MOSTRAR INFORMACIÓN ---
    # Zona de detección
    cv2.rectangle(frame, (inicio_x, inicio_y), (fin_x, fin_y), (255, 255, 255), 2)
    cv2.putText(frame, "ZONA DE DETECCION", (inicio_x, inicio_y - 15),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

    # Información principal
    info_principal = f"Color: {color_detectado} | Forma: {forma_principal}"
    cv2.putText(frame, info_principal, (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 3)
    cv2.putText(frame, info_principal, (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

    # Mostrar en consola
    if forma_principal != "No geometrico":
        print(f"DETECTADO: {forma_principal} - Color: {color_detectado}")

    # Mostrar ventanas
    cv2.imshow('Clasificador de Formas y Colores', frame)
    cv2.imshow('Umbral Adaptativo', thresh_adaptativo)
    cv2.imshow('Umbral Combinado', thresh_combinado)

    key = cv2.waitKey(1) & 0xFF
    if key == 27:  # ESC
        break
    elif key == ord(' '):  # Espacio para pausa/depuración
        print("=== PAUSA ===")
        print(f"Forma: {forma_principal}, Vertices: {vertices_principal}")
        cv2.waitKey(0)

cap.release()
cv2.destroyAllWindows()