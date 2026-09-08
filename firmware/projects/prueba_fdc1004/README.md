# Prueba FDC1004

Proyecto de prueba y calibración del sensor capacitivo de gota del monitor de goteo, usando el módulo FDC1004 (driver [`fdc1004`](../../drivers/devices/inc/fdc1004.h)) sobre ESP32-C3.

## Qué hace

1. Inicializa el bus I2C y verifica que el FDC1004 responda (lee su Device ID).
2. Configura la medición 1 en modo **single-ended** sobre el canal del módulo y dispara mediciones repetidas a 100 S/s.
3. **Calibra un valor base ("baseline")**: promedia 30 lecturas de capacitancia con aire entre las placas (sin gota) y lo imprime una vez por consola, en pF.
4. En loop continuo, imprime el **offset** (`lectura_actual - baseline`) en femtofaradios (fF) por consola. Con aire quieto el offset ronda cerca de 0; cuando pasa una gota entre las placas, se aleja de 0 momentáneamente.

La salida se puede graficar directo en un serial plotter (el de VS Code/ESP-IDF o el de Arduino IDE) para ver la forma de la señal en tiempo real.

## Cableado

El módulo FDC1004 tiene 4 pines de un lado (I2C + alimentación) y 3 del otro (sensor):

| Pin del módulo | Conexión |
|---|---|
| `VCC` | 3V3 |
| `GND` (lado I2C) | GND |
| `SDA` | GPIO_6 |
| `SCL` | GPIO_7 |
| `Canal` (`CIN2` en este módulo — verificado en el banco, no es `CIN1`) | Placa activa (arco de cobre) |
| `Shield` (el que el módulo trae junto al canal usado) | Blindaje detrás de las dos placas |
| `GND` (lado sensor) | Placa de referencia (arco de cobre, del otro lado del hueco donde cae la gota) |

⚠️ **El canal expuesto por el módulo no es necesariamente `CIN1`** — en este módulo puntual es `CIN2`. Si cambiás de módulo/proveedor, confirmá con la prueba de acercar la mano (ver más abajo) antes de asumir que sigue siendo el mismo canal.

**La placa de referencia va al `GND` del lado sensor, no a un segundo canal.** El FDC1004 mide "canal contra GND"; al estar la placa de referencia en ese mismo nodo, la capacitancia placa-placa que modula la gota queda incluida directo en la lectura del canal. El modo diferencial (dos canales, ninguno a GND) restaría la capacitancia propia de cada placa por separado y diluiría la señal de la gota en vez de resaltarla.

**El shield ya viene resuelto por el propio módulo**: en modo single-ended el FDC1004 cortocircuita internamente `SHLD1` y `SHLD2`, así que no hace falta configurar nada por firmware — solo conectar el pin `Shield` del módulo al blindaje físico detrás de las placas.

## Geometría física de las placas

Dos placas semicirculares de cobre que rodean el tubo de la cámara de goteo, enfrentadas a la misma altura (una de cada lado), con la gota cayendo por el hueco del medio entre ambas, y una capa de blindaje detrás de las dos.

## Parámetros para ajustar

Todos están al principio de [`prueba_fdc1004.c`](main/prueba_fdc1004.c):

| Macro | Qué controla |
|---|---|
| `MEASUREMENT` | Registro de medición del FDC1004 a usar (1 a 4); no hace falta tocarlo con un solo sensor |
| `SAMPLE_RATE` | Tasa de muestreo (`FDC1004_RATE_100SPS`/`200SPS`/`400SPS`) — más tasa, más ruido |
| `CAPDAC_OFFSET` | Offset en pasos de 3.125 pF; subir solo si la capacitancia base placa-placa supera el rango de ±15 pF |
| `BASELINE_SAMPLES` | Cantidad de muestras para calcular el valor base al arrancar |

## Cómo usarlo

1. Conectar el módulo como se indica arriba.
2. Flashear y abrir el monitor serie.
3. Anotar el `Baseline` (en pF) que imprime al arrancar.
4. Observar el offset (en fF) con aire quieto (para estimar el ruido de fondo) y con gotas reales pasando (para ver cuánto se aleja de 0).
5. Si la base ronda cerca de los límites de ±15 pF, ajustar `CAPDAC_OFFSET`. Si el ruido en reposo es alto, probar bajar `SAMPLE_RATE` a 100 S/s (mejor resolución) antes de tocar el hardware.

Esta prueba es exploratoria: todavía no decide si hay o no una gota — sirve para juntar datos reales y definir el umbral antes de escribir la lógica de detección final.
