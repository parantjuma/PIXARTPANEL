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
        showIPOnlyOnce=true; // Hemos conectado con wifi y mostraremos la ip una unica vez en modo info 
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
// La asignación de memoria (new)
    display = new MatrixPanel_I2S_DMA(matrix_config);
    if (display) { 
        display->begin();
        display->setBrightness8(config.brightness);
        display->fillScreen(display->color565(0, 0, 0));
// Mostrar estado inicial
        Serial.println("Estatus setup...");
        /*if (!sdMontada) {
            mostrarMensaje("SD Error!", display->color565(255, 0, 0));
        } else if (WiFi.status() == WL_CONNECTED) {
            mostrarMensaje("WiFi OK!", display->color565(0, 255, 0));
        } else {
             mostrarMensaje("AP Mode", display->color565(255, 255, 0));
        }*/
        // Aqui podria mostrar la ip de conexion unos segundos y luego continuar
        //String ipStr = WiFi.localIP().toString();
        //mostrarMensaje(ipStr.c_str(), display->color565(255, 255, 0));
        //mostrarMensaje("Iniciando sistema..", display->color565(255, 255, 0));
        //if (config.playMode == 0) {
            logHeap("Antes buildGifIndexFixedArray");
            listarArchivosGif();
            logHeap("Después buildGifIndexFixedArray");
       // }
        delay(1000);
    } else {
        Serial.println("ERROR: No se pudo asignar memoria para la matriz LED.");
    }

    Serial.printf("--- FIN SETUP VERSION %s %s \n",FIRMWARE_VERSION,FIRMWARE_DATE);

}

void loop() {
    wm.process();   // En caso de esar activo el portal cautivo permite configurar el wifi mientras reproduce gif

    // Solo ejecutamos una vez la asignación de nombre DNS
    if (WiFi.status() == WL_CONNECTED) {

        if(modoAP)
        {
            // Forzamos modo info para mostrar la ip configurada independientemente del modo que estuviera activo
            config.playMode=3;
            modoAP=false;

        }
        if(!DNSCONFIG)
        {
            
            if (MDNS.begin(config.device_name)) {
                DNSCONFIG=true;
                Serial.println("mDNS iniciado");
            }

        }
    }

    server.handleClient();  // Ejecutamos iteracion del servidor web
    yield(); 

    // Solo intentar ejecutar modos si el display ha sido inicializado con éxito
    if (display) { 
        if(showIPOnlyOnce)
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
