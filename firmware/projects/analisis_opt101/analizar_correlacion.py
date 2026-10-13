# %% [markdown]
# # Plantilla y umbral del detector de gotas (correlación cruzada) - OPT101
#
# Este script arma, con registros reales, las dos cosas que necesita el detector del firmware
# (`xcorr_detector`):
#
# - la **plantilla**: la forma típica de una gota, que se desliza sobre la señal;
# - el **umbral**: cuánto tiene que parecerse la señal a la plantilla para contar una gota.
#
# Registros (todos tomados con `MODE_FILTERED`: pasabanda 60-300 Hz, Butterworth orden 4, sin
# notch, `SAMPLE_PERIOD_US = 700`, OPT101 con Rf 100 kΩ, C 3,9 nF, antialias 4k7 + 100 nF y LED con 1k2):
#
# | Registro | Para qué se usa |
# |---|---|
# | `FiltradaconGota.csv` | armar la plantilla |
# | `FiltradasinGota.csv` | medir el ruido (falsos positivos) |
# | `Filtrada con gota.csv` (la toma anterior) | **validación**: no interviene en la plantilla |
#
# Orden del análisis:
# 1. Las tres señales.
# 2. Detectar las gotas y ver que hay dos familias de forma.
# 3. La plantilla (promedio de las gotas).
# 4. La correlación con la plantilla.
# 5. El umbral, y por qué ese valor.
# 6. Emulación del detector del firmware.
# 7. Exportar al firmware.
#
# **Cómo correr esto en VS Code:** cada bloque `# %%` es una celda ("Run Cell"); los gráficos salen
# en la Interactive Window. Solo requiere `numpy` y `matplotlib`.
#
# Autor: Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)

# %%
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------------------------------------------------------------------------
# Config
# ------------------------------------------------------------------------------------------
CARPETA = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos OPT101\Comparacion")
CON_GOTEO_CSV = CARPETA / "Filtrada para plantilla" / "FiltradaconGota.csv"
SIN_GOTEO_CSV = CARPETA / "Filtrada para plantilla" / "FiltradasinGota.csv"
VALIDACION_CSV = CARPETA / "Filtrada con gota.csv"

SAMPLE_PERIOD_US = 700.0     # el del firmware al tomar los registros
T_TRANSITORIO_S = 0.5        # se descarta el arranque del filtro (la continua entra como un escalón)
UMBRAL_EVENTO_MV = 60.0      # un evento es todo lo que pasa de esto en la señal filtrada (el ruido llega a ~23)
SEPARACION_EVENTO_S = 0.03   # muestras de un mismo evento: más cerca que esto cuenta como una sola gota
ANTES_MS, DESPUES_MS = 15.0, 30.0   # ventana de la plantilla alrededor del valle de la gota
ANCHO_GOTA_RAPIDA_MS = 3.0   # valle -> pico positivo: hasta esto es "rápida", más es "lenta"
PEAK_WINDOW_MS = 25          # ventana de búsqueda del máximo del detector (XCORR_PEAK_WINDOW_MS)
REFRACTORY_MS = 100          # tiempo que el detector ignora la señal tras detectar (XCORR_REFRACTORY_MS)

COLOR_RAPIDA = "tab:blue"
COLOR_LENTA = "tab:red"
COLOR_SIN = "tab:gray"
COLOR_VAL = "tab:green"

fs = 1_000_000.0 / SAMPLE_PERIOD_US
sk = int(T_TRANSITORIO_S * fs)
na, nd = int(ANTES_MS / 1000 * fs), int(DESPUES_MS / 1000 * fs)
L = na + nd

x_con = np.loadtxt(CON_GOTEO_CSV, delimiter=",", usecols=0)
x_sin = np.loadtxt(SIN_GOTEO_CSV, delimiter=",", usecols=0)
x_val = np.loadtxt(VALIDACION_CSV, delimiter=",", usecols=0)
print(f"fs = {fs:.1f} Hz | plantilla de {L} muestras (-{ANTES_MS:.0f} a +{DESPUES_MS:.0f} ms)")

# %% [markdown]
# ## 1. Las tres señales
#
# La señal ya está filtrada (pasabanda), así que oscila alrededor de 0 mV. Las gotas se ven como
# pulsos grandes sobre un ruido chico. En el registro sin goteo no hay gotas: solo ruido.

# %%
fig, axs = plt.subplots(3, 1, figsize=(15, 10))
for ax, x, titulo, color in ((axs[0], x_con, "Con goteo (para armar la plantilla)", COLOR_RAPIDA),
                              (axs[1], x_sin, "Sin goteo (ruido)", COLOR_SIN),
                              (axs[2], x_val, "Validación: toma anterior con goteo", COLOR_VAL)):
    ax.plot(np.arange(len(x)) / fs, x, linewidth=0.4, color=color)
    ax.set_title(titulo)
    ax.set_ylabel("mV")
    ax.grid(True, alpha=0.3)
axs[2].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

for nombre, x in (("Con goteo", x_con), ("Sin goteo", x_sin), ("Validación", x_val)):
    print(f"{nombre:11s}: {len(x)} muestras ({len(x)/fs:5.1f} s)  desvío = {x[sk:].std():5.2f} mV  "
          f"mín/máx = {x[sk:].min():.0f}/{x[sk:].max():.0f} mV")

# %% [markdown]
# ## 2. Detectar las gotas
#
# Se agrupan las muestras que pasan `UMBRAL_EVENTO_MV` (en valor absoluto), y de cada grupo se
# toma como **centro el valle más profundo**. Se alinea en el valle (y no en el valor absoluto
# máximo) porque en la gota filtrada el valle y el pico positivo son parecidos y, si se alinease
# en el que gane en cada gota, el promedio quedaría emborronado.
#
# Esta detección es solo para **armar la plantilla** (con un umbral de amplitud simple). El
# detector del firmware es el de correlación, que se evalúa más abajo.

# %%


def centros_de_gotas(sig, umbral=UMBRAL_EVENTO_MV):
    idx = np.where(np.abs(sig[sk:]) > umbral)[0] + sk
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_EVENTO_S * fs))[0] + 1)
    c = np.array([g[0] - 12 + np.argmin(sig[g[0] - 12:g[-1] + 13]) for g in grupos])
    return c[(c >= na) & (c + nd <= len(sig))]


def ancho_valle_pico(seg):
    """Milisegundos entre el valle y el pico positivo que le sigue (mide qué tan rápida es la gota)."""
    iv = np.argmin(seg)
    return (np.argmax(seg[iv:iv + 20])) / fs * 1000


c_con = centros_de_gotas(x_con)
c_val = centros_de_gotas(x_val, umbral=80.0)  # la toma anterior tiene perturbaciones de ~35 mV: umbral un poco más alto
segs_con = np.array([x_con[c - na:c + nd] for c in c_con])
segs_val = np.array([x_val[c - na:c + nd] for c in c_val])

anchos = np.array([ancho_valle_pico(s) for s in segs_con])
es_rapida = anchos <= ANCHO_GOTA_RAPIDA_MS
pp = segs_con.max(axis=1) - segs_con.min(axis=1)

print(f"Gotas en 'con goteo': {len(c_con)}  |  en la validación: {len(c_val)}")
print(f"Separación mínima entre gotas: {np.diff(c_con).min() / fs:.2f} s (el refractario de {REFRACTORY_MS} ms no las junta)")
print(f"\nDos familias de gotas, según el ancho valle -> pico positivo:")
print(f"  rápidas (<= {ANCHO_GOTA_RAPIDA_MS:.0f} ms): {es_rapida.sum():2d} gotas, pico a pico {pp[es_rapida].min():.0f}-{pp[es_rapida].max():.0f} mV")
print(f"  lentas  (>  {ANCHO_GOTA_RAPIDA_MS:.0f} ms): {(~es_rapida).sum():2d} gotas, pico a pico {pp[~es_rapida].min():.0f}-{pp[~es_rapida].max():.0f} mV")
print(f"  (la validación tiene {len(c_val)} gotas de ~{(segs_val.max(axis=1) - segs_val.min(axis=1)).mean():.0f} mV: son del tipo lento)")

fig, ax = plt.subplots(figsize=(16, 4.5))
ax.plot(np.arange(len(x_con)) / fs, x_con, linewidth=0.4, color="black")
ax.plot(c_con[es_rapida] / fs, x_con[c_con[es_rapida]], "v", color=COLOR_RAPIDA, markersize=8, label=f"gota rápida ({es_rapida.sum()})")
ax.plot(c_con[~es_rapida] / fs, x_con[c_con[~es_rapida]], "v", color=COLOR_LENTA, markersize=8, label=f"gota lenta ({(~es_rapida).sum()})")
ax.set_xlabel("Tiempo (s)")
ax.set_ylabel("mV")
ax.set_title("Registro con goteo: cada gota detectada, marcada en su valle")
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Las gotas alineadas
#
# Con el goteo más rápido y más fuerte la gota filtrada es más angosta y más grande (rápidas,
# azul); con el goteo lento es más ancha y chica (rojas). En cada familia las gotas se parecen mucho
# entre sí. Esto importa porque **una plantilla de una familia sola se parece poco a la otra**.

# %%
t_ms = np.arange(-na, nd) / fs * 1000
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 4.8))
for s in segs_con[es_rapida]:
    ax1.plot(t_ms, s, color=COLOR_RAPIDA, linewidth=0.7, alpha=0.4)
for s in segs_con[~es_rapida]:
    ax1.plot(t_ms, s, color=COLOR_LENTA, linewidth=0.7, alpha=0.4)
ax1.set_title("Gotas alineadas en el valle: rápidas (azul) y lentas (rojo)")
ax1.set_xlabel("Tiempo relativo al valle (ms)")
ax1.set_ylabel("mV")
ax1.grid(True, alpha=0.3)
for s in segs_val:
    ax2.plot(t_ms, s, color=COLOR_VAL, linewidth=0.7, alpha=0.5)
ax2.set_title("Registro de validación (toma anterior): gotas alineadas")
ax2.set_xlabel("Tiempo relativo al valle (ms)")
ax2.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 3. La plantilla
#
# La plantilla es el **promedio de todas las gotas alineadas en el valle**. Antes de promediar,
# cada gota se normaliza a energía 1, para que las gotas grandes (rápidas) no dominen sobre las
# chicas (lentas): así la plantilla queda como una forma intermedia que se parece razonablemente a
# las dos familias. Después se normaliza otra vez a energía 1, que es como la usa el detector.
#
# Se arma con las gotas del registro `con goteo`. La toma anterior queda aparte para validar.

# %%


def unitaria(v):
    return v / np.linalg.norm(v)


plantilla = unitaria(np.mean([unitaria(s) for s in segs_con], axis=0))
plantilla_rapida = unitaria(segs_con[es_rapida].mean(axis=0))
plantilla_lenta = unitaria(segs_con[~es_rapida].mean(axis=0))

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 4.8))
for s in segs_con:
    ax1.plot(t_ms, unitaria(s), color="tab:gray", linewidth=0.6, alpha=0.35)
ax1.plot(t_ms, plantilla, color="black", linewidth=2.8, label=f"Plantilla = promedio de {len(segs_con)} gotas")
ax1.axhline(0, color="black", linewidth=0.6)
ax1.set_title("Plantilla utilizada (gotas normalizadas a energía 1, en gris)")
ax1.set_xlabel("Tiempo relativo al valle (ms)")
ax1.set_ylabel("Amplitud normalizada")
ax1.legend()
ax1.grid(True, alpha=0.3)
ax2.plot(t_ms, plantilla_rapida, color=COLOR_RAPIDA, label="solo rápidas")
ax2.plot(t_ms, plantilla_lenta, color=COLOR_LENTA, label="solo lentas")
ax2.plot(t_ms, plantilla, color="black", linewidth=2.2, label="mezcla (la que se usa)")
ax2.axhline(0, color="black", linewidth=0.6)
ax2.set_title("Comparación con las plantillas de cada familia")
ax2.set_xlabel("Tiempo relativo al valle (ms)")
ax2.legend()
ax2.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

print(f"Parecido (coseno, 1 = idéntica) entre la plantilla rápida y la lenta: {plantilla_rapida @ plantilla_lenta:.2f}")
print(f"Parecido de la mezcla con la rápida: {plantilla @ plantilla_rapida:.2f}  |  con la lenta: {plantilla @ plantilla_lenta:.2f}")

# %% [markdown]
# ## 4. La correlación con la plantilla
#
# `corr[k]` es la plantilla (energía 1) multiplicada muestra a muestra por la señal en
# `x[k : k+L]` y sumada. Es grande cuando la señal tiene la forma de la plantilla **y** gran
# amplitud; queda en mV, así que una gota grande da un número grande. Donde hay una gota la
# correlación hace un pico, y donde solo hay ruido queda chica. Detectar la gota = detectar que
# la correlación pasó un umbral.

# %%


def correlacion(x):
    return np.correlate(x, plantilla, mode="valid")


def mascara_ruido(n, centros, mitad_s=0.1):
    """Muestras de 'solo ruido': sin el arranque del filtro y a más de mitad_s de cualquier gota."""
    m = np.ones(n, bool)
    m[:sk] = False
    for c in centros:
        m[max(0, c - int(mitad_s * fs)):c + int(mitad_s * fs)] = False
    return m


def pico_por_gota(corr, centros):
    """Máximo de la correlación en cada gota (el máximo cae cuando la ventana queda centrada en el valle)."""
    return np.array([corr[c - na - 6:c - na + 7].max() for c in centros])


corr_con, corr_sin, corr_val = correlacion(x_con), correlacion(x_sin), correlacion(x_val)
t_corr = lambda c: (np.arange(len(c)) + na) / fs  # noqa: E731 - eje de tiempo alineado con el valle

# máscara de ruido de cada registro (en el espacio de la correlación, la ventana entera tiene que ser ruido)


def ruido_de_correlacion(corr, centros, n_senal):
    m = mascara_ruido(n_senal, centros)
    solo_ruido = np.array([m[k:k + L].all() for k in range(len(corr))])
    return corr[solo_ruido]


r_sin = ruido_de_correlacion(corr_sin, [], len(x_sin))
r_con = ruido_de_correlacion(corr_con, c_con, len(x_con))
r_val = ruido_de_correlacion(corr_val, c_val, len(x_val))
picos_con, picos_val = pico_por_gota(corr_con, c_con), pico_por_gota(corr_val, c_val)

fig, axs = plt.subplots(3, 1, figsize=(15, 10), sharey=False)
for ax, x, corr, titulo, color in ((axs[0], x_con, corr_con, "Con goteo", COLOR_RAPIDA),
                                    (axs[1], x_sin, corr_sin, "Sin goteo", COLOR_SIN),
                                    (axs[2], x_val, corr_val, "Validación", COLOR_VAL)):
    ax.plot(t_corr(corr), corr, linewidth=0.6, color=color)
    ax.set_title(f"Correlación con la plantilla - {titulo}")
    ax.set_ylabel("Coincidencia (mV)")
    ax.grid(True, alpha=0.3)
axs[2].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 5. El umbral
#
# El umbral tiene que cumplir dos cosas a la vez:
#
# - estar **por encima del ruido más grande** (si no, el ruido cuenta como gota: falso positivo);
# - estar **por debajo de la gota más débil** (si no, se pierde esa gota).
#
# Se mide el **máximo del ruido** de la correlación (en el registro sin goteo y en los tramos
# sin gota de los otros dos) y el **pico de la gota más débil** (entre todas las gotas de los
# dos registros con goteo). El umbral se pone en el punto medio entre los dos, medido en veces
# (media geométrica): queda igual de lejos del ruido que de la gota más débil, en proporción.
#
# La regla de "12 desvíos del ruido" que se usó antes no alcanza acá: el ruido tiene picos
# sueltos que llegan a ~8 desvíos, y un umbral de 12 desvíos queda apenas por encima del ruido
# máximo. Se comprueba más abajo con la emulación del detector.

# %%
ruido_max = max(np.abs(r).max() for r in (r_sin, r_con, r_val))
gota_debil = min(picos_con.min(), picos_val.min())
umbral_geometrico = np.sqrt(ruido_max * gota_debil)
UMBRAL = float(np.round(umbral_geometrico / 5) * 5)  # redondeado a múltiplo de 5
sigma_sin = r_sin.std()

print(f"Ruido máximo de la correlación: sin goteo = {np.abs(r_sin).max():.1f}, entre gotas (con goteo) = {np.abs(r_con).max():.1f}, "
      f"validación = {np.abs(r_val).max():.1f}  ->  {ruido_max:.1f}")
print(f"Pico de la gota más débil: con goteo = {picos_con.min():.0f}, validación = {picos_val.min():.0f}  ->  {gota_debil:.0f}")
print(f"Punto medio (media geométrica) = {umbral_geometrico:.1f}  ->  UMBRAL = {UMBRAL:.0f}")
print(f"  {UMBRAL / ruido_max:.1f} veces el ruido máximo  |  {gota_debil / UMBRAL:.1f} veces por debajo de la gota más débil")
print(f"Para comparar, la regla de 12 desvíos del ruido sin goteo daría {12 * sigma_sin:.0f} "
      f"(solo {12 * sigma_sin / ruido_max:.1f} veces el ruido máximo)")

fig, ax = plt.subplots(figsize=(14, 3.6))
ax.set_xscale("log")
ax.axvspan(1, ruido_max, color=COLOR_SIN, alpha=0.35, label="ruido (hasta su máximo)")
ax.axvspan(gota_debil, picos_con.max() * 1.3, color=COLOR_RAPIDA, alpha=0.25, label="gotas (desde la más débil)")
ax.axvline(UMBRAL, color="red", linewidth=2.5, label=f"UMBRAL = {UMBRAL:.0f}")
ax.axvline(12 * sigma_sin, color="orange", linestyle="--", linewidth=1.5, label=f"regla de 12 desvíos = {12 * sigma_sin:.0f}")
for v in picos_con:
    ax.plot(v, 0.3, "|", color=COLOR_RAPIDA, markersize=22)
for v in picos_val:
    ax.plot(v, 0.7, "|", color=COLOR_VAL, markersize=22)
ax.set_xlim(5, picos_con.max() * 1.3)
ax.set_ylim(0, 1)
ax.set_yticks([0.3, 0.7])
ax.set_yticklabels(["gotas con goteo", "gotas validación"])
ax.set_xlabel("Valor de la correlación (mV, escala logarítmica)")
ax.set_title("El umbral queda entre el ruido y la gota más débil")
ax.legend(loc="upper left", ncol=2, fontsize=9)
ax.grid(True, alpha=0.3, which="both")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 6. Emulación del detector del firmware
#
# Se reproduce lo que hace `XCorrDetectorProcess` muestra a muestra: al pasar el umbral espera
# `PEAK_WINDOW_MS` quedándose con el máximo y confirma la gota; luego ignora la señal
# `REFRACTORY_MS`. Se cuenta cuántas gotas detecta en cada registro y se prueban varios umbrales
# para ver qué tan crítico es el valor elegido.
#
# **Nota sobre la ventana:** la plantilla es larga (45 ms), y la correlación pasa el umbral unos
# 16 ms *antes* de su máximo. Una ventana de búsqueda más corta que eso confirma la gota antes de
# llegar al máximo (igual la detecta, pero con un valor de pico menor). 25 ms deja margen.

# %%


def emular_detector(x, umbral, ventana_ms=PEAK_WINDOW_MS, refractario_ms=REFRACTORY_MS):
    corr = correlacion(x)
    ventana, refractario = int(ventana_ms / 1000 * fs), int(refractario_ms / 1000 * fs)
    detecciones, buscando, resto, refrac = [], False, 0, 0
    for k, v in enumerate(corr):
        if refrac > 0:
            refrac -= 1
            continue
        if buscando:
            resto -= 1
            if resto == 0:
                buscando = False
                detecciones.append((k + L - 1) / fs)
                refrac = refractario
            continue
        if v > umbral:
            buscando, resto = True, ventana
    return np.array(detecciones)


umbrales = [20, 30, 44, 60, 70, 95, 130, 200, 250, 300, 400]
reales = {"con": len(c_con), "sin": 0, "val": len(c_val)}
tabla = {}
print(f"{'umbral':>7s} | {'con goteo':>10s} | {'sin goteo (falsos)':>19s} | {'validación':>10s}")
for u in umbrales:
    n_con, n_sin, n_val = (len(emular_detector(x, u)) for x in (x_con, x_sin, x_val))
    tabla[u] = (n_con, n_sin, n_val)
    marca = "  <-- elegido" if u == round(UMBRAL) else ""
    print(f"{u:7.0f} | {n_con:4d}/{reales['con']:<5d} | {n_sin:19d} | {n_val:5d}/{reales['val']:<4d}{marca}")

fig, ax = plt.subplots(figsize=(11, 4.5))
ax.plot(umbrales, [tabla[u][0] / reales["con"] * 100 for u in umbrales], "o-", color=COLOR_RAPIDA, label="gotas detectadas, con goteo (%)")
ax.plot(umbrales, [tabla[u][2] / reales["val"] * 100 for u in umbrales], "s-", color=COLOR_VAL, label="gotas detectadas, validación (%)")
ax.plot(umbrales, [tabla[u][1] for u in umbrales], "^-", color="red", label="falsos positivos sin goteo (cantidad)")
ax.axvline(UMBRAL, color="red", linestyle="--", label=f"umbral elegido ({UMBRAL:.0f})")
ax.set_xlabel("Umbral")
ax.set_ylabel("%  /  cantidad")
ax.set_title("Qué tan crítico es el valor del umbral")
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ### El detector en acción, con el umbral elegido
#
# Cada triángulo es una gota detectada por el emulador del firmware, en los tres registros.

# %%
fig, axs = plt.subplots(3, 1, figsize=(15, 10))
for ax, x, titulo, color in ((axs[0], x_con, "Con goteo", COLOR_RAPIDA), (axs[1], x_sin, "Sin goteo", COLOR_SIN),
                              (axs[2], x_val, "Validación", COLOR_VAL)):
    det = emular_detector(x, UMBRAL)
    ax.plot(np.arange(len(x)) / fs, x, linewidth=0.4, color=color)
    if len(det):
        ax.plot(det, np.full(len(det), x.max() * 1.05), "rv", markersize=7)
    ax.set_title(f"{titulo}: {len(det)} gotas detectadas (umbral {UMBRAL:.0f})")
    ax.set_ylabel("mV")
    ax.grid(True, alpha=0.3)
axs[2].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 7. Exportar al firmware
#
# Pegar el arreglo en `prueba_opt101/main/drop_template.h` y el umbral en `XCORR_THRESHOLD`, y
# subir `XCORR_PEAK_WINDOW_MS` a 25 en `prueba_opt101.c`. Vale para `SAMPLE_PERIOD_US = 700`,
# el pasabanda 60-300 Hz ORDER_4 **sin notch** y el montaje descripto arriba. Si cambia alguno,
# hay que regenerarla.
#
# Ojo: la plantilla del detector está normalizada (energía 1), pero el detector la vuelve a
# normalizar, así que se puede pegar tal cual. El umbral queda en mV de la señal filtrada.

# %%
print(f"#define DROP_TEMPLATE_LEN {L}\n")
print("static const float DROP_TEMPLATE[DROP_TEMPLATE_LEN] = {")
for i in range(0, L, 6):
    print("    " + ", ".join(f"{v:.5f}f" for v in plantilla[i:i + 6]) + ",")
print("};")
print(f"\nXCORR_THRESHOLD      {UMBRAL:.1f}f")
print(f"XCORR_PEAK_WINDOW_MS {PEAK_WINDOW_MS}")
print(f"XCORR_REFRACTORY_MS  {REFRACTORY_MS}")
