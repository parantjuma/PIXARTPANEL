#include "config.h"
#include "files.h"

// ====================================================================
//                      CARGA DE LOGOS DESDE SD
// ====================================================================

void loadLogoGifsFromSD(const String& logosBasePath) {
    gifLogos.clear();
    if (!sdMontada) {
        Serial.println("[LOGOS] SD no montada, no se cargan logos.");
        return;
    }

    if (!SD.exists(logosBasePath.c_str())) {
        Serial.printf("[LOGOS] Carpeta no existe: %s\n", logosBasePath.c_str());
        return;
    }

    File dir = SD.open(logosBasePath.c_str());
    if (!dir || !dir.isDirectory()) {
        Serial.printf("[LOGOS] No se pudo abrir como directorio: %s\n", logosBasePath.c_str());
        if (dir) dir.close();
        return;
    }

    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = String(entry.name());
            String lower = name;
            lower.toLowerCase();
            if (lower.endsWith(".gif")) {
                // Aseguramos ruta absoluta desde el root de la SD.
                String fullPath = logosBasePath;
                if (!fullPath.endsWith("/")) fullPath += "/";
                // entry.name() puede devolver ruta relativa al directorio o absoluta dependiendo de la lib;
                // nos quedamos con el nombre de archivo.
                int lastSlash = name.lastIndexOf('/');
                String fileName = (lastSlash >= 0) ? name.substring(lastSlash + 1) : name;
                fullPath += fileName;
                gifLogos.push_back(fullPath);
            }
        }
        entry.close();
    }
    dir.close();

    // Orden estable para que el ciclo sea predecible.
    std::sort(gifLogos.begin(), gifLogos.end());

    Serial.printf("[LOGOS] Cargados %d logos desde %s\n", (int)gifLogos.size(), logosBasePath.c_str());
}

// ====================================================================
//       FUNCIONES AÑADIDAS PARA OPTIMIZACIÓN EN USO MEMORIA
// ====================================================================
/*
 Dado que la ESP32 dispone de un heap de memoria muy pequeño no es posible indexar una gran cantidad 
 de rutas en memoria enlazadas por medio de una lista.  Por ello, hacemos uso unicamente de la información
 almacenada en el archivo GIF_CACHE_FILE ("/gif_cache.txt")

 La coleccion actual de gif ronda las 11.000 entradas, lo cual tambien implica mucho recorrido en la lectura
 del archivo para el acceso a cada gif (cada ruta es una linea, habria que leer 11.000 lineas para llegar
 al ultimo gif). Por ello hacemos uso de una cache de posicionamientos sobre el archivo el cual se carga
 solo una vez en el SETUP.

 [cache por bloques de rutas]
 ............................
  
 Por temas de volumen y fragmentacion de memoria esta cache de posiciones del fichero lo almacenamos en el
 array gifOffsets. Este se inicializa en el SETUP y no se refresca en caliente (hacemos un reset al cambiar
 la seleccion de carpetas gif) para evitar fragmentación de memoria y evitar asi errores colaterales e 
 impredecibles.


 Otro ajuste es que no se almacena la posicion de cada ruta en el fichero, esto se realiza por bloques de
 un número definido de rutas (GIF_OFFSETS_BLOCK_SIZE), lo cual reduce el tamaño total del array (ej:con pruebas
 en la carga de los 11.000 gif el servidor http deja de responder y requiere mayor optimizacion). Esto seria
 un equilibrio entre tiempos de acceso y carga de memoria.

 Con el array de posiciones de fichero gifOffsets unicamente tenemos acceso a un listado enumerado de rutas, 
 pero desconocemos a que carpeta estamos accediendo. Una posible mejora es tener un array bidimensional
 para poder controlar la carpeta a la cual estamos accediendo (ya que carpetas no suele haber muchas). Esta 
 mejora no esta aplicada actualmente pero será necesaria si añadimos la posibilidad de incluir un % de probabilidad
 de aparición de gif por carpetas, y de esta forma, controlar cuantos gif saldran de cada una de las carpetas
 (tematicas).

 [reinicio tras carga de cache]
 ..............................

 En caso de recargar el array gifOffsets necesitariamos liberar la memoria almacenada por este y volver a 
 localizar un bloque de memoria nuevo. Esto, con la mala gestión de memoria que se realiza en la ESP32, puede
 generar errores impredecibles. 

 Como solucion preventiva, dado que no afecta funcionalmente en exceso, se guarda la configuración y se resetea
 el sistema para una carga limpia y ordenada de los fragmentos en memoria.
 

 [añadir gif]
 ............

 Al utilizar un array de posiciónes a fichero añadir un nuevo fichero nos supone un problema:
 - La ruta del nuevo gif se agregará al final de la lista del fichero GIF_CACHE_FILE ("/gif_cache.txt")
 - En la estructura de datos gifOffsets al ser un array fijo no podemos incrementar las posiciones. Con lo cual.
    - No se subiran manualmente muchos ficheros
    - Controlaremos el total de GIF, el cual aumentará al subir un nuevo fichero.
    - Si accedemos a un fichero fuera del alcance de los offsets almacenados en la cache, se recorreran las lineas
      secuencialmente desde el ultimo offset guardado hasta alcanzar a la posición añadida.
    - Con lo cual, la posición del ultimo bloque de rutas puede tener un numero de rutas ilimitado.
    - En la siguiente carga la cache se restaurará correctamente
En caso de aplicar la mejora de array bidimensionales (con conocimiento de directorios) se aplicaria el incremento del 
nuevo archivo dentro del subarray de la carpeta correspondiente.


[eliminar gif]
..............

Al eliminar un gif nos encontramos con otro tipo de problemas
    - Seria necesario reescribir todo el fichero GIF_CACHE_FILE ("/gif_cache.txt") sin la ruta eliminada
    - Seria necesario recargar la cache de posiciones al archivo gifOffsets
    - El tiempo de computo es alto y por temas de fragmentación de memoria requeriria reiniciar el sistema
      para una carga limpia en memoria sin fragmentacion.


Para evitar esto simplemente haremos lo siguiente:
    - Se obtiene la ruta y se comprueba si existe el archvio
    - Si el archivo no existe accedemos al archivo en la siguiente posición.
    - En caso de ser el ultimo archivo de la lista accederiamos al primer archivo (acceso rotativo)

Dado que los accesos son o aleatorios o secuenciales esta solución seria adecuada.
Una vez se vuelva a generar el fichero GIF_CACHE_FILE ("/gif_cache.txt") todo quedaria indexado correctamente.

En caso de aplicar la mejora de arrays bidimensionales esto afectaria dentro del array de la carpeta donde esta
accediendo, es decir, siguiente imagen dentro del subarray, y en caso de ser el ultimo de la lista ir al primer
gif de la carpeta (con la misma temática)


(Captura de logs y espacio de memoria antes y despues de ajuste de bloques)

18:03:52.662 -> --- INICIAMOS SETUP VERSION 2.0.9.MEMOPT_PAG_1.15 4_1_2026_19_55 
18:03:52.662 -> [Inicio setup] Heap libre: 252712 | Heap mínimo: 246844 | PSRAM: 0
18:03:57.645 -> [Tras WiFi] Heap libre: 163080 | Heap mínimo: 159452 | PSRAM: 0
18:04:02.128 -> [Tras server.begin()] Heap libre: 159888 | Heap mínimo: 159424 | PSRAM: 0
18:04:04.162 -> [Antes buildGifIndexFixedArray] Heap libre: 85564 | Heap mínimo: 83008 | PSRAM: 0
18:04:12.350 -> countLinesInFile 11053 
                [Después buildGifIndexFixedArray] Heap libre: 40536 | Heap mínimo: 39544 | PSRAM: 0
18:04:21.911 -> --- FIN SETUP VERSION 2.0.9.MEMOPT_PAG_1.15 4_1_2026_19_55 


(Captura de logs y espacio de memoria despues de ajustes de bloques)

19:00:31.282 -> --- INICIAMOS SETUP VERSION 2.0.9.MEMOPT_PAG_1.15 4_1_2026_19_55 
19:00:31.282 -> [Inicio setup] Heap libre: 252712 | Heap mínimo: 246844 | PSRAM: 0
19:00:36.262 -> [Tras WiFi] Heap libre: 163080 | Heap mínimo: 159452 | PSRAM: 0
19:00:40.298 -> [Tras server.begin()] Heap libre: 159904 | Heap mínimo: 159424 | PSRAM: 0
19:00:42.279 -> [Antes buildGifIndexFixedArray] Heap libre: 85580 | Heap mínimo: 82748 | PSRAM: 0
19:00:50.481 -> countLinesInFile 11053 
 [Después buildGifIndexFixedArray] Heap libre: 82760 | Heap mínimo: 75108 | PSRAM: 0
19:00:59.637 -> --- FIN SETUP VERSION 2.0.9.MEMOPT_PAG_1.15 4_1_2026_19_55 


*/



// Cuenta el numero de lineas de un fichero (para conocer el tamaño del fichero cache)
// ----------------------------------------------------------------------------------------------------
uint32_t countLinesInFile() {
    File file = SD.open(GIF_CACHE_FILE);
    if (!file) return 0;

    uint32_t count = 0;
    while (file.available()) {
        file.readStringUntil('\n');
        count++;
    }
    file.close();
    Serial.printf("countLinesInFile %d \n",count);
    return count;
}

// Actualiza los valores de gifOffsets, variable en la cual se almacenan los offsets a posiciones del 
// fichero GIF_CACHE_FILE para un acceso más rapido.
//
// Se almacenara una posición por cada bloque de rutas GIF_OFFSETS_BLOCK_SIZE mas 
// ----------------------------------------------------------------------------------------------------
bool buildGifIndexFixedArray() {
    // 1. Contar líneas 
    numGifs = countLinesInFile();       // Requerrido para poder reservar la memoria necesaria
    if (numGifs == 0) return false;

    
    // 2. Reservar array exacto
    //numOffsetBlocks =  (numGifs + GIF_OFFSETS_BLOCK_SIZE - 1) / GIF_OFFSETS_BLOCK_SIZE;
    numOffsetBlocks =  numGifs  / GIF_OFFSETS_BLOCK_SIZE;
    numOffsetBlocks ++; // Ajuste posicion 0, primer indice real (gif 16) en posicion 1
    //gifOffsets = new uint32_t[numGifs+EXTRAOFFSETS];
    gifOffsets = new (std::nothrow) uint32_t[numOffsetBlocks];
    if (!gifOffsets) return false;
    //extraOffsetsUsed=0;             // Inicializamos los extraoffsets usados

    // 3. Llenar array con offsets
    File file = SD.open(GIF_CACHE_FILE);
    if (!file) {
        delete[] gifOffsets;
        gifOffsets = nullptr;
        return false;
    }

    uint32_t offset = 0;
    uint32_t blockIndex = 0;
    for (uint32_t i = 0; i < numGifs; i++) {

        if (i % GIF_OFFSETS_BLOCK_SIZE == 0) {  // Unicamente almacenamos el offset cada bloque
            gifOffsets[blockIndex] = offset;
            blockIndex++;
        }

        //gifOffsets[i] = offset;
        file.readStringUntil('\n');    // leer línea
        offset = file.position();      // guardar offset siguiente línea
    }
    Serial.printf("buildGifIndexFixedArray %d offsets (posiciones %d) para %d gif \n",numOffsetBlocks,blockIndex,numGifs);
    file.close();
    return true;
}

// Devuelve la ruta ubicada en la posicion "index"  dentro del fichdero GIF_CACHE_FILE 
// Para ello se apoya de las posiciones almacenadas en el array gifOffsets (posiciones cada GIF_OFFSETS_BLOCK_SIZE)
// Localiza el offset mas cercano para hacer seek, y luego hace un avance linea a linea
//
//  index desde 0 hasta numGifs-1  (contamos con la posicion 0)
//
//  En caso de no existir el index salta la siguiente (para casos de eliminacion de archivos por ejemplo)
//  Es valido en acceso secuencial y aleatorio ya que no afecta realmente en funcionalidad.
// ----------------------------------------------------------------------------------------------------
String getGifPathByIndex(uint32_t index)
{
    if (numGifs == 0 || !gifOffsets) return "";

    uint32_t attempts = 0;
    uint32_t currentIndex = index;

    while (attempts < numGifs)
    {
        
        uint32_t block = currentIndex / GIF_OFFSETS_BLOCK_SIZE;
        uint32_t offsetInBlock = currentIndex % GIF_OFFSETS_BLOCK_SIZE;

        // Ajuste para bloques no indexados (GIFs añadidos posteriormente)
        if (block > (numOffsetBlocks - 1)) {
            offsetInBlock += (block - (numOffsetBlocks - 1)) * GIF_OFFSETS_BLOCK_SIZE;
            block = (numOffsetBlocks - 1);
        }

        File file = SD.open(GIF_CACHE_FILE);
        if (!file) {
            Serial.println("[GIF CACHE] ERROR: No se pudo abrir GIF_CACHE_FILE");
            return "";
        }

        file.seek(gifOffsets[block]);

        String line;
        for (uint32_t i = 0; i <= offsetInBlock; i++) {
            line = file.readStringUntil('\n');
        }

        file.close();

        line.trim(); // IMPORTANTE: elimina \r, espacios y evita falsos negativos
        Serial.printf("[getGifPathByIndex] Accediendo a gif[%d] index: %s\n",currentIndex, line.c_str());
        // Validar existencia del archivo
        if (!line.isEmpty() && SD.exists(line.c_str())) {
            return line;
        }

        // Archivo inválido → avanzar índice


        currentIndex++;
        if (currentIndex >= numGifs) {
            currentIndex = 0;
        }
        attempts++;
        Serial.printf("[GIF CACHE] Ruta inválida, saltando a indice %d  (intento %d)\n", currentIndex,attempts);
    }

    // Si llegamos aquí, no hay ningún GIF válido
    Serial.println("[GIF CACHE] ERROR: No se encontró ningún GIF válido en la caché");
    return "";
}
 
// Verifica si ya esta cargado el indice de offsets
// ----------------------------------------------------------------------------------------------------
bool isInitGifOffsets(){
    
    if (gifOffsets != nullptr && numGifs > 0) {
       Serial.println("isInitGifOffsets true");
       return true;
    }
    Serial.println("isInitGifOffsets false");
    return false;
    
}
// Borra e inicializa los valores gifOffsets
// ----------------------------------------------------------------------------------------------------
bool initGifOffsets(){

    
    if (gifOffsets != nullptr && numGifs > 0) {
        delete[] gifOffsets;
        numGifs=0;              // Inicializamos los gif gastados
        //extraOffsetsUsed=0;     // Inicializamos los extraoffsets usados
        gifOffsets = nullptr;
        Serial.println("initGifOffsets inicializado");
    }
    else 
    {
        Serial.println("initGifOffsets no inicializado");
    }
    Serial.println("initGifOffsets ejecutado");
}
// Elimina el fichero GIF_CACHE_FILE
// ----------------------------------------------------------------------------------------------------
bool deleteGifCache() {
    if (!sdMontada) return false;

    if (!SD.exists(GIF_CACHE_FILE)) {
        return true;
    }

    if (!SD.remove(GIF_CACHE_FILE)) {
        Serial.println("Error al borrar cachegif.txt");
        return false;
    }

    Serial.println("cachegif.txt borrado correctamente");
    return true;
}

// TOTAL gif cargados en cache: Devolvemos el tamaño offsets cargados sumado al de casillas extra offset utilizados en subidas
// ----------------------------------------------------------------------------------------------------
size_t gifOffsetSize()
{
    //return numGifs+extraOffsetsUsed;
    return numGifs;
}

// Verifica si esta vacio el array de offsets
// ----------------------------------------------------------------------------------------------------
bool isGifOffsetEmpty()
{
    return (gifOffsetSize()==0);
}


// ====================================================================
//                  FUNCIÓN DE ESCANEO DE CARPETAS PARA LA UI
// ====================================================================

// Función para escanear y listar solo las CARPETAS dentro de un path base
// ----------------------------------------------------------------------------------------------------
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
bool isUnderBasePath(const String& path) {
    if (!path.startsWith(GIFS_BASE_PATH)) return false;

    if (path.length() == strlen(GIFS_BASE_PATH)) return true;

    return path.charAt(strlen(GIFS_BASE_PATH)) == '/';
}

// Función recursiva que escanea una ruta en busca de GIFs
// Añade en el archivo GIF_CACHE_FILE los gif que localiza
// ----------------------------------------------------------------------------------------------------
void scanGifDirectory(const String& path, File& cacheFile) {   
    Serial.printf("scanGifDirectory iniciado en path [%s] \n",path.c_str());
    //  Validación de ámbito
    if (!isUnderBasePath(path)) {
        Serial.printf(
            "ERROR: El path [%s] no cuelga de GIFS_BASE_PATH [%s]\n",
            path.c_str(),
            GIFS_BASE_PATH
        );
        return;
    }

    File root = SD.open(path);
    if (!root || !root.isDirectory()) {  // Verificamos que ademas de existir la ruta esta sea un directorio
        Serial.printf("Error al abrir directorio: %s\n", path.c_str());
        return;
    }
    File entry;
    int cont=0;
    
    while (true) {
        entry = root.openNextFile();
        if (!entry) break;
        
        String fileName = entry.name();

        if (entry.isDirectory()) {
            // Construir ruta completa del subdirectorio
            String subDirPath;
            if (path == "/") {
                subDirPath = "/" + fileName;
            } else {
                subDirPath = path + "/" + fileName;
            }

            Serial.printf("Escaneando (recursivo) subdirectorio: %s\n", subDirPath.c_str());

            // 🔁 Llamada recursiva
            scanGifDirectory(subDirPath, cacheFile);
        }
        else
        {
            if (fileName.endsWith(".gif") || fileName.endsWith(".GIF")) {

                String fullPath;
                if (path == "/") {
                    fullPath = "/" + fileName;
                } else {
                    fullPath = path + "/" + fileName;
                }
                cacheFile.println(fullPath);
                cont++;
                Serial.printf("[%d] - %s\n", cont, fullPath.c_str());

            }
        }
        entry.close();  // 🔴 IMPRESCINDIBLE
    }
    Serial.printf("fin scanGifDirectory %s ........\n",path.c_str());

    root.close();
}
// Verifica si currentPath forma parte de las config.activeFolders
// ----------------------------------------------------------------------------------------------------
bool isActiveFolder(const String& currentPath) {
    for (const auto& folder : config.activeFolders) {
        if (folder == currentPath) {
            return true;
        }
    }
    return false;
}

// Añade un nuevo archivo subido desde el servidorvoid addNewGifInCacheFile(uploadPath.c_str());
// ----------------------------------------------------------------------------------------------------
void addNewGifInCacheFile(const char* uploadPath)
{
    //listarArchivosGif();  // Descartamos volver a listar los archivos gif por cada subida, seria
    // un proceso muy lento si hay carpetas con muchos gif.

    /*
    
        Modificamos el funcionamiento de la siguiente forma.

        - Se abre el archivo  GIF_CACHE_FILE "/gif_cache.txt"
        - Añadimos nueva ruta de entrada uploadPath.c_str()                        
        - Incrementamos el numero de gif numGifs
    
    */
    if (!uploadPath || uploadPath[0] == '\0') {
        Serial.println("[GIF CACHE] Ruta vacía, no se añade");
        return;
    }

    // Verificar que el archivo existe en la SD
    if (!SD.exists(uploadPath)) {
        Serial.printf("[GIF CACHE] ERROR: El archivo no existe en SD: %s\n", uploadPath);
        return;
    }    

    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_APPEND);
    if (!cacheFile) {
        Serial.println("[GIF CACHE] Error abriendo gif_cache.txt en modo APPEND");
        return;
    }

    
    cacheFile.println(uploadPath);
    cacheFile.close();

    numGifs++;

    Serial.printf("[GIF CACHE] Añadido: %s | Total GIFs: %u\n",
                uploadPath, numGifs);                 
}
// Función auxiliar para generar la firma de la configuración actual
// ----------------------------------------------------------------------------------------------------
String generateCacheSignature() {
    String signature = "";
    // MODIFICADO: Usamos config.activeFolders que es el vector donde se guardan las carpetas seleccionadas
    for (const String& folder : config.activeFolders) { 
        signature += folder + ":"; // Concatenamos las rutas separadas por :
    }
    return signature;
}

// Función principal de listado de archivos GIF (usa la lógica de validación de firma)
// Verifica la firma (comparativa de carpetas almacenadas en firma vs indicadas en configuracion
// Si no coincide genera un nuevo fichero cache con todos los gif de las carpetas seleccionadas
// ----------------------------------------------------------------------------------------------------
void listarArchivosGif() {
    // 1. Generar la firma de la configuración actual de carpetas
    String currentSignature = generateCacheSignature();
    Serial.printf("El valor de firma almacenado en preferences es [%s] \n",currentSignature);
    // Si no hay carpetas seleccionadas, la lista debe estar vacía
    if (currentSignature.length() == 0) { 
        //archivosGIF.clear(); // ya no usamos esta estructura
        //deleteGifCache(); // Eliminamos la cache de gif (archivo). Pero no lo hacemos ahora mismo por desmarcamos y marcamos el mismo directorio
       // initGifOffsets(); // Borramos la lista de gif
       // Serial.println("No hay carpetas de GIF seleccionadas. Lista vacía.");
        return;
    }

    if ((currentSignature == "/GIFS:")||(currentSignature == "/")) {
        Serial.println("La firma coincide con el valor /GIFS o / :");
       // initGifOffsets(); // Borramos la lista de gif
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
            Serial.printf("Firmas de caché  coinciden. savedSignature [%s]  currentSignature [%s] \n",savedSignature,currentSignature);
        } else {
            Serial.printf("Firmas de caché no coinciden. La configuración ha cambiado. savedSignature [%s]  currentSignature [%s] \n",savedSignature,currentSignature);
        }
    } else {
        Serial.printf("Archivo de firma de caché no encontrado. Forzando escaneo.  currentSignature [%s] \n",currentSignature);
    }
    
    // 3. Cargar o Reconstruir la lista
    //if (cacheIsValid && loadGifCache()) {    // Ya no cargamos la estructura en memoria, solo cache de offsets (array) gifOffsets
    if (cacheIsValid) {   // Verificamos que no han cambiado los directorios y la firma cache es valida
        if(isInitGifOffsets())  // Verificamos si ya habiamos cargado la estructura para no recargarla.
        {
          return; // ¡Caché válida y cargada! Salimos sin escanear y si recargar los offsets
        }
        else
        {
            // En caso de no estar inicializado (primer acceso)
           if(buildGifIndexFixedArray())  // Inicializamos el array de offsets a fichero cache
           {
                return;
           }
        }
    }

    // si no hemos salido en este punto significa que la firma no ha funcionado correctamente 
    // - la cache no es valida => hay que rehacer el fichero de cache
    // - no se ha podido generar la cache offsets con lo cual o no existe el archivo o no hay contenido => hay que reconstruir fichero cache

    // 4. Reconstrucción: Escaneo de SD y regeneración de archivos de caché
    Serial.println("Reconstruyendo lista de GIFs, escaneando SD...");
    //archivosGIF.clear();  Dejamos de utilizar la estructura de cacheGIF, hay que eliminar el archivo cache y 
    deleteGifCache(); 
    // 4a. Escanear las carpetas seleccionadas
    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_WRITE);
    if (!cacheFile) 
    {
        Serial.printf("Error en la creacion del fichero %s  ...",GIF_CACHE_FILE);
        return;
    }
    // Añadir
    for (const String& path : config.activeFolders) { 
        scanGifDirectory(path,cacheFile);
    }
    cacheFile.close();

    // 4b. Guardar el índice de GIFs
   // saveGifCache();   El fichero se crea en el escaneo del paso anterior 4a directamente.

    // 4c. Guardar la nueva firma para la próxima vez
    File newSigFile = SD.open(GIF_CACHE_SIG, FILE_WRITE);
    if (newSigFile) {
        newSigFile.print(currentSignature);
        newSigFile.close();
        Serial.println("Nueva firma de caché guardada.");
    } else {
         Serial.println("ERROR: No se pudo escribir el archivo de firma.");
    }

    // Antes intentaba reiniciar el array de offsets, pero creo que esto provoca un error inesperado por algun puntero, comento este 
    // código para incluir a continuación un reset del sistema.
    //initGifOffsets(); // Borramos la lista de gif
    //buildGifIndexFixedArray(); // Rehacemos la cache gifOffsets

    // Verificacion temporal de firma, ya que parece que no se guarda correctamente
    currentSignature = generateCacheSignature();
    sigFile = SD.open(GIF_CACHE_SIG, FILE_READ);
    if (sigFile) {
        String savedSignature = sigFile.readStringUntil('\n');
        sigFile.close();
        savedSignature.trim();

        if (savedSignature == currentSignature) {
           Serial.printf("Tras generar nueva cache las firmas de caché coinciden. savedSignature [%s]  currentSignature [%s] \n",savedSignature,currentSignature);
        } else {
            Serial.printf("Tras generar nueva cache las firmas de caché NO coinciden, ERROR!.savedSignature [%s]  currentSignature [%s] \n",savedSignature,currentSignature);
        }
    } else {
        Serial.printf("Archivo de firma de caché no encontrado. currentSignature [%s] \n",currentSignature);
    }

    // Reseteamos con la nueva configuracion. Es mas limpio a nivel de memoria. Al no reiniciar, el heap de memoria queda fragmentado 
    // y es preferente hacer una recarga de todo el sistema desde cero cacheando los offset del fichero gif generado.
    Serial.println("Reset forzado tras cambio en CacheGif.\n Evitamos problemas de fragmentacion de caso de cache de muchos ficheros.");
    ESP.restart();
    
    

}

