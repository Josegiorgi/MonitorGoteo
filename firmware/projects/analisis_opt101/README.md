# Análisis OPT101

Script de Python para analizar en el dominio del tiempo y de la frecuencia los registros tomados con [`prueba_opt101`](../prueba_opt101) en `MODE_RAW` — sirve para elegir las frecuencias de corte de los filtros (y el anti-aliasing) con datos reales, en vez de ajustarlas a ciegas mirando el plotter.

No es un proyecto de firmware (no se compila con ESP-IDF); es un script de Python independiente, agrupado en el workspace junto a `prueba_opt101` para tener el código y los gráficos a mano.

## Qué hace

Compara un registro **sin goteo** contra uno **con goteo** (dos CSV de una columna, una muestra por línea, en mV — el formato que exporta el serial plotter, tomados en `MODE_RAW`) para ver en qué frecuencias aparece la gota. Grafica, en este orden:

1. La señal sin goteo en el tiempo.
2. La señal con goteo en el tiempo.
3. La FFT sin goteo.
4. La FFT con goteo.
5. Los dos espectros superpuestos.
6. La amplitud promedio por banda de 10Hz de cada uno y el cociente con/sin: donde el cociente es mayor a 1 hay más amplitud con goteo, y ese exceso es lo que aporta la gota.

También imprime estadísticas básicas de cada registro y el ranking de bandas por cociente con/sin.

## Requisitos

- Python 3 con `numpy` y `matplotlib` instalados (no hace falta `scipy`).
- La extensión de **Python** de VS Code (`ms-python.python`) para correrlo por celdas.

## Cómo usarlo (en VS Code, por celdas)

El archivo [`analizar_espectro_gota.py`](analizar_espectro_gota.py) está dividido en celdas con `# %%` (el formato que reconoce la extensión de Python). Abriéndolo en VS Code, cada celda tiene un link **"Run Cell"** arriba — al correr una celda se abre la **Interactive Window** a la derecha, con los gráficos apareciendo ahí mismo (no hace falta abrir ningún PNG aparte). También se puede correr todo de una con el botón ▷ arriba a la derecha del editor ("Run File in Interactive Window" / ícono de play).

Antes de correr, editar estas líneas al principio del archivo:

```python
SIN_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\DataSINGOTA.csv")
CON_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\DataCONGOTA.csv")
SAMPLE_PERIOD_US = 700.0
```

`SAMPLE_PERIOD_US` tiene que coincidir con el `SAMPLE_PERIOD_US` que estaba activo en `prueba_opt101.c` al tomar los registros — de eso depende el eje de frecuencias de los gráficos. Los dos registros se recortan al mismo largo para poder comparar amplitudes.

## Historia del análisis (18/09/2026)

**Primer intento (`DatosGoteo.csv`, 27s, con goteo activo, analizado solo):** el espectro salió plano entre ~5 Hz y 200 Hz, sin ninguna banda donde se separen ruido y señal. Los picos más grandes resultaron ser transitorios de una sola muestra (la firma de ruido eléctrico/de cuantización, no de un tránsito físico de gota). Conclusión de esa etapa: no hay separación en frecuencia que explotar mirando *un solo* registro — hace falta comparar contra una referencia sin goteo.

**Segundo intento (`DatosGoteo.csv` vs. `DatosSinGoteo.csv`, dos archivos separados):** restar los espectros de potencia (PSD, método de Welch) dio que "sin goteo" tenía **más** energía que "con goteo" en todo el rango — al revés de lo esperado. La causa: las dos grabaciones se tomaron en sesiones distintas, con pisos de ruido de fondo distintos (luz ambiente, vibraciones, etc.), y esa diferencia entre sesiones es más grande que cualquier cosa que aporte la gota. Conclusión: la comparación tiene que hacerse **dentro de una misma grabación continua**, no entre archivos separados.

**Tercer intento, el que funcionó (`DatosJuntos.csv`, una grabación continua que arranca sin goteo y en el segundo ~15 pasa a con goteo, marcado a mano por la usuaria):** con las dos mitades de la misma sesión, la comparación por fin dio lo esperado:
- El desvío estándar por ventana de 1s tiene un salto notorio justo en el segundo marcado como inicio del goteo.
- La energía total (0-200Hz) es ~37% mayor con goteo que sin goteo.
- Tomando la FFT (con ventana de Hann, mismo largo en las dos mitades) de cada mitad y promediando la amplitud en bandas de 10Hz, el cociente con/sin goteo es mayor a 1 en casi toda la banda 90-190Hz, con los máximos en **140-150Hz (x1.45), 100-110Hz (x1.42) y 90-100Hz (x1.32)**. Un análisis más elaborado (PSD por método de Welch, repetido en 3 resoluciones) dio las mismas bandas, pero se dejó afuera del script por ser más difícil de explicar sin aportar una conclusión distinta.

**Nota:** las versiones anteriores del script incluían, además, el análisis de una grabación continua (`DatosJuntos.csv`, dividida en dos mitades) y un espectrograma; se quitaron para dejar solo la comparación sin/con goteo de arriba.

**Conclusión final:** el contenido distintivo de la gota está en **~90-200Hz**, no en las frecuencias bajas (30-60Hz) donde se había estado poniendo el pasabajos en el firmware — eso explica por qué nunca se lograba ver el pico: se estaba filtrando justo la banda que lleva la información. El diseño de filtro que corresponde ahora es un **pasabanda ~80-200Hz** (pasaaltos ~85Hz + pasabajos ~195Hz), implementado en `prueba_opt101.c`.
