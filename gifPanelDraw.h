

// ====================================================================
//                       FUNCIONES CORE DE VISUALIZACIÓN
// (Sin cambios funcionales, solo se han corregido los comentarios/citaciones)
// ====================================================================

// --- 1. Funciones Callback para AnimatedGIF ---

// Función de dibujo de la librería GIF (necesita estar definida)
void GIFDraw(GIFDRAW *pDraw);

// Funciones de gestión de archivo para SD
void * GIFOpenFile(const char *fname, int32_t *pSize);
void GIFCloseFile(void *pHandle);
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition);
