#include "config.h"
#include "webManage.h"
#include "webUI.h"
#include "files.h"
#include "lib.h"
#include "gifPanelDraw.h"
#include "dataConfig.h"
// ====================================================================
//                      MANEJADORES HTTP  
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


    
    config.showLogo = (server.arg("showLogo") == "1"); 
    if (server.hasArg("logoFrecuence")) config.logoFrecuence = server.arg("logoFrecuence").toInt();
    config.batoceraLink = (server.arg("batoceraLink") == "1"); 
    

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
    

    
    if (display) display->setBrightness8(config.brightness);
    savePlaybackConfig(); // Esto serializa el vector en activeFolders_str
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Guardado");

    // Listar archivos gif hará un reset, por temas de memoria, con lo cual actualizamos al final una vez almacenados correctamente los 
    // parametros de configuracion
    if (config.playMode == 0) {
        listarArchivosGif();
        // Recargar GIFs inmediatamente si cambiamos el modo/carpetas
    }    
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

                //Se verifica si currentPath es una de las rutas 
                //almacenadas en config.activeFolders (vector)
                // Si es asi, entonces hemos de añadir el archivo al fichero
                // cache para que muestre el gif, caso contrario
                // unicamente dejamos el archivo subido sin afectar a la cache 

                if(isActiveFolder(currentPath))
                {
                    String uploadPath = currentPath + upload.filename;
                    addNewGifInCacheFile(uploadPath.c_str());
                    //listarArchivosGif();  // Descartamos volver a listar los archivos gif por cada subida, seria
                    // un proceso muy lento si hay carpetas con muchos gif.

                    /*
                    
                            Modificamos el funcionamiento de la siguiente forma.

                            - Se abre el archivo  GIF_CACHE_FILE "/gif_cache.txt"
                            - Añadimos nueva ruta de entrada uploadPath.c_str()                        
                            - Incrementamos el numero de gif numGifs
                    
                    */
                }
                server.sendHeader("Location", String("/file_manager?path=") + currentPath);
                server.send(302, "text/plain", "Redirigiendo...");

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

    // Y la página actual
    uint16_t currentPage = server.hasArg("page") 
    ? server.arg("page").toInt() 
    : 1;
    if (currentPage < 1) currentPage = 1;
    

    // Asegurar que la ruta siempre termine con / (excepto si es solo "/")
    if (requestedPath != "/" && !requestedPath.endsWith("/")) {
        requestedPath += "/";
    }
    
    // Asegurar que la ruta siempre empiece con /
    if (!requestedPath.startsWith("/")) {
        requestedPath = "/" + requestedPath;
    }

    currentPath = requestedPath;
    
    // Calculamos cuantos archivos tiene la página para calcular la paginación
    uint16_t totalFiles = 0;
    Serial.println("Iniciamos cuenta de ficheros en directorio");
    File countDir = SD.open(currentPath.c_str());
    File countFile = countDir.openNextFile();

    while (countFile) {
       //  String filename = countFile.name();
        // Solo mostrar el nombre del archivo dentro del directorio actual
       // int lastSlash = filename.lastIndexOf('/');
       // filename = filename.substring(lastSlash + 1); 
        
       // if (filename.length() > 0 && filename != ".") { totalFiles++; }
       totalFiles++;
        countFile.close();
        countFile = countDir.openNextFile();
    }
    countDir.close();
    Serial.printf("Fin cuenta de ficheros en directorio. total:%d \n",totalFiles);
    uint16_t totalPages = (totalFiles + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (currentPage > totalPages) currentPage = totalPages;
    uint16_t startIndex = (currentPage - 1) * ITEMS_PER_PAGE;
    uint16_t endIndex   = startIndex + ITEMS_PER_PAGE;

    // Abrir el directorio actual
    File root = SD.open(currentPath.c_str());
    if (!root || !root.isDirectory()) {
        server.send(404, "text/html", "<h2>Error: Ruta no encontrada o no es un directorio.</h2>");
        return;
    }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent(F("<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>"+PRJ_NAME_DEFAULT+" - Archivos SD</title>"));
    server.sendContent(F("<style>body{font-family:Arial,sans-serif;background:#2c3e50;color:#ecf0f1;margin:0;padding:20px}h1{color:#f39c12}a{color:#3498db;text-decoration:none}a:hover{text-decoration:underline}.container{max-width:800px;margin:auto}.menu{margin-bottom:20px}.menu button,.menu a,.file-item button{background:#34495e;color:white;border:none;padding:10px 15px;cursor:pointer;border-radius:5px;text-decoration:none;display:inline-block;margin-right:5px}.menu button:hover,.menu a:hover,.file-item button:hover{background:#2c3e50}.file-item{display:flex;justify-content:space-between;align-items:center;padding:10px;border-bottom:1px solid #34495e}.file-item:nth-child(even){background:#243647}.file-item a{flex-grow:1;margin-right:10px}.dir{color:#f1c40f}.file{color:#bdc3c7}.current-path{background:#1abc9c;padding:5px 10px;border-radius:5px;display:block;margin-bottom:15px}.upload-form,.dir-form{margin-top:20px;padding:15px;border:1px solid #3498db;border-radius:5px}.upload-form input[type=file],.dir-form input[type=text]{padding:10px;border:1px solid #34495e;border-radius:5px;margin-right:10px;background:#2c3e50;color:#ecf0f1}.dir-form input[type=submit]{margin-top:10px}</style></head><body><div class='container'>"));
        
    // --- ENCABEZADO Y NAVEGACIÓN ---
    server.sendContent(F("<h1><a href='/'>🏠 "+PRJ_NAME_DEFAULT+"</a></h1><h2>📁 Archivos SD</h2>"));
    
    // Mostrar la ruta actual
    server.sendContent(F("<div class='current-path'>Ruta Actual: <strong>"));
    server.sendContent(currentPath);
    server.sendContent(F("</strong></div>"));


    // Botón de Volver
    if (currentPath != "/") {
        // Calcular la ruta: quita el último segmento y la barra final
        String parentPath = currentPath.substring(0, currentPath.lastIndexOf('/', currentPath.length() - 2) + 1);
        
        server.sendContent(F("<div class='menu'><a href='/file_manager?path="));
        server.sendContent(parentPath);
        server.sendContent(F("'>⬅️ Subir Directorio</a></div>"));
    }
    // --- PAGINADOR ---
    Serial.println("Pintamos paginador");
    /*server.sendContent(F("<div style='margin-top:20px;text-align:center;'>"));
    
    for (uint16_t p = 1; p <= totalPages; p++) {

        if (p == currentPage) {
            server.sendContent(F("<span style='padding:8px 12px;margin:2px;"
                                "background:#1abc9c;color:black;"
                                "border-radius:5px;font-weight:bold;'>"));
            server.sendContent(String(p));
            server.sendContent(F("</span>"));
        } else {
            server.sendContent(F("<a style='padding:8px 12px;margin:2px;"
                                "background:#34495e;color:white;"
                                "border-radius:5px;text-decoration:none;' href='/file_manager?path="));
            server.sendContent(currentPath);
            server.sendContent(F("&page="));
            server.sendContent(String(p));
            server.sendContent(F("'>"));
            server.sendContent(String(p));
            server.sendContent(F("</a>"));
        }
    }
    */
    // Paginador flex wrap
    server.sendContent(F("<div style='display:flex; flex-wrap:wrap; margin:5px 0;'>"));
    for (uint16_t p = 1; p <= totalPages; p++) {
        if (p == currentPage) {
            server.sendContent(F("<span style='padding:8px 12px;margin:2px;"
                                "background:#1abc9c;color:black;"
                                "border-radius:5px;font-weight:bold;'>"));
            server.sendContent(String(p));
            server.sendContent(F("</span>"));
        } else {
            server.sendContent(F("<a style='padding:8px 12px;margin:2px;"
                                "background:#34495e;color:white;"
                                "border-radius:5px;text-decoration:none;' href='/file_manager?path="));
            server.sendContent(currentPath);
            server.sendContent(F("&page="));
            server.sendContent(String(p));
            server.sendContent(F("'>"));
            server.sendContent(String(p));
            server.sendContent(F("</a>"));
        }
    }
    server.sendContent(F("</div>"));


    //server.sendContent(F("</div>"));

    // --- LISTADO DE ARCHIVOS ---
    server.sendContent(F("<h3>Contenido del Directorio:</h3>"));
    Serial.println("Comenzamos a pintar los archivos");
    uint16_t gifIndex = 0; // Controlamos que indice de fichero estamos viendo
    File file = root.openNextFile();
    while(file){
        
        //if (filename.length() > 0 && filename != ".") { // Evitar "." y nombres vacíos
            if (gifIndex >= startIndex && gifIndex < endIndex) {  // solo mostramos los archivos de la página obtenida por argumento
                    String filename = file.name();
                    // Solo mostrar el nombre del archivo dentro del directorio actual
                    int lastSlash = filename.lastIndexOf('/');
                    filename = filename.substring(lastSlash + 1); 

                    server.sendContent(F("<div class='file-item'>"));
                    
                    if (file.isDirectory()){
                        // Enlace a la carpeta
                        server.sendContent(F("<a class='dir' href='/file_manager?path="));
                        server.sendContent(currentPath);
                        server.sendContent(filename);
                        server.sendContent(F("/'>"));
                        server.sendContent(F("📂 "));
                        server.sendContent(filename);
                        server.sendContent(F("</a>"));

                        // Botón Borrar Directorio
                        server.sendContent(F("<button onclick='confirmDelete(\""));
                        server.sendContent(filename);
                        server.sendContent(F("\", \"dir\")'>🗑️ Borrar</button>"));
                        
                    } else {
                        // Nombre del archivo
                        server.sendContent(F("<span class='file'>"));
                        server.sendContent(F("📄 "));
                        server.sendContent(filename);
                        server.sendContent(F("</span>"));

                        // Botón Borrar Archivo
                        server.sendContent(F("<button onclick='confirmDelete(\""));
                        server.sendContent(filename);
                        server.sendContent(F("\", \"file\")'>🗑️ Borrar</button>"));
                    }
                    server.sendContent(F("</div>"));
            }
            gifIndex++;
        //}
        file.close(); // Liberar el recurso del archivo actual
        if(gifIndex > endIndex)
        {
            Serial.println("Dejamos de pintar los archivos por break");
             break; // salimos del bucle cuando ya superamos el indice final
        }
         
        file = root.openNextFile(); // Pasar al siguiente archivo
    }
    Serial.println("Listado finalizado");
    root.close(); // Cerrar el directorio raíz

    // --- FORMULARIO CREAR CARPETA ---
    server.sendContent(F("<div class='dir-form'><h3>Crear Nueva Carpeta</h3>"));
    server.sendContent(F("<form action='/create_dir' method='GET'><input type='text' name='name' placeholder='Nombre de la carpeta' required><input type='submit' value='Crear Carpeta'></form></div>"));


    // --- FORMULARIO SUBIR ARCHIVO ---
    server.sendContent(F("<div class='upload-form'><h3>Subir Archivo (al directorio actual)</h3>"));
    server.sendContent(F("<form method='POST' action='/upload' enctype='multipart/form-data'>"));
    server.sendContent(F("<input type='file' name='data' multiple required><br>"));
    server.sendContent(F("<input type='submit' value='Subir Archivo'></form></div>"));

    // --- SCRIPT DE BORRADO Y CIERRE ---
    server.sendContent(F("<script>function confirmDelete(name, type) { if (confirm('¿Estás seguro de que quieres borrar ' + (type === 'dir' ? 'la carpeta' : 'el archivo') + ' \"' + name + '\"?')) { window.location.href = '/delete?name=' + name + '&type=' + type; } }</script>"));
    server.sendContent(F("</div></body></html>"));

}