# Comparación OPT101 vs BPW34

Justificación, con datos, de por qué se elige el BPW34 como sensor de gotas del monitor de goteo.

- [`LEEME_COMPARACION_SIMPLE.md`](LEEME_COMPARACION_SIMPLE.md): cómo funciona el análisis (cinco comparaciones), resultados provisorios y cómo reemplazar los datos con las pruebas nuevas del mismo día.
- [`ESQUEMA_SELECCION_SENSOR.md`](ESQUEMA_SELECCION_SENSOR.md): estructura y datos para escribir la sección de la tesis (selección del sensor y umbral).
- `comparar_sensores.py`: el script. Se corre con `python comparar_sensores.py` y regenera `resultados_simple/` (3 figuras y 3 tablas).
- `datos/`: `opt101.csv` y `bpw34.csv` (registros provisorios del 08/10), `ensayo_actual/` (pruebas nuevas), `previos/` (BPW34 del 23/09, para el umbral) y `crudos/` (registros sin filtrar, para el respaldo).
- `respaldo_analisis_completo/`: análisis extendido con ocho métricas. Queda como respaldo; no hace falta para la tesis.

## Formato de los registros

CSV exportados del serial plotter, con 9 columnas (las que no se usan quedan en 0), tomados en `MODE_DETECTOR`.

| Archivo | Firmware | Columnas usadas | Período de muestreo |
|---|---|---|---|
| `opt101.csv` | `prueba_opt101` | 1: señal filtrada (mV, pasabanda 85-195 Hz); 2: detección (500 / 0) | 700 µs |
| `bpw34.csv` | `pruebabpw34` | 1 y 2: canales filtrados (mV, pasabanda 5-260 Hz); 3: detección (1000 / 0) | 1200 µs |

`opt101.csv` tiene 14 gotas claras, entre la muestra ~1700 y la ~9500. Después de eso no hay gotas visibles, y no se sabe si seguía cayendo agua.
