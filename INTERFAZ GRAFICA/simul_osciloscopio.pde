import processing.serial.*; 

Serial puertoSerie; 

float volumenReal = 0;     // Almacena el último valor numérico real recibido desde el puerto
float volumenInterp = 0;   // Almacena el valor suavizado mediante interpolación para las animaciones

// --- HISTORIAL PARA EL OSCILOSCOPIO ---
int maxMuestras = 50;        // Cantidad máxima de puntos que caben en la pantalla a lo largo del tiempo
float[] historialVolumen;   // Array (vector) que guarda los últimos 50 valores para crear las gráficas
int muestreoX = 14;          // Distancia en píxeles (ancho) entre cada muestra en el eje X

float desfaseFase = 0;      // Variable incremental para animar el movimiento de la onda senoidal

// --- CONFIGURACIÓN DEL UMBRAL ---
float umbralDisparo = 65.0; // Límite en decibelios (dB). Valores >= 65 provocan un '1' lógico, menores un '0'

void setup() {
  size(800, 650); // Crea una ventana de visualización de 800x650 píxeles
  
  // Inicializa el array de historial con un tamaño de 50 elementos
  historialVolumen = new float[maxMuestras];
  for (int i = 0; i < maxMuestras; i++) {
    historialVolumen[i] = 0; // Llena el array con ceros para evitar valores nulos al arrancar
  }
  
  println("Puertos disponibles:");
  printArray(Serial.list()); // Imprime en la consola de Processing la lista de puertos COM activos
  
  // Configura el puerto COM a una velocidad de transferencia de 115200 baudios
  puertoSerie = new Serial(this, "COM4", 115200); 
  delay(2000);
  
  // Configura el evento serial para que espere un salto de línea ('\n') antes de disparar el "serialEvent"
  puertoSerie.bufferUntil('\n');
  println("CONECTADO");
}

void draw() {
  background(20); // Borra el fotograma anterior pintando el fondo de gris casi negro
  
  // Suavizado dinámico: reduce la distancia al objetivo en un 15% por frame para evitar saltos bruscos
  volumenInterp = lerp(volumenInterp, volumenReal, 0.15); 
  volumenInterp = constrain(volumenInterp, 0, 100); // Restringe el volumen estrictamente entre el rango de 0 y 100

  // Puntos de referencia en el eje Y para centrar las tres representaciones visuales
  float centroArriba = height * 0.22;  // Altura central de la onda senoidal analógica
  float centroAbajo = height * 0.60;   // Altura central del osciloscopio digital (ADC)
  float centroBinario = height * 0.88; // Altura central de la gráfica de bloques lógicos (0 y 1)
  
  // =========================================================================
  // --- PARTE SUPERIOR: SEÑAL ANALÓGICA ---
  // =========================================================================
  dibujarMarco("SEÑAL ANALÓGICA DE ENTRADA (FRECUENCIA SIMULADA)", 0, height/2 - 50);
  
  // Mapea el volumen suavizado para definir qué tan alta/amplia será la onda senoidal
  float amplitudDinamica = map(volumenInterp, 0, 100, 0, 100); 
  
  stroke(50, 200, 255); // Color de la línea: Azul celeste brillante
  strokeWeight(2);      // Grosor de la línea
  noFill();             // Evita que la onda intente rellenarse por dentro
  
  beginShape(); // Inicia el trazado continuo 
  for (int x = 50; x < width - 50; x++) { 
    // 'desfaseFase' genera el movimiento horizontal y 'x * 0.04' controla la frecuencia visual de la onda
    float y = centroArriba + sin(desfaseFase + x * 0.04) * amplitudDinamica;
    vertex(x, y); // Añade el punto actual al trazado geométrico
  }
  endShape(); // Finaliza y dibuja la línea continua
  desfaseFase += 0.08; // Incrementa la fase para desplazar la onda en el siguiente fotograma


  // =========================================================================
  // --- PARTE MEDIAL: OSCILOSCOPIO DIGITAL (ADC 12 BITS) ---
  // =========================================================================
  dibujarMarco("OSCILOSCOPIO DIGITAL RECEPTOR (HISTORIAL ADC - 12 BITS)", height/2 - 50, height/2 + 130);
  
  int bits = 12;           // Definición de la resolución del cuantificador (12 bits)
  int niveles = int(pow(2, bits)); // Calcula los niveles de cuantificación de un ADC (2^12 = 4096 niveles, de 0 a 4055)
  int offsetX = 50;        // Margen izquierdo para alinear el dibujo de la gráfica
  
  // --- Dibujar línea discontinua del umbral en el ADC ---
  stroke(255, 150, 0, 100); // Color naranja con transparencia (alfa = 100)
  strokeWeight(1);
  // Convierte el valor de umbral de dB (0-100) al plano de píxeles correspondiente en el ADC
  float yUmbralADC = map(umbralDisparo, 0, 100, centroAbajo + 80, centroAbajo - 80);
  for (int i = 50; i < width - 50; i += 10) {
    line(i, yUmbralADC, i + 5, yUmbralADC); // Dibuja segmentos de 5 píxeles espaciados para hacer el efecto discontinuo
  }

  // =========================================================================
  // --- PARTE INFERIOR: REPRESENTACIÓN LÓGICA BINARIA (0 y 1) ---
  // =========================================================================
  stroke(100); // Color gris para la línea divisoria
  strokeWeight(1);
  line(0, height/2 + 130, width, height/2 + 130); // Línea que separa la gráfica ADC de la binaria
  
  fill(150);
  textSize(12);
  text("REPRESENTACIÓN LÓGICA (0 / 1)", 20, height/2 + 155);
  text("1 -", offsetX - 20, centroBinario - 25); // Posición visual del nivel alto (1)
  text("0 -", offsetX - 20, centroBinario + 25); // Posición visual del nivel bajo (0)

  // =========================================================================
  // --- BUCLE DE DIBUJO PARA AMBAS GRÁFICAS TEMPORALES (ADC Y BINARIA) ---
  // =========================================================================
  for (int i = 0; i < maxMuestras - 1; i++) {
    float volActual = historialVolumen[i];     // Valor en el instante de tiempo actual (i)
    float volSiguiente = historialVolumen[i+1]; // Valor en el siguiente instante de tiempo (i+1)
    
    // Calcula la coordenada X en píxeles para el punto actual y el siguiente
    float x1 = offsetX + (i * muestreoX);
    float x2 = offsetX + ((i + 1) * muestreoX);
    
    // Genera un gradiente dinámico: si el volumen está en 0 es verde, si está en 100 es rojo
    float r = map(volActual, 0, 100, 0, 255);
    float g = map(volActual, 0, 100, 255, 0);
    
    // ---------------------------------------------------------------------
    // RENDERIZADO DEL ADC (Simulación de pasos discretos / Escalones)
    // ---------------------------------------------------------------------
    // Convierte el valor de dB (0-100) a un número entero discreto entre 0 y 4095 (cuantificación)
    int valorDigital = int(map(volActual, 0, 100, 0, niveles - 1));
    // Convierte ese número entero digital a su posición correspondiente en píxeles en la pantalla
    float yDigital = map(valorDigital, 0, niveles - 1, centroAbajo + 80, centroAbajo - 80);
    
    stroke(r, g, 50); // Aplica el color del gradiente
    strokeWeight(2);
    line(x1, yDigital, x2, yDigital); // Dibuja la línea horizontal del nivel actual (retención de orden cero)
    
    // Conecta de manera vertical los escalones digitales para simular la señal del ADC sin diagonales
    if (i < maxMuestras - 2) {
      int nextDigital = int(map(volSiguiente, 0, 100, 0, niveles - 1));
      float yNextDigital = map(nextDigital, 0, niveles - 1, centroAbajo + 80, centroAbajo - 80);
      line(x2, yDigital, x2, yNextDigital); // Línea vertical de transición
    }
    
    fill(r, g, 0);
    ellipse(x1, yDigital, 4, 4); // Dibuja un punto para marcar explícitamente dónde se tomó la muestra
    noFill();
    
    // ---------------------------------------------------------------------
    // RENDERIZADO BINARIO (Señal Digital Cuadrada 0 y 1)
    // ---------------------------------------------------------------------
    // Si el volumen supera el umbral, asigna la altura del renglón "1", sino, la del renglón "0"
    float yBinariaActual = (volActual >= umbralDisparo) ? (centroBinario - 25) : (centroBinario + 25);
    float yBinariaSiguiente = (volSiguiente >= umbralDisparo) ? (centroBinario - 25) : (centroBinario + 25);
    
    // Asignación de colores binarios según su estado lógico
    if (volActual >= umbralDisparo) {
      stroke(0, 255, 100); // Verde brillante para un "1" lógico activo
    } else {
      stroke(100, 100, 120); // Gris apagado para un "0" lógico inactivo
    }
    strokeWeight(2.5); // Hace la señal cuadrada ligeramente más gruesa 
    
    line(x1, yBinariaActual, x2, yBinariaActual); // Dibuja el estado estable horizontal (0 o 1)
    
    // Detecta si hay un cambio de estado entre la muestra actual y la siguiente
    if (yBinariaActual != yBinariaSiguiente) {
      stroke(255, 255, 50); // Cambia el color a amarillo para resaltar el flanco
      line(x2, yBinariaActual, x2, yBinariaSiguiente); // Dibuja la línea vertical del flanco de subida o de bajada
    }
  }
  
  // PANEL DE TEXTO DE INFORMACIÓN ---
  fill(255); // Color blanco para las letras
  textSize(14);
  textAlign(RIGHT);
  // Muestra los dB en tiempo real con 2 decimales y el umbral configurado actual
  text("Actual: " + nf(volumenInterp, 0, 2) + " dB (Umbral: " + int(umbralDisparo) + " dB)", width - 50, height - 20);
}

// Función auxiliar para dibujar de manera automatizada las cajas contenedoras y los títulos de sección
void dibujarMarco(String titulo, float yMin, float yMax) {
  stroke(60); // Gris oscuro para los límites del recuadro
  strokeWeight(1);
  line(0, yMax, width, yMax); // Dibuja la línea divisoria horizontal inferior
  
  fill(200); // Color gris claro para el texto del título
  textSize(14);
  textAlign(LEFT);
  text(titulo, 20, yMin + 30); // Posiciona el título en la esquina superior izquierda de su sección
}

// =========================================================================
// --- CONTROLADOR DE EVENTOS SERIALES (INTERRUPCIÓN EN SEGUNDO PLANO) ---
// =========================================================================
void serialEvent(Serial puerto) {
  try {
    String dato = puerto.readStringUntil('\n'); // Lee los bytes del buffer hasta encontrar el salto de línea
    if (dato != null) {
      dato = trim(dato); // Elimina caracteres invisibles como espacios extras, retornos de carro (\r) o tabuladores
      
      String[] partes = splitTokens(dato, " :"); // Fragmenta la cadena de texto usando espacios o dos puntos como separadores
      String ultimoValor = partes[partes.length - 1]; // Obtiene la última posición del array, donde debería estar el número de los dB
      float lecturaOK = float(ultimoValor); // Intenta convertir ese fragmento de texto a tipo numérico decimal
      
      if (!Float.isNaN(lecturaOK)) { // Verifica que la conversión haya sido exitosa (que no sea un "Not a Number")
        volumenReal = lecturaOK; // Actualiza el valor real que usará el programa
        
        // --- DESPLAZAMIENTO DEL HISTORIAL (Efecto cola / FIFO) ---
        // Desplaza todos los datos del array una posición hacia la izquierda, perdiendo la muestra más vieja (índice 0)
        for (int i = 0; i < maxMuestras - 1; i++) {
          historialVolumen[i] = historialVolumen[i+1];
        }
        // Coloca la nueva lectura del sensor en la última posición disponible del historial (índice 49)
        historialVolumen[maxMuestras - 1] = volumenReal;
      }
    }
  } catch(Exception e) {
    // Si la lectura falla por ruido electromagnético o desconexión, captura el error para que la interfaz gráfica no se congele
    println("ERROR EN SERIAL: " + e.getMessage());
  }
}
