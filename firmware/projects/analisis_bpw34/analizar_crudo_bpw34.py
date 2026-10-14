# %% [markdown]
# # Análisis de datos crudos - BPW34 (dos canales), toma del 14/10/2026
#
# Registros tomados con `MODE_RAW` de `pruebabpw34` (dos fotodiodos BPW34 con transimpedancia
# MCP6004, Rf = 4,7 kΩ, antialias RC 4k7 + 100 nF, `SAMPLE_PERIOD_US = 1200`): una columna por
# canal, en mV.
#
# | Registro | Qué es |
# |---|---|
# | `ModorawCONgotas.csv` | con LED y con goteo (38 s) |
# | `ModorawSINgotaa.csv` | con LED y sin goteo (58 s) |
#
# **Ojo:** el registro sin goteo se tomó antes, con el dispositivo apoyado de otra forma (nivel
# base 1190 / 1302 mV contra ~1330 / ~990 mV con goteo). El ruido sirve de referencia, pero los
# espectros de las dos tomas no son estrictamente comparables.
#
# Orden:
# 1. Señales en el tiempo.
# 2. Espectro con y sin goteo, por canal.
# 3. Las gotas de cada canal: detección, amplitud y forma.
# 4. Relación señal/ruido.
# 5. ¿Dónde está la energía de la gota? Elección del pasabanda.
# 6. Prueba de pasabandas candidatos (con el mismo filtro que el firmware).
#
# La plantilla y el umbral de la correlación se arman más adelante, sobre la señal filtrada.
#
# Solo requiere `numpy` y `matplotlib`.
#
# Autor: Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)

# %%
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt
from numpy.lib.stride_tricks import sliding_window_view

# ------------------------------------------------------------------------------------------
# Config
# ------------------------------------------------------------------------------------------
CARPETA = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos BPW34\Comparacion")
CON_GOTEO_CSV = CARPETA / "ModorawCONgotas.csv"
SIN_GOTEO_CSV = CARPETA / "ModorawSINgotaa.csv"

SAMPLE_PERIOD_US = 1200.0   # el del firmware al tomar los registros
ANTIALIAS_HZ = 339.0        # RC analógico 4k7 + 100 nF
UMBRAL_EVENTO_MV = 100.0    # un evento es todo lo que se aparta más de esto del nivel base local
VENTANA_BASE_S = 0.5        # mediana móvil que sirve de nivel base local
SEPARACION_MIN_S = 0.15     # eventos más cerca que esto cuentan como una sola gota
SEMIANCHO_GOTA_MS = 40.0    # ventana alrededor de cada gota para el espectro y la forma
ANCHO_BANDA_HZ = 20.0
BASELINE_SAMPLES = 100      # el firmware resta el promedio de las primeras muestras antes de filtrar
T_TRANSITORIO_S = 1.0       # se descarta el arranque del filtro

COLOR_CAN = ("tab:blue", "tab:orange")
COLOR_SIN = "tab:gray"

fs = 1_000_000.0 / SAMPLE_PERIOD_US
x_con = np.loadtxt(CON_GOTEO_CSV, delimiter=",", usecols=(0, 1))
x_sin = np.loadtxt(SIN_GOTEO_CSV, delimiter=",", usecols=(0, 1))
print(f"fs = {fs:.1f} Hz  |  Nyquist = {fs/2:.1f} Hz")
for nombre, x in (("Con goteo", x_con), ("Sin goteo", x_sin)):
    for ch in range(2):
        v = x[:, ch]
        print(f"{nombre:10s} canal {ch+1}: {len(v)} muestras ({len(v)/fs:5.1f} s)  media={v.mean():7.1f} mV  desvío={v.std():6.2f}  mín/máx={v.min():.0f}/{v.max():.0f}")

# %% [markdown]
# ## 1. Señales en el tiempo
#
# En el registro con goteo se ven pulsos hacia abajo (la gota tapa la luz) y hacia arriba (la
# gota hace de lente). El goteo no es regular: hay ráfagas y pausas (el clamp no regula bien).
# También hay saltos del nivel base (por ejemplo a los ~17 s), que aparecen **a la vez en los dos
# canales**: probablemente por moverse el dispositivo al sostenerlo.

# %%
fig, axs = plt.subplots(2, 2, figsize=(16, 8))
for ch in range(2):
    axs[0, ch].plot(np.arange(len(x_con)) / fs, x_con[:, ch], linewidth=0.4, color=COLOR_CAN[ch])
    axs[0, ch].set_title(f"Con goteo - canal {ch + 1}")
    axs[1, ch].plot(np.arange(len(x_sin)) / fs, x_sin[:, ch], linewidth=0.4, color=COLOR_SIN)
    axs[1, ch].set_title(f"Sin goteo - canal {ch + 1}")
    for f_ in (0, 1):
        axs[f_, ch].set_ylabel("mV")
        axs[f_, ch].grid(True, alpha=0.3)
axs[1, 0].set_xlabel("Tiempo (s)")
axs[1, 1].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 2. Espectro con y sin goteo, por canal
#
# Sin la media, ventana de Hann, amplitud en mV. Los registros tienen largos distintos: se
# recortan al más corto. Cociente por bandas de 20 Hz (>1: hay más amplitud con goteo).

# %%
N = min(len(x_con), len(x_sin))
ventana = np.hanning(N)
freqs = np.fft.rfftfreq(N, d=1 / fs)


def espectro(v):
    v = v[:N] - np.mean(v[:N])
    return 2 * np.abs(np.fft.rfft(v * ventana)) / ventana.sum()


bins = np.arange(0, fs / 2 + ANCHO_BANDA_HZ, ANCHO_BANDA_HZ)
centros = (bins[:-1] + bins[1:]) / 2


def por_bandas(amp):
    return np.array([amp[(freqs >= bins[i]) & (freqs < bins[i + 1])].mean() for i in range(len(bins) - 1)])


fig, axs = plt.subplots(2, 2, figsize=(16, 8))
for ch in range(2):
    a_con, a_sin = espectro(x_con[:, ch]), espectro(x_sin[:, ch])
    axs[0, ch].plot(freqs, a_sin, linewidth=0.6, color=COLOR_SIN, label="Sin goteo")
    axs[0, ch].plot(freqs, a_con, linewidth=0.6, color=COLOR_CAN[ch], alpha=0.8, label="Con goteo")
    axs[0, ch].set_yscale("log")
    axs[0, ch].set_title(f"FFT - canal {ch + 1}")
    axs[0, ch].set_ylabel("Amplitud (mV, escala log)")
    axs[0, ch].axvline(ANTIALIAS_HZ, color="black", linestyle=":", linewidth=0.8)
    axs[0, ch].legend()
    axs[0, ch].grid(True, alpha=0.3, which="both")
    cociente = por_bandas(a_con) / por_bandas(a_sin)
    axs[1, ch].bar(centros, cociente, width=ANCHO_BANDA_HZ * 0.8, color=COLOR_CAN[ch])
    axs[1, ch].axhline(1, color="black", linewidth=1)
    axs[1, ch].set_yscale("log")
    axs[1, ch].set_title(f"Cociente con/sin por bandas de {ANCHO_BANDA_HZ:.0f} Hz - canal {ch + 1}")
    axs[1, ch].set_xlabel("Frecuencia (Hz)")
    axs[1, ch].grid(True, alpha=0.3, which="both")
    print(f"Canal {ch + 1}: cociente con/sin mediano por banda = {np.median(cociente):.0f}, "
          f"mínimo = {cociente.min():.0f} (en {centros[np.argmin(cociente)]:.0f} Hz)")
    top = np.argsort(por_bandas(a_sin))[::-1][:3]
    print("   picos del ruido sin goteo (bandas más altas): " + ", ".join(f"{centros[i]:.0f} Hz ({por_bandas(a_sin)[i]:.2f} mV)" for i in sorted(top)))
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 3. Las gotas de cada canal
#
# Cada canal se resta de su **nivel base local** (mediana móvil de `VENTANA_BASE_S`), que sigue
# los saltos lentos. Un evento es todo lo que se aparta más de `UMBRAL_EVENTO_MV` de ese nivel.
# De cada evento se toma como centro la muestra de mayor apartamiento.

# %%


def nivel_base(v, ventana_s=VENTANA_BASE_S):
    k = int(ventana_s * fs) | 1
    return np.median(sliding_window_view(np.pad(v, k // 2, mode="edge"), k), axis=1)


def eventos(d, umbral=UMBRAL_EVENTO_MV):
    idx = np.where(np.abs(d) > umbral)[0]
    if len(idx) == 0:
        return np.array([], dtype=int)
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_MIN_S * fs))[0] + 1)
    return np.array([g[np.argmax(np.abs(d[g]))] for g in grupos])


d_con = np.column_stack([x_con[:, ch] - nivel_base(x_con[:, ch]) for ch in range(2)])
d_sin = np.column_stack([x_sin[:, ch] - nivel_base(x_sin[:, ch]) for ch in range(2)])
centros_gota = [eventos(d_con[:, ch]) for ch in range(2)]

w = int(SEMIANCHO_GOTA_MS / 1000 * fs)
for ch in range(2):
    c = centros_gota[ch]
    print(f"Canal {ch + 1}: {len(c)} gotas (|desvío| > {UMBRAL_EVENTO_MV:.0f} mV)")
t1, t2 = centros_gota[0] / fs, centros_gota[1] / fs
coinciden = sum(np.min(np.abs(t1 - t)) < 0.01 for t in t2)
print(f"Gotas del canal 2 que también ve el canal 1 (±10 ms): {coinciden} de {len(t2)}")
print(f"Separación entre gotas: media = {np.diff(t2).mean():.2f} s, mín = {np.diff(t2).min():.2f}, máx = {np.diff(t2).max():.2f} (irregular)")
eventos_sin = [eventos(d_sin[:, ch]) for ch in range(2)]
print(f"Eventos en el registro sin goteo: canal 1 = {len(eventos_sin[0])}, canal 2 = {len(eventos_sin[1])}")

pp = [np.array([d_con[i - 8:i + 12, ch].max() - d_con[i - 8:i + 12, ch].min() for i in centros_gota[ch]]) for ch in range(2)]
for ch in range(2):
    print(f"Canal {ch + 1}: pico a pico de las gotas = {pp[ch].mean():.0f} mV (mín {pp[ch].min():.0f}, máx {pp[ch].max():.0f})")

# %% [markdown]
# ### Amplitud de cada gota y comparación entre canales

# %%
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 4.5))
for ch in range(2):
    ax1.plot(centros_gota[ch] / fs, pp[ch], "o", color=COLOR_CAN[ch], label=f"Canal {ch + 1}")
ax1.set_xlabel("Tiempo (s)")
ax1.set_ylabel("Pico a pico (mV)")
ax1.set_title("Amplitud de cada gota")
ax1.legend()
ax1.grid(True, alpha=0.3)
n = min(len(pp[0]), len(pp[1]))
ax2.plot(pp[1][:n], pp[0][:n], "o", color="black")
ax2.set_xlabel("Canal 2: pico a pico (mV)")
ax2.set_ylabel("Canal 1: pico a pico (mV)")
ax2.set_title("Cada punto = una gota (si la gota pasa más cerca de un fotodiodo, se desplaza)")
ax2.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Forma de la gota
#
# Las gotas alineadas en su valle más profundo, y el promedio. Es una caída, un rebote **por encima**
# del nivel base y otra caída, en ~10 ms: la gota desvía la luz al entrar y al salir del haz y,
# en el medio, hace de lente y la concentra sobre el fotodiodo.

# %%
t_ms = np.arange(-w, w) / fs * 1000
fig, axs = plt.subplots(1, 2, figsize=(15, 4.8))
formas = []
for ch in range(2):
    segs = []
    for i in centros_gota[ch]:
        if i - 8 < 0 or i + 12 > len(d_con):
            continue
        j = i - 8 + np.argmin(d_con[i - 8:i + 12, ch])  # alinear en el valle más profundo
        if j - w >= 0 and j + w <= len(d_con):
            segs.append(d_con[j - w:j + w, ch])
    segs = np.array(segs)
    for s in segs:
        axs[ch].plot(t_ms, s, color=COLOR_CAN[ch], linewidth=0.6, alpha=0.3)
    axs[ch].plot(t_ms, segs.mean(axis=0), color="black", linewidth=2.2, label=f"promedio de {len(segs)} gotas")
    axs[ch].axhline(0, color="black", linewidth=0.6)
    axs[ch].set_title(f"Canal {ch + 1}: gotas alineadas en el valle")
    axs[ch].set_xlabel("Tiempo relativo al valle (ms)")
    axs[ch].set_ylabel("mV respecto del nivel base")
    axs[ch].legend()
    axs[ch].grid(True, alpha=0.3)
    formas.append(segs)
fig.tight_layout()
plt.show()
print(f"Con {1000 / fs:.1f} ms entre muestras, la gota (~10 ms) queda descripta con ~{int(10 / (1000 / fs))} muestras.")

# %% [markdown]
# ## 4. Relación señal/ruido
#
# Ruido = desvío del registro **sin goteo** (restado de su nivel base local). Señal = pico a pico
# de cada gota. Es el ruido del ADC más el tono de 100 Hz del alumbrado.

# %%
for ch in range(2):
    ruido = d_sin[:, ch].std()
    print(f"Canal {ch + 1}: ruido = {ruido:.1f} mV  |  gota media = {pp[ch].mean():.0f} mV -> {pp[ch].mean() / ruido:.0f} veces "
          f"({20 * np.log10(pp[ch].mean() / ruido):.0f} dB)  |  peor gota = {pp[ch].min():.0f} mV -> {pp[ch].min() / ruido:.0f} veces "
          f"({20 * np.log10(pp[ch].min() / ruido):.0f} dB)")

# %% [markdown]
# ## 5. ¿Dónde está la energía de la gota?
#
# Espectro de energía de la gota promedio (cada gota en una ventana de ±`SEMIANCHO_GOTA_MS`, sin
# la media, con ventana de Hann), acumulado en frecuencia. Con esto se eligen los cortes del
# pasabanda: el pasaaltos puede ir por debajo de donde está el primer 5 % de la energía, y el
# pasabajos por encima de donde está el 95 %.

# %%
nfft = 2048
f_gota = np.fft.rfftfreq(nfft, 1 / fs)
fig, ax = plt.subplots(figsize=(11, 4.8))
energia = []
for ch in range(2):
    P = np.mean([np.abs(np.fft.rfft((s - s.mean()) * np.hanning(2 * w), nfft)) ** 2 for s in formas[ch]], axis=0)
    cum = np.cumsum(P) / P.sum()
    ax.plot(f_gota, cum * 100, color=COLOR_CAN[ch], label=f"Canal {ch + 1}")
    q = lambda p: f_gota[np.searchsorted(cum, p)]  # noqa: E731
    energia.append((P, cum))
    bandas = [(0, 5), (5, 20), (20, 60), (60, 130), (130, 260), (260, fs / 2)]
    print(f"Canal {ch + 1}: energía de la gota: 5% por debajo de {q(.05):.0f} Hz, 50% de {q(.5):.0f}, 95% de {q(.95):.0f}, 99% de {q(.99):.0f} Hz")
    print("   por banda (%): " + ", ".join(f"{b0:.0f}-{b1:.0f} Hz: {100 * P[(f_gota >= b0) & (f_gota < b1)].sum() / P.sum():.1f}" for b0, b1 in bandas))
for y in (5, 95):
    ax.axhline(y, color="gray", linestyle=":", linewidth=0.8)
ax.axvline(ANTIALIAS_HZ, color="black", linestyle=":", linewidth=0.8)
ax.set_xlim(0, fs / 2)
ax.set_xlabel("Frecuencia (Hz)")
ax.set_ylabel("Energía acumulada de la gota (%)")
ax.set_title("Energía de la gota acumulada en frecuencia (líneas: 5 % y 95 %)")
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 6. Prueba de pasabandas candidatos
#
# Cada candidato se aplica a los dos registros con el **mismo filtro que el firmware**: pasaaltos y
# pasabajos Butterworth en cascada de biquads (fórmulas de `esp_dsp`, Q de `iir_filter.c`),
# restando antes el promedio de las primeras `BASELINE_SAMPLES` muestras, como hace el firmware, y
# descartando el arranque. Con la señal filtrada se arma una plantilla (promedio de las gotas
# alineadas en el valle) y se mide el **margen**: el pico de la correlación de la gota más débil
# dividido el máximo de la correlación en el ruido (registro sin goteo y tramos sin gota del
# registro con goteo, que incluyen los saltos de nivel base). Más alto = más fácil de separar.
#
# Ojo: la plantilla se arma y se prueba con las mismas gotas, así que el margen es optimista; sirve
# para **comparar candidatos entre sí**, no como valor absoluto.

# %%
Q_BUTTERWORTH = {2: [1 / 1.414], 4: [1 / 0.765, 1 / 1.848]}


def biquad(tipo, f_corte, q):
    w0 = 2 * np.pi * f_corte / fs
    alfa, c = np.sin(w0) / (2 * q), np.cos(w0)
    b = np.array([(1 - c) / 2, 1 - c, (1 - c) / 2]) if tipo == "lp" else np.array([(1 + c) / 2, -(1 + c), (1 + c) / 2])
    a = np.array([1 + alfa, -2 * c, 1 - alfa])
    return b / a[0], a / a[0]


def filtrar(v, f_alto, f_bajo, orden=4):
    secciones = [biquad("hp", f_alto, q) for q in Q_BUTTERWORTH[orden]] + [biquad("lp", f_bajo, q) for q in Q_BUTTERWORTH[orden]]
    y = np.asarray(v, float) - np.mean(v[:BASELINE_SAMPLES])
    for b, a in secciones:
        out = np.empty_like(y)
        z1 = z2 = 0.0
        for n_, val in enumerate(y):
            o = b[0] * val + z1
            z1 = b[1] * val - a[1] * o + z2
            z2 = b[2] * val - a[2] * o
            out[n_] = o
        y = out
    return y


sk = int(T_TRANSITORIO_S * fs)
na, nd = int(0.015 * fs), int(0.030 * fs)
L = na + nd


def margen_de_candidato(f_alto, f_bajo, ch):
    y_con, y_sin = filtrar(x_con[:, ch], f_alto, f_bajo), filtrar(x_sin[:, ch], f_alto, f_bajo)
    cs = [c for c in centros_gota[ch] if c > sk + na and c + nd < len(y_con)]
    # centro = valle más profundo de la señal filtrada cerca del evento crudo
    cs = np.array([c - 8 + np.argmin(y_con[c - 8:c + 12]) for c in cs])
    cs = cs[(cs >= na) & (cs + nd <= len(y_con))]
    segs = np.array([y_con[c - na:c + nd] for c in cs])
    p = segs.mean(axis=0)
    p /= np.linalg.norm(p)
    corr_con, corr_sin = np.correlate(y_con, p, "valid"), np.correlate(y_sin, p, "valid")
    mascara = np.ones(len(y_con), bool)
    mascara[:sk] = False
    for c in cs:
        mascara[max(0, c - int(0.15 * fs)):c + int(0.15 * fs)] = False
    solo_ruido = np.array([mascara[k:k + L].all() for k in range(len(corr_con))])
    ruido_max = max(np.abs(corr_con[solo_ruido]).max(), np.abs(corr_sin[sk:]).max())
    picos = np.array([corr_con[c - na - 4:c - na + 5].max() for c in cs])
    return picos.min() / ruido_max, picos.min(), ruido_max


candidatos = [(0.5, 260), (2, 260), (5, 260), (10, 260), (20, 260), (40, 260), (5, 150), (5, 200), (5, 300), (20, 300)]
print(f"{'pasabanda (Hz)':>15s} | {'margen canal 1':>14s} | {'margen canal 2':>14s}   (pico de la gota más débil / ruido máximo de la correlación)")
resultados = {}
for f_alto, f_bajo in candidatos:
    m = [margen_de_candidato(f_alto, f_bajo, ch) for ch in range(2)]
    resultados[(f_alto, f_bajo)] = m
    marca = "  <-- actual del firmware" if (f_alto, f_bajo) == (5, 260) else ""
    print(f"{f_alto:6.1f} - {f_bajo:4.0f}   | {m[0][0]:14.1f} | {m[1][0]:14.1f}{marca}")

# %% [markdown]
# ### La señal filtrada con el pasabanda actual (5-260 Hz)

# %%
F_ALTO, F_BAJO = 5.0, 260.0
fig, axs = plt.subplots(2, 1, figsize=(16, 7), sharex=True)
for ch in range(2):
    y = filtrar(x_con[:, ch], F_ALTO, F_BAJO)
    axs[ch].plot(np.arange(len(y)) / fs, y, linewidth=0.4, color=COLOR_CAN[ch])
    axs[ch].set_title(f"Canal {ch + 1} filtrado ({F_ALTO:g}-{F_BAJO:g} Hz): las gotas, sin las derivas del nivel base")
    axs[ch].set_ylabel("mV")
    axs[ch].grid(True, alpha=0.3)
axs[1].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()
