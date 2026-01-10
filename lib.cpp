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




