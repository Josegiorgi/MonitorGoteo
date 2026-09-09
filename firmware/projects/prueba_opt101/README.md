# Prueba OPT101

Proyecto de prueba del sensor óptico de gota del monitor de goteo, usando el fotodiodo con amplificador de transimpedancia integrado OPT101 (driver [`analog_io_mcu`](../../drivers/microcontroller/inc/analog_io_mcu.h)) sobre ESP32-C3.

## Qué hace

El OPT101 y un LED enfrentado forman una barrera de luz alrededor del hueco por donde cae la gota. Mientras el haz llega completo al sensor, la salida se mantiene en un nivel base; al pasar una gota el haz se interrumpe parcialmente y la tensión cae de forma momentánea.

Este proyecto solo lee el pin ADC en loop (cada 20 ms) e imprime el valor crudo (0 a 4095, sin calibrar a mV) por consola. Sirve para verificar el cableado, ver el nivel base con el haz libre, y observar la forma del pulso cuando pasa una gota — antes de escribir la lógica de detección final.

La salida se puede graficar directo en un serial plotter (el de VS Code/ESP-IDF o el de Arduino IDE) para ver la forma de la señal en tiempo real.

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
| `SAMPLE_PERIOD_MS` | Período entre muestras; bajarlo si el pulso de la gota se ve recortado en el plotter |

## Cómo usarlo

1. Conectar el OPT101 y el LED como se indica arriba, enfrentados a través del hueco de la cámara de goteo.
2. Flashear y abrir el monitor serie (o el serial plotter).
3. Con el haz libre (sin gota), anotar el nivel base.
4. Dejar caer gotas reales y observar cuánto y por cuánto tiempo cae el valor respecto al nivel base.

Esta prueba es exploratoria: todavía no decide si hay o no una gota — sirve para juntar datos reales y definir el umbral antes de escribir la lógica de detección final.
