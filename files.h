// ====================================================================
//       FUNCIONES AÑADIDAS PARA OPTIMIZACIÓN EN USO MEMORIA
// ====================================================================

// Cuenta el numero de lineas de un fichero
uint32_t countLinesInFile();
// Actualiza el gifOffsets que da acceso a las rutas de archivo por medio de la cache GIF_CACHE_FILE
bool buildGifIndexFixedArray();
// Devuelve la ruta ubicada en la posicion "index"  dentro del fichdero GIF_CACHE_FILE 
String getGifPathByIndex(uint32_t index);
// Verifica si ya esta cargado el indice de offsets
bool isInitGifOffsets();
// Borra e inicializa los valores gifOffsets
bool initGifOffsets();
// borra el archivo gifcache.txt
bool deleteGifCache();
// TOTAL gif cargados en cache: Devolvemos el tamaño offsets cargados sumado al de casillas extra offset utilizados en subidas
size_t gifOffsetSize();
// Verifica si esta vacio el array de offsets
bool isGifOffsetEmpty();


// ====================================================================
//                  FUNCIÓN DE ESCANEO DE CARPETAS PARA LA UI
// ====================================================================

// Función para escanear y listar solo las CARPETAS dentro de un path base
void scanFolders(String basePath);

// ====================================================================
//                 GESTIÓN DE ARCHIVOS GIF & CACHÉ
// ====================================================================
 
// Función recursiva que escanea una ruta en busca de GIFs
// Genera directamente el archivo GIF_CACHE_FILE
void scanGifDirectory(const String& path, File& cacheFile);
// Verifica si currentPath forma parte de las config.activeFolders
bool isActiveFolder(const String& currentPath);
// Añade un nuevo archivo subido desde el servidorvoid addNewGifInCacheFile(uploadPath.c_str());
void addNewGifInCacheFile(const char* uploadPath);
// Función auxiliar para generar la firma de la configuración actual
String generateCacheSignature();
// Función principal de listado de archivos GIF (usa la lógica de validación de firma)
void listarArchivosGif();
// VAlida si el path esta dentro de la carpeta GIF definida
bool isUnderBasePath(const String& path);