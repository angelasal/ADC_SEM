import processing.serial.*;

Serial puertoSerie;

float volumenReal = 0;
float volumenInterp = 0; 

// --- HISTORIAL PARA EL OSCILOSCOPIO ---
int maxMuestras = 50;       
float[] historialVolumen;   
int muestreoX = 14;         

float desfaseFase = 0; 

// --- CONFIGURACIÓN DEL UMBRAL ---
float umbralDisparo = 50.0; // A partir de 50 dB se activa el "1" lógico

void setup() {
  size(800, 650); // Hacemos la ventana un pelín más alta (650px) para que quepa todo cómodo
  
  historialVolumen = new float[maxMuestras];
  for (int i = 0; i < maxMuestras; i++) {
    historialVolumen[i] = 0;
  }
  
  println("Puertos disponibles:");
  printArray(Serial.list());
  puertoSerie = new Serial(this, "COM4", 115200); 
  delay(2000);
  puertoSerie.bufferUntil('\n');
  println("CONECTADO");
}

void draw() {
  background(20);
  
  volumenInterp = lerp(volumenInterp, volumenReal, 0.15); 
  volumenInterp = constrain(volumenInterp, 0, 100);

  float centroArriba = height * 0.22; 
  float centroAbajo = height * 0.60;  // Subimos un poco el centro del ADC
  float centroBinario = height * 0.88; // Nueva zona para la línea de 0 y 1
  
  // --- PARTE SUPERIOR: SEÑAL ANALÓGICA ---
  dibujarMarco("SEÑAL ANALÓGICA DE ENTRADA (FRECUENCIA SIMULADA)", 0, height/2 - 50);
  float amplitudDinamica = map(volumenInterp, 0, 100, 0, 100); 
  
  stroke(50, 200, 255); 
  strokeWeight(2);
  noFill();
  beginShape();
  for (int x = 50; x < width - 50; x++) {
    float y = centroArriba + sin(desfaseFase + x * 0.04) * amplitudDinamica;
    vertex(x, y);
  }
  endShape();
  desfaseFase += 0.08; 


  // --- PARTE MEDIAL: OSCILOSCOPIO DIGITAL (ADC 12 BITS) ---
  dibujarMarco("OSCILOSCOPIO DIGITAL RECEPTOR (HISTORIAL ADC - 12 BITS)", height/2 - 50, height/2 + 130);
  
  int bits = 12; 
  int niveles = int(pow(2, bits)); 
  int offsetX = 50; 
  
  // Dibujar línea discontinua del umbral en el ADC para saber cuándo cruzamos
  stroke(255, 150, 0, 100); // Naranja semitransparente
  strokeWeight(1);
  float yUmbralADC = map(umbralDisparo, 0, 100, centroAbajo + 80, centroAbajo - 80);
  for (int i = 50; i < width - 50; i += 10) {
    line(i, yUmbralADC, i + 5, yUmbralADC); 
  }

  // --- PARTE INFERIOR: REPRESENTACIÓN LÓGICA BINARIA (0 y 1) ---
  stroke(100);
  strokeWeight(1);
  line(0, height/2 + 130, width, height/2 + 130); // Separador visual para los bits
  
  fill(150);
  textSize(12);
  text("REPRESENTACIÓN LÓGICA (0 / 1)", 20, height/2 + 155);
  text("1 -", offsetX - 20, centroBinario - 25);
  text("0 -", offsetX - 20, centroBinario + 25);

  // --- BUCLE DE DIBUJO PARA AMBAS GRÁFICAS TEMPORALES ---
  for (int i = 0; i < maxMuestras - 1; i++) {
    float volActual = historialVolumen[i];
    float volSiguiente = historialVolumen[i+1];
    
    float x1 = offsetX + (i * muestreoX);
    float x2 = offsetX + ((i + 1) * muestreoX);
    
    // Color dinámico según dB
    float r = map(volActual, 0, 100, 0, 255);
    float g = map(volActual, 0, 100, 255, 0);
    
    // ==========================================
    // RENDERIZADO DEL ADC (Mismo de antes)
    // ==========================================
    int valorDigital = int(map(volActual, 0, 100, 0, niveles - 1));
    float yDigital = map(valorDigital, 0, niveles - 1, centroAbajo + 80, centroAbajo - 80);
    
    stroke(r, g, 50);
    strokeWeight(2);
    line(x1, yDigital, x2, yDigital);
    
    if (i < maxMuestras - 2) {
      int nextDigital = int(map(volSiguiente, 0, 100, 0, niveles - 1));
      float yNextDigital = map(nextDigital, 0, niveles - 1, centroAbajo + 80, centroAbajo - 80);
      line(x2, yDigital, x2, yNextDigital);
    }
    fill(r, g, 0);
    ellipse(x1, yDigital, 4, 4);
    noFill();
    
    // ==========================================
    // NUEVO: RENDERIZADO BINARIO (0 y 1)
    // ==========================================
    // Evaluamos el estado actual y el siguiente basándonos en el umbral
    float yBinariaActual = (volActual >= umbralDisparo) ? (centroBinario - 25) : (centroBinario + 25);
    float yBinariaSiguiente = (volSiguiente >= umbralDisparo) ? (centroBinario - 25) : (centroBinario + 25);
    
    // Si está en ALTO (1) la pintamos verde brillante, si está en BAJO (0) azul oscuro/gris
    if (volActual >= umbralDisparo) {
      stroke(0, 255, 100); // Verde lógico
    } else {
      stroke(100, 100, 120); // Gris azulado
    }
    strokeWeight(2.5); // Un poco más gruesa para que se distinga bien
    
    // Línea horizontal del estado actual
    line(x1, yBinariaActual, x2, yBinariaActual);
    
    // Línea vertical si hay un cambio de estado (flanco de subida o bajada)
    if (yBinariaActual != yBinariaSiguiente) {
      stroke(255, 255, 50); // Destello amarillo en el cambio de flanco
      line(x2, yBinariaActual, x2, yBinariaSiguiente);
    }
  }
  
  // HUD e información de texto
  fill(255);
  textSize(14);
  textAlign(RIGHT);
  text("Actual: " + nf(volumenInterp, 0, 2) + " dB (Umbral: " + int(umbralDisparo) + " dB)", width - 50, height - 20);
}

void dibujarMarco(String titulo, float yMin, float yMax) {
  stroke(60);
  strokeWeight(1);
  line(0, yMax, width, yMax); 
  
  fill(200);
  textSize(14);
  textAlign(LEFT);
  text(titulo, 20, yMin + 30);
}

// ===============================
// SERIAL EVENT 
// ===============================
void serialEvent(Serial puerto) {
  try {
    String dato = puerto.readStringUntil('\n');
    if (dato != null) {
      dato = trim(dato);
      String[] partes = splitTokens(dato, " :");
      String ultimoValor = partes[partes.length - 1];
      float lecturaOK = float(ultimoValor);
      
      if (!Float.isNaN(lecturaOK)) {
        volumenReal = lecturaOK;
        
        for (int i = 0; i < maxMuestras - 1; i++) {
          historialVolumen[i] = historialVolumen[i+1];
        }
        historialVolumen[maxMuestras - 1] = volumenReal;
      }
    }
  } catch(Exception e) {
    println("ERROR EN SERIAL: " + e.getMessage());
  }
}
