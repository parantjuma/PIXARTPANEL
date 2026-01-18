#include "config.h"

// Variables globales para el sistema
WebServer server(80);
Preferences preferences;
WiFiManager wm;
AnimatedGIF gif;
// Puntero a la Matriz 
MatrixPanel_I2S_DMA *display = nullptr; 
File currentFile; 

// Variables de estado y reproducción
bool sdMontada = false;
//std::vector<String> archivosGIF;     // ELIMINAR!! tras optimizacion codigo
std::vector<String> gifLogos;        // Rutas de GIFs dentro de /logos (se carga en setup)
int currentGifIndex = 0; 
int x_offset = 0; // Offset para centrado GIF
int y_offset = 0;

        // nuevas variables 
uint32_t* gifOffsets = nullptr; // Array fijo de offsets al fichero cache GIF_CACHE_FILE donde obtener rutas
size_t numOffsetBlocks=0;
size_t numGifs = 0;             // número de elementos en el array gifOffsets
bool DNSCONFIG=false;


// Variables de modos
int xPosMarquesina = 0;
unsigned long lastScrollTime = 0;

// --- GESTIÓN DE RUTA SD ---
String currentPath = "/"; // Ruta actual para el administrador de archivos
File fsUploadFile; // Variable global para la subida de archivos
File FSGifFile;    // Variable global para el manejo de archivos GIF (CRÍTICA)

Config config;

std::vector<String> allFolders; // Lista de todas las carpetas

bool showIPOnlyOnce=false;
size_t showIPOnlyOnceCount=2;
bool modoAP=false;