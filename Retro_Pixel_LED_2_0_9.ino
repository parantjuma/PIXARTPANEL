#include <WiFi.h>
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
#define FIRMWARE_VERSION "2.0.9" // MODIFICADO: Nueva versión del firmware
#define PREF_NAMESPACE "pixel_config"
#define DEVICE_NAME_DEFAULT "RetroPixel-Default"
#define TZ_STRING_SPAIN "CET-1CEST,M3.5.0,M10.5.0/3" // Cadena TZ por defecto segura
#define GIFS_BASE_PATH "/gifs" // Directorio base para los GIFs
#define GIF_CACHE_FILE "/gif_cache.txt" // Archivo para guardar el índice de GIFs
#define GIF_CACHE_SIG "/gif_cache.sig" // Nuevo archivo de firma
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
const int PANEL_RES_X = 64; 
const int PANEL_RES_Y = 32;
#define MATRIX_HEIGHT PANEL_RES_Y 

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
std::vector<String> archivosGIF; 
int currentGifIndex = 0; 
int x_offset = 0; // Offset para centrado GIF
int y_offset = 0;

// Variables de modos
int xPosMarquesina = 0;
unsigned long lastScrollTime = 0;
const char* ntpServer = "pool.ntp.org";

// --- GESTIÓN DE RUTA SD ---
String currentPath = "/"; // Ruta actual para el administrador de archivos
File fsUploadFile; // Variable global para la subida de archivos
File FSGifFile;    // Variable global para el manejo de archivos GIF (CRÍTICA)
//std::vector<String> selectedGifFolders; // Variable global para guardar las carpetas que el usuario ha seleccionado para reproducir

// ====================================================================
//                          ESTRUCTURA DE DATOS
// ====================================================================

struct Config {
    // 1. Controles de Reproducción
    int brightness = 150;
    int playMode = 0; // 0: GIFs, 1: Texto, 2: Reloj
    String slidingText = "Retro Pixel LED v" + String(FIRMWARE_VERSION);
    int textSpeed = 50;
    int gifRepeats = 1;
    bool randomMode = false;
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
Config config;

std::vector<String> allFolders;
// Lista de todas las carpetas

// Declaraciones de funciones
void handleRoot();
void handleSave();
void handleConfig();
void handleSaveConfig();
void handleFactoryReset();
void handleRestart();
void handleOTA();
void handleOTAUpload();
void notFound();

// NUEVAS DECLARACIONES PARA GESTIÓN DE ARCHIVOS
void handleFileManager();
void handleFileUpload();
void handleFileDelete();
String fileManagerPage();

String webPage();
String configPage();
void loadConfig();
void savePlaybackConfig();
void saveSystemConfig();
void initTime();

// Funciones de visualización y control de modos
void mostrarMensaje(const char* mensaje, uint16_t color);
void listarArchivosGif();
void ejecutarModoGif();
void ejecutarModoTexto();
void ejecutarModoReloj();
void scanFolders();
// ====================================================================
//                      MANEJO DE CONFIGURACIÓN (PREFERENCES)
// ====================================================================

void loadConfig() { 
    preferences.begin(PREF_NAMESPACE, true);
// modo lectura
    
    config.brightness = preferences.getInt("brightness", 150);
    config.playMode = preferences.getInt("playMode", 0);
    config.slidingText = preferences.getString("slidingText", config.slidingText);
    config.textSpeed = preferences.getInt("textSpeed", 50);
    config.gifRepeats = preferences.getInt("gifRepeats", 1);
    config.randomMode = preferences.getBool("randomMode", false);
// 1. Cargar string serializado (compatibilidad)
    config.activeFolders_str = preferences.getString("activeFolders", "/GIFS");
// 2. Deserializar el string en el vector config.activeFolders
    config.activeFolders.clear();
    int start = 0;
    int end = config.activeFolders_str.indexOf(',');
    while (end != -1) {
        config.activeFolders.push_back(config.activeFolders_str.substring(start, end));
        start = end + 1;
        end = config.activeFolders_str.indexOf(',', start);
    }
    // Añadir el último elemento (o el único si no hay comas)
    if (start < config.activeFolders_str.length()) {
        config.activeFolders.push_back(config.activeFolders_str.substring(start));
    }
    
    // Carga de configuración de Sistema/Reloj
    config.timeZone = preferences.getString("timeZone", TZ_STRING_SPAIN);
    config.format24h = preferences.getBool("format24h", true);
    // Carga del color del reloj
    config.clockColor = preferences.getULong("clockColor", 0xFF0000);
    config.showSeconds = preferences.getBool("showSeconds", true);
    config.showDate = preferences.getBool("showDate", false);
    
    // Carga del color del texto (usando la clave corta 'slideColor' que resolvió la persistencia)
    config.slidingTextColor = preferences.getULong("slideColor", 0x00FF00);
    config.panelChain = preferences.getInt("panelChain", 2); // Usa el nuevo valor por defecto
    
    // device_name: Lectura especial para char array
    String nameStr = preferences.getString("deviceName", DEVICE_NAME_DEFAULT);
    strncpy(config.device_name, nameStr.c_str(), sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';

    preferences.end();
}

void savePlaybackConfig() { 
    preferences.begin(PREF_NAMESPACE, false);
// modo escritura
    
    preferences.putInt("brightness", config.brightness);
    preferences.putInt("playMode", config.playMode);
    preferences.putString("slidingText", config.slidingText);
    preferences.putInt("textSpeed", config.textSpeed);
    preferences.putInt("gifRepeats", config.gifRepeats);
    preferences.putBool("randomMode", config.randomMode);
    
    // Guardar carpetas (serializando de vector a String)
    String foldersToSave;
    for (size_t i = 0; i < config.activeFolders.size(); ++i) {
        foldersToSave += config.activeFolders[i];
        if (i < config.activeFolders.size() - 1) {               
            foldersToSave += ",";
        }
    }
    config.activeFolders_str = foldersToSave;
    preferences.putString("activeFolders", config.activeFolders_str);
    
    preferences.end();
}

void saveSystemConfig() { 
    preferences.begin(PREF_NAMESPACE, false); // Modo escritura
    
    preferences.putString("timeZone", config.timeZone);
    preferences.putBool("format24h", config.format24h);
    
// Configuración del Modo Reloj
    preferences.putULong("clockColor", config.clockColor);
    preferences.putBool("showSeconds", config.showSeconds);
    preferences.putBool("showDate", config.showDate);
// Color del texto deslizante (clave corta "slideColor")
    const char* newKey = "slideColor";
    preferences.putULong(newKey, config.slidingTextColor);
// Configuración de Hardware/Sistema
    preferences.putInt("panelChain", config.panelChain);
    preferences.putString("deviceName", config.device_name);
    
    preferences.end();
// Confirma la escritura en la NVS
}

// ====================================================================
//                      FUNCIÓN CRÍTICA DE REINICIO
// ====================================================================

void handleFactoryReset() {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.clear(); // Borra todas las configuraciones guardadas
    preferences.end();
    
    wm.resetSettings();
// Borra la configuración WiFi
    
    server.send(200, "text/html", "<h2>Restablecimiento Completo</h2><p>Todos los ajustes (incluida la conexión WiFi) han sido borrados. El dispositivo se reiniciará en 3 segundos e iniciará el Portal Cautivo.</p>");
    delay(3000);
    ESP.restart();
}


// ====================================================================
//                      FUNCIÓN CRÍTICA DE TIEMPO
// ====================================================================
void initTime() {
    const char* tz_to_use = TZ_STRING_SPAIN;
    if (!config.timeZone.isEmpty() && config.timeZone.length() >= 4) {
        tz_to_use = config.timeZone.c_str();
    } else {
        Serial.println("ADVERTENCIA: Zona horaria vacía/inválida. Usando valor por defecto seguro.");
    }
    
    configTzTime(tz_to_use, ntpServer); 
    
    Serial.printf("Configurando NTP con servidor: %s, Zona Horaria: %s\n", ntpServer, tz_to_use);
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < 10000 && attempts < 10) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        attempts++;
    }
    Serial.println("");
}

// ====================================================================
//                      MANEJADORES HTTP Y WEB
// ====================================================================

// --- Utilidad para conversión de color HEX a uint32_t ---
uint32_t parseHexColor(String hex) {
    if (hex.startsWith("#")) hex.remove(0, 1);
// Usar 0x00FF00 (Verde) como valor por defecto si la longitud es incorrecta
    if (hex.length() != 6) return 0x00FF00;
    return strtoul(hex.c_str(), NULL, 16);
}

// --- Rutas del Servidor ---
void handleRoot() { server.send(200, "text/html", webPage());
}
void handleConfig() { server.send(200, "text/html", configPage()); }

void handleSave() { 
    if (server.hasArg("b")) config.brightness = server.arg("b").toInt();
    if (server.hasArg("pm")) config.playMode = server.arg("pm").toInt();
    if (server.hasArg("st")) config.slidingText = server.arg("st");
    if (server.hasArg("ts")) config.textSpeed = server.arg("ts").toInt();
    if (server.hasArg("r")) config.gifRepeats = server.arg("r").toInt();
    if (server.hasArg("m")) config.randomMode = server.arg("m").toInt() == 1; // 0=Secuencial, 1=Aleatorio

    // Limpiar y reconstruir la lista de carpetas activas (vector)
    config.activeFolders.clear();
    for(size_t i = 0; i < server.args(); ++i) {
        if (server.argName(i) == "f") {
            config.activeFolders.push_back(server.arg(i));
        }
    }
    // Si no se seleccionó ninguna carpeta, asegurar que al menos haya una entrada vacía si la SD está montada
    if (config.activeFolders.empty() && sdMontada) {
         config.activeFolders.push_back("/");
    }
    
    if (config.playMode == 0) {
        listarArchivosGif();
// Recargar GIFs inmediatamente si cambiamos el modo/carpetas
    }
    
    if (display) display->setBrightness8(config.brightness);
    savePlaybackConfig(); // Esto serializa el vector en activeFolders_str
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Guardado");
}

void handleSaveConfig() { 
    bool timeChanged = false;
// Configuración de Sistema (Nombre y PanelChain)
    if (server.hasArg("deviceName")) { 
        String nameStr = server.arg("deviceName");
        strncpy(config.device_name, nameStr.c_str(), sizeof(config.device_name) - 1);
        config.device_name[sizeof(config.device_name) - 1] = '\0';
    }
    
    // Panel Chain
    if (server.hasArg("pc")) config.panelChain = server.arg("pc").toInt();
// Configuración de Tiempo
    if (server.hasArg("tz") && server.arg("tz") != config.timeZone) {
        config.timeZone = server.arg("tz");
        timeChanged = true;
    }
    config.format24h = (server.arg("f24h") == "1"); 
    config.showSeconds = (server.arg("ss") == "1");
    config.showDate = (server.arg("sd") == "1");

    // Color Reloj
    if (server.hasArg("cc")) {
        config.clockColor = parseHexColor(server.arg("cc"));
    }
    
    // Color Texto Deslizante
    if (server.hasArg("stc")) {
        config.slidingTextColor = parseHexColor(server.arg("stc"));
    }
    
    saveSystemConfig();
    
    if (timeChanged) {
        initTime();
// Reconfigurar NTP inmediatamente
    }

    server.sendHeader("Location", "/config");
    server.send(302, "text/plain", "Configuración Guardada.");

}

void handleRestart() { 
    server.send(200, "text/html", "<h2>Reiniciando...</h2><p>El dispositivo se reiniciará en 3 segundos.</p>");
    delay(3000);
    ESP.restart();
}

// --- Manejadores OTA (Se mantiene la versión de la UI anterior) ---
void handleOTA() { 
  String html = R"raw(
    <!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Actualización OTA</title>
    <style>body{font-family:Arial;background:#f0f2f5;color:#333;margin:0;padding:20px;}
    .c{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:15px;box-shadow:0 8px 25px rgba(0,0,0,.15);}
    h1{text-align:center;color:#2c3e50;}input[type='file'],button{margin:10px 0;padding:12px;border-radius:8px;border:1px solid #ccc;width:100%;box-sizing:border-box;font-size:16px;}
    .button-group{display:flex;justify-content:space-between;gap:10px;margin-top:20px;}
    .back-btn{background:#e67e22;}
    </style></head><body><div class='c'>
    <h1>Actualización de Firmware (OTA)</h1>
    <form method='POST' action='/ota_upload' enctype='multipart/form-data'>
      <input type='file' name='firmware'>
      <button type='submit'>Subir y Actualizar</button>
    </form>
    <p id="progress"></p>
    <hr>
    <div class='button-group'>
    <button type='button' class='back-btn' onclick="window.location.href='/'">Volver</button>
    </div>
    <script>
    document.querySelector('form').onsubmit = function() {
        document.getElementById('progress').innerHTML = 'Subiendo... NO desconecte el dispositivo. Esto puede tardar unos minutos.';
        setTimeout(function(){ 
            document.getElementById('progress').innerHTML = 'Instalando el firmware. El dispositivo se reiniciará automáticamente...'; 
        }, 10000);
    };
    </script>
    </div></body></html>
  )raw";
  server.send(200, "text/html", html);
}

void handleOTAUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Actualización: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { 
            Serial.printf("Actualización exitosa: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
        
        if (Update.isRunning()){
    
            server.send(200, "text/plain", "Actualizacion Exitosa! Reiniciando...");
            delay(100);
            ESP.restart();
        } else {
            server.send(500, "text/plain", "Fallo la actualizacion. Revise el monitor serie.");
        }
    }
}
void notFound() { server.send(404, "text/plain", "Not Found"); }

// ====================================================================
//                          INTERFAZ WEB PRINCIPAL
// ====================================================================

String webPage() {
    int brightnessPercent = (int)(((float)config.brightness / 255.0) * 100.0); 

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Retro Pixel LED</title>";
    html += "<style>";
    html += "body{font-family:Arial;background:#f0f2f5;color:#333;margin:0;padding:20px;}";
    html += ".c{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:15px;box-shadow:0 8px 25px rgba(0,0,0,.15);}";
    html += "h1{text-align:center;color:#2c3e50;margin:0;}";
    html += "input:not([type='checkbox']):not([type='color']),select,button{margin:10px 0;padding:12px;border-radius:8px;border:1px solid #ccc;width:100%;box-sizing:border-box;font-size:16px;}";
    html += ".button-group{display:flex;justify-content:space-between;gap:10px;margin-top:20px;}";
    html += ".button-group button{width:calc(25% - 7px);margin:0;font-weight:bold;color:#fff;border:none;cursor:pointer;padding:12px 0;}";
    html += ".save-btn{background:#2ecc71;}";
    html += ".save-btn:hover{background:#27ae60;}";
    html += ".update-btn{background:#3498db;}";
    html += ".update-btn:hover{background:#2980b9;}";
    html += ".config-btn{background:#e67e22;}";
    html += ".config-btn:hover{background:#d35400;}";
    html += ".file-btn{background:#9b59b6;}";
    html += ".file-btn:hover{background:#8e44ad;}";
    html += "label{display:block;margin:15px 0 5px;font-weight:bold;}";
    html += ".label-brightness{display:flex;justify-content:space-between;align-items:center;margin:15px 0 5px;font-weight:bold;}";
    html += ".brightness-percent{font-size:1em;color:#3498db;}"; 
    html += ".cb label{font-weight:normal;display:block;margin:8px 0;}";
    html += ".footer{display:flex;justify-content:space-between;font-size:0.8em;color:#777;margin-top:20px;}";
    html += "</style></head><body><div class='c'>";
// Cabecera
    html += "<h1>Retro Pixel LED</h1><hr><form action='/save'>";
// 1. CONTROL DE BRILLO (Layout Modificado)
    html += "<label class='label-brightness'>";
    html += "Nivel de Brillo ";
    html += "<span class='brightness-percent' id='brightnessValue'>" + String(brightnessPercent) + "%</span>";
    html += "</label>";
    html += "<input type='range' name='b' min='0' max='255' value='" + String(config.brightness) + "' oninput='updateBrightness(this.value)'>";
    
    html += "<hr>";
// Separador

    // 2. CONTROL: Modo de Reproducción
    html += "<label>Modo de Reproducción</label>";
    html += "<select name='pm' id='playModeSelect'>";
    html += String("<option value='0'") + (config.playMode == 0 ? " selected" : "") + ">GIFs</option>";
    html += String("<option value='1'") + (config.playMode == 1 ? " selected" : "") + ">Texto Deslizante</option>";
    html += String("<option value='2'") + (config.playMode == 2 ? " selected" : "") + ">Reloj</option>";
    html += "</select><hr>";
// 3. CONTROLES: Texto Deslizante
    html += "<div id='textConfig' style='display:" + String(config.playMode == 1 ? "block" : "none") + ";'>";
    html += "<label>Texto a Mostrar (Marquesina)</label>";
    html += "<input type='text' name='st' value='" + config.slidingText + "' maxlength='50'>";
    html += "<label>Velocidad de Deslizamiento (ms)</label>";
    html += "<input type='number' name='ts' min='10' max='1000' value='" + String(config.textSpeed) + "'>";
    html += "<hr></div>";
    
    // 4. Controles de GIFs
    html += "<div id='gifConfig' style='display:" + String(config.playMode == 0 ? "block" : "none") + ";'>";
    html += "<label>Repeticiones por GIF</label><input type='number' name='r' min='1' max='50' value='" + String(config.gifRepeats) + "'>";
    html += "<label>Orden de Reproducción</label><select name='m'>";
    html += String("<option value='0'") + (config.randomMode ? "" : " selected") + ">Secuencial</option>";
    html += String("<option value='1'") + (config.randomMode ? " selected" : "") + ">Aleatorio</option>";
    html += "</select><hr>";
    html += "<b>Carpetas disponibles:</b><div class='cb'>";
    
    if (sdMontada) {
        if (allFolders.empty()) {
            html += "<p>No se encontraron carpetas en la SD. (¿Archivos directamente en la raíz?)</p>";
        } else {
            for (const String& f : allFolders) {
                // Comprueba si la carpeta 'f' está en el vector de carpetas activas
                bool c = (std::find(config.activeFolders.begin(), config.activeFolders.end(), f) != config.activeFolders.end());
                html += String("<label><input type='checkbox' name='f' value='" + f + "'") + (c ? " checked" : "") + "> " + f + "</label>";
            }
        }
    } else {
        html += "<p>¡Tarjeta SD no montada! No se pueden listar carpetas.</p>";
    }
    html += "</div><hr></div>";
    
    // 5. Controles de Reloj (Placeholder)
    html += "<div id='clockConfig' style='display:" + String(config.playMode == 2 ? "block" : "none") + ";'>";
    html += "<p style='text-align:center;'>La configuracion de Hora y Apariencia del Reloj se realiza en la seccion de Configuracion General.</p>";
    html += "<hr></div>";


    // Grupo de botones en línea (4 botones)
    html += "<div class='button-group'>";
    html += "<button type='submit' class='save-btn'>Guardar</button>"; 
    html += "<button type='button' class='update-btn' onclick=\"window.location.href='/ota'\">OTA</button>"; 
    html += "<button type='button' class='config-btn' onclick=\"window.location.href='/config'\">Ajustes</button>"; 
    html += "<button type='button' class='file-btn' onclick=\"window.location.href='/file_manager'\">Archivos SD</button>";
    html += "</div>";
    html += "</form>";
    // Pie de página: Intercambio de posiciones
    html += "<div class='footer'>";
    html += "<span>Retro Pixel LED v" + String(FIRMWARE_VERSION) + " by fjgordillo86</span>";
    // Ahora a la Izquierda
    html += "<span><b>IP:</b> " + WiFi.localIP().toString() + "</span>";
// Ahora a la Derecha
    html += "</div>";
// Script JS para manejar visibilidad de la configuración y porcentaje de brillo
    html += "<script>";
    html += "function updateBrightness(val) {";
    html += "  var percent = Math.round((val / 255) * 100);";
    html += "  document.getElementById('brightnessValue').innerHTML = percent + '%';";
    html += "}";
    html += "document.getElementById('playModeSelect').addEventListener('change', function() {";
    html += "  var mode = this.value;";
    html += "  document.getElementById('gifConfig').style.display = (mode == '0') ? 'block' : 'none';";
    html += "  document.getElementById('textConfig').style.display = (mode == '1') ? 'block' : 'none';";
    html += "  document.getElementById('clockConfig').style.display = (mode == '2') ? 'block' : 'none';";
    html += "});";
    html += "</script>";

    html += "</div></body></html>";
    return html;
}

// ====================================================================
//                      INTERFAZ WEB CONFIGURACIÓN
// ====================================================================

String configPage() {
    char hexColor[8];
    sprintf(hexColor, "#%06X", config.clockColor);
    
    char hexTextColor[8];
    sprintf(hexTextColor, "#%06X", config.slidingTextColor); 

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Configuración | Retro Pixel LED</title>";
    html += "<style>";
    html += "body{font-family:Arial;background:#f0f2f5;color:#333;margin:0;padding:20px;}";
    html += ".c{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:15px;box-shadow:0 8px 25px rgba(0,0,0,.15);}";
    html += "h1{text-align:center;color:#2c3e50;margin:0;}";
    html += "h2{border-bottom:2px solid #ccc;padding-bottom:5px;margin-top:30px;color:#3498db;}";
    html += "input:not([type='checkbox']):not([type='color']),select,button{margin:10px 0;padding:12px;border-radius:8px;border:1px solid #ccc;width:100%;box-sizing:border-box;font-size:16px;}";
    html += "input[type='color']{width:50px;height:50px;padding:0;border:none;border-radius:5px;vertical-align:middle;margin-right:15px;}";
    html += "label{display:block;margin:15px 0 5px;font-weight:bold;}";
    html += ".checkbox-group{display:flex;align-items:center;margin-top:10px;}";
    html += ".checkbox-group label{margin:0;padding-left:10px;font-weight:normal;}";
    html += ".config-group{display:flex;justify-content:space-between;gap:10px;margin-top:30px;}";
    html += ".config-group button{width:calc(33.33% - 7px);margin:0;font-weight:bold;color:#fff;border:none;cursor:pointer;padding:12px 0; border-radius:8px;}";
    html += ".save-btn{background:#2ecc71;}";
    html += ".save-btn:hover{background:#27ae60;}";
    html += ".back-btn{background:#e67e22;}";
    html += ".back-btn:hover{background:#d35400;}";
    html += ".restart-btn{background:#3498db;}";
// Color cambiado a azul para distinción
    html += ".restart-btn:hover{background:#2980b9;}";
// NUEVO Estilo para botón de Restablecer (rojo peligro)
    html += ".reset-btn{background:#e74c3c;margin-top:20px;}"; 
    html += ".reset-btn:hover{background:#c0392b;}";
    html += ".footer{display:flex;justify-content:space-between;font-size:0.8em;color:#777;margin-top:20px;}";
    html += "</style></head><body><div class='c'>";
    
    // Cabecera sin imagen
    html += "<h1>Configuración General</h1><hr><form action='/save_config'>";
// ----------------------------------------------------
    // SECCIÓN 1: CONFIGURACIÓN DE HORA Y FECHA
    // ----------------------------------------------------
    html += "<h2>1. Hora y Fecha</h2>";
// Zona Horaria (TZ String)
    html += "<label for='tz'>Zona Horaria (TZ String)</label>";
    html += "<input type='text' id='tz' name='tz' value='" + config.timeZone + "' placeholder='" + String(TZ_STRING_SPAIN) + "'>";
// Cadena de explicación de la TZ
    html += "<small>Valor por defecto para España: <code>" + String(TZ_STRING_SPAIN) + "</code>. La gestión del horario de verano/invierno (DST) se hace automáticamente si la cadena TZ es correcta. <a href='https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv' target='_blank'>Más TZ Strings</a></small>";
    
// Formato de Hora (12h/24h)
    html += "<label for='f24h'>Formato de Hora</label>";
    html += "<select id='f24h' name='f24h'>";
    html += String("<option value='1'") + (config.format24h ? " selected" : "") + ">24 Horas (HH:MM)</option>";
    html += String("<option value='0'") + (!config.format24h ? " selected" : "") + ">12 Horas (HH:MM AM/PM)</option>";
    html += "</select>";
// Mostrar Segundos
    html += "<label>Opciones de Visualización</label>";
    html += "<div class='checkbox-group' style='margin-bottom:10px;'>";
    html += String("<input type='checkbox' id='ss' name='ss' value='1'") + (config.showSeconds ? " checked" : "") + ">";
    html += "<label for='ss'>Mostrar Segundos</label></div>";
    
    // Mostrar Fecha
    html += "<div class='checkbox-group'>";
    html += String("<input type='checkbox' id='sd' name='sd' value='1'") + (config.showDate ? " checked" : "") + ">";
    html += "<label for='sd'>Mostrar Fecha (Se desliza)</label></div>";

    // ----------------------------------------------------
    // SECCIÓN 2: CONFIGURACIÓN DE COLORES
    // ----------------------------------------------------
    html += "<h2>2. Configuración de Colores</h2>";
// Color del Reloj
    html += "<label>Color de los Dígitos del Reloj</label>";
    html += "<div style='display:flex;align-items:center;'>";
    html += "<input type='color' id='cc' name='cc' value='" + String(hexColor) + "'>";
    html += "<label for='cc' style='margin:0;'>Selecciona un color</label></div>";
// Color del Texto Deslizante
    html += "<label>Color del Texto Deslizante</label>";
    html += "<div style='display:flex;align-items:center;'>";
    html += "<input type='color' id='stc' name='stc' value='" + String(hexTextColor) + "'>"; 
    html += "<label for='stc' style='margin:0;'>Selecciona un color</label></div>";
// ----------------------------------------------------
    // SECCIÓN 3: CONFIGURACIÓN DE HARDWARE Y SISTEMA
    // ----------------------------------------------------
    html += "<h2>3. Hardware y Sistema</h2>";
// Número de Paneles LED
    html += "<label for='pc'>Número de Paneles LED (en cadena)</label>";
    html += "<input type='number' id='pc' name='pc' min='1' max='8' value='" + String(config.panelChain) + "'>";
    html += "<small>El valor actual es: " + String(config.panelChain) + ". Requiere reinicio para un cambio físico de configuración.</small>";
// Grupo de botones para Guardar, Reiniciar y Volver
    html += "<div class='config-group' style='margin-top:30px;'>";
    html += "<button type='submit' class='save-btn'>Guardar</button>"; 
    html += "<button type='button' class='restart-btn' onclick=\"if(confirm('¿Seguro que quieres reiniciar el dispositivo?')) { window.location.href='/restart'; }\">Reiniciar</button>";
    html += "<button type='button' class='back-btn' onclick=\"window.location.href='/'\">Volver</button>"; 
    html += "</div>";

    html += "</form><hr>";
// Botón de Restablecimiento de Fábrica Total (Único)
    html += "<div style='margin-top:20px; text-align:center;'>";
    html += "<button class='reset-btn' onclick=\"if(confirm('⚠️ ¿Seguro que quieres RESTABLECER TODOS LOS AJUSTES? Esto borrará la configuración de la matriz, los modos, los colores Y la conexión WiFi. Los archivos de la tarjeta SD permanecerán intactos. El dispositivo se reiniciará en modo de configuración inicial.')) { window.location.href='/factory_reset'; }\">Restablecer Ajustes</button>";
    html += "</div>";


    // Pie de página
    html += "<div class='footer'>";
    html += "<span>Retro Pixel LED v" + String(FIRMWARE_VERSION) + " by fjgordillo86</span>"; 
    html += "<span><b>IP:</b> " + WiFi.localIP().toString() + "</span>";
    html += "</div>";
    
    html += "</div></body></html>";
    return html;
}

// ====================================================================
//                    MANEJADORES DE GESTIÓN DE ARCHIVOS
// ====================================================================

// --- MANEJO DE SUBIDA DE ARCHIVOS (Optimizado para Multi-Upload) --- 

void handleFileUpload(){ 
    if (!sdMontada) { 
        server.send(500, "text/plain", "Error: SD no montada.");
        return; 
    } 
    
    HTTPUpload& upload = server.upload(); 
    
    // 1. INICIO de la subida del archivo (UPLOAD_FILE_START)
    if (upload.status == UPLOAD_FILE_START) { 
        // La ruta de destino es la ruta actual (currentPath) + el nombre del archivo
        String uploadPath = currentPath + upload.filename;
        
        Serial.printf("Inicio de la subida a: %s\n", uploadPath.c_str()); 
        
        // Abrir el archivo de la SD para escribir (sobrescribe si existe)
        // La variable global fsUploadFile se usará para escribir el contenido.
        fsUploadFile = SD.open(uploadPath.c_str(), FILE_WRITE);

        if (!fsUploadFile) {
            Serial.printf("ERROR: No se pudo abrir el archivo %s para escribir.\n", uploadPath.c_str());
        }
        
    // 2. ESCRITURA de datos (UPLOAD_FILE_WRITE)
    } else if (upload.status == UPLOAD_FILE_WRITE) { 
        if(fsUploadFile) { 
            fsUploadFile.write(upload.buf, upload.currentSize);
        } 
        
    // 3. FIN de la subida del archivo (UPLOAD_FILE_END)
    } else if (upload.status == UPLOAD_FILE_END) { 
        if (fsUploadFile) { 
            fsUploadFile.close(); 
            Serial.printf("Subida finalizada. Nombre: %s | Tamaño: %d bytes\n", upload.filename.c_str(), upload.totalSize);
            
            // 🛑 LÓGICA DE OPTIMIZACIÓN: Solo refrescar la lista de GIFs si el archivo subido es un GIF.
            String filename = upload.filename;
            if (filename.endsWith(".gif") || filename.endsWith(".GIF")) {
                 listarArchivosGif(); 
            }
        } else {
            Serial.printf("Error en la subida del archivo %s: Falló la escritura o la apertura.\n", upload.filename.c_str());
        }
    } 
}

// --- MANEJO DE BORRADO DE ARCHIVOS Y CARPETAS ---

void handleFileDelete() {
    if (!sdMontada) {
        server.send(500, "text/plain", "Error: SD no montada.");
        return;
    }

    if (server.hasArg("name") && server.hasArg("type")) {
        String filename = server.arg("name");
        String filetype = server.arg("type");
        
        // Asegurar que la ruta actual está correctamente configurada
        String fullPath = currentPath + filename;
        
        if (filetype == "dir") {
            // Para directorios, la función rmdir requiere la ruta sin barra final
            if (SD.rmdir(fullPath.c_str())) { 
                Serial.printf("Directorio borrado: %s\n", fullPath.c_str());
            } else {
                Serial.printf("ERROR: No se pudo borrar el directorio (verifique si está vacío): %s\n", fullPath.c_str());
            }
        } else {
            if (SD.remove(fullPath.c_str())) {
                Serial.printf("Archivo borrado: %s\n", fullPath.c_str());
            } else {
                Serial.printf("ERROR: No se pudo borrar el archivo: %s\n", fullPath.c_str());
            }
        }
    }
    // Redirigir siempre de vuelta al administrador de archivos en la ruta actual
    server.sendHeader("Location", String("/file_manager?path=") + currentPath);
    server.send(302, "text/plain", "Redirigiendo...");
}

// --- MANEJO DE CREACIÓN DE CARPETAS ---

void handleCreateDir() {
    if (server.hasArg("name")) {
        String newDirName = server.arg("name");
        String fullPath = currentPath + newDirName;

        // Limpiar la ruta final (asegurar '/' al final)
        if (!fullPath.endsWith("/")) {
            fullPath += "/";
        }

        if (SD.mkdir(fullPath.c_str())) {
            Serial.printf("Carpeta creada: %s\n", fullPath.c_str());
        } else {
            Serial.printf("ERROR: No se pudo crear la carpeta: %s\n", fullPath.c_str());
        }
    }
    // Redirigir siempre de vuelta al administrador de archivos en la ruta actual
    server.sendHeader("Location", String("/file_manager?path=") + currentPath);
    server.send(302, "text/plain", "Redirigiendo...");
}

// --- MANEJO DE ARCHIVOS (VISUALIZACIÓN Y NAVEGACIÓN) ---

void handleFileManager() {
    if (!sdMontada) {
        server.send(500, "text/html", "<h2>Error: SD no montada.</h2>");
        return;
    }
    
    // 1. OBTENER Y ESTABLECER RUTA ACTUAL
    String requestedPath = server.hasArg("path") ? server.arg("path") : "/";
    
    // Asegurar que la ruta siempre termine con / (excepto si es solo "/")
    if (requestedPath != "/" && !requestedPath.endsWith("/")) {
        requestedPath += "/";
    }
    
    // Asegurar que la ruta siempre empiece con /
    if (!requestedPath.startsWith("/")) {
        requestedPath = "/" + requestedPath;
    }

    currentPath = requestedPath;
    
    // Abrir el directorio actual
    File root = SD.open(currentPath.c_str());
    if (!root || !root.isDirectory()) {
        server.send(404, "text/html", "<h2>Error: Ruta no encontrada o no es un directorio.</h2>");
        return;
    }

    String content = F("<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>RetroPixel LED - Archivos SD</title>");
    content += F("<style>body{font-family:Arial,sans-serif;background:#2c3e50;color:#ecf0f1;margin:0;padding:20px}h1{color:#f39c12}a{color:#3498db;text-decoration:none}a:hover{text-decoration:underline}.container{max-width:800px;margin:auto}.menu{margin-bottom:20px}.menu button,.menu a,.file-item button{background:#34495e;color:white;border:none;padding:10px 15px;cursor:pointer;border-radius:5px;text-decoration:none;display:inline-block;margin-right:5px}.menu button:hover,.menu a:hover,.file-item button:hover{background:#2c3e50}.file-item{display:flex;justify-content:space-between;align-items:center;padding:10px;border-bottom:1px solid #34495e}.file-item:nth-child(even){background:#243647}.file-item a{flex-grow:1;margin-right:10px}.dir{color:#f1c40f}.file{color:#bdc3c7}.current-path{background:#1abc9c;padding:5px 10px;border-radius:5px;display:block;margin-bottom:15px}.upload-form,.dir-form{margin-top:20px;padding:15px;border:1px solid #3498db;border-radius:5px}.upload-form input[type=file],.dir-form input[type=text]{padding:10px;border:1px solid #34495e;border-radius:5px;margin-right:10px;background:#2c3e50;color:#ecf0f1}.dir-form input[type=submit]{margin-top:10px}</style></head><body><div class='container'>");
    
    // --- ENCABEZADO Y NAVEGACIÓN ---
    content += F("<h1><a href='/'>🏠 RetroPixel LED</a></h1><h2>📁 Archivos SD</h2>");
    
    // Mostrar la ruta actual
    content += F("<div class='current-path'>Ruta Actual: <strong>");
    content += currentPath;
    content += F("</strong></div>");

    // Botón de Volver
    if (currentPath != "/") {
        // Calcular la ruta: quita el último segmento y la barra final
        String parentPath = currentPath.substring(0, currentPath.lastIndexOf('/', currentPath.length() - 2) + 1);
        
        content += F("<div class='menu'><a href='/file_manager?path=");
        content += parentPath;
        content += F("'>⬅️ Subir Directorio</a></div>");
    }

    // --- LISTADO DE ARCHIVOS ---
    content += F("<h3>Contenido del Directorio:</h3>");
    
    File file = root.openNextFile();
    while(file){
        String filename = file.name();
        // Solo mostrar el nombre del archivo dentro del directorio actual
        int lastSlash = filename.lastIndexOf('/');
        filename = filename.substring(lastSlash + 1); 
        
        if (filename.length() > 0 && filename != ".") { // Evitar "." y nombres vacíos
            content += F("<div class='file-item'>");
            
            if (file.isDirectory()){
                // Enlace a la carpeta
                content += F("<a class='dir' href='/file_manager?path=");
                content += currentPath;
                content += filename;
                content += F("/'>");
                content += F("📂 ");
                content += filename;
                content += F("</a>");

                // Botón Borrar Directorio
                content += F("<button onclick='confirmDelete(\"");
                content += filename;
                content += F("\", \"dir\")'>🗑️ Borrar</button>");
                
            } else {
                // Nombre del archivo
                content += F("<span class='file'>");
                content += F("📄 ");
                content += filename;
                content += F("</span>");
                
                // Botón Borrar Archivo
                content += F("<button onclick='confirmDelete(\"");
                content += filename;
                content += F("\", \"file\")'>🗑️ Borrar</button>");
            }
            content += F("</div>");
        }
        file.close(); // Liberar el recurso del archivo actual
        file = root.openNextFile(); // Pasar al siguiente archivo
    }
    root.close(); // Cerrar el directorio raíz

    // --- FORMULARIO CREAR CARPETA ---
    content += F("<div class='dir-form'><h3>Crear Nueva Carpeta</h3>");
    content += F("<form action='/create_dir' method='GET'><input type='text' name='name' placeholder='Nombre de la carpeta' required><input type='submit' value='Crear Carpeta'></form></div>");
    
    // --- FORMULARIO SUBIR ARCHIVO ---
    content += F("<div class='upload-form'><h3>Subir Archivo (al directorio actual)</h3>");
    content += F("<form method='POST' action='/upload' enctype='multipart/form-data'>");
    content += F("<input type='file' name='data' multiple required><br>");
    content += F("<input type='submit' value='Subir Archivo'></form></div>");

    // --- SCRIPT DE BORRADO Y CIERRE ---
    content += F("<script>function confirmDelete(name, type) { if (confirm('¿Estás seguro de que quieres borrar ' + (type === 'dir' ? 'la carpeta' : 'el archivo') + ' \"' + name + '\"?')) { window.location.href = '/delete?name=' + name + '&type=' + type; } }</script>");
    content += F("</div></body></html>");

    server.send(200, "text/html", content);
}

String fileManagerPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Gestor de Archivos SD</title>";
    html += "<style>";
    html += "body{font-family:Arial;background:#f0f2f5;color:#333;margin:0;padding:20px;}";
    html += ".c{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:15px;box-shadow:0 8px 25px rgba(0,0,0,.15);}";
    html += "h1{text-align:center;color:#2c3e50;margin:0;}";
    html += "h2{border-bottom:2px solid #ccc;padding-bottom:5px;margin-top:30px;color:#3498db;}";
    html += "input[type='file'], button, .back-btn{margin:10px 0;padding:12px;border-radius:8px;border:1px solid #ccc;width:100%;box-sizing:border-box;font-size:16px;cursor:pointer;}";
    html += ".upload-btn{background:#2ecc71;color:#fff;border:none;}";
    html += ".back-btn{background:#e67e22;color:#fff;border:none;}";
    html += "table{width:100%;border-collapse:collapse;margin-top:20px;}";
    html += "th, td{padding:10px;border:1px solid #eee;text-align:left;}";
    html += "th{background:#f4f4f4;}";
    html += ".delete-btn{background:#e74c3c;color:#fff;border:none;padding:5px 10px;border-radius:4px;cursor:pointer;}";
    html += ".footer{font-size:0.8em;color:#777;margin-top:20px;}";
    html += "</style></head><body><div class='c'>";
    
    html += "<h1>Gestor de Archivos SD</h1><hr>";

    // SECCIÓN DE SUBIDA
    html += "<h2>Subir Nuevo Archivo</h2>";
    if (sdMontada) {
        html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
        html += "<input type='file' name='data_file' required>";
        html += "<button type='submit' class='upload-btn'>Subir Archivo a SD</button>";
        html += "</form>";
    } else {
        html += "<p style='color:red;'>⚠️ **Error:** La tarjeta SD no está montada. No es posible subir archivos.</p>";
    }
    html += "<hr>";

    // SECCIÓN DE LISTADO Y ELIMINACIÓN
    html += "<h2>Archivos en la Raíz de la SD</h2>";
    if (sdMontada) {
        html += "<table><tr><th>Nombre de Archivo</th><th>Tamaño (KB)</th><th>Acción</th></tr>";
        
        File root = SD.open("/");
        if (root) {
            File entry = root.openNextFile();
            while (entry) {
                if (!entry.isDirectory()) {
                    String fileName = entry.name();
                    long fileSize = entry.size();
                    
                    html += "<tr>";
                    html += "<td>" + fileName + "</td>";
                    html += "<td>" + String(fileSize / 1024.0, 2) + "</td>";
                    // Botón de eliminar con confirmación JS
                    html += "<td><button class='delete-btn' onclick=\"if(confirm('¿Estás seguro de que quieres eliminar " + fileName + "?')) { window.location.href='/delete?name=" + fileName + "'; }\">Eliminar</button></td>";
                    html += "</tr>";
                }
                entry.close();
                entry = root.openNextFile();
            }
            root.close();
        } else {
            html += "<tr><td colspan='3'>Error al abrir la carpeta raíz de la SD.</td></tr>";
        }
        
        html += "</table>";
    } else {
        html += "<p style='color:red;'>No se pueden listar archivos. La tarjeta SD no está disponible.</p>";
    }

    // Pie
    html += "<hr><button type='button' class='back-btn' onclick=\"window.location.href='/'\">Volver a la Configuración Principal</button>";
    html += "<div class='footer'>Retro Pixel LED v" + String(FIRMWARE_VERSION) + "</div>";
    html += "</div></body></html>";
    return html;
}

// ====================================================================
//                       FUNCIONES CORE DE VISUALIZACIÓN
// (Sin cambios funcionales, solo se han corregido los comentarios/citaciones)
// ====================================================================

// --- 1. Funciones Callback para AnimatedGIF ---

// Función de dibujo de la librería GIF (necesita estar definida)
void GIFDraw(GIFDRAW *pDraw)
{
    // Las variables deben estar declaradas en el ámbito global o como 'extern'
    extern int x_offset; 
    extern int y_offset; 
    extern MatrixPanel_I2S_DMA *display; 

    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;
    int iCount; // Variable para el conteo de píxeles opacos

    if (!display) return; 

    // BaseX: Punto de inicio del frame, incluyendo el offset de centrado
    int baseX = pDraw->iX + x_offset; 
    
    // Altura y ancho del frame
    iWidth = pDraw->iWidth;
    if (iWidth > 128) 
        iWidth = 128; 
        
    usPalette = pDraw->pPalette;
    
    // Y: Coordenada Y de inicio del dibujo, incluyendo el offset de centrado
    y = pDraw->iY + pDraw->y + y_offset; 

    s = pDraw->pPixels;

    // Lógica para frames con transparencia o método de descarte (Disposal)
    if (pDraw->ucHasTransparency) { 
        
        iCount = 0;
        
        for (x = 0; x < iWidth; x++) {
            if (s[x] == pDraw->ucTransparent) {
                if (iCount) { 
                    for(int xOffset_ = 0; xOffset_ < iCount; xOffset_++ ){
                        // 🛑 CORRECCIÓN: SUMAMOS 128 (Primera línea)
                        display->drawPixel(baseX + x - iCount + xOffset_ + 128, y, usTemp[xOffset_]); 
                    }
                    iCount = 0;
                }
            } else {
                usTemp[iCount++] = usPalette[s[x]];
            }
        }
        
        if (iCount) {
            for(int xOffset_ = 0; xOffset_ < iCount; xOffset_++ ){
                // 🛑 CORRECCIÓN: SUMAMOS 128 (Segunda línea)
                display->drawPixel(baseX + x - iCount + xOffset_ + 128, y, usTemp[xOffset_]); 
            }
        }

    } else { // No hay transparencia (dibujo simple de línea completa)
        s = pDraw->pPixels;
        for (x=0; x<iWidth; x++)
            // 🛑 CORRECCIÓN: SUMAMOS 128 (Tercera línea)
            display->drawPixel(baseX + x + 128, y, usPalette[*s++]); 
    }
} /* GIFDraw() */

// Funciones de gestión de archivo para SD
static void * GIFOpenFile(const char *fname, int32_t *pSize)
{
    // Usamos la variable global FSGifFile
    extern File FSGifFile; 
    FSGifFile = M5STACK_SD.open(fname);
    if (FSGifFile) {
        *pSize = FSGifFile.size();
        return (void *)&FSGifFile; // Devolver puntero a la variable global
    }
    return NULL;
}

static void GIFCloseFile(void *pHandle)
{
    File *f = static_cast<File *>(pHandle);
    if (f != NULL)
        f->close();
}

static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // El 'ugly work-around' de su ejemplo
    if ((pFile->iSize - pFile->iPos) < iLen)
        iBytesRead = pFile->iSize - pFile->iPos - 1; 
    if (iBytesRead <= 0)
        return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
}


static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    return pFile->iPos;
}


// --- 2. Funciones de Utilidad de Visualización ---

void mostrarMensaje(const char* mensaje, uint16_t color = 0xF800 /* Rojo */) {
    if (!display) return;
    display->fillScreen(0); // Negro
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(color);
    display->setCursor(0, MATRIX_HEIGHT / 2 - 4);
    display->print(mensaje);
    display->flipDMABuffer();
}

// ====================================================================
//                  FUNCIÓN DE ESCANEO DE CARPETAS PARA LA UI
// ====================================================================

// Función para escanear y listar solo las CARPETAS dentro de un path base
void scanFolders(String basePath) {
    allFolders.clear(); // Limpiamos la lista global de carpetas
    
    // 1. Aseguramos que el directorio base exista.
    if (!SD.exists(basePath)) {
        SD.mkdir(basePath); // Creamos la carpeta /gifs si no existe
        Serial.printf("Directorio base creado: %s\n", basePath.c_str());
        return; 
    }

    File root = SD.open(basePath);
    if (!root || !root.isDirectory()) {
        Serial.printf("Error: %s no es un directorio válido.\n", basePath.c_str());
        return;
    }
    
    Serial.printf("Escaneando subcarpetas dentro de: %s\n", basePath.c_str());
    
    File entry = root.openNextFile();
    while(entry){
        if(entry.isDirectory()){
            String dirName = entry.name();
            // Construimos la ruta completa (Ejemplo: /gifs/animals)
            String fullPath = basePath + "/" + dirName; 
            
            // Añadir a la lista que se muestra en la interfaz web
            allFolders.push_back(fullPath); 
        }
        entry = root.openNextFile();
    }
    root.close();
    
    // Ordenar la lista para mostrarla alfabéticamente en la web
    if (allFolders.size() > 0) {
        std::sort(allFolders.begin(), allFolders.end());
    }
}

// ====================================================================
//                 GESTIÓN DE ARCHIVOS GIF & CACHÉ
// ====================================================================

// Nueva función para leer la lista de GIFs desde el archivo de caché
bool loadGifCache() {
    archivosGIF.clear(); // Limpiamos la lista actual
    
    // Abrir el archivo de caché para lectura
    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_READ);
    if (!cacheFile) {
        Serial.println("Cache de GIFs no encontrada. Forzando escaneo de SD.");
        return false; // El caché no existe o falló la lectura
    }

    Serial.println("Cargando lista de GIFs desde el caché...");
    
    // Leer cada línea del archivo (cada línea es una ruta de GIF)
    while (cacheFile.available()) {
        String line = cacheFile.readStringUntil('\n');
        line.trim(); // Eliminar espacios en blanco o retornos de carro
        if (line.length() > 0) {
            archivosGIF.push_back(line);
        }
    }
    
    cacheFile.close();
    
    if (archivosGIF.empty()) {
        Serial.println("Cache vacía. Forzando escaneo de SD.");
        return false;
    }
    
    Serial.printf("Cache cargada. %d GIFs encontrados.\n", archivosGIF.size());
    return true; // Cache cargada con éxito
}

// Nueva función para escribir la lista de GIFs al archivo de caché
void saveGifCache() {
    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_WRITE);
    if (!cacheFile) {
        Serial.println("Error: No se pudo crear/abrir el archivo de caché.");
        return;
    }

    Serial.println("Guardando lista actual de GIFs en caché...");
    for (const String& path : archivosGIF) {
        cacheFile.println(path); // Escribimos cada ruta en una nueva línea
    }
    
    cacheFile.close();
    Serial.println("Caché de GIFs guardada correctamente.");
}

// Función recursiva que escanea una ruta en busca de GIFs
void scanGifDirectory(String path) {
    File root = SD.open(path);
    if (!root) {
        Serial.printf("Error al abrir directorio: %s\n", path.c_str());
        return;
    }
    
    File entry = root.openNextFile();
    while(entry){
        if(entry.isDirectory()){
            // Si es un directorio, lo ignoramos para la lista plana de archivos
            // Si en el futuro quiere escanear recursivamente, esta es la línea a cambiar.
        } else {
            // Es un archivo, verificar si es un GIF
            String fileName = entry.name();
            if (fileName.endsWith(".gif") || fileName.endsWith(".GIF")) {
                String fullPath = path + "/" + fileName; // Construimos la ruta completa
                // Limpiamos el doble slash si la ruta es la raíz (/)
                if (path == "/") { 
                    fullPath = fileName;
                }
                
                archivosGIF.push_back(fullPath); // Añadimos la ruta a la lista global
            }
        }
        entry = root.openNextFile();
    }
    root.close();
}


// Función auxiliar para generar la firma de la configuración actual
String generateCacheSignature() {
    String signature = "";
    // MODIFICADO: Usamos config.activeFolders que es el vector donde se guardan las carpetas seleccionadas
    for (const String& folder : config.activeFolders) { 
        signature += folder + ":"; // Concatenamos las rutas separadas por :
    }
    return signature;
}

// Función principal de listado de archivos GIF (usa la lógica de validación de firma)
void listarArchivosGif() {
    // 1. Generar la firma de la configuración actual de carpetas
    String currentSignature = generateCacheSignature();
    
    // Si no hay carpetas seleccionadas, la lista debe estar vacía
    if (currentSignature.length() == 0) { 
        archivosGIF.clear();
        Serial.println("No hay carpetas de GIF seleccionadas. Lista vacía.");
        return;
    }

    // 2. Intentar validar la caché leyendo el archivo de firma
    // CORRECCIÓN: Declaramos 'cacheIsValid' (era el error de compilación)
    bool cacheIsValid = false; 
    File sigFile = SD.open(GIF_CACHE_SIG, FILE_READ);
    
    if (sigFile) {
        String savedSignature = sigFile.readStringUntil('\n');
        sigFile.close();
        savedSignature.trim();

        if (savedSignature == currentSignature) {
            cacheIsValid = true;
        } else {
            Serial.println("Firmas de caché no coinciden. La configuración ha cambiado.");
        }
    } else {
        Serial.println("Archivo de firma de caché no encontrado. Forzando escaneo.");
    }
    
    // 3. Cargar o Reconstruir la lista
    if (cacheIsValid && loadGifCache()) { 
        return; // ¡Caché válida y cargada! Salimos sin escanear.
    }

    // 4. Reconstrucción: Escaneo de SD y regeneración de archivos de caché
    Serial.println("Reconstruyendo lista de GIFs, escaneando SD...");
    archivosGIF.clear();

    // 4a. Escanear las carpetas seleccionadas
    for (const String& path : config.activeFolders) { 
        scanGifDirectory(path);
    }

    // 4b. Guardar el índice de GIFs
    saveGifCache(); 

    // 4c. Guardar la nueva firma para la próxima vez
    File newSigFile = SD.open(GIF_CACHE_SIG, FILE_WRITE);
    if (newSigFile) {
        newSigFile.print(currentSignature);
        newSigFile.close();
        Serial.println("Nueva firma de caché guardada.");
    } else {
         Serial.println("ERROR: No se pudo escribir el archivo de firma.");
    }
}

// --- 3. Funciones de Modos de Reproducción ---
// Variables globales necesarias para el loop
extern unsigned long lastFrameTime; // Declarar esta variable en el ámbito global si aún no existe
// extern WebServer server; // Asumiendo que el objeto server es global

void ejecutarModoGif() {
    if (!display) return; 
    
    // Verificación de la SD y lista de archivos
    if (!sdMontada || archivosGIF.empty()) {
        mostrarMensaje(!sdMontada ? "SD ERROR" : "NO GIFS");
        delay(200);
        return;
    }

    // Rotación de la lista de GIFs
    if (currentGifIndex >= archivosGIF.size()) {
        listarArchivosGif();
        currentGifIndex = 0;
        if (archivosGIF.empty()) return;
    }
    
    String gifPath = archivosGIF[currentGifIndex]; 
    
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

    currentGifIndex++; 
}

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

// ====================================================================
//                             SETUP Y LOOP
// ====================================================================

void setup() {
    Serial.begin(115200);
    if (!SPIFFS.begin(true)) {
        Serial.println("Error al montar SPIFFS.");
    }

    loadConfig();
// 1. Inicialización de la SD 
    SPI.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Error al montar la tarjeta SD!");
        sdMontada = false;
        delay(100);
    } else {
        Serial.println("Tarjeta SD montada correctamente.");
        sdMontada = true;
        scanFolders(GIFS_BASE_PATH);
// Escanear carpetas para la UI
    }
    
    gif.begin(LITTLE_ENDIAN_PIXELS);
// 2. Conexión WiFi y Servidor 
    
    if (config.device_name[0] == '\0') {
        strncpy(config.device_name, DEVICE_NAME_DEFAULT, sizeof(config.device_name) - 1);
        config.device_name[sizeof(config.device_name) - 1] = '\0'; 
    }

    wm.setHostname(config.device_name);
    WiFi.mode(WIFI_AP_STA); 
    wm.setSaveConfigCallback(nullptr); 

    Serial.println("Intentando conectar o iniciando portal cautivo...");
    delay(500);
    if (!wm.autoConnect("Retro Pixel LED")) { 
        Serial.println("Fallo de conexión y timeout del portal. Reiniciando...");
        delay(3000);
        ESP.restart();
    } 

    Serial.println("\nConectado a WiFi.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
// 3. Inicialización de la Matriz LED 
    const int FINAL_MATRIX_WIDTH = PANEL_RES_X * config.panelChain;
    HUB75_I2S_CFG::i2s_pins pin_config = {
        R1_PIN, G1_PIN, B1_PIN,
        R2_PIN, G2_PIN, B2_PIN,
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
        LAT_PIN, OE_PIN, CLK_PIN
    };
    HUB75_I2S_CFG matrix_config(
        FINAL_MATRIX_WIDTH, 
        MATRIX_HEIGHT,      
        config.panelChain,  
        pin_config          
    );
// La asignación de memoria (new)
    display = new MatrixPanel_I2S_DMA(matrix_config);
    if (display) { 
        display->begin();
        display->setBrightness8(config.brightness);
        display->fillScreen(display->color565(0, 0, 0));
// Mostrar estado inicial
        if (!sdMontada) {
            mostrarMensaje("SD Error!", display->color565(255, 0, 0));
        } else if (WiFi.status() == WL_CONNECTED) {
            mostrarMensaje("WiFi OK!", display->color565(0, 255, 0));
        } else {
             mostrarMensaje("AP Mode", display->color565(255, 255, 0));
        }
        
        if (config.playMode == 0) {
            listarArchivosGif();
        }
        delay(1000);
    } else {
        Serial.println("ERROR: No se pudo asignar memoria para la matriz LED.");
    }
    
    // 4. Configurar Hora y Servidor Web
    initTime();
    
    // --- RUTAS WEB PRINCIPALES ---
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_GET, handleSave);
    server.on("/config", HTTP_GET, handleConfig); 
    server.on("/save_config", HTTP_GET, handleSaveConfig); 
    server.on("/restart", HTTP_GET, handleRestart); 
    server.on("/factory_reset", HTTP_GET, handleFactoryReset);
    
    // --- NUEVAS RUTAS DE GESTIÓN DE ARCHIVOS SD ---
    server.on("/file_manager", HTTP_GET, handleFileManager);
    server.on("/delete", HTTP_GET, handleFileDelete);
    // --- NUEVAS RUTAS DE GESTIÓN DE ARCHIVOS SD ---
    server.on("/file_manager", HTTP_GET, handleFileManager);
    server.on("/delete", HTTP_GET, handleFileDelete);
    // 🛑 NUEVA RUTA: Para crear carpetas
    server.on("/create_dir", HTTP_GET, handleCreateDir); 
    // Para la subida, se utiliza una sintaxis especial para manejar archivos POST.
    server.on("/upload", HTTP_POST, [](){ server.send(200, "text/plain", "Subida completada."); }, handleFileUpload);
    
    // --- RUTAS OTA ---
    server.on("/ota", HTTP_GET, handleOTA);
    server.on("/ota_upload", HTTP_POST, [](){ server.send(200, "text/plain", "Subida OK. Instalando..."); }, handleOTAUpload);
    
    server.onNotFound(notFound);

    server.begin();
    Serial.println("Servidor HTTP iniciado.");
}

void loop() {
    server.handleClient();
    
    // Solo intentar ejecutar modos si el display ha sido inicializado con éxito
    if (display) { 
        switch (config.playMode) {
            case 0:
                ejecutarModoGif();
                break;
            case 1:
                ejecutarModoTexto();
                break;
            case 2:
                ejecutarModoReloj();
                break;
            default:
                display->fillScreen(0);
                break;
        }
    }
    delay(1); 
}
