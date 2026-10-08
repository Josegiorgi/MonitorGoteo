# Esquema de la sección "Selección del sensor y umbral de detección"

Estructura y datos para escribir la sección (el texto lo escribe la autora). Los valores salen de `comparar_sensores.py` con los registros provisorios del 08/10 y se reemplazan con las pruebas nuevas. Ver [LEEME_COMPARACION_SIMPLE.md](LEEME_COMPARACION_SIMPLE.md).

## 1. Sensores comparados (hojas de datos)

| | OPT101 | BPW34 |
|---|---|---|
| Área activa | 5,2 mm² | 7,5 mm² |
| Respuesta espectral | 0,45 A/W a 650 nm (rojo) | pico en 900 nm, ≈ 0,63 A/W a 950 nm |
| Ancho de banda | 14 kHz (1 MΩ interna) | fotodiodo de 100 ns; amplificador de ≈ 498 kHz |
| Transimpedancia | 1 MΩ fija | 4,7 kΩ (diseño propio) |

El LED es infrarrojo, alimentado con 5 V en ambos ensayos, con la misma resistencia en serie y a la misma distancia. El antialias (4,7 kΩ y 100 nF, ≈ 338 Hz) es el mismo, así que el ancho de banda de los sensores no limita la detección en ninguno.

## 2. Señales obtenidas

Figura `fig1_senales_con_gotas` (señal filtrada con las gotas detectadas) y `fig2_relacion_gota_ruido` (separación entre gota y ruido).

## 3. Gotas detectadas (mismo algoritmo de correlación, mismo criterio de umbral)

| | OPT101 | BPW34 canal 1 | BPW34 canal 2 |
|---|---|---|---|
| Gotas detectadas | 14 | 31 | 31 |
| Falsos positivos | pendiente (registro sin goteo) | pendiente | pendiente |
| Gota / ruido, media y gota más débil | 38,4 y 36,4 dB | 56,6 y 56,0 dB | 46,8 y 45,7 dB |
| Margen (gota más débil / ruido máximo) | 4,0 | 47 | 63 |

Figura `fig3_conteo_vs_umbral`: el conteo del BPW34 queda en 31 gotas para cualquier umbral desde unas 5 veces el ruido hasta el límite del gráfico. El del OPT101 se mantiene en 14 solo entre 7 y 27 veces el ruido. Los dos canales del BPW34 detectaron las mismas gotas.

## 4. Umbral de detección del BPW34 (con el sensor ya elegido)

**Criterio.** El umbral tiene que quedar por encima del peor ruido de la correlación y por debajo de la gota más débil, con el mismo margen de los dos lados (punto medio geométrico).

| | Canal 1 | Canal 2 |
|---|---|---|
| Peor ruido (correlación) | 19,5 | 16,5 |
| Gota más débil (correlación) | 459 | 536 |
| **Umbral propuesto** | **≈ 95** | **≈ 94** |
| Veces sobre el ruido / bajo la gota más débil | 4,9 / 4,9 | 5,7 / 5,7 |

El valor del firmware (300) detecta las 31 gotas, pero queda a solo 1,5 a 1,8 veces de la gota más débil.

**Para cerrarlo.** Hace falta un registro sin goteo con las perturbaciones normales de uso (mover suavemente el gabinete, acercar la mano al haz, cambiar la luz). Si la correlación máxima ahí supera la mitad del umbral, se sube hasta tener 2 veces de margen sobre ese ruido, sin pasar de la mitad de la gota más débil.
