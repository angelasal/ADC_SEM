// --- VARIABLES GLOBALES ---
float volumenInterp = 0; // Almacena el valor suavizado que vemos en pantalla

void setup() {
  size(400, 600); // Crea una ventana de 400px de ancho por 600px de alto
}

void draw() {
  background(20); // Pinta el fondo de gris casi negro en cada frame para "limpiar" el rastro anterior

  // 1. OBTENCIÓN DEL DATO (Simulación)
  // Aquí guardarmemos el valor que se recoja de mqtt
  float volumenReal = random(0, 100); 

  // 2. SUAVIZADO (INTERPOLACIÓN LINEAL)
  // lerp hace que la barra se mueva de forma fluida. 
  // 0.15 significa que la barra avanza un 15% hacia el objetivo en cada frame.
  volumenInterp = lerp(volumenInterp, volumenReal, 0.15);

  // 3. MAPEO DE COORDENADAS
  // Convierte el volumen (0 a 100) en una posición vertical de píxeles (550 a 50).
  float yPos = map(volumenInterp, 0, 100, height - 50, 50); //como parametros recibe (dato, datomin, datomax, base, altura)

  // 4. DIBUJO DE LA INTERFAZ ESTÁTICA
  dibujarInterfaz();
  
  // 5. CÁLCULO DE COLOR DINÁMICO
  // Si el volumen sube, el rojo (r) aumenta y el verde (g) disminuye.
  float r = map(volumenInterp, 0, 100, 0, 255); //como parametros recibe (dato, datomax, datomin, codigo colores (en dos coordenadas))
  float g = map(volumenInterp, 0, 100, 255, 0);
  fill(r, g, 0); // Aplica el color calculado al relleno de la barra
  noStroke();    // Quita el borde a la barra de color, esta función mejora la estética únicamente
  
  // 6. DIBUJO DE LA BARRA DE VOLUMEN
  // rectMode(CORNERS) permite dibujar definiendo (x1, y1, x2, y2), entonces se utiliza para dibujar un rectángulo (o lo que sea con 4 lados)
  rectMode(CORNERS);
  // Dibujamos desde el ancho 150 al 250. La base es fija (height-50) y el tope es yPos.
  rect(150, height - 50, 250, yPos); //se le pasa (x1(base izq), y1(altura mín), x2, (base der), y2(altura max))
  
  // 7. TEXTO INFORMATIVO
  fill(255); // Color blanco para el texto
  textAlign(CENTER); //lo colocamos en el centro
  textSize(16); //le pasamos como másximo 16 caracteres
  // nf() formatea el número para mostrar solo 2 decimales
  text(nf(volumenInterp, 0, 2) + " dB", width/2, height - 20); //la primera parte le especifica que del número coja solo 2 decimales y lo colacamos en la coordenada que le pasamos (al centro debajo de la barra)
}

// Función auxiliar para no ensuciar el draw() con líneas decorativas
void dibujarInterfaz() {
  stroke(100);       // Color gris para las líneas de la escala
  strokeWeight(2);   // Grosor de las líneas
  noFill();          // El marco del contenedor no tiene fondo
  
  // Dibujamos el rectángulo que sirve de "contenedor" vacío
  rectMode(CORNERS);
  rect(150, height - 50, 250, 50); //con la misma lógica de antes 
  
  // Bucle para crear las marcas de la escala (0, 10, 20... 100)
  for (int i = 0; i <= 10; i++) {
    // Calculamos la altura de cada marca basándonos en el índice i
    float yTick = map(i, 0, 10, height - 50, 50);
    
    stroke(100);
    line(140, yTick, 150, yTick); // Dibuja la pequeña raya horizontal
    
    fill(150);       // Color gris claro para los números
    textAlign(RIGHT);
    textSize(10);
    // Multiplicamos i por 10 para mostrar la escala de 0 a 100
    text(i * 10, 135, yTick + 4);
  }
}
