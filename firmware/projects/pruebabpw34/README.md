# Prueba BPW34

Proyecto de prueba del sensor óptico de gota del monitor de goteo usando dos fotodiodos **BPW34**, cada uno con su amplificador de acondicionamiento, sobre ESP32-C3.

Lee los dos canales del ADC con un período fijo (timer) y transmite ambos valores en una línea `canal1,canal2` (el serial plotter los grafica como dos series). Tiene tres modos, seleccionables con `TEST_MODE`:

- **`MODE_RAW`**: los valores calibrados en mV, sin filtrar.
- **`MODE_FILTERED`**: cada canal pasa por un pasabanda Butterworth de 4° orden (pasaaltos de 5 Hz + pasabajos de 260 Hz) y se transmite en mV enteros, centrado en 0 (sin la continua). Los cortes salen del espectro de energía de la gota promedio medido en [`analisis_bpw34`](../analisis_bpw34).
- **`MODE_DETECTOR`** (por defecto): además del pasabanda, cada canal pasa por un detector de correlación cruzada con **su propia plantilla** de la gota ([`drop_template.h`](main/drop_template.h), el promedio de 7 gotas filtradas reales). Cuando **cualquiera de los dos canales** detecta una gota, se prende un LED (`GPIO_10`) durante 200 ms. Sigue transmitiendo los dos canales filtrados, igual que `MODE_FILTERED`.

## Cableado

| Señal | ESP32-C3 |
|---|---|
| Salida del amplificador del BPW34 n°1 | `GPIO_2` (ADC1_CH2) |
| Salida del amplificador del BPW34 n°2 | `GPIO_3` (ADC1_CH3) |
| Alimentación de los amplificadores | 3V3 |
| LED emisor (con resistencia en serie) | 5V |
| LED indicador de gota (solo `MODE_DETECTOR`), con resistencia en serie a GND | `GPIO_10` (`LED_2`) |
| GND de amplificadores y LED | GND (común con el ESP32-C3) |

- Entre la salida de cada amplificador y su GPIO va un **antialias RC**: 4.7 kΩ en serie y 100 nF a GND, cerca del pin (corte ≈ 339 Hz, por debajo de Nyquist, 417 Hz). Las plantillas del detector se tomaron con ese RC puesto.
- Se evita `GPIO_0` (CH0) porque el driver `analog_io_mcu` lo comparte con el DAC.
- Los amplificadores van a 3.3V, así que su salida no puede superar la entrada máxima del ADC. Con la atenuación de 12 dB del driver, la calibración del ESP32-C3 es precisa hasta ~2.5 V: conviene ajustar la ganancia para que el nivel base quede por debajo de eso.
- La línea de 5V del LED no debe llegar a ningún GPIO.

## Acondicionamiento (amplificador de transimpedancia)

Cada BPW34 tiene su propio amplificador de transimpedancia con 1/4 de MCP6004, trabajando en modo fotovoltaico (sin polarización inversa):

```
                    Cf = 68 pF
                ┌──────||──────┐
                │   Rf = 4.7k  │
                ├────/\/\/─────┤
                │              │
BPW34 cátodo ───┴──── (−) ─┐   │
                          MCP6004 ──┴──── Vout → GPIO_2 / GPIO_3
         GND ──────── (+) ─┘
BPW34 ánodo ─── GND

MCP6004: V+ = 3.3V, V− = GND
```

Vout = Ip · Rf (positiva, crece con la luz).

| Componente | Valor | Por qué |
|---|---|---|
| U1 | MCP6004 (1/4), V+ = 3.3V, V− = GND | Rail-to-rail de entrada y salida, funciona con fuente simple desde 1.8V. GBW ≈ 1 MHz, e<sub>n</sub> ≈ 28 nV/√Hz a 1 kHz (valores típicos de la hoja de datos) |
| Rf | 4.7 kΩ | Con el LED a la distancia del soporte, Ip ≈ 300 µA: la salida queda cerca de 1.4 V, en la zona precisa del ADC y con margen para que la gota la mueva para cualquiera de los dos lados |
| Cf | 68 pF | Estabilidad del transimpedancia (compensa la capacidad de entrada: Cd ≈ 70 pF del fotodiodo + ~6 pF del operacional). Corte en 1/(2π·Rf·Cf) ≈ 500 kHz |

**Margen de estabilidad con el MCP6004.** El Cf mínimo para ~45° de margen de fase es
Cf,mín = [1 + √(1 + 8π·Rf·Cin·GBW)] / (4π·Rf·GBW). Con Rf = 4.7k, Cin ≈ 76 pF y GBW ≈ 1 MHz da **≈ 70 pF**: los 68 pF quedan justo en el límite. El circuito es estable (la señal se ve bien), pero con poco margen: puede tener algo de sobrepico cerca de los ~500 kHz, y la carga capacitiva de la entrada del ADC lo reduce un poco más. Esto no afecta a la gota (que está en la banda de ~100 Hz), pero si se quiere margen, subir Cf a 100-150 pF (corte 340-230 kHz) o directamente a ~68 nF (corte ~500 Hz, que además hace de antialiasing).

### Cambios respecto del circuito de referencia

El esquema de partida es una nota de aplicación de Analog Devices (AD8604, Rf = 69.8k, Cf = 6.2 pF, Ip = 43.4 µA) pensada para **fuente partida** (±2.75 a ±3 V). Se armó con un MCP6004 en lugar del AD8604, y para usarlo con fuente simple de 3.3V hubo que cambiar dos cosas:

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
| Tensión de ruido del MCP6004 (e<sub>n</sub> ≈ 28 nV/√Hz) × ganancia de ruido | ganancia de ruido ≈ 1 + Cin/Cf ≈ 2.1 en alta frecuencia → ≈ 59 nV/√Hz | ≈ 50 µV rms (la ganancia de ruido cae a partir de GBW / 2.1 ≈ 470 kHz) |
| **Total** (suma cuadrática) | | **≈ 65 µV rms (≈ 0.07 mV)** |

Para comparar:

- **Resolución del ADC:** 12 bits sobre ~3.1 V → ≈ 0.8 mV por cuenta. El ruido analógico total, incluso integrando todo el ancho de banda y dejando que se pliegue entero, queda **por debajo de 1 LSB**: lo que se ve en la salida es básicamente el ruido propio del ADC (unas pocas cuentas), no el del sensor.
- **Señal de la gota:** la gota modula una fotocorriente grande. Una variación de solo el 1% de Ip (3 µA) ya son ~14 mV a la salida (~18 cuentas del ADC), unas 200 veces el ruido analógico estimado.

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
| `TEST_MODE` | `MODE_RAW`, `MODE_FILTERED` o `MODE_DETECTOR` |
| `SAMPLE_PERIOD_US` | Período entre muestras (por defecto 1200 us, ~833 Hz). No puede bajar de ~1 ms: con la consola a 115200 baudios, imprimir la línea con los dos valores tarda hasta ~960 us |
| `CUTOFF_HIGHPASS_HZ` | Corte del pasaaltos (por defecto 5 Hz). Saca la continua y la deriva lenta; en el análisis, solo el 5% de la energía de la gota queda por debajo de ~6-9 Hz |
| `CUTOFF_LOWPASS_HZ` | Corte del pasabajos (por defecto 260 Hz). El 95% de la energía de la gota queda por debajo de ~245-261 Hz; por encima solo hay ruido blanco hasta Nyquist (~417 Hz) |
| `FILTER_ORDER` | Orden del Butterworth, para ambos filtros (por defecto `ORDER_4`) |
| `DETECTION_LED` | LED que se prende al detectar una gota (por defecto `LED_2` = GPIO_10; evitar `LED_1` = GPIO_20, que es el RX de la UART de la consola) |
| `XCORR_THRESHOLD_CH1` / `XCORR_THRESHOLD_CH2` | Umbral de la correlación de cada canal, en mV de la señal filtrada (59.7 y 51.4 = 12 veces el desvío de la correlación sin goteo; la gota más chica da ~14 veces el umbral) |
| `XCORR_PEAK_WINDOW_MS` / `XCORR_REFRACTORY_MS` | Tiempo que el detector se queda con el máximo tras pasar el umbral (15 ms) y tiempo que ignora la señal después de detectar (100 ms) |
| `LED_ON_TIME_MS` | Cuánto queda prendido el LED tras cada gota (200 ms) |
| `BASELINE_SAMPLES` | Muestras que se promedian al arrancar para estimar el nivel base de cada canal, que se resta antes de filtrar |

### Detalles del filtrado

- **Un filtro por canal.** Se usan las instancias de filtro (`iir_filter_t`) del componente `iir_filter` de middleware: cada canal tiene su propio pasabajos y su propio pasaaltos, con su propia memoria. Las funciones originales `LowPassFilter()` / `HiPassFilter()` tienen un único filtro interno y mezclarían los dos canales.
- **Se resta el nivel base antes de filtrar.** Los biquads de esp-dsp son de forma directa II: con ~1200 mV de continua a la entrada y un pasaaltos tan bajo (5 Hz contra fs ≈ 833 Hz), su estado interno crece a cientos de miles y en `float` se pierde resolución. Simulado en `float` de 32 bits con un registro real, el error baja de ~0.13 mV rms (picos de ~3 mV) a ~0.001 mV al restar el nivel base medido al arrancar.
- **Salida en mV enteros.** A diferencia del OPT101, que necesitaba decimales, acá la gota mueve cientos de mV y el ruido es de unos pocos mV, así que un decimal no aporta nada y la línea no entraría en el período de muestreo.
- **Un detector por canal.** Igual que con los filtros, se usan las instancias (`xcorr_detector_t`) del componente `xcorr_detector`: cada canal tiene su plantilla, su umbral y su propio estado. Las funciones originales (`XCorrInit()` / `XCorrProcess()`) siguen existiendo para `prueba_opt101`.
- **Las plantillas dependen de toda la cadena:** `SAMPLE_PERIOD_US`, los cortes del pasabanda y el antialias RC. Si cambia algo de eso, hay que regenerarlas con la última celda de `analisis_bpw34`.
- **Verificación:** simulando en Python la misma lógica del detector sobre los registros filtrados, da 0 detecciones sin goteo y 7 de 7 con goteo en los dos canales. El LED se prende ~25 ms después del mínimo de la gota (el detector necesita recibir la gota entera antes de compararla con la plantilla).
- **Qué esperar:** según el análisis, el filtro saca la deriva pero mejora la relación señal/ruido menos de 1 dB. Además, el pasaaltos deja una cola lenta después de cada gota (~50-100 mV durante más de 20 ms).

## Cómo usarlo

1. Conectar los amplificadores y el LED como se indica arriba, con los fotodiodos a la sombra (sin sol directo).
2. Flashear y abrir el monitor serie o el serial plotter (115200 baudios).
3. Con el haz libre, cada canal debería quedar alrededor de 1-2 V. Si está en ~0V, revisar la orientación del BPW34; si está pegado arriba (≳3000 mV), bajar Rf o la corriente del LED (Rf ≈ 1.5 V / Ip).
4. Dejar caer gotas y observar la respuesta de cada fotodiodo.
5. Con `TEST_MODE = MODE_FILTERED`, la señal sin goteo debería oscilar alrededor de 0 con unos pocos mV de ruido, y cada gota verse como una caída de cientos de mV. Al arrancar, el ESP32 mide el nivel base con el haz libre: conviene que no caiga una gota justo en ese momento (dura unos milisegundos).
