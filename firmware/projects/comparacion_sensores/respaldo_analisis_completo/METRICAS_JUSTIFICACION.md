# Métricas de detección: qué mide cada una y por qué

Base para el capítulo de Metodología. Todo lo que sigue corresponde al script [`analizar_deteccion.py`](analizar_deteccion.py), que lee los registros de `datos/` sin modificarlos. Las tablas y figuras salen en `resultados/`.

## Alcance y criterio

Se comparan el OPT101 (1 canal) y el BPW34 (2 canales) **solo en su capacidad de detectar gotas**. Como las transimpedancias son muy distintas (≈ 1 MΩ contra 4,7 kΩ), la amplitud en mV no se usa para comparar. Se usan métricas que no dependen de la ganancia (SNR, margen entre señal y ruido, consistencia, robustez). Los valores en mV (ΔV, σ) se informan solo como datos descriptivos.

Las frecuencias de muestreo (700 µs para el OPT101 y 1200 µs para el BPW34) son una condición de diseño de cada método, ya justificada en análisis previos. No se reanalizan ni se diezman.

## Registros usados

| Registro | Sensor / modo | Muestreo | Duración | Contenido |
|---|---|---|---|---|
| `datos/opt101.csv` | OPT101, `MODE_DETECTOR` (08/10) | 700 µs | 19,0 s | señal filtrada (pasabanda 85-195 Hz) y detección del firmware |
| `datos/bpw34.csv` | BPW34, `MODE_DETECTOR` (08/10) | 1200 µs | 13,2 s | canales 1 y 2 filtrados (pasabanda 5-260 Hz) y detección del firmware |
| `datos/crudos/opt101_crudo_18-09_DatosGoteo.csv` | OPT101, `MODE_RAW` (18/09) | 700 µs | 26,8 s | señal sin filtrar, solo para contraste y deriva |
| `datos/crudos/bpw34_crudo_23-09_SenalconGoteo.csv` | BPW34, `MODE_RAW` (23/09) | 1200 µs | 19,1 s | ídem |

Los dos registros principales son del mismo día, tomados en forma secuencial (no simultánea). Los registros crudos son de sesiones anteriores y se usan solo para las métricas que necesitan el nivel de continua (3 y 8).

## Detector común

Es el detector que ya usaban el firmware y los análisis previos (`xcorr_detector.c`), con la misma lógica reproducida en Python:

1. Se correlaciona la señal filtrada con la plantilla de la gota del sensor (`drop_template.h` de cada proyecto, una por canal en el BPW34). Cada plantilla se normaliza a energía unitaria, de modo que la correlación queda en mV.
2. Cuando la correlación supera el umbral, se espera una ventana de pico de 15 ms y se confirma la gota al final de esa ventana.
3. Después de cada detección hay un período refractario de 100 ms.

**Umbral:** `K_REF · σ_corr`, con `K_REF = 12`. Es el mismo criterio de los análisis previos (12 desvíos de la correlación sin gota). `σ_corr` se estima con el MAD de la correlación (robusto) y se refina tres veces calculando el desvío estándar fuera de ±(100 ms antes, 50 ms después) de cada detección.

**Por qué este detector.** Una vez que la plantilla está definida, la correlación hace que el criterio de detección sea el mismo para ambos sensores. Un umbral directo sobre la amplitud depende de la forma del pulso, que es distinta en cada sensor. Las plantillas provienen de registros anteriores (OPT101: sesiones de septiembre; BPW34: 23/09), no de los registros que se comparan, así que no hay circularidad.

**Nota sobre los umbrales del firmware.** Los umbrales que quedaron cargados (13,7 en el OPT101 y 300 en el BPW34) no siguen el mismo criterio: el primero salió de 12 desvíos con el ruido de aquella sesión, y el segundo se subió a mano. Para comparar se aplica la misma regla a los dos, y las detecciones guardadas en los CSV se informan aparte (21 para el OPT101 y 62 muestras marcadas para el BPW34, dos por gota, una por canal).

## Parámetros declarados

| Parámetro | Valor | Justificación |
|---|---|---|
| `K_REF` | 12 | Criterio de los análisis previos |
| Barrido de k | 3 a 30 (paso 1) y 35 a 200 (paso 5) | Cubre desde por debajo del ruido hasta mucho más que la gota más débil |
| Ventana de pico | 15 ms | Igual al firmware. La plantilla oscila y su correlación tiene lóbulos laterales a unos 6 ms del pico |
| Período refractario | 100 ms | Igual al firmware. ≈ 13 veces el ancho del pulso y menor que 0,6 veces la mediana de intervalos (≈ 0,25 s) |
| Exclusión para ruido | 100 ms antes y 50 ms después de cada detección | Cubre el pulso y su rebote inmediato |
| Línea base local | mediana móvil de 1 s | Más larga que el pulso, más corta que la deriva |
| Ventana del pulso | 100 ms antes de la detección | La detección se confirma ~15 ms después del cruce y la plantilla dura ~40 ms (56 muestras a 700 µs y 32 a 1200 µs) |
| Ventana de coincidencia entre canales | ±15 ms | Igual a la ventana de pico. El desfase observado fue de 3,2 ms en promedio y 4,8 ms como máximo |
| Intervalos anómalos | < 0,6× o > 1,6× la mediana | Criterio pedido para el ensayo |
| Meseta | conteo dentro de ±10 % del conteo a `K_REF` | Criterio declarado |
| Ventanas de deriva | 2 s | Mayores que el pulso e intervalo entre gotas |
| LSB del ADC | 0,76 mV (3100 mV / 4096) | Nominal, 12 bits con atenuación de 12 dB. Es aproximado |

## Las métricas

### 1. Ruido de línea base (σ)

- **Qué mide.** La variabilidad de la señal filtrada cuando no cae una gota.
- **Por qué importa.** Fija el umbral mínimo de detección: cuanto mayor es σ respecto del pulso, más gotas débiles se pierden o más falsos disparos hay.
- **Cómo.** MAD × 1,4826 de la señal en las ventanas sin gota (fuera de ±(100 ms antes, 50 ms después) de cada detección). Se informa también el desvío estándar. Se compara con el LSB nominal del ADC (0,76 mV).
- **Por qué MAD.** En el BPW34 los pulsos están separados ~0,4 s y la cola lenta de cada uno (por el pasaaltos de 5 Hz) queda dentro de las "ventanas sin gota". Eso infla el desvío estándar (2,6 mV y 5,9 mV, contra 1,5 mV y 3,0 mV con MAD). El MAD no se ve afectado.
- **Interpretación.** El σ del OPT101 es de 1,4 cuentas y el del BPW34 de 2,0 y 3,9 cuentas. El ruido del OPT101 está cerca de la cuantización del ADC.
- **Limitaciones.** El ruido se mide dentro del mismo registro con goteo (no hay un registro sin goteo del mismo día). Por eso incluye restos de gotas no detectadas y colas.

### 2. Relación señal-ruido (SNR), métrica principal

- **Qué mide.** Cuán separado está el pulso de cada gota del ruido.
- **Por qué importa.** Integra el tamaño del pulso y el ruido en un solo valor. No depende de la ganancia porque la señal y el ruido se amplifican juntos.
- **Cómo.** Para cada gota, ΔV es el máximo valor absoluto de (señal − línea base local) en los 100 ms anteriores a la detección. `SNR = 20·log10(ΔV / σ)` en dB. Se informan media ± DE, mínimo y percentil 5. El mínimo y el percentil 5 importan más que la media: la gota más débil es la que se pierde.
- **Limitaciones.** El SNR se calcula después del filtrado digital, que es distinto en cada sensor (85-195 Hz y 5-260 Hz). Por lo tanto no es una propiedad pura del sensor. Como los registros ya vienen filtrados, no se puede igualar el filtro.

### 3. Contraste (ΔV / V_base)

- **Qué mide.** Qué fracción de la luz recibida modula la gota.
- **Por qué importa.** Es independiente de la ganancia y de la intensidad del LED. Un contraste alto da margen frente a cambios de iluminación.
- **Cómo.** Se necesita el nivel de continua, que los registros filtrados no tienen. Se calcula sobre los registros crudos: eventos con |x − mediana| > 10 σ_MAD, agrupados con 100 ms de separación, ΔV = máximo desvío respecto de la mediana del registro, contraste = ΔV / mediana.
- **Limitaciones (importantes).** (a) Los registros crudos son de otras fechas (18/09 y 23/09). (b) El nivel base del OPT101 (≈ 2499 mV) está en el límite del rango de salida y su valor es casi constante entre sesiones, por lo que la señal podría estar comprimida y el contraste subestimado. (c) El σ crudo de ambos (1,48 mV) es el valor mínimo que permite el MAD con datos enteros, o sea que se limita por resolución. (d) En el OPT101 se detectaron 36 eventos en 26,8 s y el ΔV medio (20,6 mV) está cerca del umbral de evento (14,8 mV), así que parte de los eventos puede no ser gota. Se informa como dato descriptivo y no entra en la decisión.

### 4. Repetibilidad del pulso

- **Qué mide.** Cuánto varía la amplitud entre gotas y cuánto dura el pulso.
- **Por qué importa.** Si la amplitud cambia mucho de una gota a otra (por ejemplo, porque no pasan siempre por el mismo lugar del haz), las gotas pequeñas quedan cerca del umbral.
- **Cómo.** Coeficiente de variación de ΔV (desvío / media) por sensor y canal. Duración del pulso como ancho a mitad de altura (FWHM): tiempo entre la primera y la última muestra con |señal − base| ≥ ΔV/2 dentro de la ventana del pulso.
- **Limitaciones.** Como el pulso es oscilante (caída, rebote, caída), el FWHM mide la extensión de las oscilaciones principales y no un único lóbulo. Es un dato descriptivo.

### 5. Consistencia de detección sin referencia externa

- **Qué mide.** Si los intervalos entre detecciones son coherentes con un goteo casi periódico.
- **Por qué importa.** No hay conteo de referencia, pero el goteo por gravedad con caudal fijo es casi periódico. Un intervalo menor que 0,6 veces la mediana sugiere una doble detección. Uno mayor que 1,6 veces sugiere una gota perdida.
- **Cómo.** Serie de intervalos entre detecciones, mediana, coeficiente de variación y porcentaje de intervalos anómalos. Se informa también cuántas medianas pasan entre la última detección y el final del registro.
- **Limitaciones.** No detecta errores sistemáticos que mantengan el ritmo, ni gotas perdidas al principio o al final del registro. En el OPT101 el registro tiene ~12 s sin detecciones al final (≈ 30 intervalos medianos) y no se sabe si seguía cayendo agua. Por eso se analiza completo y recortado a los primeros 6,6 s. Es un indicador de consistencia, no de exactitud.

### 6. Robustez al umbral

- **Qué mide.** Cuánto cambia el conteo al variar el umbral.
- **Por qué importa.** En el dispositivo el umbral se fija una vez y tiene que funcionar en distintas condiciones. Un sensor con buena separación detecta la misma cantidad de gotas en un rango amplio (meseta ancha). Uno marginal cambia el conteo con cualquier ajuste.
- **Cómo.** Se repite la detección para k entre 3 y 200 con el σ de la correlación fijo y se grafica el conteo contra k. El ancho de la meseta es el rango de k, contiguo y alrededor de `K_REF`, con conteo dentro de ±10 % del conteo a `K_REF`.
- **Métrica complementaria (margen de separación).** Correlación de la gota más débil dividida por la correlación máxima del ruido. No depende del umbral y se lee directamente: un valor cercano a 1 indica que no hay umbral que funcione.
- **Limitaciones.** El barrido llega a k = 200, y en el BPW34 la meseta llega a ese tope, por lo que su ancho está acotado por el barrido y es un mínimo. En el OPT101 la caída de la meseta a k altos depende de que haya gotas débiles en el registro.

### 7. Concordancia entre canales (solo BPW34)

- **Qué mide.** Cuántas gotas detectan ambos canales y cuántas solo uno.
- **Por qué importa.** Dos canales permiten cubrir gotas que no caen por el centro de la cámara. Esta métrica cuantifica cuánto aporta el segundo canal.
- **Cómo.** Se emparejan las detecciones de los dos canales dentro de ±15 ms. Se informa el porcentaje detectado por ambos, solo por el canal 1 y solo por el canal 2, y el desfase entre canales. Las métricas 5 y 6 se calculan también para la detección combinada (OR).
- **Limitaciones.** La concordancia de 100 % indica que en este registro los dos canales ven las mismas gotas. No demuestra que el segundo canal rescate gotas que el primero perdería, porque en este registro no hubo ninguna.

### 8. Deriva de línea base

- **Qué mide.** Cuánto se desplaza el nivel de continua durante el registro.
- **Por qué importa.** Con un umbral fijo, si la línea base se desplaza durante la infusión, la detección se degrada con el tiempo.
- **Cómo.** Mediana en ventanas de 2 s sobre los registros crudos, pendiente por regresión lineal (mV/min) y variación máxima (también en unidades de σ crudo).
- **Limitaciones.** Los registros crudos duran entre 19 s y 27 s. La deriva de una infusión ocurre en minutos u horas, así que el resultado **no es concluyente** y se informa solo como dato. Además, el filtrado de los registros principales (pasaaltos de 5 Hz y 85 Hz) elimina la deriva, por lo que no se puede evaluar en ellos.

## Limitaciones generales del ensayo

1. No hubo referencia independiente de conteo (ni manual ni por video). Las métricas son indicadores de consistencia, no de exactitud.
2. Los registros no fueron simultáneos: se tomaron en secuencia, con los sensores intercambiados.
3. Las ganancias de transimpedancia son distintas entre sensores (≈ 1 MΩ contra 4,7 kΩ). Por eso solo se comparan métricas independientes de la ganancia. El LED infrarrojo se alimentó con 5 V en los dos ensayos, con la misma resistencia en serie y a la misma distancia del fotodiodo, por lo que la excitación del LED es la misma.
4. Los filtros digitales son distintos en cada sensor, y el SNR se calcula después del filtrado.
5. El registro del OPT101 tiene solo 14 gotas claras (frente a 31 del BPW34), y no se sabe si el goteo continuó después de los primeros 6,6 s.
6. El ruido se mide dentro de los registros con goteo, sin un registro sin goteo del mismo día.
7. El contraste y la deriva se calcularon con registros crudos de otras fechas, y el nivel base del OPT101 está cerca del límite del rango de salida.
