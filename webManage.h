// ====================================================================
//                      MANEJADORES HTTP  
// ====================================================================

// --- Utilidad para conversión de color HEX a uint32_t ---
uint32_t parseHexColor(String hex);

// --- Rutas del Servidor ---
void handleRoot();
void handleConfig();
void handleSave();
void handleSaveConfig();
void handleRestart();

// --- Manejadores OTA (Se mantiene la versión de la UI anterior) ---
void handleOTA();
void handleOTAUpload();
void notFound();


// ====================================================================
//                    MANEJADORES DE GESTIÓN DE ARCHIVOS
// ====================================================================

// --- MANEJO DE SUBIDA DE ARCHIVOS (Optimizado para Multi-Upload) --- 

void handleFileUpload();

// --- MANEJO DE BORRADO DE ARCHIVOS Y CARPETAS ---

void handleFileDelete() ;

// --- MANEJO DE CREACIÓN DE CARPETAS ---

void handleCreateDir() ;

// --- MANEJO DE ARCHIVOS (VISUALIZACIÓN Y NAVEGACIÓN) ---

void handleFileManager() ;