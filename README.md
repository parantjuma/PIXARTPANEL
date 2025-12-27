# ✨ Retro Pixel LED v2.0.9

## 💡 Descripción del Proyecto

**Retro Pixel LED** es un firmware avanzado para dispositivos ESP32 diseñado para controlar matrices de LEDs (como las matrices HUB75 PxP o similares) a través de una interfaz web sencilla.

Permite a los usuarios cambiar el **modo de reproducción** (GIFs, Texto Deslizante o Reloj), ajustar el brillo, y modificar la configuración del sistema de manera inalámbrica (OTA). Es ideal para crear pantallas decorativas de estilo retro, relojes digitales, y visualizadores de información personalizables.

## 🚀 Características Principales (v2.0.9)

| Característica | Descripción | Estado |
| :--- | :--- | :--- |
| **Múltiples Modos** | Reproducción de GIFs, Texto Deslizante (Marquesina) y Reloj NTP. | Estándar |
| **Gestión de Archivos SD** | Interfaz Web dedicada para subir, listar, borrar y crear directorios en la Micro SD. | **Nuevo (v2.x)** |
| **Indexación Persistente GIF** | Sistema de caché de archivos para **carga instantánea** del modo GIF, sin escaneo lento de la SD. | **Mejorado (v2.0.9)** |
| **Filtro de Carpetas UI** | La interfaz web solo lista las subcarpetas del directorio `/gifs` para una selección limpia. | **Nuevo (v2.0.9)** |
| **Interfaz Web** | Control total de brillo, modos y personalización (colores, velocidad, mensajes). | Estándar |
| **Actualización Remota (OTA)** | Permite cargar nuevo *firmware* y datos de forma inalámbrica. | Estándar |

---

## ⚙️ Instalación y Configuración

### 1. Requisitos de Hardware

* **Microcontrolador:** ESP32.
* **Pantalla LED:** Matriz LED compatible con HUB75.
* **Almacenamiento:** Módulo de Tarjeta Micro SD (SPI) compatible con ESP32.

### 2. Librerías de Arduino Necesarias

* `WiFiManager` (Para la gestión de Wi-Fi)
* `ESP32-HUB75-MatrixPanel-I2S-DMA` (Para la gestión del panel LED)
* `AnimatedGIF` (Decodificador de GIFs, use el fork compatible con ESP32/SD)
* `SD` (Núcleo ESP32)
* Otras librerías estándar del *framework* ESP32 (`Preferences`, `WebServer`, etc.)

### 3. Preparación de la Tarjeta SD

El *firmware* ahora requiere una estructura de directorios clara para el modo GIF:

1.  Formatee la tarjeta Micro SD como **FAT32**.
2.  Cree el directorio base: **`/gifs`** en la raíz.
3.  Cree sus colecciones como **subcarpetas** dentro de `/gifs` (ej., `/gifs/Arcade`, `/gifs/Consolas`).

**Estructura de la SD Requerida:**
├── gifs/ │ ├── Arcade/ <-- Aquí van los GIFs │ └── Consolas/ <-- Aquí van los GIFs ├── gif_cache.txt <-- Generado por el firmware (Índice) └── gif_cache.sig <-- Generado por el firmware (Firma de validación)
### 4. Carga Inicial

1.  Abra el proyecto en su entorno de desarrollo (IDE de Arduino/VSCode + PlatformIO).
2.  Configure correctamente los pines del ESP32 para la matriz LED y la tarjeta SD.
3.  Cargue el código al ESP32.

---

## 🌐 Uso y Optimización (v2.0.9)

### 1. Configuración del Modo GIF y Caché

La principal mejora de rendimiento se gestiona a través de la interfaz web:

1.  Acceda a la dirección IP del ESP32.
2.  Navegue a la sección **"Configuración de Modos"**.
3.  En la configuración del Modo GIF, la interfaz web mostrará **SOLO las subcarpetas** dentro de `/gifs` (gracias al filtro en `scanFolders`).
4.  **Seleccione las carpetas** que desea incluir en la reproducción y guarde la configuración.

#### ⏱️ Mecanismo de Caché (Rendimiento Instantáneo)

* **Validación:** El sistema genera una "firma" única de las carpetas seleccionadas.
* **Reconstrucción:** Solo si la firma actual no coincide con la guardada en `/gif_cache.sig`, el sistema realizará el escaneo lento de la SD para reconstruir el índice de GIFs en `/gif_cache.txt`.
* **Carga Rápida:** Si la firma es válida, la lista de GIFs se carga instantáneamente desde `/gif_cache.txt`.

## ⚖️ Licencia y Agradecimientos

Este proyecto de *firmware* se publica bajo la **Licencia MIT**.

Agradecemos a los desarrolladores de las librerías mencionadas, cuyo trabajo bajo licencias permisivas (principalmente **MIT**) hace posible este proyecto. Consulta el archivo `LICENSE` para conocer los términos completos.
