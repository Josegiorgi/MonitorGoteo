# Prueba BPW34

Proyecto de prueba del sensor óptico de gota del monitor de goteo usando dos fotodiodos **BPW34**, cada uno con su amplificador de acondicionamiento, sobre ESP32-C3.

Por ahora solo levanta los datos: lee los dos canales del ADC con un período fijo (timer) y transmite ambos valores calibrados en mV, sin filtrar, en una línea `canal1,canal2` (el serial plotter los grafica como dos series).

## Cableado

| Señal | ESP32-C3 |
|---|---|
| Salida del amplificador del BPW34 n°1 | `GPIO_2` (ADC1_CH2) |
| Salida del amplificador del BPW34 n°2 | `GPIO_3` (ADC1_CH3) |
| Alimentación de los amplificadores | 3V3 |
| LED emisor (con resistencia en serie) | 5V |
| GND de amplificadores y LED | GND (común con el ESP32-C3) |

- Se evita `GPIO_0` (CH0) porque el driver `analog_io_mcu` lo comparte con el DAC.
- Los amplificadores van a 3.3V, así que su salida no puede superar la entrada máxima del ADC. Con la atenuación de 12 dB del driver, la calibración del ESP32-C3 es precisa hasta ~2.5 V: conviene ajustar la ganancia para que el nivel base quede por debajo de eso.
- La línea de 5V del LED no debe llegar a ningún GPIO.

## Acondicionamiento (amplificador de transimpedancia)

Cada BPW34 tiene su propio amplificador de transimpedancia con 1/4 de AD8604, trabajando en modo fotovoltaico (sin polarización inversa):

```
                    Cf = 68 pF
                ┌──────||──────┐
                │   Rf = 4.7k  │
                ├────/\/\/─────┤
                │              │
BPW34 cátodo ───┴──── (−) ─┐   │
                           AD8604 ──┴──── Vout → GPIO_2 / GPIO_3
         GND ──────── (+) ─┘
BPW34 ánodo ─── GND

AD8604: V+ = 3.3V, V− = GND
```

Vout = Ip · Rf (positiva, crece con la luz).

| Componente | Valor | Por qué |
|---|---|---|
| U1 | AD8604 (1/4), V+ = 3.3V, V− = GND | Rail-to-rail de entrada y salida, funciona con fuente simple de 3.3V |
| Rf | 4.7 kΩ | Con el LED a la distancia del soporte, Ip ≈ 300 µA: la salida queda cerca de 1.4 V, en la zona precisa del ADC y con margen para que la gota la mueva para cualquiera de los dos lados |
| Cf | 68 pF | Estabilidad del transimpedancia (compensa la capacidad del fotodiodo, Cd ≈ 70 pF). Corte en 1/(2π·Rf·Cf) ≈ 500 kHz |

### Cambios respecto del circuito de referencia

El esquema de partida es una nota de aplicación de Analog Devices (Rf = 69.8k, Cf = 6.2 pF, Ip = 43.4 µA) pensada para **fuente partida** (±2.75 a ±3 V). Para usarlo con fuente simple de 3.3V hubo que cambiar dos cosas:

1. **Orientación del BPW34: cátodo al pin (−), ánodo a GND.** Con fuente simple la salida solo puede moverse entre 0 y 3.3V. Con el fotodiodo al revés (ánodo al pin −), Vout = −Ip·Rf: la salida intenta ir a una tensión negativa y queda clavada en ~0V. Es lo que pasaba al principio: salida en 0 aunque el LED estuviera pegado al sensor.
2. **Rf baja de 69.8k a 4.7k.** Con el LED tan cerca, la corriente es ~7 veces la del diseño original (≈300 µA contra 43.4 µA): con 69.8k, 47k y 10k la salida saturaba. Rf se eligió como ≈ 1.5 V / Ip.

### Luz ambiente

El BPW34 es sensible desde el visible hasta ~1100 nm, así que capta la luz del sol además de la del LED: en las pruebas, con sol directo el amplificador saturaba incluso con Rf = 10k. Si el amplificador satura, la gota se pierde antes de llegar al ADC (ningún filtro digital la recupera), por eso el soporte tiene que ser opaco y tapar a los fotodiodos de todo menos del LED. Si hace falta más rechazo, existe la versión con filtro de luz visible (BPW34F).

## Por qué la señal sale limpia

Con Cf = 68 pF el amplificador no filtra nada en la banda que importa (corte ~500 kHz contra un muestreo de ~833 Hz), así que todo el ruido analógico hasta ~cientos de kHz se pliega (aliasing) sobre la señal muestreada. Aun así la señal se ve limpia, porque ese ruido es mucho más chico que la resolución del ADC y que la señal de la gota.

**Estimación del ruido a la salida del amplificador** (valores típicos, T = 300 K, Ip ≈ 300 µA, Rf = 4.7k):

| Fuente | Densidad a la salida | En la banda del amplificador |
|---|---|---|
| Térmico de Rf: √(4kT/Rf) · Rf | 1.9 pA/√Hz · 4.7k ≈ 9 nV/√Hz | ≈ 8 µV rms (ancho de banda de ruido 1.57 · 500 kHz ≈ 780 kHz) |
| Shot de la fotocorriente: √(2qIp) · Rf | 9.8 pA/√Hz · 4.7k ≈ 46 nV/√Hz | ≈ 40 µV rms |
| Tensión de ruido del AD8604 (e<sub>n</sub> del orden de 30 nV/√Hz, ver hoja de datos) × ganancia de ruido | ganancia de ruido ≈ 1 + Cd/Cf ≈ 2 en alta frecuencia | ≈ 170 µV rms (caso pesimista: integrado hasta ~4 MHz, la mitad del GBW) |
| **Total** (suma cuadrática) | | **≈ 0.2 mV rms** |

Para comparar:

- **Resolución del ADC:** 12 bits sobre ~3.1 V → ≈ 0.8 mV por cuenta. El ruido analógico total, incluso integrando todo el ancho de banda y dejando que se pliegue entero, queda **por debajo de 1 LSB**: lo que se ve en la salida es básicamente el ruido propio del ADC (unas pocas cuentas), no el del sensor.
- **Señal de la gota:** la gota modula una fotocorriente grande. Una variación de solo el 1% de Ip (3 µA) ya son ~14 mV a la salida (~18 cuentas del ADC), unas 70 veces el ruido analógico estimado.

Las claves son tres:

1. **Mucha luz → mucha señal.** Con el LED pegado, Ip ≈ 300 µA. El ruido shot crece solo con √Ip, mientras que la señal crece con Ip, así que la relación señal/ruido mejora con la corriente.
2. **Rf chica → poco ruido absoluto.** Todas las fuentes de ruido del transimpedancia escalan con Rf (o con la ganancia de ruido, que acá es ≈ 1-2). Con 4.7k quedan en decenas o cientos de µV.
3. **Modo fotovoltaico.** Sin polarización, el BPW34 no tiene corriente de oscuridad (ni su ruido shot), y su resistencia de shunt (Rsh ≈ 5 GΩ ≫ Rf) deja la ganancia de ruido en ≈ 1 en baja frecuencia.

Para contrastar, el OPT101 (ver [`prueba_opt101`](../prueba_opt101)) trabaja con una Rf interna de 1 MΩ y mucha menos fotocorriente: la gota daba pulsos del orden de 1 mV o menos, comparables al ruido, y hubo que filtrar y correlacionar para encontrarla.

**Limitaciones de la justificación:**

- Es una estimación con valores típicos. La comprobación experimental es grabar un tramo sin goteo, calcular el desvío estándar de cada canal (ruido real) y compararlo con la amplitud de las gotas en un tramo con goteo.
- Solo cubre el ruido propio del circuito. Las interferencias (parpadeo de tubos fluorescentes a 100 Hz, fuentes conmutadas, PWM cercanos) no escalan con Rf: una a 100 Hz cae dentro de la banda de la gota, y una de alta frecuencia se plegaría sin atenuación porque Cf = 68 pF no hace de antialiasing. Si aparecen, el primer paso es blindar el sensor, y el segundo, subir Cf a ≈ 68 nF (corte ≈ 500 Hz).

## Parámetros para ajustar

Están al principio de [`pruebabpw34.c`](main/pruebabpw34.c):

| Macro | Qué controla |
|---|---|
| `ADC_CHANNEL_BPW34_1` / `ADC_CHANNEL_BPW34_2` | Canales ADC de cada amplificador (`CH0` a `CH3` = `GPIO_0` a `GPIO_3`) |
| `SAMPLE_PERIOD_US` | Período entre muestras (por defecto 1200 us, ~833 Hz). No puede bajar de ~1 ms: con la consola a 115200 baudios, imprimir la línea con los dos valores tarda hasta ~960 us |

## Cómo usarlo

1. Conectar los amplificadores y el LED como se indica arriba, con los fotodiodos a la sombra (sin sol directo).
2. Flashear y abrir el monitor serie o el serial plotter (115200 baudios).
3. Con el haz libre, cada canal debería quedar alrededor de 1-2 V. Si está en ~0V, revisar la orientación del BPW34; si está pegado arriba (≳3000 mV), bajar Rf o la corriente del LED (Rf ≈ 1.5 V / Ip).
4. Dejar caer gotas y observar la respuesta de cada fotodiodo.
