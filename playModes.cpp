#include "config.h"
#include "gifPanelDraw.h"
#include "files.h"
#include "playModes.h"


// --- 2. Funciones de Utilidad de Visualización ---
void mostrarMensaje(const char* mensaje, uint16_t color = 0xF800 /* Rojo */) {
    if (!display) return;
    Serial.printf("mostrarMensaje [%s] \n",mensaje);
    // Usar el color configurado para el texto deslizante
    uint16_t colorTexto = display->color565(
        (config.slidingTextColor >> 16) & 0xFF,
        (config.slidingTextColor >> 8) & 0xFF,
        config.slidingTextColor & 0xFF
    ); 
    display->setTextSize(1); 
    display->setTextWrap(false); 
    display->setTextColor(colorTexto);     
    
    display->fillScreen(display->color565(0, 0, 0));
    display->setCursor(0, MATRIX_HEIGHT / 2 - 4);
    display->print(mensaje);
    display->flipDMABuffer();
}


// ====================================================================
//                    EJECUCION MODO GIF
// ====================================================================

void ejecutarModoGif() {
    if (numGifs==0) return;

    if (!display) return; 
    
    // Verificación de la SD y lista de archivos
    //if (!sdMontada || archivosGIF.empty()) {
    if (!sdMontada || isGifOffsetEmpty()) {
        mostrarMensaje(!sdMontada ? "SD ERROR" : "NO GIFS");
        delay(200);
        return;
    }

    // Rotación de la lista de GIFs
    //if (currentGifIndex >= archivosGIF.size()) {
    if (currentGifIndex >= gifOffsetSize()) {
        //listarArchivosGif();  No creo que sea necesario cargar la lista de gif si ha llegado al final de la lista
        currentGifIndex = 0;
        //if (archivosGIF.empty()) return;
        if (isGifOffsetEmpty()) return;
    }
    
    //String gifPath = archivosGIF[currentGifIndex]; 
    String gifPath = getGifPathByIndex(currentGifIndex); 
    
    // Bucle de repetición del GIF (config.gifRepeats)
    for (int rep = 0; rep < config.gifRepeats; ++rep) { 
        if (config.playMode != 0) return; 
        
        if (gif.open(gifPath.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
            
            // 🛑 CÁLCULO FINAL DE CENTRADO (Universal)
            // Calculamos el offset de centrado usando las dimensiones reales de la matriz (128x32).
            x_offset = (128 - gif.getCanvasWidth()) / 2; 
            y_offset = (32 - gif.getCanvasHeight()) / 2; 
            
            // Este offset (x_offset, y_offset) será utilizado en GIFDraw
            // para posicionar el GIF. La compensación de +128 debe permanecer fija en GIFDraw.

            // DEBUG: para verificar el offset.
            Serial.printf("GIF: %s | Ancho GIF: %d | Offset X calculado: %d\n", gifPath.c_str(), gif.getCanvasWidth(), x_offset);

            // Limpiamos la pantalla
            display->clearScreen(); 

            int delayMs;
            
            // Bucle principal de reproducción de frames
            while (gif.playFrame(true, &delayMs) && config.playMode == 0) {
                
                // Manejo de WebServer y espera no bloqueante
                server.handleClient(); 
                
                unsigned long targetTime = millis() + delayMs;
                while (millis() < targetTime) {
                    server.handleClient(); 
                    yield(); 
                }
                
                // Si el modo cambia durante la espera, salimos del bucle
                if (config.playMode != 0) break;
            }
            gif.close();
            
        } else {
            Serial.printf("Error abriendo GIF: %s\n", gifPath.c_str());
            mostrarMensaje("Error GIF", display->color565(255, 255, 0));
            
            unsigned long start = millis();
            while (millis() - start < 1000) {
                 server.handleClient();
                 yield();
            }
        }
    }
    
    if(config.randomMode)
    {
        // La elección de la posición del array será aleatoria entre todos los valores posibles
        Serial.println("Selección de gif aleatorio ");
        currentGifIndex=random(0, gifOffsetSize());
    }
    else
    {
        // Caso contrario elegimos la siguiente imagen
        Serial.println("Selección de gif secuencial");
        currentGifIndex++; 
       if (currentGifIndex >= gifOffsetSize()) {
            currentGifIndex = 0; // volver al inicio
        }
    }
    
    // no usa random mode?config.randomMode
    Serial.printf("Siguiente GifIndex:%d randomMode[%d] \n", currentGifIndex,config.randomMode);
}

// ====================================================================
//                     EJECUCIÓN MODO TEXTO
// ====================================================================
void ejecutarModoTexto() {
    if (!display) return; 
    // Usar el color configurado para el texto deslizante
    uint16_t colorTexto = display->color565(
        (config.slidingTextColor >> 16) & 0xFF,
        (config.slidingTextColor >> 8) & 0xFF,
        config.slidingTextColor & 0xFF
    ); 
    display->setTextSize(1); 
    display->setTextWrap(false); 
    display->setTextColor(colorTexto); 

    if (millis() - lastScrollTime > config.textSpeed) {
        lastScrollTime = millis(); 
        xPosMarquesina--;

        int anchoTexto = config.slidingText.length() * 6; 

        if (xPosMarquesina < -anchoTexto) {
            xPosMarquesina = display->width(); 
        }

        display->fillScreen(display->color565(0, 0, 0));
        display->setCursor(xPosMarquesina, MATRIX_HEIGHT / 2 - 4);
        display->print(config.slidingText);
        display->flipDMABuffer(); 
    }
}

// ====================================================================
//                     INFORMACION MODO RELOJ
// ====================================================================
void ejecutarModoReloj() {
    if (!display) return; 
    struct tm timeinfo; 
    if(!getLocalTime(&timeinfo)){
        mostrarMensaje("NTP ERROR", display->color565(0, 0, 255));
        delay(100);
        return; 
    }
    
    uint16_t colorReloj = display->color565(
        (config.clockColor >> 16) & 0xFF,
        (config.clockColor >> 8) & 0xFF,
        config.clockColor & 0xFF
    ); 
    char timeFormat[10]; 
    if (config.format24h) {
        strcpy(timeFormat, config.showSeconds ? "%H:%M:%S" : "%H:%M"); 
    } else {
        // NOTA: EL FORMATO ORIGINAL DE SU CÓDIGO INCLUÍA %p (AM/PM), 
        // lo que ocupa más espacio. Si sigue habiendo cortes, elimine "%p"
        strcpy(timeFormat, config.showSeconds ? "%I:%M:%S %p" : "%I:%M %p"); 
    }

    char timeString[20]; 
    strftime(timeString, sizeof(timeString), timeFormat, &timeinfo); 
    
    char dateString[11]; 
    strftime(dateString, sizeof(dateString), "%d/%m/%Y", &timeinfo); 

    display->fillScreen(display->color565(0, 0, 0)); 
    display->setTextColor(colorReloj); 
    
    // --- CÁLCULO Y DIBUJO DE LA HORA (TAMAÑO 2) ---
    display->setTextSize(2); 
    
    // 1. Cálculo del centrado horizontal (X)
    int xHora = (display->width() - (strlen(timeString) * 12)) / 2;
    
    // 2. APLICACIÓN DE LA CORRECCIÓN: EMPUJAR 65px A LA IZQUIERDA
    xHora += 65; 
    
    // 3. Dibujo de la hora
    // Aseguramos que la posición X no sea negativa y la posición Y sea 8
    display->setCursor(xHora > 0 ? xHora : 0, 8); 
    display->print(timeString); 
    
    // --- DIBUJO DE LA FECHA (MARQUESINA - TAMAÑO 1) ---
    display->setTextSize(1); 
    
    // La variable lastScrollTime debe ser global
    extern unsigned long lastScrollTime; 
    
    if (config.showDate) {
        // xPosFecha debe ser una variable global para que la marquesina funcione
        extern int xPosMarquesina; 
        int anchoFecha = strlen(dateString) * 6;

        // Lógica de desplazamiento (usa config.textSpeed de la configuración)
        if (millis() - lastScrollTime > config.textSpeed) { 
            lastScrollTime = millis(); 
            xPosMarquesina--;
            if (xPosMarquesina < -anchoFecha) {
                xPosMarquesina = display->width(); 
            }
        }
        // El cursor de la fecha se ajusta a la posición de la marquesina
        display->setCursor(xPosMarquesina, MATRIX_HEIGHT - 8); 
        display->print(dateString);
    }
    
    display->flipDMABuffer(); 
    delay(50);
}