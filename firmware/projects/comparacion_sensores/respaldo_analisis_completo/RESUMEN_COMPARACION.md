# Resumen de la comparación OPT101 vs BPW34

Estado: **con los datos actuales (08/10/2026)**. Se actualiza cuando estén las pruebas nuevas en la cámara de goteo (BPW34, OPT101, BPW34, OPT101, con muestras con y sin goteo).

Cómo se calculó cada cosa: [METRICAS_JUSTIFICACION.md](METRICAS_JUSTIFICACION.md). Para regenerar todo: `python analizar_deteccion.py`.

## Registros

- OPT101: `datos/opt101.csv`, 19,0 s a 700 µs, 14 gotas claras (en los primeros ~6,6 s). Ganancia ×1 (pin 4 puenteado al pin 5).
- BPW34: `datos/bpw34.csv`, 13,2 s a 1200 µs, 31 gotas, dos canales.
- Los dos son del 08/10, tomados en secuencia (no simultáneos), con el mismo detector de correlación y la regla de 12 desvíos.

## Valores clave

| Métrica | OPT101 | BPW34 canal 1 | BPW34 canal 2 | BPW34 combinado (OR) |
|---|---|---|---|---|
| Gotas detectadas | 14 | 31 | 31 | 31 |
| Ruido σ (MAD) | 1,04 mV (1,4 cuentas) | 1,48 mV (2,0) | 2,97 mV (3,9) | — |
| SNR media ± DE | 33,8 ± 1,1 dB | 51,5 ± 0,8 dB | 43,5 ± 0,6 dB | — |
| SNR mínimo / percentil 5 | 31,5 / 32,0 dB | 50,3 / 50,4 dB | 41,9 / 42,4 dB | — |
| CV de la amplitud (ΔV) | 0,118 | 0,091 | 0,073 | — |
| Duración del pulso (FWHM) | 7,5 ± 0,3 ms | 4,5 ± 0,9 ms | 5,4 ± 2,2 ms | — |
| Intervalos anómalos | 0 % | 0 % | 0 % | 0 % |
| Meseta de k (conteo ±10 %) | 7 a 27 (ancho 20) | 3 a 200 (≥ 197) | 3 a 200 (≥ 197) | 4 a 200 (≥ 196) |
| Gota más débil / ruido máximo (correlación) | 4,0 | 47 | 63 | — |

Otros datos:

- **Conteo exacto estable:** OPT101, 14 gotas para k de 7 a 27. BPW34, 31 gotas para k de 5 a 200 (con k = 3 y 4 aparecen 3 y 1 detecciones extra en el canal 1).
- **Umbrales del firmware** frente a la regla común (12 desvíos): OPT101 13,7 contra 34,7. BPW34 300 contra 24,6 y 30,7. Con el ruido de este registro, el umbral del firmware del OPT101 equivale a k ≈ 4,7, por debajo de su meseta (k ≥ 7), y por eso había falsos positivos (21 detecciones del firmware en el registro, contra 14 gotas).
- **Concordancia entre canales (BPW34):** 31 de 31 gotas detectadas por ambos canales, ninguna solo por uno. Desfase medio de 3,2 ms y máximo de 4,8 ms.
- **Contraste (registros crudos, otras fechas):** OPT101 0,82 % ± 0,10 (nivel base ≈ 2499 mV); BPW34 54,6 % ± 3,2 (canal 1) y 53,2 % ± 3,7 (canal 2). Ver advertencias abajo.
- **Deriva:** no concluyente (registros de 19 a 27 s).

## Tabla de selección

Orden de prioridad pedido: consistencia (5), robustez al umbral (6), SNR mínimo y percentil 5 (2), y después el resto.

| Prioridad | Métrica | OPT101 | BPW34 | Mejor |
|---|---|---|---|---|
| 1 | Consistencia de intervalos (5) | 0 % anómalos | 0 % anómalos (canales y OR) | **Empate** |
| 2 | Robustez al umbral (6) | meseta de k de 7 a 27; margen 4,0 | meseta de k de 3 (o 5) a 200; margen 47 y 63 | **BPW34** |
| 3 | SNR mínimo / percentil 5 (2) | 31,5 / 32,0 dB | 50,3 / 50,4 dB (c1) y 41,9 / 42,4 dB (c2) | **BPW34** |
| 4 | Repetibilidad de la amplitud (4) | CV 0,118 | CV 0,091 (c1) y 0,073 (c2) | **BPW34** |
| 5 | Ruido en cuentas del ADC (1) | 1,4 cuentas | 2,0 y 3,9 cuentas | **OPT101** |
| 6 | Concordancia entre canales (7) | no aplica (un canal) | 100 % de gotas en ambos canales | solo BPW34 |
| 7 | Contraste (3) | 0,82 % | 54,6 % y 53,2 % | descriptivo (ver advertencias) |
| 8 | Deriva (8) | no concluyente | no concluyente | — |
| — | Duración del pulso (4) | 7,5 ms | 4,5 y 5,4 ms | descriptivo |

## Conclusión

Con los datos actuales, el BPW34 detecta mejor. En consistencia de intervalos hay empate. En robustez al umbral, el BPW34 mantiene el conteo en un rango muchísimo más amplio (el margen entre la gota más débil y el ruido máximo es de 47 a 63 veces contra 4 en el OPT101) y, en SNR, su gota más débil queda entre 10 y 19 dB por encima de la del OPT101. En repetibilidad de la amplitud también es mejor. El OPT101 solo resulta mejor en el ruido expresado en cuentas del ADC (1,4 contra 2,0 y 3,9), que por sí solo no indica una mejor detección: su gota queda a menos distancia del ruido.

Esto confirma la elección preliminar del BPW34. La diferencia no depende del umbral elegido: queda a la vista en el ancho de la meseta y en el margen entre señal y ruido.

## Características de los sensores y cómo se relacionan con los resultados

Datos de las hojas de datos (OPT101: Texas Instruments SBBS002; BPW34: Vishay 81521) y del circuito armado:

| Característica | OPT101 | BPW34 | Relación con la detección |
|---|---|---|---|
| Área activa | 5,2 mm² (2,29 × 2,29 mm) | 7,5 mm² | El BPW34 recibe ≈ 44 % más luz con la misma iluminación (+3,2 dB de fotocorriente) y cubre una zona mayor del haz |
| Respuesta a la longitud de onda | 0,45 A/W a 650 nm (rojo). Especificado en el visible y decae hacia el infrarrojo | Pico en 900 nm, rango útil de 600 a 1050 nm. ≈ 0,63 A/W a 950 nm (47 µA con 1 mW/cm² sobre 7,5 mm²) | El LED es infrarrojo (λ exacta sin determinar; los comunes son de 850 o 940 nm). Cae en la zona de mejor respuesta del BPW34 y fuera de la banda especificada del OPT101 |
| Ancho de banda | 14 kHz con la resistencia interna de 1 MΩ | Fotodiodo: tiempos de subida y bajada de 100 ns. Polo del amplificador: ≈ 498 kHz (4,7 kΩ con 68 pF) | Ninguno limita la detección: la gota llega hasta ~200-260 Hz y el antialias de 338 Hz es el mismo en los dos |
| Transimpedancia y rango de salida | 1 MΩ fija. Salida máxima `VS − 1,3 V` (≈ 2,0 V a 3,3 V) | 4,7 kΩ, ajustable en el diseño | El nivel base del OPT101 quedó en ≈ 2,5 V (en el límite del rango), mientras que en el BPW34 quedó en ≈ 1,2 V |

**Cómo se relacionan con los resultados**

1. **Área.** Explica como mucho unos 3 dB de los 10 a 19 dB que separan los SNR mínimos de los dos sensores. Por sí sola no alcanza. Su aporte principal es de diseño: un fotodiodo mayor intercepta una zona más ancha del haz, lo que da más tolerancia a que la gota no pase siempre por el mismo lugar. Esto se apoya en la hoja de datos, pero no está medido en estos registros (haría falta un barrido de posición).
2. **Longitud de onda.** Con el mismo LED, el BPW34 convierte la luz en fotocorriente más eficientemente que el OPT101, que trabaja fuera de la banda donde está especificado. El factor exacto depende de la longitud de onda del LED, que no se conoce. Para cuantificarlo hay que leer la curva de responsividad espectral de la hoja de datos del OPT101 en la longitud de onda del LED.
3. **Ancho de banda.** No es un factor que explique la diferencia: los dos superan por mucho la banda de la gota, y el antialias define el ancho de banda efectivo en ambos. Se menciona para descartarlo como causa y no como ventaja de un sensor.
4. **Rango de salida y ganancia.** La ganancia fija de 1 MΩ del OPT101 llevó el nivel base cerca del tope de su salida, y eso deja poco margen para la señal y limita la ganancia. En el BPW34, la ganancia se elige en el diseño y el nivel base queda cómodo dentro del rango del ADC.
5. **Efecto combinado.** Más fotocorriente (área y respuesta espectral) con una ganancia ajustada al rango se traduce en una gota más grande respecto del ruido del conversor. Esto es coherente con el SNR de 42 a 50 dB del BPW34 contra 32 dB del OPT101. Esta relación es una interpretación a partir de las hojas de datos y no se midió por separado cada factor.

## Limitaciones

1. **No hubo referencia independiente de conteo.** Las métricas indican consistencia y no exactitud.
2. **Los registros no fueron simultáneos:** se tomaron en secuencia, con los sensores intercambiados.
3. **Los sensores difieren en ganancia (1 MΩ contra 4,7 kΩ).** Por eso no se compara la amplitud en mV, y se usan SNR, margen y robustez. El LED infrarrojo se alimentó con 5 V en los dos ensayos, con la misma resistencia en serie y a la misma distancia del fotodiodo, por lo que la excitación del LED es la misma.
4. **Filtros digitales distintos** (85-195 Hz y 5-260 Hz): el SNR se mide después del filtrado.
5. **Muestra chica del OPT101:** 14 gotas claras (frente a 31). Después de los primeros ~6,6 s no hay detecciones durante ~12 s, y no se sabe si seguía cayendo agua. Se analizó el registro completo y recortado, y los resultados son iguales salvo la meseta (7 a 27 contra 6 a 30).
6. **El ruido se mide dentro del registro con goteo,** con MAD para evitar la cola de los pulsos. No hay un registro sin goteo del mismo día.
7. **Contraste y deriva** salen de registros crudos de otras fechas (18/09 y 23/09). El nivel base del OPT101 (≈ 2499 mV) está en el límite del rango de salida y su contraste puede estar subestimado. Los registros duran menos de 30 s, por lo que la deriva no es concluyente.
8. **Plantillas:** cada sensor usa su propia plantilla, generada con registros de sesiones anteriores. No provienen de los registros que se comparan.

## Archivos generados

Figuras (PNG 300 dpi y SVG) en `respaldo_analisis_completo/resultados/figuras/`. Para verlas todas juntas, con epígrafes, abrí [resultados/FIGURAS.md](resultados/FIGURAS.md) con la vista previa de Markdown (`Ctrl+Shift+V`):

1. `fig1_ventana_representativa`: señal normalizada con detecciones.
2. `fig2_pulso_promedio`: pulso promedio ± 1 DE.
3. `fig3_boxplot_snr`: SNR por sensor y canal.
4. `fig4_intervalos`: intervalos contra tiempo, con bandas de 0,6× y 1,6×.
5. `fig5_robustez_umbral`: gotas detectadas contra k.
6. `fig6_concordancia_canales`: concordancia entre canales del BPW34.
7. `fig_revision_opt101_1/2` y `fig_revision_bpw34_1/2`: ventanas de 10 s con detecciones marcadas, para verificar el conteo.

Tablas (CSV y Markdown) en `resultados/tablas/`: métricas de detección, registros crudos, concordancia y barrido de umbral, más un CSV por gota de cada sensor.
