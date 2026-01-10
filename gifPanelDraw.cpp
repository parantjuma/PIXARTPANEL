#include "config.h"
#include "gifPanelDraw.h"
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
void * GIFOpenFile(const char *fname, int32_t *pSize)
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

void GIFCloseFile(void *pHandle)
{
    File *f = static_cast<File *>(pHandle);
    if (f != NULL)
        f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
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


int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    return pFile->iPos;
}
