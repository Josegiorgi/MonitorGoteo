# Prueba OPT101

Proyecto de prueba del sensor óptico de gota del monitor de goteo, usando el fotodiodo con amplificador de transimpedancia integrado OPT101 (driver [`analog_io_mcu`](../../drivers/microcontroller/inc/analog_io_mcu.h)) sobre ESP32-C3.

## Qué hace

El OPT101 y un LED enfrentado forman una barrera de luz alrededor del hueco por donde cae la gota. Mientras el haz llega completo al sensor, la salida se mantiene en un nivel base; al pasar una gota el haz se interrumpe/refracta parcialmente y la tensión cambia de forma momentánea.

Este proyecto lee el ADC de forma periódica, disparada por un timer de hardware (driver [`timer_mcu`](../../drivers/microcontroller/inc/timer_mcu.h)), la pasa por un **filtro pasabanda** (pasabajos + pasaaltos en cascada) y transmite el resultado por consola **en tiempo real, sin cortes**. Sirve primero para verificar que el LED no sature la salida del OPT101, y después para ver la forma del pulso cuando pasa una gota.

**Filtro pasabanda:** usa el componente [`signal_processing`](../../middleware/signal_processing/inc/iir_filter.h) (Butterworth IIR, biquads en cascada) de la cátedra de Sistemas de Adquisición y Procesamiento de Señales ([SAPS_FIUNER](https://github.com/prototipado/SAPS_FIUNER)), ya presente en este repo.

- **Pasabajos** (`CUTOFF_LOWPASS_HZ`): suaviza el ruido de muestra a muestra.
- **Pasaaltos** (`CUTOFF_HIGHPASS_HZ`), en cascada después del pasabajos: saca la continua/deriva de fondo, dejando la señal centrada en 0 (la gota aparece como pico positivo o negativo, ya no como una tensión absoluta — por eso la salida ahora es un entero con signo, no una tensión en mV).

`LowPassFilter()`/`HiPassFilter()` guardan su estado internamente entre llamadas, así que se los llama con una muestra por vez (`signal_lenght=1`) y funcionan como filtro en tiempo real, no por bloques.

**Por qué timer_mcu y no `vTaskDelay`:** el pulso de la gota dura pocos milisegundos, así que hace falta muestrear rápido. Con `vTaskDelay`, el período mínimo real queda atado al tick de FreeRTOS, y pedir un período más corto que un tick redondea a `vTaskDelay(0)`, que no le cede el CPU a la tarea IDLE y dispara el watchdog. La interrupción del timer solo notifica a una tarea (no lee el ADC ni hace `printf` ahí mismo, que no es seguro dentro de una ISR); la lectura, el filtrado y la impresión pasan en una tarea normal que espera esa notificación.

**Sobre el margen de tiempo:** `SAMPLE_PERIOD_US` es un valor fijo (por defecto 1000us, el techo del rango 0.5-1ms necesario para resolver bien el pico de la gota). Tiene que quedar por encima de lo que tarda leer+filtrar+imprimir una muestra, o la tarea nunca llega a bloquearse de verdad entre muestra y muestra y aparece el problema del watchdog (queda siempre con una notificación pendiente en vez de ceder el CPU) — si eso pasa, la salida es simplificar el filtro (`FILTER_ORDER` más bajo) o revisar el baudrate de la consola antes que subir el período por encima de 1ms.

La salida se puede graficar directo en un serial plotter (el de VS Code/ESP-IDF o el de Arduino IDE) para ver la forma de la señal en tiempo real.

## Contexto de las pruebas anteriores

Los primeros registros (CSV del 15/09/2026) mostraron que el "con goteo" era indistinguible del "sin goteo" — solo deriva lenta, ningún transitorio atribuible a una gota. Se descartó que fuera un problema de resolución temporal (muestrear más rápido no cambió nada), y se confirmó con una prueba manual (pasar un objeto opaco por el hueco) que el haz y el sensor están bien alineados. El sospechoso principal identificado: **el LED y el OPT101 están separados de la pared del tubo por un gap de aire**, y el tubo curvo actúa como lente en cada cara — con ese gap, cualquier variación mecánica chica se traduce en un desplazamiento grande de dónde cae la luz sobre el sensor, lo que puede explicar la deriva de fondo. Está en curso un rediseño del soporte para que agarre la cámara sosteniendo al sensor y al LED directamente, sin ese gap.

Mientras tanto, este proyecto se simplificó (se sacó un modo experimental de resta de baseline que se había agregado) para retomar desde cero con el soporte nuevo, viendo primero el valor absoluto en mV.

## Cableado

| Pin del OPT101 | Conexión |
|---|---|
| `+V` (pin 1) | 3V3 |
| `GND` (pines 3, 4 y 5, unidos entre sí) | GND |
| `Vo` (pin 8, salida) | `GPIO_2` (ADC1_CH2) |

El OPT101 debe quedar enfrentado al LED emisor, con el hueco por donde cae la gota entre ambos. La salida `Vo` ya viene amplificada por el transimpedancia interno del chip, no requiere circuito externo adicional para esta prueba.

## Parámetros para ajustar

Están al principio de [`prueba_opt101.c`](main/prueba_opt101.c):

| Macro | Qué controla |
|---|---|
| `ADC_CHANNEL` | Canal ADC donde está conectado `Vo` (`CH0` a `CH3`, ver [`analog_io_mcu.h`](../../drivers/microcontroller/inc/analog_io_mcu.h)) |
| `SAMPLE_PERIOD_US` | Período fijo entre muestras, en microsegundos. Tiene que quedar entre 0.5 y 1ms para resolver bien el pico de la gota (por defecto 1000us, el techo de ese rango, para tener más margen posible contra el watchdog) |
| `CUTOFF_LOWPASS_HZ` | Corte del pasabajos (por defecto 60 Hz — punto intermedio: 30 Hz aplanaba el pulso de la gota junto con el ruido, 100 Hz dejaba pasar demasiado ruido) |
| `CUTOFF_HIGHPASS_HZ` | Corte del pasaaltos (por defecto 1 Hz — solo saca la deriva de fondo, bastante por debajo del contenido en frecuencia del pulso) |
| `FILTER_ORDER` | Orden del Butterworth (`ORDER_2`, `ORDER_4`, `ORDER_6` u `ORDER_8`), para ambos filtros |

## Cómo usarlo

1. Conectar el OPT101 y el LED como se indica arriba, enfrentados a través del hueco de la cámara de goteo.
2. Flashear y abrir el monitor serie (o el serial plotter). Al arrancar, revisar que el ADC no esté saturando (si querés ver la tensión absoluta antes del filtro para chequear esto, es fácil agregar un segundo `printf` con `voltage_mV` en `ADCTask`, antes de filtrar).
3. Con el haz libre (sin gota), la salida filtrada debería oscilar cerca de 0 (ya sin continua). Dejar caer gotas reales y observar el pico (positivo o negativo) que aparece al pasar cada una.
4. Si el pico queda muy achatado por el ruido o muy aplanado por el filtro, ajustar `CUTOFF_LOWPASS_HZ` (subir dejaría pasar más ruido pero menos filtrado del pulso, bajar lo contrario).

Esta prueba es exploratoria: todavía no decide si hay o no una gota — sirve para juntar datos reales y definir el umbral antes de escribir la lógica de detección final.
