# ✨ PIXELARTPANEL

## 💡 Descripción del proyecto

**PIXELARTPANEL** es una adaptación del proyecto **Retro Pixel LED** en su versión v2.0.9, el cual implementa un firmware avanzado para dispositivos ESP32, diseñado para controlar matrices de LEDs (como las matrices HUB75 PxP o similares) a través de una interfaz web sencilla.

Permite a los usuarios cambiar el **modo de reproducción** (GIFs, texto deslizante o reloj), ajustar el brillo y modificar la configuración del sistema de manera inalámbrica (OTA). Es ideal para crear pantallas decorativas de estilo retro, relojes digitales y visualizadores de información personalizables.

## 🔧 Modificaciones

### 1.0 Beta (17/01/2026)

- **Fragmentación de código**: División del archivo `.ino` en varios archivos `.h` y `.cpp`.

- **Adaptación del modelo de caché** para el acceso a la colección de GIFs:  
  Debido a las limitaciones de la ESP32 para almacenar en memoria estructuras TADS con un gran volumen de rutas de GIF en la SD (por ejemplo, más de 11.000 entradas), y con el objetivo de evitar problemas de fragmentación de memoria, se ha implementado un nuevo modelo basado en un array de *offsets* sobre el fichero de caché de GIFs.  
  Este enfoque reduce el tamaño necesario a tan solo unos pocos bytes por entrada. En el monitor serie se muestra información sobre el consumo de memoria durante el `setup`, lo que permite supervisar la memoria libre tras el arranque.

- **Modo portal cautivo no bloqueante**:  
  Si el panel cambia de ubicación y pierde la conexión con la red WiFi configurada, continúa mostrando la última configuración activa (GIF, texto deslizante u hora).  
  La WiFi puede configurarse en cualquier momento sin necesidad de detener el funcionamiento del panel. Resulta especialmente práctico al conectar el dispositivo en una nueva ubicación cuando no es necesario modificar la configuración.  
  IP del portal cautivo: `192.168.4.1`.

- **DNSServer**:  
  Acceso mediante `http://pixelartpanel.local/` en lugar de una IP directa (en fase de pruebas).

- **Facilitar IP de acceso a la web de configuración**:  
  Se muestra la información de acceso (IP local y URL vía DNS `http://pixelartpanel.local/`) en el primer arranque cuando la conexión WiFi se establece correctamente.

- **Modo Info**:  
  Se añade un nuevo modo que muestra en el panel información relevante como la IP y la URL local de acceso.

- **Paginación del File Manager**:  
  El gestor de archivos incorpora paginación en carpetas de gran tamaño para evitar la generación de páginas web excesivamente grandes.  
  No obstante, queda pendiente su optimización, ya que el acceso a ficheros sigue siendo lento y el primer sondeo de la carpeta (lectura secuencial para contar archivos) constituye un cuello de botella.

- **Logos especiales**: Se ga añadudi la posibilidad de definir una carpeta de **logos especiales** que contenga GIFs representativos del panel (por ejemplo, el nombre de una máquina arcade).  
  El modo GIF se adaptará para alternar estos logos cada *X* GIFs de la colección, generando secuencias del tipo:  
  `LOGO → GIF → GIF → GIF → … → LOGO → GIF → GIF → GIF`,  
  asegurando así la visualización periódica del logo preferido.
  Para ello en el modo de reproducción GIF se añaden 2 parámetros mas:
      - Mostrar logos: Un check que activa el modo Logo
      - Nº de gifs entre logos: Indica cuantos logos se muestran de la colección seleccionada hasta el proximo logo de la carpeta de logos especiales.

  Estos logos especiales se encuentran en la carpeta "\logos"

## 🚀 Mejoras futuras

- Optimizar la paginación en el acceso a carpetas con un gran volumen de archivos. El acceso a la SD en directorios con muchos ficheros es excesivamente lento (puede llegar a tardar hasta 1 hora en carpetas con 3.000 archivos).  
  Las posibles soluciones incluyen la integración de una librería más rápida para el acceso a la SD o la implementación de una caché que almacene el número de archivos por carpeta, evitando así el recuento secuencial inicial.

- Ampliar la información del **Modo Info**:
  - Mostrar información del heap de memoria libre.
  - Mostrar información de la red WiFi a la que está conectada.
  - Mostrar información de la firma de la caché actual.
  - Mostrar el número de GIFs aleatorios cacheados.

- Implementar conectividad con **Batocera**, basándose en los desarrollos de Retro Pixel LED de **fjgordillo86**.  
  Además, se analizará la posibilidad de que Batocera envíe un PNG reducido vía POST a la ESP32 con la imagen scrapeada del juego. Esto evitaría la necesidad de disponer de una colección completa de GIFs, aprovechando las imágenes ya incluidas en los packs de Batocera.  
  El reescalado debería realizarse en Batocera teniendo en cuenta las dimensiones de 128x64. Idealmente, la imagen se ajustaría a una dimensión y el panel mostraría el contenido mediante scroll horizontal o vertical.

- **Sincronización con el botón de apagado de la arcade**:  
  El panel deberá sincronizarse con otra ESP32 instalada en la máquina arcade, encargada de gestionar el apagado de los componentes y el modo *standby* mediante relés.  
  Esta ESP32 se conectará con PIXELARTPANEL para activar un modo reloj con bajo brillo cuando la arcade esté apagada y restaurar el último modo configurado cuando la arcade se inicie.

- **Mejora en proceso de creacion de archivo cache de rutas a coleccion de gif**: Cuando seleccionamos nuevas carpetas gif, en el monitor serie puede verse el progreso, pero si gestionamos desde la web no sabemos que esta ocurriendo ya que la ESP se queda completamente ocupada escaneando la SD y creando el fichero cache. Una posible mejora a este proceso seria mostrar algun tipo de mensaje por medio del servidor web cuando estamos ejecutando algun tipo de tarea pesada como esta. De esta forma sabemos que no hemos de apagar la ESP y que no se ha quedado bloqueado el sistema. Por ejemplo podriamos tener un dato sobre el número total de gif escaneados. Para hacer esta mejora tenemos que ejecutar una iteración del servidor web dentro del bucle de escaneo de la ESP, ademas de mostrar por web algun tipo de mensaje al guardar la configuracion.

## 🙏 Agradecimientos

Este proyecto se basa y toma como referencia el excelente trabajo realizado en el proyecto  
[RetroPixelLED](https://github.com/fjgordillo86/RetroPixelLED).

Quiero expresar mi agradecimiento a su autor, **fjgordillo86**, por compartir de forma abierta su conocimiento, código y experiencia, lo que ha servido como base e inspiración para el desarrollo de este proyecto.  
Su trabajo ha sido clave para comprender y ampliar las posibilidades de control de paneles LED HUB75 con ESP32.

Gracias por contribuir a la comunidad y facilitar que otros proyectos puedan crecer a partir de tu trabajo.
