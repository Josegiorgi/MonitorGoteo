# %% [markdown]
# # Simulación del pasabanda propuesto sobre los registros crudos
#
# Aplica a los tres registros (base sin LED, base con LED, con LED y goteo) el mismo filtro que
# usa el firmware: pasaaltos + pasabajos Butterworth en cascada de biquads, con los
# coeficientes de `esp_dsp` (`dsps_biquad_gen_hpf_f32` / `dsps_biquad_gen_lpf_f32`, fórmulas
# del cookbook de Robert Bristow-Johnson) y los Q de `iir_filter.c`. Así lo que se ve acá es lo
# que va a salir del firmware, y se pueden probar distintos cortes antes de tocar el código.
#
# Orden:
# 1. Respuesta en frecuencia del filtro (con el antialias analógico y el OPT101 como referencia).
# 2. Señal con goteo: cruda vs. filtrada, y una gota en detalle.
# 3. FFT de los tres registros, antes y después de filtrar.
#
# Solo requiere `numpy` y `matplotlib`.
#
# Autor: Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)

# %%
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------------------------------------------------------------------------
# Config: probar acá distintos cortes y órdenes
# ------------------------------------------------------------------------------------------
CARPETA = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos OPT101\Comparacion")
BASE_SIN_LED_CSV = CARPETA / "Datos100kySINLED.csv"
BASE_CON_LED_CSV = CARPETA / "Datoscon100kyLED1k2.csv"
GOTEO_CSV = CARPETA / "Sinfiltrocongoteo100ky12elLED.csv"

SAMPLE_PERIOD_US = 700.0
CUTOFF_HIGHPASS_HZ = 60.0
CUTOFF_LOWPASS_HZ = 300.0
FILTER_ORDER = 4          # 2, 4, 6 u 8, para ambos filtros (igual que FILTER_ORDER del firmware)
ANTIALIAS_HZ = 339.0      # RC analógico 4k7 + 100 nF
OPT101_HZ = 408.0         # OPT101 con 100 kΩ y 3,9 nF

COLOR_SIN_LED = "tab:gray"
COLOR_BASE = "tab:blue"
COLOR_GOTEO = "tab:red"

fs = 1_000_000.0 / SAMPLE_PERIOD_US

# %% [markdown]
# ## El filtro
#
# Cada filtro es una cascada de biquads (`ORDER/2` secciones) con los Q de Butterworth de
# `iir_filter.c`. `filtrar` procesa la señal muestra a muestra como el firmware (forma directa II,
# estado inicial en cero).

# %%
Q_BUTTERWORTH = {2: [1 / 1.414], 4: [1 / 0.765, 1 / 1.848],
                 6: [1 / 0.518, 1 / 1.414, 1 / 1.932], 8: [1 / 0.390, 1 / 1.111, 1 / 1.663, 1 / 1.962]}


def biquad(tipo, f_corte, q):
    # misma fórmula que dsps_biquad_gen_lpf_f32 / dsps_biquad_gen_hpf_f32 (f = f_corte / fs)
    w0 = 2 * np.pi * f_corte / fs
    alfa = np.sin(w0) / (2 * q)
    c = np.cos(w0)
    if tipo == "lp":
        b = np.array([(1 - c) / 2, 1 - c, (1 - c) / 2])
    else:
        b = np.array([(1 + c) / 2, -(1 + c), (1 + c) / 2])
    a = np.array([1 + alfa, -2 * c, 1 - alfa])
    return b / a[0], a / a[0]


def seccion(tipo, f_corte, orden):
    return [biquad(tipo, f_corte, q) for q in Q_BUTTERWORTH[orden]]


SECCIONES = seccion("hp", CUTOFF_HIGHPASS_HZ, FILTER_ORDER) + seccion("lp", CUTOFF_LOWPASS_HZ, FILTER_ORDER)


def filtrar(x, secciones=SECCIONES):
    y = np.asarray(x, dtype=float)
    for b, a in secciones:
        out = np.empty_like(y)
        z1 = z2 = 0.0
        for n, v in enumerate(y):
            o = b[0] * v + z1
            z1 = b[1] * v - a[1] * o + z2
            z2 = b[2] * v - a[2] * o
            out[n] = o
        y = out
    return y


def respuesta(freqs, secciones=SECCIONES):
    z = np.exp(-1j * 2 * np.pi * freqs / fs)
    h = np.ones_like(z)
    for b, a in secciones:
        h *= (b[0] + b[1] * z + b[2] * z ** 2) / (1 + a[1] * z + a[2] * z ** 2)
    return h


# %% [markdown]
# ## 1. Respuesta en frecuencia
#
# Línea punteada: los cortes del filtro. Líneas finas: el antialias analógico y el polo del
# OPT101, que también atenúan por encima de ~340 Hz. A la derecha, la respuesta total
# (filtro digital x los dos analógicos, modelados como RC de primer orden).

# %%
f = np.linspace(1, fs / 2, 4000)
h = respuesta(f)
h_analog = 1 / np.sqrt(1 + (f / ANTIALIAS_HZ) ** 2) / np.sqrt(1 + (f / OPT101_HZ) ** 2)

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 4.8))
ax1.semilogx(f, 20 * np.log10(np.abs(h) + 1e-12), color="black", label="Filtro digital")
ax1.semilogx(f, 20 * np.log10(h_analog), color="tab:green", linewidth=1, label="Antialias + OPT101 (analógico)")
ax1.semilogx(f, 20 * np.log10(np.abs(h) * h_analog + 1e-12), color="tab:orange", linewidth=1.5, label="Total")
for fc in (CUTOFF_HIGHPASS_HZ, CUTOFF_LOWPASS_HZ):
    ax1.axvline(fc, color="gray", linestyle=":")
for fr, txt in ((50, "50 Hz"), (100, "100 Hz")):
    ax1.axvline(fr, color="tab:purple", linewidth=0.6, alpha=0.6)
    ax1.text(fr, -78, txt, color="tab:purple", fontsize=8, ha="center")
ax1.set_ylim(-80, 5)
ax1.set_xlabel("Frecuencia (Hz)")
ax1.set_ylabel("Ganancia (dB)")
ax1.set_title(f"Pasabanda {CUTOFF_HIGHPASS_HZ:.0f}-{CUTOFF_LOWPASS_HZ:.0f} Hz, Butterworth orden {FILTER_ORDER}")
ax1.legend(loc="lower left")
ax1.grid(True, alpha=0.3, which="both")

ax2.plot(f, np.abs(h), color="black", label="Filtro digital")
ax2.plot(f, np.abs(h) * h_analog, color="tab:orange", label="Total")
ax2.set_xlabel("Frecuencia (Hz)")
ax2.set_ylabel("Ganancia (lineal)")
ax2.set_title("Misma respuesta, en escala lineal")
ax2.legend()
ax2.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

for fr in (50, 100):
    print(f"Ganancia a {fr} Hz: {abs(respuesta(np.array([float(fr)]))[0]):.2f}")

# %% [markdown]
# ## Aplicar el filtro a los tres registros

# %%
x_sled = np.loadtxt(BASE_SIN_LED_CSV, delimiter=",", usecols=0)
x_base = np.loadtxt(BASE_CON_LED_CSV, delimiter=",", usecols=0)
x_goteo = np.loadtxt(GOTEO_CSV, delimiter=",", usecols=0)

y_sled, y_base, y_goteo = filtrar(x_sled), filtrar(x_base), filtrar(x_goteo)

# el filtro arranca con estado cero, y la continua (~900 mV) entra como un escalón: se descartan
# los primeros 0,5 s para estadísticas y espectros
T_TRANSITORIO_S = 0.5
sk = int(T_TRANSITORIO_S * fs)
print(f"Se descartan los primeros {T_TRANSITORIO_S} s ({sk} muestras) por el transitorio del filtro\n")
for nombre, y in (("Base sin LED", y_sled), ("Base con LED", y_base), ("Con LED y goteo", y_goteo)):
    print(f"{nombre:16s}: desvío filtrado = {y[sk:].std():6.2f} mV   min/max = {y[sk:].min():.0f}/{y[sk:].max():.0f}")

# %% [markdown]
# ## 2. Señal con goteo: cruda vs. filtrada

# %%
t = np.arange(len(x_goteo)) / fs
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 7), sharex=True)
ax1.plot(t, x_goteo, linewidth=0.4, color=COLOR_GOTEO)
ax1.set_ylabel("mV")
ax1.set_title("Con goteo: cruda")
ax1.grid(True, alpha=0.3)
ax2.plot(t, y_goteo, linewidth=0.4, color="tab:green")
ax2.set_xlabel("Tiempo (s)")
ax2.set_ylabel("mV")
ax2.set_title(f"Con goteo: filtrada ({CUTOFF_HIGHPASS_HZ:.0f}-{CUTOFF_LOWPASS_HZ:.0f} Hz)")
ax2.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Una gota, antes y después de filtrar
#
# Se ve cuánto se deforma el pulso: el pasaaltos le agrega un rebote (undershoot) y el pasabajos
# la suaviza. Esa forma filtrada es la que va a tener que recordar la plantilla.

# %%
dev = np.abs(x_goteo - np.median(x_goteo))
idx = np.where(dev[sk:] > 150)[0] + sk
grupos = np.split(idx, np.where(np.diff(idx) > int(0.1 * fs))[0] + 1)
centros = np.array([g[np.argmax(dev[g])] for g in grupos])
print(f"Gotas detectadas por amplitud (después del transitorio): {len(centros)}")

c = centros[len(centros) // 2]
w = int(0.06 * fs)
tt = (np.arange(-w, w)) / fs * 1000
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 4.5))
ax1.plot(tt, x_goteo[c - w:c + w] - np.median(x_goteo), marker="o", markersize=3, color=COLOR_GOTEO)
ax1.set_title("Gota cruda (sin la continua)")
ax2.plot(tt, y_goteo[c - w:c + w], marker="o", markersize=3, color="tab:green")
ax2.set_title("La misma gota, filtrada")
for ax in (ax1, ax2):
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_xlabel("Tiempo relativo al pico (ms)")
    ax.set_ylabel("mV")
    ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

pp_f = np.array([y_goteo[k - 15:k + 25].max() - y_goteo[k - 15:k + 25].min() for k in centros])
print(f"Pico a pico de las gotas filtradas: media={pp_f.mean():.0f} mV, min={pp_f.min():.0f}, max={pp_f.max():.0f}")
print(f"Ruido filtrado (desvío, base con LED): {y_base[sk:].std():.2f} mV  "
      f"-> peor gota / ruido = {pp_f.min() / y_base[sk:].std():.0f}")

# %% [markdown]
# ## 3. FFT antes y después de filtrar
#
# Misma normalización que el resto del análisis (sin la media, ventana de Hann, amplitud en mV).

# %%
N = min(len(x_sled), len(x_base), len(x_goteo)) - sk
ventana = np.hanning(N)
freqs = np.fft.rfftfreq(N, d=1 / fs)


def espectro(x):
    x = x[sk:sk + N]
    x = x - x.mean()
    return 2 * np.abs(np.fft.rfft(x * ventana)) / ventana.sum()


fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 5), sharey=True)
for ax, datos, titulo in ((ax1, (x_sled, x_base, x_goteo), "Cruda"), (ax2, (y_sled, y_base, y_goteo), "Filtrada")):
    for x, color, et in zip(datos, (COLOR_SIN_LED, COLOR_BASE, COLOR_GOTEO), ("Base sin LED", "Base con LED", "Con goteo")):
        ax.plot(freqs, espectro(x), linewidth=0.7, color=color, label=et, alpha=0.85)
    ax.set_yscale("log")
    ax.set_xlabel("Frecuencia (Hz)")
    ax.set_title(f"FFT {titulo.lower()}")
    ax.legend()
    ax.grid(True, alpha=0.3, which="both")
    for fc in (CUTOFF_HIGHPASS_HZ, CUTOFF_LOWPASS_HZ):
        ax.axvline(fc, color="black", linestyle=":", linewidth=0.8)
ax1.set_ylabel("Amplitud (mV, escala log)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 4. Con un notch en 100 Hz
#
# El zumbido de la red (50 Hz y sobre todo 100 Hz, ~22 mV) pasa por el pasabanda casi sin
# atenuar. Se agrega en cascada un filtro notch (biquad del cookbook de RBJ) en `F_NOTCH_HZ` y
# se compara el resultado. `Q_NOTCH` define el ancho: ancho de banda ≈ F_NOTCH_HZ / Q_NOTCH. Un
# Q alto saca solo la línea de 100 Hz pero deja pasar el zumbido si la red se corre un poco
# (en los registros hay picos en 99,9 / 100,0 / 100,2 Hz); un Q bajo es más tolerante pero
# deforma más la gota.

# %%
F_NOTCH_HZ = 100.0
Q_NOTCH = 8.0


def biquad_notch(f0, q):
    w0 = 2 * np.pi * f0 / fs
    alfa = np.sin(w0) / (2 * q)
    c = np.cos(w0)
    b = np.array([1.0, -2 * c, 1.0])
    a = np.array([1 + alfa, -2 * c, 1 - alfa])
    return b / a[0], a / a[0]


SECCIONES_NOTCH = SECCIONES + [biquad_notch(F_NOTCH_HZ, Q_NOTCH)]
yn_sled, yn_base, yn_goteo = (filtrar(x, SECCIONES_NOTCH) for x in (x_sled, x_base, x_goteo))

hn = respuesta(f, SECCIONES_NOTCH)
fig, ax = plt.subplots(figsize=(10, 4.5))
ax.semilogx(f, 20 * np.log10(np.abs(h) + 1e-12), color="black", label="Pasabanda solo")
ax.semilogx(f, 20 * np.log10(np.abs(hn) + 1e-12), color="tab:red", label=f"Pasabanda + notch {F_NOTCH_HZ:.0f} Hz (Q={Q_NOTCH:g})")
for fr in (50, 100):
    ax.axvline(fr, color="tab:purple", linewidth=0.6, alpha=0.6)
ax.set_ylim(-60, 5)
ax.set_xlabel("Frecuencia (Hz)")
ax.set_ylabel("Ganancia (dB)")
ax.set_title("Respuesta en frecuencia con y sin notch")
ax.legend(loc="lower right")
ax.grid(True, alpha=0.3, which="both")
fig.tight_layout()
plt.show()

print("Desvío (mV) de las bases, sin el transitorio:")
for nombre, y1, y2 in (("Base sin LED", y_sled, yn_sled), ("Base con LED", y_base, yn_base)):
    print(f"  {nombre:14s}: sin notch {y1[sk:].std():5.2f}  ->  con notch {y2[sk:].std():5.2f}")

# %% [markdown]
# ### Efecto sobre la señal y sobre la gota

# %%
fig = plt.figure(figsize=(16, 9))
gs = fig.add_gridspec(2, 2)
ax1 = fig.add_subplot(gs[0, :])
tb = np.arange(len(y_base)) / fs
ax1.plot(tb, y_base, linewidth=0.5, color="tab:green", label=f"Sin notch (desvío {y_base[sk:].std():.1f} mV)")
ax1.plot(tb, yn_base, linewidth=0.5, color="tab:red", label=f"Con notch (desvío {yn_base[sk:].std():.1f} mV)")
ax1.set_xlim(0.5, 3)
ax1.set_ylim(-60, 60)
ax1.set_xlabel("Tiempo (s)")
ax1.set_ylabel("mV")
ax1.set_title("Base con LED (sin goteo), filtrada")
ax1.legend(loc="upper right")
ax1.grid(True, alpha=0.3)

ax2 = fig.add_subplot(gs[1, 0])
ax2.plot(tt, y_goteo[c - w:c + w], marker="o", markersize=3, color="tab:green", label="Sin notch")
ax2.plot(tt, yn_goteo[c - w:c + w], marker="o", markersize=3, color="tab:red", label="Con notch")
ax2.axhline(0, color="black", linewidth=0.6)
ax2.set_xlabel("Tiempo relativo al pico (ms)")
ax2.set_ylabel("mV")
ax2.set_title("Una gota, filtrada")
ax2.legend()
ax2.grid(True, alpha=0.3)

ax3 = fig.add_subplot(gs[1, 1])
ax3.plot(freqs, espectro(y_base), linewidth=0.7, color="tab:green", label="Sin notch")
ax3.plot(freqs, espectro(yn_base), linewidth=0.7, color="tab:red", label="Con notch")
ax3.set_yscale("log")
ax3.set_xlim(0, 400)
ax3.set_ylim(1e-3, 10)
ax3.set_xlabel("Frecuencia (Hz)")
ax3.set_ylabel("Amplitud (mV, escala log)")
ax3.set_title("FFT de la base con LED, filtrada")
ax3.legend()
ax3.grid(True, alpha=0.3, which="both")
fig.tight_layout()
plt.show()

pp_n = np.array([yn_goteo[k - 15:k + 25].max() - yn_goteo[k - 15:k + 25].min() for k in centros])
print(f"Gotas: pico a pico media sin notch = {pp_f.mean():.0f} mV, con notch = {pp_n.mean():.0f} mV")
print(f"Peor gota / ruido (desvío base con LED): sin notch = {pp_f.min() / y_base[sk:].std():.0f}, "
      f"con notch = {pp_n.min() / yn_base[sk:].std():.0f}")
