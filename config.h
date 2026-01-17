// ====================================================================
//                            PIXART PANEL 
// ====================================================================

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <Update.h>       // Librería para la funcionalidad OTA
#include <DNSServer.h>    // Requerido por WiFiManager
#include <WiFiManager.h>  // Librería para gestión WiFi

// --- LIBRERÍAS DE HARDWARE ---
#include "SD.h"           // Gestión de la Micro SD
#include "AnimatedGIF.h"  // Decodificador de GIFs (Sintaxis antigua/fork)
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // Gestión del panel LED

// ====================================================================
//                          CONSTANTES & FIRMWARE
// ====================================================================
#define FIRMWARE_VERSION "2.0.9.MEMOPT_PAG_1.16" // MODIFICADO: Nueva versión del firmware
#define FIRMWARE_DATE "11_1_2026_16_53" // Fecha de la version
#define PREF_NAMESPACE "pixel_config"
#define WIFI_DEFAULT "PixelArt Panel Wifi"  // IP 192.168.4.1
#define DEVICE_NAME_DEFAULT "PixelArtPanel"
#define TZ_STRING_SPAIN "CET-1CEST,M3.5.0,M10.5.0/3" // Cadena TZ por defecto segura
#define GIFS_BASE_PATH "/pa_gifs" // Directorio base para los GIFs
#define LOGO_BASE_PATH "/logo"    // Directorio base para los GIFs logos cuando se active el modo Logo
#define GIF_CACHE_FILE "/pa_gif_cache.txt" // Archivo para guardar el índice de GIFs
#define GIF_CACHE_SIG "/pa_gif_cache.sig" // Nuevo archivo de firma
#define M5STACK_SD SD


// --- NUEVA DEFINICIÓN DE PINES HUB75 (según tu propuesta) ---
// Estos pines usan una combinación segura fuera del bus SD/Flash.
#define CLK_PIN       16
#define OE_PIN        15
#define LAT_PIN       4
#define A_PIN         33
#define B_PIN         32
#define C_PIN         22
#define D_PIN         17
#define E_PIN         -1 // Desactivado para paneles 64x32 (escaneo 1/16)
#define R1_PIN        25
#define G1_PIN        26
#define B1_PIN        27
#define R2_PIN        14
#define G2_PIN        12
#define B2_PIN        13

// --- NUEVA DEFINICIÓN DE PINES SPI SD (Nativos VSPI) ---
// Estos pines usan el bus VSPI de hardware para mayor velocidad.
#define SD_CS_PIN     5   
#define VSPI_MISO     19
#define VSPI_MOSI     23
#define VSPI_SCLK     18

// Definiciones de panel
constexpr int PANEL_RES_X = 64; 
constexpr int PANEL_RES_Y = 32;
#define MATRIX_HEIGHT PANEL_RES_Y 

// Variables globales para el sistema
extern WebServer server;
extern Preferences preferences;
extern WiFiManager wm;
extern AnimatedGIF gif;
// Puntero a la Matriz 
extern MatrixPanel_I2S_DMA* display; 
extern File currentFile; 
extern bool DNSCONFIG;
extern bool showIPOnlyOnce;     // Si esta a true activa el modo info solo una pasada
extern bool modoAP;             // True si estamos en modo AP
extern size_t showIPOnlyOnceCount; // Establecemos el numero de veces que se muestra la ip al iniciar 

// Variables de estado y reproducción
extern bool sdMontada;
extern std::vector<String> archivosGIF;     // ELIMINAR!! tras optimizacion codigo
extern int currentGifIndex; 
extern int x_offset; // Offset para centrado GIF
extern int y_offset;

// Variables para obtener rutas a gif
#define GIF_OFFSETS_BLOCK_SIZE 16   // Hacemos uso de la carga de gif por bloques para reducir el tamaño de memoria necesario.
                                    // solo apuntamos al inicio de cada bloque, para agilizar la busqueda de la fila, siendo necesarias
                                    // en el peor de los casos 16 busquedas desde el offset guardado.

extern uint32_t* gifOffsets;         // Array fijo de offsets al fichero cache GIF_CACHE_FILE (un offset cada GIF_OFFSETS_BLOCK_SIZE rutas)
extern size_t numOffsetBlocks;            // numero de posiciones del array anterior
extern size_t numGifs;               // número de elementos en el array gifOffsets

constexpr uint16_t ITEMS_PER_PAGE = 20; // Maximo numero de gif por pagina mostrados por el gestor web 

// Variables de modos
extern int xPosMarquesina;
extern unsigned long lastScrollTime;
constexpr char* ntpServer = "pool.ntp.org";

// --- GESTIÓN DE RUTA SD ---
extern String currentPath; // Ruta actual para el administrador de archivos
extern File fsUploadFile; // Variable global para la subida de archivos
extern File FSGifFile;    // Variable global para el manejo de archivos GIF (CRÍTICA)


// ====================================================================
//                          ESTRUCTURA DE DATOS
// ====================================================================

struct Config {
    // 1. Controles de Reproducción
    int brightness = 150;
    int playMode = 0; // 0: GIFs, 1: Texto, 2: Reloj 3:Info (ip)
    String slidingText = "Retro Pixel LED v" + String(FIRMWARE_VERSION);
    int textSpeed = 50;
    int gifRepeats = 1;
    bool randomMode = false;
    bool showLogo = false;      // Obtiene (si lo hay) un logo de la carpeta /logo
    bool logoFrecuence = 0;     // Si showLogo=true Numero de gif aleatorios hasta que se vuelva a mostrar un logo, 
                                // si es 0 solo se mostrarán los logos sin ningun gif aleatorio de la coleccion.
    bool batoceraLink = false;  // Permite dar prioridad a los gif de Batocera sobre los gif aleatorios cuando hay uno activo
    // activeFolders (vector) para uso en runtime, activeFolders_str para Preferences
    std::vector<String> activeFolders; 
    String activeFolders_str = "/GIFS";
    // 2. Configuración de Hora/Fecha
    String timeZone = TZ_STRING_SPAIN; 
    bool format24h = true;
    // 3. Configuración del Modo Reloj
    uint32_t clockColor = 0xFF0000;
    // Rojo (por defecto)
    bool showSeconds = true;
    bool showDate = false;
    // 4. Configuración del Modo Texto Deslizante
    uint32_t slidingTextColor = 0x00FF00;
    // Verde (por defecto)
    // 5. Configuración de Hardware/Sistema
    int panelChain = 2; 
    // "Número de Paneles LED (en Cadena)"
    char device_name[40] = {0};
};
extern Config config;

extern std::vector<String> allFolders; // Lista de todas las carpetas
