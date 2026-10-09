# Análisis OPT101

Script de Python para analizar los registros crudos tomados con [`prueba_opt101`](../prueba_opt101) en `MODE_RAW`. No es un proyecto de firmware (no se compila con ESP-IDF): es un script independiente, agrupado en el workspace para tener el código y los gráficos a mano.

## Montaje de los registros

OPT101 con **Rf = 100 kΩ** y **C = 3,9 nF** entre los pines 2 y 5. LED alimentado con 3,3 V y una resistencia de 1,2 kΩ para que no sature la entrada. Antes del ADC hay un **antialias analógico RC de 4k7 y 100 nF** (fc ≈ 339 Hz); el OPT101 con 100 kΩ y 3,9 nF ya limita en ≈ 408 Hz. La señal que se analiza está entonces atenuada por encima de ~340 Hz, y la forma de la gota incluye el efecto de ambos filtros. Los análisis anteriores (otra ganancia, con el sensor saturado) se descartaron.

Tres registros, en mV, una muestra por línea (formato del serial plotter; solo se usa la primera columna):

1. Base **sin LED**.
2. Base **con LED, sin goteo**.
3. **Con LED y con goteo**.

## Qué hace

[`analizar_crudo_100k.py`](analizar_crudo_100k.py), en este orden:

1. Los tres registros en el tiempo, con estadísticas.
2. FFT de los tres superpuestos (escala log) y picos del ruido de fondo (50/100 Hz de la red).
3. Amplitud promedio por banda de 20 Hz, con goteo vs. base con LED, y cociente con/sin.
4. Forma de la gota cruda (una gota, y las 20 alineadas con su promedio) y relación señal/ruido.

La correlación cruzada con plantilla se rehace más adelante, sobre la señal ya filtrada.

## Cómo usarlo

Requiere Python 3 con `numpy` y `matplotlib` (no hace falta `scipy`). En VS Code, con la extensión de Python, cada bloque `# %%` es una celda con link **Run Cell**; los gráficos salen en la Interactive Window.

Editar al principio del archivo las rutas de los CSV y `SAMPLE_PERIOD_US`, que tiene que coincidir con el del firmware al tomar los registros (de eso depende el eje de frecuencias).
