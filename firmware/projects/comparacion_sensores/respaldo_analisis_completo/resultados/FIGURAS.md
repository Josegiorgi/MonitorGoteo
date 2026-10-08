# Figuras de la comparación OPT101 vs BPW34

Se generan con `python analizar_deteccion.py` (PNG a 300 dpi y SVG en esta misma carpeta, `figuras/`). Colores fijos: OPT101 en naranja, BPW34 canal 1 en azul, canal 2 en verde y detección combinada en violeta.

## 1. Ventana representativa

Señal normalizada por el ruido de cada sensor (la escala vertical es independiente en cada panel). ▾ marca las detecciones.

![Ventana representativa](figuras/fig1_ventana_representativa.png)

## 2. Pulso promedio de la gota ± 1 DE

Alineado en la detección y normalizado por el ΔV medio de cada sensor. El OPT101 tiene más muestras por pulso por su período de muestreo (700 µs contra 1200 µs), lo que no implica mejor detección.

![Pulso promedio](figuras/fig2_pulso_promedio.png)

## 3. SNR por gota

El rombo negro marca el percentil 5.

![Boxplot de SNR](figuras/fig3_boxplot_snr.png)

## 4. Intervalos entre detecciones

La banda gris va de 0,6 a 1,6 veces la mediana.

![Intervalos](figuras/fig4_intervalos.png)

## 5. Robustez al umbral

Gotas detectadas en función de k (umbral = k · σ de la correlación). La línea punteada es k = 12.

![Robustez al umbral](figuras/fig5_robustez_umbral.png)

## 6. Concordancia entre canales del BPW34

![Concordancia entre canales](figuras/fig6_concordancia_canales.png)

## Figuras de revisión (10 s por sensor, con las detecciones marcadas)

OPT101:

![OPT101, 0 a 10 s](figuras/fig_revision_opt101_1.png)
![OPT101, 10 a 19 s](figuras/fig_revision_opt101_2.png)

BPW34:

![BPW34, 0 a 10 s](figuras/fig_revision_bpw34_1.png)
![BPW34, 10 a 13 s](figuras/fig_revision_bpw34_2.png)
