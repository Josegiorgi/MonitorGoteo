# Prueba capacitivo con ESP32

Proyecto de prueba y calibración del sensor capacitivo de gota del monitor de goteo, usando el touch integrado del ESP32-S3 (driver [`touch_mcu`](../../drivers/microcontroller/inc/touch_mcu.md)).

## Qué hace

1. Inicializa el canal de touch conectado al electrodo sensor.
2. Ajusta la sensibilidad del sensor (`TouchSetSensitivity`).
3. **Calibra un valor base ("baseline")**: promedia 30 lecturas crudas con aire entre los electrodos (sin gota) y lo imprime una vez por consola.
4. En loop continuo, imprime el **offset** (`lectura_actual - baseline`) cada 20ms. Con aire quieto el offset ronda cerca de 0; cuando pasa una gota entre los electrodos, se aleja de 0 momentáneamente.

La salida (una columna de números por consola) se puede graficar directo en un serial plotter (el de VS Code/ESP-IDF o el de Arduino IDE) para ver la forma de la señal en tiempo real.

## Cableado

| Electrodo | Conexión |
|---|---|
| Sensor (activo, cinta de cobre) | GPIO4 (`TOUCH_CH_4`) |
| Referencia (cinta de cobre, del otro lado del hueco donde cae la gota) | GND |

**Sin shield**: en esta placa (Super Mini) GPIO14 —el canal de shield, fijo por hardware— no es un pin de borde soldable normal, solo existe como vía interna de la PCB. Esta prueba corre solo con sensor + referencia. Si en algún momento se suelda un cable a esa vía y se confirma continuidad con multímetro, se puede sumar `TouchShieldEnable()` (ver el driver).

## Geometría física de los electrodos

Los dos electrodos son arcos de cinta de cobre que rodean el tubo de la cámara de goteo, enfrentados a la misma altura (uno de cada lado), con la gota cayendo por el hueco del medio entre ambos — no anillos completos apilados verticalmente.

## Parámetros para ajustar

Todos están al principio de [`prueba_capacitivo_con_esp32.c`](main/prueba_capacitivo_con_esp32.c):

| Macro | Qué controla |
|---|---|
| `TOUCH_CHANNEL` | Canal de touch a usar (cambia si se recablea el sensor a otro GPIO) |
| `TOUCH_THRESHOLD` | Umbral pasado a `TouchInit` (no se usa todavía en esta prueba, que solo lee `TouchReadRaw`) |
| `READ_PERIOD_MS` | Cada cuánto se toma una muestra |
| `BASELINE_SAMPLES` | Cantidad de muestras para calcular el valor base al arrancar |
| `TouchSetSensitivity(nivel)` en `app_main` | Sensibilidad del sensor: 0 (menos ruido) a 2 (más sensible, más ruido) |

## Cómo usarlo

1. Conectar el sensor y la referencia como se indica arriba.
2. Flashear y abrir el monitor serie.
3. Anotar el `Baseline` que imprime al arrancar.
4. Observar el offset con aire quieto (para estimar el ruido de fondo) y con gotas reales pasando (para ver cuánto se aleja de 0).
5. Ajustar `TouchSetSensitivity` y/o el cableado según lo que se observe, hasta que el salto de una gota se distinga claramente del ruido en reposo.

Esta prueba es exploratoria: todavía no decide si hay o no una gota (no usa `TouchDetect` ni el `threshold`) — sirve para juntar datos reales y definir esos valores antes de escribir la lógica de detección final.
