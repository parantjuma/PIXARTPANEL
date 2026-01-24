#include <Arduino.h>
#include "config.h"         // Librerias y configuracion programa
#include "dataConfig.h"     // Lib para guardar y recuperar configuracion 
#include "lib.h"            // Funciones varias de sistema
#include "webManage.h"      // Web Handlers 
#include "webUI.h"          // Web pages
#include "files.h"          // Acceso a archivos almacenados en SD
#include "gifPanelDraw.h"   // Pintado de gif
#include "playModes.h"      // Modoes de reproducción
//#include <ESPmDNS.h>

// ====================================================================
//                             SETUP Y LOOP
// ====================================================================

void setup() {
     
    numGifs=0;  // Al incializar, el numero de gif cargados es 0
    DNSCONFIG=false;

    Serial.begin(115200);
    Serial.printf("--- INICIAMOS SETUP VERSION %s %s \n",FIRMWARE_VERSION,FIRMWARE_DATE);
    logHeap("Inicio setup");
    if (!SPIFFS.begin(true)) {
        Serial.println("Error al montar SPIFFS.");
    }

    loadConfig();
    //config.playMode=1; // fuerzo modo texto para ver la ip
    printPreferencesInfo();
    printConfigInfo();

// 1. Inicialización de la SD 
    SPI.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Error al montar la tarjeta SD!");
        sdMontada = false;
        delay(100);
    } else {
        Serial.println("Tarjeta SD montada correctamente.");
        sdMontada = true;
        // Cargamos la lista de logos (si existen) desde /logos
        loadLogoGifsFromSD();
        scanFolders(GIFS_BASE_PATH);
// Escanear carpetas para la UI
    }
    
    gif.begin(LITTLE_ENDIAN_PIXELS);
// 2. Conexión WiFi y Servidor 
    
    if (config.device_name[0] == '\0') {
        strncpy(config.device_name, DEVICE_NAME_DEFAULT, sizeof(config.device_name) - 1);
        config.device_name[sizeof(config.device_name) - 1] = '\0'; 
    }

    /*
        
        WiFi.mode(WIFI_AP_STA);
        WIFI_AP	Crea red WiFi propia
        WIFI_STA	Se conecta a un router

        WIFI_AP_STA Ambos a la vez, no uno u otro.

        - Portal cautivo activo
        - Conexión WiFi normal cuando sea posible

    */

    wm.setHostname(config.device_name);
    WiFi.mode(WIFI_AP_STA); 
    wm.setConfigPortalBlocking(false);  // Establece el modo wifi como no bloqueante => permite ejecutar gif con portal
    wm.setSaveConfigCallback(nullptr);  // Desactiva posible callback cuando se configura el wifi

    Serial.println("Intentando conectar o iniciando portal cautivo...");
    delay(500);
    // Intentamos conectar como STA (conectar a un router) y devuelve true si lo logra
    // Autoconnect es bloqueante mientras el portal cautivo esta activo
    if (!wm.autoConnect(WIFI_DEFAULT)) { 
       // Serial.println("Fallo de conexión y timeout del portal. Reiniciando...");
       // delay(3000);
       // ESP.restart();

       // Cambiamos comportamiento, no reiniciamos, permitimos inicio del panel, y vamos ejecutando wm para que continue
       // ejecutando el portal cautivo hasta que conecte a wifi. 
       //
       // Lo unico que ocurre que el servidor web será inalcanzable mientras no haya wifi.
        Serial.println("Fallo de conexión y timeout del portal en SETUP.");
        modoAP=true; // Dado que nos ha dado fallo la conexión wifi indicamos que estamos en modo AP
    } 
    else
    {
        
    }

    Serial.println("\nConectado a WiFi.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    logHeap("Tras WiFi");

    /*
        El modo AP y el portal cautivo funcionan en modo bloqueante, es decir, 
        paraliza la ejecución del SETUP y no llega nunca al loop. 

        Al ocurrir esto, cuando se mueve el dispositivo de zona y pierde la wifi 
        la animación del panel deja de funcionar. 

        Realmente, no es necesario un bloqueo de funcionamiento solo por perder
        el control de la gestión del panel via HTTP.  Lo ideal en estos casos
        es:


            - Si no conecta correctamente en modo STA (router configurado) 
              debe de mantener activo el AP para poder configurarlo.
            - El panel de mientras deberia mostrar la ultima configuracion
               (reloj, texto o imagenes)
            - Cuando se configure correctamente la wifi conectamos con el router
    
        Para ello 

        Usar WiFiManager en modo no bloqueante:
    
        setup

            wm.setConfigPortalBlocking(false);
            wm.startConfigPortal("Retro Pixel LED");

        loop
        
        void loop() {
            wm.process();       // Atiende portal
            actualizarPanel();  // Animaciones
        }
    
        Es posible levantar correctamente el servidor web durante el modo AP
        ya que no es dependiente de a wifi a la cual este conectada.
        
        Si es cierto que no será posible acceder al servidor web mientras este el portal cautivo
        Esto es debido a que todas las llamadas a la ip son capturadas por el portal
        y no permite acceder a las url del servidor. 

        Posible solución cambiar el puerto del servidor de conexión (8080) pero es
        menos intuitivo.


    */

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
  //  server.on("/file_manager", HTTP_GET, handleFileManager);
  //  server.on("/delete", HTTP_GET, handleFileDelete);
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
    logHeap("Tras server.begin()");

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

    /*
        INFORMACION UTIL LIBRERIA mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display@^3.0.11
        ======================================================================================

        I2S en modo paralelo + DMA

        La librería usa el periférico I2S del ESP32 para generar un bus paralelo de alta velocidad hacia el HUB75.
        Por tanto:
        Hay CLK, LAT, OE, R/G/B, A/B/C/D/E

        La “velocidad” que ajustas es frecuencia del reloj I2S
        https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
        https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/wiki



        i2sspeed (esto es “la velocidad”)
        ---------------------------------
        matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_20M;


        Rangos reales (prácticos):

        Valor	Comentario
        8 MHz	Ultra estable, bajo brillo
        10 MHz	Buen equilibrio
        16 MHz	Muy usado
        20 MHz	🔥 Límite práctico (calidad cableado/importante)

        👉 Más no siempre es mejor
        A partir de 20 MHz:

        cables
        longitud
        calidad del panel
        empiezan a provocar ruido/ghosting.


        min_refresh_rate
        ----------------
        matrix_config.min_refresh_rate = 120;

        Esto es CLAVE para GIFs:

        < 60 → flicker visible
        60–90 → aceptable
        120 → perfecto
        180 → puede penalizar color depth


       Latch_blanking (anti-ghosting)
       ------------------------------
       matrix_config.latch_blanking = config.latchBlanking;

        Valores típicos:

        1 → rápido, más ghosting
        2–3 → equilibrio
        4 → paneles problemáticos

        📌 Cada modelo de panel es distinto → este parámetro es oro 


        clkphase
        --------
        matrix_config.clkphase = false;

        Esto:

        - cambia el flanco de captura
        - reduce píxeles fantasma
        - suele depender del panel
        👉 Correctísimo que lo tengas configurable.


        double_buff
        -----------
        matrix_config.double_buff = true;


        Recomendación:
        ✅ siempre ON para GIFs
        consume más RAM
        evita tearing
        Tú ya lo sabes 😄        

        Qué es DMA_FastRefresh?
        -----------------------

        DMA_FastRefresh no es una opción mágica, es un ejemplo avanzado incluido en la librería
        ESP32 HUB75 LED MATRIX PANEL DMA Display

        👉 Su objetivo es maximizar la tasa de refresco real del panel HUB75 usando:

        I2S a alta frecuencia
        DMA agresivo
        reducción de trabajo por frame
        sacrificando algunas cosas (si no tienes cuidado)
        Es básicamente el modo “overclock consciente” de la librería.

        Como forzarlo

        matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_16M o 20M;
        matrix_config.min_refresh_rate = 120;
        matrix_config.double_buff = true;
        matrix_config.latch_blanking = 2 o 3;
        matrix_config.clkphase = false;

    */

    /*
    // --- APLICACIÓN DE AJUSTES AVANZADOS  ---  ( ajustes aplicados en Retro Pixel LED)
    
    // 1. Velocidad I2S (Mapeo del índice 0-3 a las constantes de la librería)
    if (config.i2sSpeed == 0)      matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_8M;
    else if (config.i2sSpeed == 1) matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    else if (config.i2sSpeed == 2) matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_16M;
    else if (config.i2sSpeed == 3) matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    else matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    // 2. Latch Blanking (Anti-Ghosting)
    // El rango válido 1-4.
    matrix_config.latch_blanking = config.latchBlanking;

    // 3. Tasa de Refresco Mínima
    // Si por error viene un 0, forzamos 60Hz.
    if (config.minRefreshRate < 30) config.minRefreshRate = 60;
    matrix_config.min_refresh_rate = config.minRefreshRate;

    // 4. Doble Buffer (Siempre activo para GIFs)
    //matrix_config.double_buff = true;

    // 5. Sincronización
    matrix_config.clkphase = false;

    // Crear el objeto Display con la nueva configuración
    display = new MatrixPanel_I2S_DMA(matrix_config);
*/

    matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_16M;
    //matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    matrix_config.min_refresh_rate = 120;
    //matrix_config.double_buff = true;  // no lo soporta este software probablemente por memoria, quizas con S3
    matrix_config.latch_blanking = 2;
    //matrix_config.latch_blanking = 3;
    matrix_config.clkphase = false;    // ELIMINA PIXELES FANTASMAS

    
// La asignación de memoria (new)
    display = new MatrixPanel_I2S_DMA(matrix_config);
    if (display) { 
        display->begin();
        display->setBrightness8(config.brightness);
        display->fillScreen(display->color565(0, 0, 0));
        // Si tenemos la DS montada activamos  el modo GIF
        if(sdMontada)
        {
            // Mostrar estado inicial
            Serial.println("Estatus setup...");
            logHeap("Antes buildGifIndexFixedArray");
            listarArchivosGif();
            logHeap("Después buildGifIndexFixedArray");
            //delay(1000);
        }
        else
        {
            // Deberiamos avisar que no tenemos la SD montada
        }
    } else {
        Serial.println("ERROR: No se pudo asignar memoria para la matriz LED.");
    }
    showInfoOnlyOnce=true; // Activamos el mensaje de información de arranque 
    xPosMarquesina = display->width(); 
    Serial.printf("--- FIN SETUP VERSION %s %s \n",FIRMWARE_VERSION,FIRMWARE_DATE);

}

void loop() {
    wm.process();   // En caso de esar activo el portal cautivo permite configurar el wifi mientras reproduce gif

    // Intentamos asignar la DNS hasta que nos de el ok en cada bucle
    // Ademas, en caso de pasar de modo AP a modo wifi reiniciamos => TEST: verificar si al reiniciar con la wifi conectada funciona correctamente
    // la asignacion de dominio a la
    if (WiFi.status() == WL_CONNECTED) {
        if(!DNSCONFIG)
        {
            if (MDNS.begin(config.device_name)) {
                DNSCONFIG=true;
                Serial.println("mDNS iniciado");
               // ESP.restart();
            }
        }
        if(modoAP)
        {
            modoAP=false;
            ESP.restart(); // Reiniciamos cuando pasamos de modo AP a modo wifi para comprobar si asigna dominio a la primera. 
                           // Este reset podemos eliminarlo a futuro.
        }

    }

    server.handleClient();  // Ejecutamos iteracion del servidor web
    yield(); 

    // Solo intentar ejecutar modos si el display ha sido inicializado con éxito
    if (display) { 
        if(showInfoOnlyOnce)
        {
            ejecutarModoInfo(); // Indiferentemente del modo configurado mostramos el info una pasada
        }
        else
        {
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
                case 3: 
                    ejecutarModoInfo();
                    break;
                default:
                    display->fillScreen(0);
                    break;
            }
            
        }

    }
    delay(1); 
}
