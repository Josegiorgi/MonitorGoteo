# Análisis BPW34

Script de Python para analizar en el dominio del tiempo y de la frecuencia los registros tomados con [`pruebabpw34`](../pruebabpw34) (dos canales crudos, en mV), y elegir con datos reales los cortes del pasaaltos y del pasabajos del firmware. Es la contraparte de [`analisis_opt101`](../analisis_opt101) para el sensor nuevo.

No es un proyecto de firmware (no se compila con ESP-IDF): es un script de Python independiente, agrupado en el workspace junto a `pruebabpw34`.

## Qué hace

Compara un registro **sin goteo** contra uno **con goteo** (CSV `canal1,canal2,0,...` exportado del serial plotter), para cada uno de los dos fotodiodos, en el mismo orden que `analisis_opt101`:

1. Estadísticas básicas y las dos señales en el tiempo (sin detectar eventos todavía).
2. La FFT sin goteo, la FFT con goteo y las dos superpuestas.
3. La comparación por bandas de 10 Hz (amplitud promedio y cociente con/sin).
4. El promedio de las gotas: detecta las gotas, las superpone alineadas y calcula la forma típica. También revisa si el registro "sin goteo" tiene algún evento.
5. La relación señal/ruido (amplitud de la gota contra el desvío del ruido, medido en un tramo sin eventos).
6. Un pasabanda Butterworth de 4° orden (igual al del firmware, implementado con numpy) aplicado a los registros crudos, comparando la gota y la relación señal/ruido antes y después de filtrar.
7. **Señales filtradas por el firmware** (`MODE_FILTERED`): el mismo análisis (tiempo, FFT, bandas), la forma de la gota filtrada, la **plantilla** de cada canal y la **correlación cruzada** con la plantilla (umbral, falsos positivos, comparación con la detección por amplitud, validación leave-one-out y exportación de las plantillas a C), como en `analisis_opt101`.

8. **Prueba con ruido inventado:** suma a los registros filtrados reales un ruido simulado (blanco, 50 Hz con armónicos y picos aislados, el mismo en los dos canales, pasado por el pasabanda del firmware) y compara tres estrategias de umbral con la misma lógica del detector del firmware: umbral fijo, umbral adaptativo y adaptativo + control de forma.

Los gráficos de las secciones de señales crudas (1 a 9) están desactivados (comentados, con la marca `[gráfico desactivado: ...]`); las cuentas y los `print` siguen corriendo. Se vuelven a ver descomentando esas líneas.

## Requisitos y uso

- Python 3 con `numpy` y `matplotlib` (no hace falta `scipy`), y la extensión de **Python** de VS Code.
- Abrir [`analizar_bpw34.py`](analizar_bpw34.py) y correr celda por celda ("Run Cell"); los gráficos salen en la **Interactive Window**.
- Antes de correr, revisar las rutas `SIN_CSV_PATH` / `CON_CSV_PATH` y que `SAMPLE_PERIOD_US` coincida con el del firmware al tomar los registros (1200 µs). Los cortes a probar se cambian en `CORTE_PASAALTOS_HZ` / `CORTE_PASABAJOS_HZ`.

## Resultados (23/09/2026, `SeñalsinnGoteo.csv` y `SeñalconGoteo.csv`)

El registro sin goteo limpio dura 9.65 s, así que el registro con goteo se recorta a ese largo (13 gotas). Los dos se tomaron en sesiones distintas: el ruido de fondo puede no ser exactamente el mismo en las dos (ver la advertencia de [`analisis_opt101`](../analisis_opt101)).

| | BPW34 n°1 (GPIO_2) | BPW34 n°2 (GPIO_3) |
|---|---|---|
| Nivel base (sin goteo) | 1223 mV | 1184 mV |
| Ruido de fondo (desvío, sin goteo) | 4.3 mV (~5 cuentas del ADC) | 1.5 mV (~2 cuentas) |
| Tono de 100 Hz (sin goteo) | ~1.1 mV | ~0.3 mV |
| Gotas detectadas (9.65 s) | 13, cada 0.78 s (77 gotas/min) | 13, las mismas (diferencia ≤ 1.2 ms) |
| Mínimo de la gota | −680 ± 38 mV | −655 ± 36 mV |
| Rebote por encima de la base | +343 mV | +548 mV |
| Gota / ruido | 138 a 166 veces (≥ 43 dB) | 382 a 460 veces (≥ 52 dB) |
| Cociente con/sin por bandas de 10 Hz | ~17-20 en 10-50 Hz y en 150-190 Hz | ~50-60 en 130-210 Hz |
| 5% / 95% de la energía de la gota | por debajo de 6 Hz / 245 Hz | por debajo de 9 Hz / 261 Hz |

- **Registro sin goteo:** no tiene ningún evento. (El registro anterior, `SeñalsinGoteo.csv`, tenía una gota en t = 12.25 s que contaminaba el espectro "sin goteo" y bajaba los cocientes.)
- **Diferencia entre canales:** el canal 2 tiene casi 3 veces menos ruido que el canal 1 y mucho menos tono de 100 Hz. Como los dos usan el mismo ADC y el mismo circuito, la diferencia probablemente venga del cableado o de cuánta luz ambiente le llega a cada fotodiodo; conviene revisarlo.
- **Forma de la gota:** una caída de ~650 mV, un rebote por **encima** del nivel base (+340 / +500 mV) y una segunda caída, todo en ~10 ms. Una hipótesis es que la gota desvía la luz al entrar y al salir del haz, y en el medio hace de lente y la concentra sobre el fotodiodo. Con 1.2 ms entre muestras, la gota queda descripta con solo ~10 muestras.
- **Ruido de fondo:** es blanco (plano en todo el espectro, ~0.1 mV por componente en el canal 1 y ~0.035 mV en el canal 2) más un tono de **100 Hz** (parpadeo de la iluminación, el doble de los 50 Hz de la red). Su desvío (1.5-4.3 mV) es mayor que el estimado para el circuito analógico (~0.07 mV): está dominado por el ruido propio del ADC del ESP32-C3.
- **Espectro de la gota:** a diferencia del OPT101 (90-200 Hz), la gota es de **banda ancha**: tiene energía desde ~10 Hz hasta ~260 Hz, con un mínimo alrededor de 70-90 Hz. El 99% de la energía está por debajo de ~290-315 Hz, cerca de Nyquist (417 Hz): el muestreo actual alcanza, pero con poco margen.

### Qué implica para el filtrado

- **Filtrar casi no mejora la relación señal/ruido.** Con el pasabanda 5-260 Hz, el ruido baja de 4.3 a 3.4 mV (canal 1) y de 1.5 a 1.2 mV (canal 2), pero la gota también se achica y se deforma: la mejora queda entre +0.2 y +0.8 dB. Con la gota entre ~140 y ~460 veces por encima del ruido, alcanza con un umbral sobre la señal cruda (restando el nivel base).
- **Pasaaltos (bajas frecuencias):** su utilidad real es sacar la continua y las variaciones lentas (luz ambiente, deriva). El 5% de la energía de la gota queda por debajo de 6-9 Hz, así que un corte de **~5 Hz** no le saca casi nada. Ojo: un Butterworth de 4° orden a 5 Hz deja una cola positiva lenta después de cada gota (~50-100 mV durante más de 20 ms), y en `float` del firmware un corte tan bajo relativo a fs puede ser numéricamente delicado (como pasó con el OPT101). Una alternativa más simple es restar un nivel base que se actualice lento (media móvil exponencial).
- **Pasabajos (altas frecuencias):** el 95% de la energía está por debajo de ~250 Hz, así que un corte de **~260 Hz** conserva la gota y solo saca ruido blanco de 260-417 Hz. No hace falta un notch de 100 Hz: el tono es cientos de veces más chico que la gota.

## Resultados con las señales filtradas (23/09/2026, `SeñalFiltradasinGoteo.csv` y `SeñalFiltradaconGoteo.csv`)

Registros tomados con `MODE_FILTERED` (pasabanda 5-260 Hz) y con el antialias RC de 4.7 kΩ + 100 nF (~339 Hz) ya colocado.

| | BPW34 n°1 (GPIO_2) | BPW34 n°2 (GPIO_3) |
|---|---|---|
| Ruido (desvío, filtrada sin goteo) | 3.9 mV (máx. ±15 mV) | 3.5 mV (máx. ±13 mV) |
| Gotas (17.4 s) | 7, una cada 2.63 s | 7, las mismas |
| Mínimo de la gota filtrada | −393 ± 12 mV | −469 ± 31 mV |
| Rebote | +252 mV | +420 mV |
| Umbral de la correlación (12 desvíos sin goteo) | 59.7 | 51.4 |
| Máximo de la correlación sin goteo | 3.9 desvíos (0 falsos positivos) | 3.9 desvíos (0 falsos positivos) |
| Correlación en las gotas | 846 a 861 (la más chica, 14 veces el umbral) | 730 a 880 (la más chica, 14 veces el umbral) |
| Leave-one-out | 7 de 7 | 7 de 7 |

- **Forma de la gota filtrada:** en el canal 1 es muy repetible (caída a ~−390 mV, rebote a ~+250 mV y una cola positiva lenta, que es la respuesta del pasaaltos de 5 Hz). En el canal 2 es más oscilante (dos caídas y dos rebotes en ~10 ms) y varía más de gota a gota: con 1.2 ms entre muestras, detalles tan rápidos quedan descriptos con pocas muestras y cada gota cae distinto respecto del instante de muestreo.
- **La plantilla (±20 ms, 32 muestras) corta la cola del pasaaltos**, que todavía vale ~50-100 mV a +20 ms. No afecta la detección, pero si se quiere una plantilla más completa hay que alargar la ventana.
- **Correlación:** detecta las 7 gotas en los dos canales, sin falsos positivos, en el mismo instante que la detección por amplitud (en una gota del canal 2 el pico de la correlación cae 6 ms corrido, sobre un lóbulo lateral de la plantilla oscilante). Con la gota ~14 veces por encima del umbral, hay mucho margen.
- **Cocientes con/sin por bandas más bajos que con las crudas (~10 contra ~17-60):** no es que el filtro empeore la señal. La amplitud promedio del espectro depende de cuántas gotas hay en el registro, y en este goteaba más lento (una gota cada 2.63 s contra una cada 0.78 s). Para comparar cocientes entre registros hace falta el mismo ritmo de goteo.
- **Ruido:** con el antialias puesto el ruido quedó en ~3.5-3.9 mV, como se esperaba: está dominado por el ADC, que el antialias no toca.
- **Para usar las plantillas en el firmware:** `xcorr_detector` guarda un único estado global (como tenía `iir_filter`), así que para correlacionar los dos canales a la vez hay que agregarle instancias por canal.

## Prueba con ruido inventado (23/09/2026)

Al probar el firmware, conectar otra computadora al mismo enchufe metió ruido que el detector tomó como gotas. Para ver cómo reacciona la correlación, el script suma a los registros filtrados reales un ruido **inventado** (niveles a la entrada del ADC, pasado por el mismo pasabanda del firmware y el mismo en los dos canales). Resultado con los dos canales combinados como en el firmware (falsas sin goteo / falsas con goteo, gotas detectadas de 7):

| Escenario | Umbral fijo (firmware actual) | Umbral adaptativo (10 desvíos) | Adaptativo + forma (≥ 0.6) |
|---|---|---|---|
| Sin ruido agregado | 0 / 0, 7/7 | 0 / 0, 7/7 | 0 / 0, 7/7 |
| Blanco (15 mV) | 7 / 12, 7/7 | 0 / 0, 7/7 | 0 / 0, 7/7 |
| Red 50 Hz (40 mV) | 67 / 68, 5/7 | 1 / 1, 7/7 | 0 / 0, 7/7 |
| Picos (3/s, 300 mV) | 32 / 27, 7/7 | 30 / 22, 7/7 | 17 / 16, 7/7 |
| Todo junto | 67 / 71, 1/7 | 3 / 2, 7/7 | 1 / 1, 7/7 |

- **El umbral fijo no aguanta ruido nuevo:** con 50 Hz, el detector dispara cada ~115 ms y el LED quedaría prendido.
- **El umbral adaptativo** (estimar el desvío de la correlación mientras funciona y usar 10 veces ese valor) resuelve el ruido constante (blanco y 50 Hz). Su límite es cuando 10 x el desvío llega a la altura de las gotas; con 50 Hz eso le pasa primero al canal 1, cuya plantilla tiene mucho contenido en bajas frecuencias.
- **Los picos aislados son lo más difícil:** casi no mueven el umbral adaptativo, y el control de forma descarta solo la mitad.
- **Combinar los canales no ayuda** con ruido que entra por el enchufe, porque llega igual a los dos.
- Los niveles de ruido son inventados: para ajustar los parámetros hay que grabar el ruido real (`MODE_FILTERED` con la otra computadora enchufada). La solución de fondo es que el ruido no entre (alimentación filtrada o a batería, masa en estrella, desacople junto al MCP6004, cables cortos).
