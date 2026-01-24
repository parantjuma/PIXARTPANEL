// ====================================================================
//                     Funciones de Modos de Reproducción
// ====================================================================

// Variables globales necesarias para el loop
extern unsigned long lastFrameTime; // Declarar esta variable en el ámbito global si aún no existe
// extern WebServer server; // Asumiendo que el objeto server es global


// MUESTRA MENSAJE EN PANTALLA
void mostrarMensaje(const char* mensaje, uint16_t color); 
// EJECUTA MODO GIF
void ejecutarModoGif();
// EJECUTA MODO TEXTO
void ejecutarModoTexto();
// EJECUTA MODO RELOJ
void ejecutarModoReloj();
// EJECUTA MODO INFO
void ejecutarModoInfo();

