#include "config.h"
#include "lib.h"

// ====================================================================
//                     INFORMACION ESTADO HEAP MEMORIA
// ====================================================================
void logHeap(const char* tag) {
    Serial.printf("[%s] Heap libre: %u | Heap mínimo: %u | PSRAM: %u\n",
        tag,
        ESP.getFreeHeap(),
        ESP.getMinFreeHeap(),
        ESP.getFreePsram()
    );
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
//                      MUESTRA POR SERIAL EL ESTADO DE CONFIG
// ====================================================================
void printConfigInfo() {
    Serial.println();
    Serial.println(F("=== CONFIG INFO ==="));

    // 1. Controles de Reproducción
    Serial.print(F("brightness: "));
    Serial.println(config.brightness);

    Serial.print(F("playMode: "));
    Serial.println(config.playMode);

    Serial.print(F("slidingText: "));
    if (config.slidingText.length() > 0) {
        Serial.println(config.slidingText);
    } else {
        Serial.println(F("(null)"));
    }

    Serial.print(F("textSpeed: "));
    Serial.println(config.textSpeed);

    Serial.print(F("gifRepeats: "));
    Serial.println(config.gifRepeats);

    Serial.print(F("randomMode: "));
    Serial.println(config.randomMode ? F("true") : F("false"));

    // activeFolders (vector)
    Serial.print(F("activeFolders: "));
    if (config.activeFolders.empty()) {
        Serial.println(F("(empty)"));
    } else {
        Serial.println();
        for (size_t i = 0; i < config.activeFolders.size(); i++) {
            Serial.print(F("  ["));
            Serial.print(i);
            Serial.print(F("]: "));
            Serial.println(config.activeFolders[i]);
        }
    }

    Serial.print(F("activeFolders_str: "));
    if (config.activeFolders_str.length() > 0) {
        Serial.println(config.activeFolders_str);
    } else {
        Serial.println(F("(null)"));
    }

    // 2. Configuración Hora / Fecha
    Serial.print(F("timeZone: "));
    if (config.timeZone.length() > 0) {
        Serial.println(config.timeZone);
    } else {
        Serial.println(F("(null)"));
    }

    Serial.print(F("format24h: "));
    Serial.println(config.format24h ? F("true") : F("false"));

    // 3. Modo Reloj
    Serial.print(F("clockColor: 0x"));
    Serial.println(config.clockColor, HEX);

    Serial.print(F("showSeconds: "));
    Serial.println(config.showSeconds ? F("true") : F("false"));

    Serial.print(F("showDate: "));
    Serial.println(config.showDate ? F("true") : F("false"));

    // 4. Texto deslizante
    Serial.print(F("slidingTextColor: 0x"));
    Serial.println(config.slidingTextColor, HEX);

    // 5. Hardware / Sistema
    Serial.print(F("panelChain: "));
    Serial.println(config.panelChain);

    Serial.print(F("device_name: "));
    if (config.device_name[0] != '\0') {
        Serial.println(config.device_name);
    } else {
        Serial.println(F("(null)"));
    }

    Serial.println(F("=== CONFIG INFO END ==="));
    Serial.println();
}
