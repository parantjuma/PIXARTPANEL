#include "config.h"
#include "dataConfig.h"
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