# Comparación OPT101 vs BPW34: versión simple

Responde una sola pregunta: **¿cuál de los dos sensores detecta mejor las gotas?** Se resuelve con el script [`comparar_sensores.py`](comparar_sensores.py), que usa el mismo algoritmo de detección del firmware (correlación con la plantilla de la gota y un umbral).

## Qué hace el script

Para cada registro y cada canal repite estos cinco pasos:

1. Lee la señal filtrada (mV), tal como la entrega el firmware.
2. La correlaciona con la plantilla de la gota del sensor.
3. Fija el umbral: **12 veces el desvío del ruido de la correlación**.
4. Detecta las gotas: cuando la correlación cruza el umbral, espera 15 ms, se queda con el máximo y confirma la gota. Después ignora 100 ms (período refractario). Es la misma lógica que el firmware.
5. Mide las cinco cosas de la tabla de abajo.

## Qué se compara (solo cinco cosas)

| # | Qué se mide | Cómo | Para qué sirve |
|---|---|---|---|
| 1 | **Gotas detectadas** | Cantidad de gotas que marca el detector (contra las esperadas, si hay conteo de referencia) | Es el resultado principal |
| 2 | **Falsos positivos** | Detecciones en el registro sin goteo | Si detecta gotas donde no hay |
| 3 | **Relación gota / ruido (dB)** | Amplitud pico a pico de la gota dividida por el ruido; se informa la media y la de la gota más débil | Qué tan separada está la gota del ruido (no depende de la ganancia) |
| 4 | **Margen** | Correlación de la gota más débil dividida por la correlación máxima del ruido | Si cabe un umbral entre el ruido y la gota (un valor cerca de 1 indica que no) |
| 5 | **Conteo contra umbral** (gráfico) | Gotas detectadas al mover el umbral | Si el resultado depende o no del umbral elegido |

Los valores en mV (amplitud y ruido) se informan solo como datos. No se usan para comparar, porque las ganancias de los dos circuitos son distintas.

## Resultados provisorios (registros del 08/10, sin registro sin goteo)

| Canal | Gotas detectadas | Amplitud media / mínima | Ruido | Gota / ruido media / mínima | Margen |
|---|---|---|---|---|---|
| OPT101 | 14 | 87 / 69 mV | 1,04 mV | 38,4 / 36,4 dB | 4,0 |
| BPW34 canal 1 | 31 | 1007 / 931 mV | 1,48 mV | 56,6 / 56,0 dB | 47 |
| BPW34 canal 2 | 31 | 650 / 572 mV | 2,97 mV | 46,8 / 45,7 dB | 63 |

Con el mismo detector, el BPW34 detecta las gotas con una separación del ruido entre 8 y 20 dB mayor, y un margen para elegir el umbral mucho más amplio. Estos valores se reemplazan con las pruebas nuevas.

## Cómo reemplazar los datos con las pruebas nuevas (mismo día, ambos sensores)

1. Hacer las pruebas en la cámara de goteo, un sensor por vez, en este orden: **BPW34, OPT101, BPW34, OPT101** (ronda 1 y ronda 2). Por sensor y ronda, dos registros en `MODE_DETECTOR`: uno con goteo (~90 s) y uno sin goteo (~30 s, con el regulador cerrado).
2. Copiar los CSV a `datos/ensayo_actual/` con este nombre exacto:

```
bpw34_r1_goteo.csv      bpw34_r1_sin_goteo.csv
opt101_r1_goteo.csv     opt101_r1_sin_goteo.csv
bpw34_r2_goteo.csv      bpw34_r2_sin_goteo.csv
opt101_r2_goteo.csv     opt101_r2_sin_goteo.csv
```

3. Opcional, para tener el conteo de referencia: crear `datos/ensayo_actual/referencia.csv` con las columnas `archivo,gotas_esperadas` (por ejemplo `bpw34_r1_goteo.csv,102`), calculadas pesando el agua recolectada y dividiendo por el peso de una gota.
4. Correr:

```bash
python comparar_sensores.py
```

Con registros en `datos/ensayo_actual/`, el script los usa y deja de usar los provisorios. Con el registro sin goteo, el ruido se mide ahí (más limpio) y aparecen los falsos positivos. Las figuras y tablas se regeneran solas en `resultados_simple/`.

## Umbral del BPW34 (sensor elegido)

El script calcula, para cada canal, el **peor ruido** y la **gota más débil** de todos los registros del BPW34 (los nuevos y los del 23/09 en `datos/previos/`). El umbral propuesto es el punto medio entre ambos (raíz del producto), que deja el mismo margen de los dos lados.

| Canal | Peor ruido | Gota más débil | Umbral propuesto | Veces sobre el ruido | Veces bajo la gota más débil |
|---|---|---|---|---|---|
| BPW34 canal 1 | 19,5 | 459 | ≈ 95 | 4,9 | 4,9 |
| BPW34 canal 2 | 16,5 | 536 | ≈ 94 | 5,7 | 5,7 |

El umbral que quedó en el firmware (300) detecta todas las gotas pero queda a solo 1,5 a 1,8 veces de la gota más débil. Con las pruebas nuevas conviene agregar un registro sin goteo con las perturbaciones normales de uso (mover suavemente el gabinete, acercar la mano al haz, cambiar la luz). Si ahí la correlación máxima supera la mitad del umbral, se sube hasta tener un margen de 2 veces sobre ese ruido.

## Archivos

- `comparar_sensores.py`: el script.
- `datos/`: registros provisorios del 08/10 (`opt101.csv`, `bpw34.csv`), `ensayo_actual/` (pruebas nuevas), `previos/` (BPW34 del 23/09, solo para el umbral) y `crudos/` (sin filtrar, para el respaldo).
- `resultados_simple/figuras/` (PNG 300 dpi y SVG): `fig1_senales_con_gotas`, `fig2_relacion_gota_ruido` y `fig3_conteo_vs_umbral`.
- `resultados_simple/tablas/` (CSV y Markdown): `tabla_comparacion`, `tabla_umbral` y `tabla_umbral_detalle`.
- `respaldo_analisis_completo/`: el análisis extendido (ocho métricas). Queda como respaldo por si el jurado pregunta; no hace falta para la tesis.
