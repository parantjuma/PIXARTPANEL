# ✨ PIXELARTPANEL

## 💡 Descripción del Proyecto

**PIXELARTPANEL** es una adaptación del proyecto  Retro Pixel LED en su versión v2.0.9, el cual implementa un firmware avanzado para dispositivos ESP32 diseñado para controlar matrices de LEDs (como las matrices HUB75 PxP o similares) a través de una interfaz web sencilla.

Permite a los usuarios cambiar el **modo de reproducción** (GIFs, Texto Deslizante o Reloj), ajustar el brillo, y modificar la configuración del sistema de manera inalámbrica (OTA). Es ideal para crear pantallas decorativas de estilo retro, relojes digitales, y visualizadores de información personalizables.

#  Modificaciones

1.0 Beta (17_01_2026)
---------------------

- Fragmentación del archivo .ino en varios archivos .h y .cpp
- Adaptación el modelo de cache para el acceso a la coleccion de gif. Debido a las dificultades   de la ESP32 para almacenar en memoria TADS para un gran volumen de rutas de gif en la SD (p.ej 11k entrdas de rutas gif) y evitar problemas de fragmentación de memoria. El modelo actual utiliza una cache de offsets sobre el fichero cache de gif.
- Establecer el modo Portal Cautivo no bloqueante, de forma que si el panel se mueve de ubicación
y pierde la conexión con la wifi configurada el panel continua con su ultima configuración en pantalla (mostrar gif, texto deslizante, hora)
- Acceso por medio de http://pixelartpanel.local/ en vez de por IP (en fase de pruebas)
- Mostrar información de IP en primer acceso en caso de tener WIFI configurada y conexion con exito.
- Se añade el Modo Info: Muestra por el panel una serie de datos como ip, url local de acceso

## Agradecimientos

Este proyecto se basa y toma como referencia el excelente trabajo realizado en el proyecto
[RetroPixelLED](https://github.com/fjgordillo86/RetroPixelLED).

Quiero expresar mi agradecimiento a su autor, **fjgordillo86**, por compartir de forma abierta su conocimiento,
código y experiencia, lo que ha servido como base e inspiración para el desarrollo de este proyecto.
Su trabajo ha sido clave para comprender y ampliar las posibilidades de control de paneles LED HUB75 con ESP32.

Gracias por contribuir a la comunidad y facilitar que otros proyectos puedan crecer a partir de tu trabajo.
