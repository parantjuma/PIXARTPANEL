#include "config.h"
#include "webManage.h"
#include "webUI.h"


// ====================================================================
//                          INTERFAZ WEB PRINCIPAL
// ====================================================================

String webPage() {
    int brightnessPercent = (int)(((float)config.brightness / 255.0) * 100.0); 

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>"+PRJ_NAME_DEFAULT+"</title>";
    html += "<style>";
    html += "body{font-family:Arial;background:#f0f2f5;color:#333;margin:0;padding:20px;}";
    html += ".c{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:15px;box-shadow:0 8px 25px rgba(0,0,0,.15);}";
    html += "h1{text-align:center;color:#2c3e50;margin:0;}";
    html += "input:not([type='checkbox']):not([type='color']),select,button{margin:10px 0;padding:12px;border-radius:8px;border:1px solid #ccc;width:100%;box-sizing:border-box;font-size:16px;}";
    html += ".button-group{display:flex;justify-content:space-between;gap:10px;margin-top:20px;}";
    html += ".button-group button{width:calc(25% - 7px);margin:0;font-weight:bold;color:#fff;border:none;cursor:pointer;padding:12px 0;}";
    html += ".save-btn{background:#2ecc71;}";
    html += ".save-btn:hover{background:#27ae60;}";
    html += ".update-btn{background:#3498db;}";
    html += ".update-btn:hover{background:#2980b9;}";
    html += ".config-btn{background:#e67e22;}";
    html += ".config-btn:hover{background:#d35400;}";
    html += ".file-btn{background:#9b59b6;}";
    html += ".file-btn:hover{background:#8e44ad;}";
    html += "label{display:block;margin:15px 0 5px;font-weight:bold;}";
    html += ".label-brightness{display:flex;justify-content:space-between;align-items:center;margin:15px 0 5px;font-weight:bold;}";
    html += ".brightness-percent{font-size:1em;color:#3498db;}"; 
    html += ".cb label{font-weight:normal;display:block;margin:8px 0;}";
    html += ".footer{display:flex;justify-content:space-between;font-size:0.8em;color:#777;margin-top:20px;}";
    html += ".checkbox-group{display:flex;align-items:center;margin-top:10px;}";
    html += ".checkbox-group label{margin:0;padding-left:10px;font-weight:normal;}";
    html += "</style></head><body><div class='c'>";
// Cabecera
    html += "<h1>"+PRJ_NAME_DEFAULT+"</h1><hr><form action='/save'>";
// 1. CONTROL DE BRILLO (Layout Modificado)
    html += "<label class='label-brightness'>";
    html += "Nivel de Brillo ";
    html += "<span class='brightness-percent' id='brightnessValue'>" + String(brightnessPercent) + "%</span>";
    html += "</label>";
    html += "<input type='range' name='b' min='0' max='255' value='" + String(config.brightness) + "' oninput='updateBrightness(this.value)'>";
    
    html += "<hr>";
// Separador

    // 2. CONTROL: Modo de Reproducción
    html += "<label>Modo de Reproducción</label>";
    html += "<select name='pm' id='playModeSelect'>";
    html += String("<option value='0'") + (config.playMode == 0 ? " selected" : "") + ">GIFs</option>";
    html += String("<option value='1'") + (config.playMode == 1 ? " selected" : "") + ">Texto Deslizante</option>";
    html += String("<option value='2'") + (config.playMode == 2 ? " selected" : "") + ">Reloj</option>";
    html += String("<option value='3'") + (config.playMode == 3 ? " selected" : "") + ">Info</option>";
    html += "</select><hr>";
// 3. CONTROLES: Texto Deslizante
    html += "<div id='textConfig' style='display:" + String(config.playMode == 1 ? "block" : "none") + ";'>";
    html += "<label>Texto a Mostrar (Marquesina)</label>";
    html += "<input type='text' name='st' value='" + config.slidingText + "' maxlength='50'>";
    html += "<label>Velocidad de Deslizamiento (ms)</label>";
    html += "<input type='number' name='ts' min='10' max='1000' value='" + String(config.textSpeed) + "'>";
    html += "<hr></div>";
    
    // 4. Controles de GIFs
    html += "<div id='gifConfig' style='display:" + String(config.playMode == 0 ? "block" : "none") + ";'>";
    html += "<label>Repeticiones por GIF</label><input type='number' name='r' min='1' max='50' value='" + String(config.gifRepeats) + "'>";
    html += "<label>Orden de Reproducción</label><select name='m'>";
    html += String("<option value='0'") + (config.randomMode ? "" : " selected") + ">Secuencial</option>";
    html += String("<option value='1'") + (config.randomMode ? " selected" : "") + ">Aleatorio</option>";
    html += "</select><hr>";

    html += "<div class='checkbox-group'>";
    html += String("<input type='checkbox' id='showLogo' name='showLogo' value='1'") + (config.showLogo ? " checked" : "") + ">";
    html += "<label for='sd'>Mostrar Logos</label></div>";

    html += "<label>Nº GIF entre Logos</label><input type='number' name='logoFrecuence' min='0'  value='" + String(config.logoFrecuence) + "'>";

    html += "<div class='checkbox-group'>";
    html += String("<input type='checkbox' id='batoceraLink' name='batoceraLink' value='1'") + (config.batoceraLink ? " checked" : "") + ">";
    html += "<label for='sd'>Conexión con batocera</label></div>";


    html += "<b>Carpetas disponibles:</b><div class='cb'>";
    
    if (sdMontada) {
        if (allFolders.empty()) {
            html += "<p>No se encontraron carpetas en la SD. (¿Archivos directamente en la raíz?)</p>";
        } else {
            for (const String& f : allFolders) {
                // Comprueba si la carpeta 'f' está en el vector de carpetas activas
                bool c = (std::find(config.activeFolders.begin(), config.activeFolders.end(), f) != config.activeFolders.end());
                html += String("<label><input type='checkbox' name='f' value='" + f + "'") + (c ? " checked" : "") + "> " + f + "</label>";
            }
        }
    } else {
        html += "<p>¡Tarjeta SD no montada! No se pueden listar carpetas.</p>";
    }
    html += "</div><hr></div>";
    
    // 5. Controles de Reloj (Placeholder)
    html += "<div id='clockConfig' style='display:" + String(config.playMode == 2 ? "block" : "none") + ";'>";
    html += "<p style='text-align:center;'>La configuracion de Hora y Apariencia del Reloj se realiza en la seccion de Configuracion General.</p>";
    html += "<hr></div>";


    // Grupo de botones en línea (4 botones)
    html += "<div class='button-group'>";
    html += "<button type='submit' class='save-btn'>Guardar</button>"; 
    html += "<button type='button' class='update-btn' onclick=\"window.location.href='/ota'\">OTA</button>"; 
    html += "<button type='button' class='config-btn' onclick=\"window.location.href='/config'\">Ajustes</button>"; 
    html += "<button type='button' class='file-btn' onclick=\"window.location.href='/file_manager'\">Archivos SD</button>";
    html += "</div>";
    html += "</form>";
    // Pie de página: Intercambio de posiciones
    html += "<div class='footer'>";
    html += "<span style='font-size:8'>Based on project Retro Pixel LED v2.0.9</span>";
    // Ahora a la Izquierda
    html += "<span><b>IP:</b> " + WiFi.localIP().toString() + "</span>";
// Ahora a la Derecha
    html += "</div>";
// Script JS para manejar visibilidad de la configuración y porcentaje de brillo
    html += "<script>";
    html += "function updateBrightness(val) {";
    html += "  var percent = Math.round((val / 255) * 100);";
    html += "  document.getElementById('brightnessValue').innerHTML = percent + '%';";
    html += "}";
    html += "document.getElementById('playModeSelect').addEventListener('change', function() {";
    html += "  var mode = this.value;";
    html += "  document.getElementById('gifConfig').style.display = (mode == '0') ? 'block' : 'none';";
    html += "  document.getElementById('textConfig').style.display = (mode == '1') ? 'block' : 'none';";
    html += "  document.getElementById('clockConfig').style.display = (mode == '2') ? 'block' : 'none';";
    html += "});";
    html += "</script>";

    html += "</div></body></html>";
    return html;
}

// ====================================================================
//                      INTERFAZ WEB CONFIGURACIÓN
// ====================================================================

String configPage() {
    char hexColor[8];
    sprintf(hexColor, "#%06X", config.clockColor);
    
    char hexTextColor[8];
    sprintf(hexTextColor, "#%06X", config.slidingTextColor); 

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Configuración | "+PRJ_NAME_DEFAULT+"</title>";
    html += "<style>";
    html += "body{font-family:Arial;background:#f0f2f5;color:#333;margin:0;padding:20px;}";
    html += ".c{max-width:600px;margin:auto;background:#fff;padding:25px;border-radius:15px;box-shadow:0 8px 25px rgba(0,0,0,.15);}";
    html += "h1{text-align:center;color:#2c3e50;margin:0;}";
    html += "h2{border-bottom:2px solid #ccc;padding-bottom:5px;margin-top:30px;color:#3498db;}";
    html += "input:not([type='checkbox']):not([type='color']),select,button{margin:10px 0;padding:12px;border-radius:8px;border:1px solid #ccc;width:100%;box-sizing:border-box;font-size:16px;}";
    html += "input[type='color']{width:50px;height:50px;padding:0;border:none;border-radius:5px;vertical-align:middle;margin-right:15px;}";
    html += "label{display:block;margin:15px 0 5px;font-weight:bold;}";
    html += ".checkbox-group{display:flex;align-items:center;margin-top:10px;}";
    html += ".checkbox-group label{margin:0;padding-left:10px;font-weight:normal;}";
    html += ".config-group{display:flex;justify-content:space-between;gap:10px;margin-top:30px;}";
    html += ".config-group button{width:calc(33.33% - 7px);margin:0;font-weight:bold;color:#fff;border:none;cursor:pointer;padding:12px 0; border-radius:8px;}";
    html += ".save-btn{background:#2ecc71;}";
    html += ".save-btn:hover{background:#27ae60;}";
    html += ".back-btn{background:#e67e22;}";
    html += ".back-btn:hover{background:#d35400;}";
    html += ".restart-btn{background:#3498db;}";
// Color cambiado a azul para distinción
    html += ".restart-btn:hover{background:#2980b9;}";
// NUEVO Estilo para botón de Restablecer (rojo peligro)
    html += ".reset-btn{background:#e74c3c;margin-top:20px;}"; 
    html += ".reset-btn:hover{background:#c0392b;}";
    html += ".footer{display:flex;justify-content:space-between;font-size:0.8em;color:#777;margin-top:20px;}";
    html += "</style></head><body><div class='c'>";
    
    // Cabecera sin imagen
    html += "<h1>Configuración General</h1><hr><form action='/save_config'>";
// ----------------------------------------------------
    // SECCIÓN 1: CONFIGURACIÓN DE HORA Y FECHA
    // ----------------------------------------------------
    html += "<h2>1. Hora y Fecha</h2>";
// Zona Horaria (TZ String)
    html += "<label for='tz'>Zona Horaria (TZ String)</label>";
    html += "<input type='text' id='tz' name='tz' value='" + config.timeZone + "' placeholder='" + String(TZ_STRING_SPAIN) + "'>";
// Cadena de explicación de la TZ
    html += "<small>Valor por defecto para España: <code>" + String(TZ_STRING_SPAIN) + "</code>. La gestión del horario de verano/invierno (DST) se hace automáticamente si la cadena TZ es correcta. <a href='https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv' target='_blank'>Más TZ Strings</a></small>";
    
// Formato de Hora (12h/24h)
    html += "<label for='f24h'>Formato de Hora</label>";
    html += "<select id='f24h' name='f24h'>";
    html += String("<option value='1'") + (config.format24h ? " selected" : "") + ">24 Horas (HH:MM)</option>";
    html += String("<option value='0'") + (!config.format24h ? " selected" : "") + ">12 Horas (HH:MM AM/PM)</option>";
    html += "</select>";
// Mostrar Segundos
    html += "<label>Opciones de Visualización</label>";
    html += "<div class='checkbox-group' style='margin-bottom:10px;'>";
    html += String("<input type='checkbox' id='ss' name='ss' value='1'") + (config.showSeconds ? " checked" : "") + ">";
    html += "<label for='ss'>Mostrar Segundos</label></div>";
    
    // Mostrar Fecha
    html += "<div class='checkbox-group'>";
    html += String("<input type='checkbox' id='sd' name='sd' value='1'") + (config.showDate ? " checked" : "") + ">";
    html += "<label for='sd'>Mostrar Fecha (Se desliza)</label></div>";

    // ----------------------------------------------------
    // SECCIÓN 2: CONFIGURACIÓN DE COLORES
    // ----------------------------------------------------
    html += "<h2>2. Configuración de Colores</h2>";
// Color del Reloj
    html += "<label>Color de los Dígitos del Reloj</label>";
    html += "<div style='display:flex;align-items:center;'>";
    html += "<input type='color' id='cc' name='cc' value='" + String(hexColor) + "'>";
    html += "<label for='cc' style='margin:0;'>Selecciona un color</label></div>";
// Color del Texto Deslizante
    html += "<label>Color del Texto Deslizante</label>";
    html += "<div style='display:flex;align-items:center;'>";
    html += "<input type='color' id='stc' name='stc' value='" + String(hexTextColor) + "'>"; 
    html += "<label for='stc' style='margin:0;'>Selecciona un color</label></div>";
// ----------------------------------------------------
    // SECCIÓN 3: CONFIGURACIÓN DE HARDWARE Y SISTEMA
    // ----------------------------------------------------
    html += "<h2>3. Hardware y Sistema</h2>";
// Número de Paneles LED
    html += "<label for='pc'>Número de Paneles LED (en cadena)</label>";
    html += "<input type='number' id='pc' name='pc' min='1' max='8' value='" + String(config.panelChain) + "'>";
    html += "<small>El valor actual es: " + String(config.panelChain) + ". Requiere reinicio para un cambio físico de configuración.</small>";
// Grupo de botones para Guardar, Reiniciar y Volver
    html += "<div class='config-group' style='margin-top:30px;'>";
    html += "<button type='submit' class='save-btn'>Guardar</button>"; 
    html += "<button type='button' class='restart-btn' onclick=\"if(confirm('¿Seguro que quieres reiniciar el dispositivo?')) { window.location.href='/restart'; }\">Reiniciar</button>";
    html += "<button type='button' class='back-btn' onclick=\"window.location.href='/'\">Volver</button>"; 
    html += "</div>";

    html += "</form><hr>";
// Botón de Restablecimiento de Fábrica Total (Único)
    html += "<div style='margin-top:20px; text-align:center;'>";
    html += "<button class='reset-btn' onclick=\"if(confirm('⚠️ ¿Seguro que quieres RESTABLECER TODOS LOS AJUSTES? Esto borrará la configuración de la matriz, los modos, los colores Y la conexión WiFi. Los archivos de la tarjeta SD permanecerán intactos. El dispositivo se reiniciará en modo de configuración inicial.')) { window.location.href='/factory_reset'; }\">Restablecer Ajustes</button>";
    html += "</div>";


    // Pie de página
    html += "<div class='footer'>";
    html += "<span style='font-size:8'>Based on project Retro Pixel LED v2.0.9</span>"; 
    html += "<span><b>IP:</b> " + WiFi.localIP().toString() + "</span>";
    html += "</div>";
    
    html += "</div></body></html>";
    return html;
}
