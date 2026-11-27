# Monitor de goteo

Firmware de un monitor de infusión por gravedad. Un equipo con dos fotodiodos BPW34 cuenta las gotas que caen en la cámara de goteo, calcula el volumen infundido y el caudal, y avisa con alarmas en pantalla y con sonido cuando la infusión se aparta de lo programado. Todo lo que ocurre queda guardado en un registro que la enfermera descarga con una aplicación Android por Bluetooth.

Es el prototipo de un trabajo final de la carrera de Bioingeniería de la FIUNER. Funciona sobre un ESP32-C3 con ESP-IDF 5.3 y FreeRTOS.

## Qué hace

- Detecta cada gota por correlación cruzada con una plantilla, sobre la señal filtrada de dos fotodiodos.
- Calcula volumen y caudal (mediana de los últimos 10 intervalos entre gotas).
- Alarmas de flujo detenido, fin de sachet, volumen completo, desvío del caudal, colocación del equipo (acelerómetro ADXL345), batería baja y goteo sin monitoreo, con patrones sonoros distintos.
- Interfaz con pantalla OLED y encoder: clave clínica, configuración de la infusión, ajuste de goteo, monitoreo y menú de ajustes.
- Registro de la infusión en una partición propia de la flash, con eventos y una muestra por minuto.
- Bluetooth Low Energy para descargar el registro con la aplicación de la enfermera.

## Dónde está cada cosa

| Carpeta | Contenido |
|:--|:--|
| [`firmware/projects/monitor_goteo`](firmware/projects/monitor_goteo) | El firmware final. Empezar por su [README](firmware/projects/monitor_goteo/README.md). |
| [`firmware/components`](firmware/components) | Los módulos del monitor: sensor óptico, detección de gotas, sensado (volumen y caudal), alarmas, buzzer, inclinación, batería, pantalla, interfaz de usuario, registro y comunicación. Cada uno explica su interfaz en el comentario de su archivo `.h`. |
| [`firmware/drivers`](firmware/drivers) y [`firmware/middleware`](firmware/middleware) | Drivers de la placa y de los dispositivos, filtro IIR y detector por correlación cruzada. |
| [`firmware/third_party`](firmware/third_party) | u8g2, la biblioteca que dibuja en la OLED. |
| [`firmware/projects`](firmware/projects) | Además del firmware final, los proyectos de prueba usados durante el desarrollo (ver la tabla de abajo). |
| [`documentación`](documentación) | Documentación del monitor y material de la cátedra. Ver su [índice](documentación/README.md). |

## Proyectos

| Proyecto | Para qué sirve |
|:--|:--|
| `monitor_goteo` | **Firmware final** |
| `pruebabpw34` | Prueba del sensor óptico BPW34: señal cruda, filtrada y detector. De acá salen los filtros y la plantilla de la gota |
| `analisis_bpw34` | Scripts de Python que analizan los registros del BPW34 (espectro, filtro, plantilla, umbrales, ángulo de pérdida de detección). No se compila |
| `prueba_opt101` y `analisis_opt101` | Lo mismo para el sensor OPT101, que se evaluó antes de elegir el BPW34 |
| `prueba_fdc1004` | Prueba del sensor capacitivo FDC1004 |
| `prueba_adxl345` | Lectura del acelerómetro y de sus ángulos |
| `prueba_interfaz` | Todas las pantallas de la OLED manejadas con el encoder |
| `test_display` | OLED con el encoder y el conteo de gotas |
| `test_i2c_scan` | Busca la OLED y el ADXL345 en el bus I2C |
| `test_alarmas` | Verifica la lógica de las alarmas con un reloj simulado |
| `test_buzzer_alarmas` | Recorre los patrones sonoros de las alarmas |
| `test_bateria` | Medición de la batería y sus alarmas |
| `test_ble` | Bluetooth y descarga del registro |

## Cómo compilar y grabar

1. Instalar ESP-IDF 5.3 y Visual Studio Code siguiendo la [guía de instalación](documentación/instalación.md).
2. Abrir el workspace [`firmware/EProgramable_C3.code-workspace`](firmware/EProgramable_C3.code-workspace).
3. Elegir el proyecto `monitor_goteo`, compilar y grabar como indica la [guía de compilación](documentación/compilación.md). El destino es `esp32c3`.

Antes de grabar la versión final hay que dejar `USAR_BLE = 1` y `ESCENARIO = ESCENARIO_NORMAL` al principio de `main/monitor_goteo.c`. Los demás escenarios acortan los tiempos para probar.

## Documentación

- Capítulo del firmware con las decisiones de diseño: [`documentación/firmware.md`](documentación/firmware.md).
- Contrato con la aplicación Android: [`documentación/cambios_para_la_app.md`](documentación/cambios_para_la_app.md).
- Guion de pruebas del firmware final: [`documentación/pruebas_firmware_final.md`](documentación/pruebas_firmware_final.md).

## Créditos

El firmware base (drivers de la placa ESP-EDU y estructura del repositorio) es de la cátedra Electrónica Programable de la FIUNER, a partir de [prototipado/EProgramable_FIUNER](https://github.com/prototipado/EProgramable_FIUNER): Juan Manuel Reta, Eduardo Filomena, Juan Ignacio Cerrudo, Albano Peñalva, Lucía Eletti y Diego Arévalo. El monitor de goteo es obra de Josefina Giorgi.

* [Campus Virtual de la Cátedra](http://campus.ingenieria.uner.edu.ar/course/view.php?id=455)
* [IDF API para ESP32-C6](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/index.html)
* [Repositorio de Hardware (ESP-EDU)](https://github.com/prototipado/ESP-EDU)
