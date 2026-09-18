# Prueba OPT101

Proyecto de prueba del sensor óptico de gota del monitor de goteo, usando el fotodiodo con amplificador de transimpedancia integrado OPT101 (driver [`analog_io_mcu`](../../drivers/microcontroller/inc/analog_io_mcu.h)) sobre ESP32-C3.

## Qué hace

El OPT101 y un LED enfrentado forman una barrera de luz alrededor del hueco por donde cae la gota. Mientras el haz llega completo al sensor, la salida se mantiene en un nivel base; al pasar una gota el haz se interrumpe/refracta parcialmente y la tensión cambia de forma momentánea.

Este proyecto lee el ADC de forma periódica, disparada por un timer de hardware (driver [`timer_mcu`](../../drivers/microcontroller/inc/timer_mcu.h)), y transmite el resultado por consola **en tiempo real, sin cortes**. Tiene 3 modos seleccionables por `TEST_MODE`:

- **`MODE_RAW`**: transmite el valor calibrado en mV, sin filtrar. Sirve para verificar que el sensor/ADC responden bien, y como referencia para comparar si `MODE_FILTERED` da algo raro (para saber si el problema está en el filtro o en otro lado).
- **`MODE_FILTERED`** (modo activo por defecto): pasa la lectura por un **pasabanda** (pasaaltos + pasabajos Butterworth en cascada) antes de transmitirla.
- **`MODE_DETECTOR`**: además del pasabanda, pasa la señal filtrada por un **detector de gotas por correlación cruzada** con una plantilla (el promedio de 8 gotas reales) y prende un LED cada vez que detecta una. Ver más abajo.

Usa el componente [`signal_processing`](../../middleware/signal_processing/inc/iir_filter.h) (Butterworth IIR, biquads en cascada) de la cátedra de Sistemas de Adquisición y Procesamiento de Señales ([SAPS_FIUNER](https://github.com/prototipado/SAPS_FIUNER)), ya presente en este repo. `LowPassFilter()`/`HiPassFilter()` guardan su estado internamente entre llamadas, así que se los llama con una muestra por vez (`signal_lenght=1`) y funcionan como filtro en tiempo real, no por bloques. La salida queda centrada en 0 (la gota aparece como pico positivo o negativo, ya no como una tensión absoluta).

**De dónde salen los cortes del pasabanda (`CUTOFF_HIGHPASS_HZ` ~85Hz, `CUTOFF_LOWPASS_HZ` ~195Hz):** de un análisis espectral real hecho con [`analisis_opt101`](../analisis_opt101), no de prueba y error. Comparando (con la FFT) el espectro de una misma grabación antes y después de empezar a gotear, el exceso de energía con goteo se concentra en ~90-200Hz (con máximos en 100-120Hz, 150-160Hz y 180-190Hz) — casi nada por debajo de los 20Hz. Antes se había estado poniendo el pasabajos en 30-60Hz, exactamente la banda que *no* tiene la información de la gota, por eso nunca se lograba ver el pico. El pasaaltos, además, ya no tiene el problema de inestabilidad numérica que tenía a cortes de 1-5Hz — a 85Hz la relación corte/muestreo es mucho más sana.

La salida se imprime con dos decimales (`%.2f`), a propósito: después de sacar la continua, lo que queda puede ser de apenas unos pocos mV (o menos) de amplitud. Redondear a entero ahí destruye la resolución justo donde más importa — un pulso chico de menos de 1 mV se ve como "0" en la mayoría de las muestras, y la señal aparenta ser una onda cuadrada (saltando entre unos pocos valores enteros) en vez de una curva continua.

**Por qué timer_mcu y no `vTaskDelay`:** el pulso de la gota dura pocos milisegundos, así que hace falta muestrear rápido. Con `vTaskDelay`, el período mínimo real queda atado al tick de FreeRTOS, y pedir un período más corto que un tick redondea a `vTaskDelay(0)`, que no le cede el CPU a la tarea IDLE y dispara el watchdog. La interrupción del timer solo notifica a una tarea (no lee el ADC ni hace `printf` ahí mismo, que no es seguro dentro de una ISR); la lectura, el filtrado y la impresión pasan en una tarea normal que espera esa notificación.

**Sobre el margen de tiempo:** `SAMPLE_PERIOD_US` es un valor fijo (por defecto 1000us, el techo del rango 0.5-1ms necesario para resolver bien el pico de la gota). Tiene que quedar por encima de lo que tarda leer+filtrar+imprimir una muestra, o la tarea nunca llega a bloquearse de verdad entre muestra y muestra y aparece el problema del watchdog (queda siempre con una notificación pendiente en vez de ceder el CPU) — si eso pasa, la salida es simplificar el filtro (`FILTER_ORDER` más bajo) o revisar el baudrate de la consola antes que subir el período por encima de 1ms.

**Detector por correlación cruzada (`MODE_DETECTOR`).** El componente [`xcorr_detector`](../../middleware/signal_processing/inc/xcorr_detector.h) (middleware) desliza la plantilla de la gota ([`drop_template.h`](main/drop_template.h), el promedio de las 8 gotas de `FiltradaCONGOTA.csv`, ±20ms alrededor del pico) sobre la señal filtrada: en cada muestra calcula cuánto se parece la última ventana a la plantilla. Donde hay una gota la correlación da un pico grande (~55-68 con las gotas de referencia); con solo ruido no pasa de ~5. Al pasar el umbral (`XCORR_THRESHOLD`) el detector espera `XCORR_PEAK_WINDOW_MS` quedándose con el máximo (la plantilla oscila y su correlación tiene lóbulos laterales que también pasarían el umbral), confirma la gota, y la ignora durante `XCORR_REFRACTORY_MS` para no contarla dos veces. La detección llega ~25 ms después de la gota (la ventana de correlación es causal: tiene que ver pasar la gota completa). Por cada gota se prende `DETECTION_LED` durante `LED_ON_TIME_MS`, y la señal filtrada se sigue transmitiendo muestra a muestra igual que en `MODE_FILTERED`, para verla en el serial plotter al mismo tiempo (no se imprime texto extra, que rompería el formato de una columna numérica del plotter). La plantilla y el umbral salen de [`analisis_opt101`](../analisis_opt101), y validados con ese mismo análisis: 8 de 8 gotas detectadas, 0 falsas en el registro sin goteo y 25 en `Datafiltrada.csv` (otro registro, con gotas más chicas y rápidas).

La plantilla vale para `SAMPLE_PERIOD_US = 700` y el pasabanda actual; si cambia alguno de los dos hay que volver a generarla con la última celda del script de análisis.

La salida se puede graficar directo en un serial plotter (el de VS Code/ESP-IDF o el de Arduino IDE) para ver la forma de la señal en tiempo real. Para un análisis más a fondo (espectro FFT, espectrograma) de un registro guardado en `MODE_RAW`, ver el proyecto hermano [`analisis_opt101`](../analisis_opt101).

## Contexto de las pruebas anteriores

Los primeros registros (CSV del 15/09/2026) mostraron que el "con goteo" era indistinguible del "sin goteo" — solo deriva lenta, ningún transitorio atribuible a una gota. El sospechoso principal identificado en ese momento: **el LED y el OPT101 estaban separados de la pared del tubo por un gap de aire**, con el soporte viejo. Se rediseñó el soporte para que agarre la cámara sosteniendo al sensor y al LED directamente, sin ese gap — y con el soporte nuevo, sí se llegó a detectar el goteo (aunque al principio mezclado con bastante ruido).

Filtrar ese ruido llevó varias vueltas (ver el changelog de [`prueba_opt101.c`](main/prueba_opt101.c) para el detalle completo): un pasabajos solo no alcanzaba (subir el corte para sacar ruido siempre terminaba aplanando el pulso también, y viceversa), y el pasaaltos Butterworth de la cátedra resultó numéricamente inestable a los cortes bajos (~1Hz) que parecían necesarios. La salida fue analizar el espectro real de la señal (ver [`analisis_opt101`](../analisis_opt101)) en vez de seguir ajustando cortes a ciegas — eso reveló que la información de la gota está en ~90-200Hz, no en las frecuencias bajas donde se venía filtrando, y que el pasaaltos es numéricamente seguro en ese rango de corte más alto.

## Cableado

| Pin del OPT101 | Conexión |
|---|---|
| `+V` (pin 1) | 3V3 |
| `GND` (pines 3, 4 y 5, unidos entre sí) | GND |
| `Vo` (pin 8, salida) | `GPIO_2` (ADC1_CH2) |
| LED indicador (solo `MODE_DETECTOR`) | `GPIO_10` (`LED_2`), con una resistencia en serie, a GND |

El OPT101 debe quedar enfrentado al LED emisor, con el hueco por donde cae la gota entre ambos. La salida `Vo` ya viene amplificada por el transimpedancia interno del chip, no requiere circuito externo adicional para esta prueba.

## Parámetros para ajustar

Están al principio de [`prueba_opt101.c`](main/prueba_opt101.c):

| Macro | Qué controla |
|---|---|
| `TEST_MODE` | `MODE_RAW` o `MODE_FILTERED` |
| `ADC_CHANNEL` | Canal ADC donde está conectado `Vo` (`CH0` a `CH3`, ver [`analog_io_mcu.h`](../../drivers/microcontroller/inc/analog_io_mcu.h)) |
| `SAMPLE_PERIOD_US` | Período fijo entre muestras, en microsegundos. Tiene que quedar entre 0.5 y 1ms para resolver bien el pico de la gota (por defecto 1000us, el techo de ese rango, para tener más margen posible contra el watchdog) |
| `CUTOFF_HIGHPASS_HZ` | Corte del pasaaltos (por defecto 85 Hz — por debajo de esto, el análisis espectral no mostró diferencia entre con/sin goteo) |
| `CUTOFF_LOWPASS_HZ` | Corte del pasabajos (por defecto 195 Hz — el análisis espectral mostró exceso de energía con goteo hasta ahí, con máximos en 100-120Hz, 150-160Hz y 180-190Hz) |
| `FILTER_ORDER` | Orden del Butterworth, para ambos filtros (por defecto `ORDER_4` — con `ORDER_2` la transición entre "pasa" y "corta" era muy suave). Cada escalón de orden es una etapa (biquad) más en cascada por filtro — más cómputo por muestra, tenerlo en cuenta si vuelve el watchdog |
| `DETECTION_LED` | LED que se prende al detectar una gota (solo `MODE_DETECTOR`). Por defecto `LED_2` = GPIO_10 (`LED_3` = GPIO_5). Evitar `LED_1` = GPIO_20, que es el RX de la UART de la consola |
| `XCORR_THRESHOLD` | Umbral de la correlación, en mV de la señal filtrada (por defecto 13.7 = 12 veces el desvío de la correlación sobre la señal sin goteo; entre el ruido más alto, ~11.8, y la gota más chica confirmada, ~15.5) |
| `XCORR_PEAK_WINDOW_MS` / `XCORR_REFRACTORY_MS` | Tiempo que el detector se queda con el máximo tras pasar el umbral (15 ms) y tiempo que ignora la señal después de detectar (100 ms) |
| `LED_ON_TIME_MS` | Cuánto queda prendido el LED tras cada gota (200 ms) |

## Cómo usarlo

1. Conectar el OPT101 y el LED como se indica arriba, enfrentados a través del hueco de la cámara de goteo.
2. Flashear y abrir el monitor serie (o el serial plotter). Con `TEST_MODE = MODE_RAW`, revisar que el ADC no esté saturando (el valor en mV no debería estar pegado al techo, ~3300 mV).
3. Con el haz libre (sin gota), la salida filtrada debería oscilar cerca de 0 (ya sin continua). Dejar caer gotas reales y observar el pico (positivo o negativo) que aparece al pasar cada una.
4. Si el pico queda muy achatado por el ruido o muy aplanado por el filtro, se puede volver a analizar con [`analisis_opt101`](../analisis_opt101) y ajustar `CUTOFF_HIGHPASS_HZ`/`CUTOFF_LOWPASS_HZ` según lo que muestre el espectro, en vez de ajustar a ciegas.

Esta prueba es exploratoria: todavía no decide si hay o no una gota — sirve para juntar datos reales y definir el umbral antes de escribir la lógica de detección final.
