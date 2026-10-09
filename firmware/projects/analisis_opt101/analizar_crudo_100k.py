# %% [markdown]
# # Análisis de datos crudos - OPT101 con Rf = 100 kΩ y C = 3,9 nF
#
# Registros **crudos** (`MODE_RAW`, mV, una muestra por línea) tomados con el OPT101 con una resistencia de
# realimentación de 100 kΩ y un capacitor de 3,9 nF entre los pines 2 y 5. El LED va alimentado
# con 3,3 V y una resistencia de 1,2 kΩ para que no sature la entrada.
#
# Tres registros:
# 1. **Base sin LED**: ruido/luz ambiente del sensor solo.
# 2. **Base con LED, sin goteo**: nivel de continua y ruido con el LED prendido.
# 3. **Con LED y con goteo**: lo que queremos detectar.
#
# Orden del análisis:
# 1. Señales en el tiempo y estadísticas.
# 2. FFT de cada registro y comparación por bandas (con goteo vs. sin goteo).
# 3. Forma de la gota (cruda) y relación señal/ruido.
#
# La correlación cruzada con plantilla se rehace más adelante, sobre la señal ya filtrada.
#
# **Cómo correr esto en VS Code:** cada bloque `# %%` es una celda ("Run Cell"). Solo requiere
# `numpy` y `matplotlib`.
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
BASE_SIN_LED_CSV = CARPETA / "Datos100kySINLED.csv"
BASE_CON_LED_CSV = CARPETA / "Datoscon100kyLED1k2.csv"
GOTEO_CSV = CARPETA / "Sinfiltrocongoteo100ky12elLED.csv"

SAMPLE_PERIOD_US = 700.0  # tiene que coincidir con el SAMPLE_PERIOD_US del firmware al tomar los registros
F_MAX_HZ = 714.0          # se grafica el espectro hasta Nyquist (los registros son crudos, sin antialias digital)
ANCHO_BANDA_HZ = 20.0

COLOR_SIN_LED = "tab:gray"
COLOR_BASE = "tab:blue"
COLOR_GOTEO = "tab:red"

# %% [markdown]
# ## Cargar los registros
#
# Solo se usa la primera columna (el resto son ceros fijos de canales sin usar del plotter).

# %%
x_sled = np.loadtxt(BASE_SIN_LED_CSV, delimiter=",", usecols=0)
x_base = np.loadtxt(BASE_CON_LED_CSV, delimiter=",", usecols=0)
x_goteo = np.loadtxt(GOTEO_CSV, delimiter=",", usecols=0)

fs = 1_000_000.0 / SAMPLE_PERIOD_US
print(f"fs = {fs:.1f} Hz  |  Nyquist = {fs/2:.1f} Hz")
for nombre, x in (("Base SIN LED", x_sled), ("Base CON LED (sin goteo)", x_base), ("CON LED y CON goteo", x_goteo)):
    print(f"{nombre:26s}: {len(x):6d} muestras ({len(x)/fs:5.2f} s)  media={x.mean():7.1f} mV  "
          f"mediana={np.median(x):7.1f}  desvío={x.std():6.2f}  min/max={x.min():.0f}/{x.max():.0f}")

# %% [markdown]
# ## 1. Señales en el tiempo
#
# Mismo eje vertical en las tres no sirve (el LED sube la continua unos 800 mV), así que cada
# una tiene su eje. Lo que importa es la escala de la gota respecto del ruido.

# %%
fig, axs = plt.subplots(3, 1, figsize=(14, 9))
for ax, x, titulo, color in ((axs[0], x_sled, "Base SIN LED", COLOR_SIN_LED),
                              (axs[1], x_base, "Base CON LED, sin goteo", COLOR_BASE),
                              (axs[2], x_goteo, "CON LED y CON goteo", COLOR_GOTEO)):
    ax.plot(np.arange(len(x)) / fs, x, linewidth=0.4, color=color)
    ax.set_title(titulo)
    ax.set_ylabel("mV")
    ax.grid(True, alpha=0.3)
axs[2].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 2. FFT de cada registro
#
# Se resta la media, ventana de Hann y se normaliza para que el eje vertical sea la amplitud en
# mV. Los registros tienen largos distintos: para comparar amplitudes se recortan al más corto.

# %%
N = min(len(x_sled), len(x_base), len(x_goteo))
ventana = np.hanning(N)
freqs = np.fft.rfftfreq(N, d=1 / fs)


def espectro_amplitud(x):
    x = x[:N] - np.mean(x[:N])
    return 2 * np.abs(np.fft.rfft(x * ventana)) / ventana.sum()


amp_sled = espectro_amplitud(x_sled)
amp_base = espectro_amplitud(x_base)
amp_goteo = espectro_amplitud(x_goteo)
mask = freqs <= min(F_MAX_HZ, fs / 2)

fig, ax = plt.subplots(figsize=(14, 5))
ax.plot(freqs[mask], amp_sled[mask], linewidth=0.7, color=COLOR_SIN_LED, label="Base sin LED")
ax.plot(freqs[mask], amp_base[mask], linewidth=0.7, color=COLOR_BASE, label="Base con LED (sin goteo)")
ax.plot(freqs[mask], amp_goteo[mask], linewidth=0.7, color=COLOR_GOTEO, alpha=0.7, label="Con LED y goteo")
ax.set_yscale("log")
ax.set_xlabel("Frecuencia (Hz)")
ax.set_ylabel("Amplitud (mV, escala log)")
ax.set_title("FFT de los tres registros")
ax.legend()
ax.grid(True, alpha=0.3, which="both")
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Picos del ruido de fondo
#
# Las dos bases muestran una oscilación periódica fuerte (±25 mV en el tiempo). Se listan los
# picos más altos de cada base: salen en 50 Hz y sobre todo en 100 Hz (red eléctrica y su
# segundo armónico, típico de la luz ambiente artificial), no es aliasing del muestreo.

# %%
for nombre, amp in (("Base sin LED", amp_sled), ("Base con LED", amp_base)):
    top = np.argsort(amp[1:])[::-1][:6] + 1
    print(f"{nombre}: " + ", ".join(f"{freqs[i]:.1f}Hz ({amp[i]:.1f}mV)" for i in sorted(top)))

# %% [markdown]
# ## 3. Comparación por bandas: con goteo vs. base con LED
#
# Cociente de la amplitud promedio por banda. Cociente ~1: ruido de fondo presente en ambos.
# Cociente >1: lo que aporta la gota. La comparación es entre sesiones distintas (el piso de
# ruido puede cambiar), así que se lee como orientativa; la base con LED es la referencia
# correcta porque tiene el mismo montaje que el registro con goteo.

# %%
bins = np.arange(0, min(F_MAX_HZ, fs / 2) + ANCHO_BANDA_HZ, ANCHO_BANDA_HZ)
centros = (bins[:-1] + bins[1:]) / 2


def prom_bandas(amp):
    return np.array([amp[(freqs >= bins[i]) & (freqs < bins[i + 1])].mean() for i in range(len(bins) - 1)])


prom_base = prom_bandas(amp_base)
prom_goteo = prom_bandas(amp_goteo)
cociente = prom_goteo / prom_base

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
ancho = ANCHO_BANDA_HZ * 0.35
ax1.bar(centros - ancho / 2, prom_base, width=ancho, color=COLOR_BASE, label="Base con LED")
ax1.bar(centros + ancho / 2, prom_goteo, width=ancho, color=COLOR_GOTEO, label="Con goteo")
ax1.set_ylabel("Amplitud promedio (mV)")
ax1.set_title(f"Amplitud promedio por banda de {ANCHO_BANDA_HZ:.0f} Hz")
ax1.legend()
ax1.grid(True, alpha=0.3)
ax2.bar(centros, cociente, width=ANCHO_BANDA_HZ * 0.8, color=[COLOR_GOTEO if c > 1 else COLOR_SIN_LED for c in cociente])
ax2.axhline(1, color="black", linewidth=1)
ax2.set_xlabel("Frecuencia (Hz)")
ax2.set_ylabel("Cociente con/sin")
ax2.set_title("Cociente con goteo / base con LED")
ax2.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

print(f"Bandas de {ANCHO_BANDA_HZ:.0f} Hz ordenadas por cociente con/sin:")
for i in np.argsort(cociente)[::-1][:8]:
    print(f"  {bins[i]:5.0f}-{bins[i+1]:5.0f} Hz: sin={prom_base[i]:.3f} mV, con={prom_goteo[i]:.3f} mV, cociente={cociente[i]:.2f}")

# %% [markdown]
# ## 4. Forma de la gota (señal cruda)
#
# Se resta la mediana (el nivel de continua con el LED) y se detecta cada evento cuando la
# señal se aparta más de `UMBRAL_EVENTO_MV` de ella. Eventos a menos de `SEPARACION_MIN_S`
# cuentan como una sola gota. En el centro se toma la muestra de mayor apartamiento.

# %%
UMBRAL_EVENTO_MV = 150.0  # el ruido de la base con LED no pasa de ~±40 mV, la gota llega a ~±500
SEPARACION_MIN_S = 0.1
SEMIANCHO_ZOOM_MS = 10.0
SEMIANCHO_CONTEXTO_MS = 100.0

x_g = x_goteo - np.median(x_goteo)  # el registro con goteo, centrado en su continua


def centros_de_eventos(puntaje, umbral):
    idx = np.where(puntaje > umbral)[0]
    if len(idx) == 0:
        return np.array([], dtype=int)
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_MIN_S * fs))[0] + 1)
    return np.array([g[np.argmax(puntaje[g])] for g in grupos])


def recortes(x, centros, semiancho_ms):
    w = int(round(semiancho_ms / 1000 * fs))
    validos = [c for c in centros if c - w >= 0 and c + w <= len(x)]
    return np.array([x[c - w:c + w] for c in validos]), np.arange(-w, w) / fs * 1000


centros_gota = centros_de_eventos(np.abs(x_g), UMBRAL_EVENTO_MV)
dt = np.diff(centros_gota) / fs
print(f"Gotas detectadas (|desvío| > {UMBRAL_EVENTO_MV:.0f} mV): {len(centros_gota)}")
print(f"Separación entre gotas (s): media={dt.mean():.3f}, min={dt.min():.3f}, max={dt.max():.3f}")
print(f"Mínimo / máximo de cada gota (mV): {[(int(x_g[c-6:c+8].min()), int(x_g[c-6:c+8].max())) for c in centros_gota]}")

# Una gota en detalle + todas superpuestas
c_ej = centros_gota[len(centros_gota) // 2]
fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 4.5))
for ax, semiancho, sub in ((ax1, SEMIANCHO_CONTEXTO_MS, "contexto"), (ax2, SEMIANCHO_ZOOM_MS * 2, "zoom")):
    seg, tt = recortes(x_g, [c_ej], semiancho)
    ax.plot(tt, seg[0], marker="o", markersize=3, linewidth=1, color=COLOR_GOTEO)
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_xlabel("Tiempo relativo al pico (ms)")
    ax.set_ylabel("mV respecto de la continua")
    ax.set_title(f"Una gota - {sub} (±{semiancho:.0f} ms)")
    ax.grid(True, alpha=0.3)
segs_all, tt_all = recortes(x_g, centros_gota, SEMIANCHO_ZOOM_MS * 2)
for s in segs_all:
    ax3.plot(tt_all, s, linewidth=0.7, color=COLOR_GOTEO, alpha=0.35)
ax3.plot(tt_all, segs_all.mean(axis=0), linewidth=2.5, color="black", label=f"Promedio de {len(segs_all)} gotas")
ax3.axhline(0, color="black", linewidth=0.6)
ax3.set_xlabel("Tiempo relativo al pico (ms)")
ax3.set_title("Todas las gotas alineadas")
ax3.legend()
ax3.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Relación señal/ruido
#
# Ruido = desvío de la base con LED (misma configuración, sin gotas). Señal = amplitud pico a
# pico de cada gota. También se mide el ruido de la base sin LED para ver cuánto aporta el LED.

# %%
pp = np.array([x_g[c - 6:c + 8].max() - x_g[c - 6:c + 8].min() for c in centros_gota])
sigma_base = x_base.std()
print(f"Ruido (desvío) base con LED: {sigma_base:.2f} mV  |  base sin LED: {x_sled.std():.2f} mV")
print(f"Pico a pico de las gotas: media={pp.mean():.0f} mV, min={pp.min():.0f}, max={pp.max():.0f}")
print(f"SNR (pico a pico / desvío de la base con LED): media={pp.mean()/sigma_base:.0f}, peor gota={pp.min()/sigma_base:.0f}")
print(f"Excursión máxima de la base con LED (max-min): {x_base.max() - x_base.min():.0f} mV  "
      f"-> la gota más chica es {pp.min() / (x_base.max() - x_base.min()):.0f} veces mayor")
