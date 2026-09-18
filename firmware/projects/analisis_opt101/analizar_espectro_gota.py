# %% [markdown]
# # Análisis espectral - prueba_opt101
#
# Compara un registro **sin goteo** contra uno **con goteo** (CSV con una muestra por línea, en
# mV, tomados con `MODE_RAW` de `prueba_opt101`) para ver en qué frecuencias aparece la gota, y
# así elegir con datos reales las frecuencias de corte del pasabanda del firmware.
#
# Orden del análisis:
# 1. Señal sin goteo y señal con goteo, en el tiempo.
# 2. FFT sin goteo y FFT con goteo.
# 3. Comparación de ambos espectros (superpuestos, por bandas de 10Hz y cociente con/sin).
#
# **Cómo correr esto en VS Code:** con la extensión de Python instalada, cada bloque separado
# por `# %%` es una "celda" - aparece un link "Run Cell" arriba de cada una, y los gráficos
# salen en la "Interactive Window". Solo requiere `numpy` y `matplotlib`.
#
# Autor: Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)

# %%
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------------------------------------------------------------------------
# Config: editar estas líneas para cada par de registros que se quiera comparar.
# ------------------------------------------------------------------------------------------
SIN_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\DataSINGOTA.csv")
CON_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\DataSINGOTA (2).csv")
SAMPLE_PERIOD_US = 700.0  # tiene que coincidir con el SAMPLE_PERIOD_US del firmware al tomar los registros
F_MAX_HZ = 200.0          # se grafica el espectro de 0 a F_MAX_HZ
ANCHO_BANDA_HZ = 10.0     # ancho de las bandas para promediar la amplitud en la comparación

COLOR_SIN = "tab:gray"
COLOR_CON = "tab:red"

# %% [markdown]
# ## Cargar los dos registros
#
# Solo se usa la primera columna del CSV (el resto son ceros fijos de canales sin usar del
# plotter). Para poder comparar amplitudes, los dos registros se recortan al mismo largo.

# %%
x_sin = np.loadtxt(SIN_CSV_PATH, delimiter=",", usecols=0)
x_con = np.loadtxt(CON_CSV_PATH, delimiter=",", usecols=0)

fs = 1_000_000.0 / SAMPLE_PERIOD_US
N = min(len(x_sin), len(x_con))
x_sin = x_sin[:N]
x_con = x_con[:N]
t = np.arange(N) / fs

print(f"Frecuencia de muestreo (fs): {fs:.1f} Hz  |  Nyquist: {fs/2:.1f} Hz")
print(f"Muestras por registro (recortadas al mismo largo): {N} ({N/fs:.2f} s)")
for nombre, x in (("Sin goteo", x_sin), ("Con goteo", x_con)):
    print(f"{nombre}: media={np.mean(x):.1f} mV, desvío={np.std(x):.2f} mV, min/max={np.min(x):.0f}/{np.max(x):.0f} mV")

# %% [markdown]
# ## 1. Señal sin goteo (en el tiempo)

# %%
y_min = min(x_sin.min(), x_con.min()) - 2
y_max = max(x_sin.max(), x_con.max()) + 2  # mismo eje vertical en las dos, para poder compararlas a ojo

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 4))
# ax.plot(t, x_sin, linewidth=0.5, color=COLOR_SIN)
# ax.set_ylim(y_min, y_max)
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Señal SIN goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 2. Señal con goteo (en el tiempo)

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 4))
# ax.plot(t, x_con, linewidth=0.5, color=COLOR_CON)
# ax.set_ylim(y_min, y_max)
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Señal CON goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## FFT de cada registro
#
# A cada registro se le resta la media (la continua, que si no domina el espectro en 0Hz) y se
# lo multiplica por una ventana de Hann (que suaviza los bordes, para que el corte de la señal
# no invente frecuencias falsas). Después se hace la FFT y se normaliza para que el eje
# vertical sea la amplitud en mV de cada componente de frecuencia.

# %%
ventana_hann = np.hanning(N)


def espectro_amplitud(x):
    x = x - np.mean(x)
    return 2 * np.abs(np.fft.rfft(x * ventana_hann)) / ventana_hann.sum()


freqs = np.fft.rfftfreq(N, d=1 / fs)
amp_sin = espectro_amplitud(x_sin)
amp_con = espectro_amplitud(x_con)

mask = freqs <= min(F_MAX_HZ, fs / 2)
amp_max = max(amp_sin[mask].max(), amp_con[mask].max()) * 1.05  # mismo eje vertical en las dos FFT

# %% [markdown]
# ## 3. FFT sin goteo

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 5))
# ax.plot(freqs[mask], amp_sin[mask], linewidth=0.8, color=COLOR_SIN)
# ax.set_ylim(0, amp_max)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Amplitud (mV)")
# ax.set_title("FFT SIN goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 4. FFT con goteo

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 5))
# ax.plot(freqs[mask], amp_con[mask], linewidth=0.8, color=COLOR_CON)
# ax.set_ylim(0, amp_max)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Amplitud (mV)")
# ax.set_title("FFT CON goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 5. Comparación: los dos espectros superpuestos

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 5))
# ax.plot(freqs[mask], amp_sin[mask], linewidth=0.8, color=COLOR_SIN, label="Sin goteo", alpha=0.9)
# ax.plot(freqs[mask], amp_con[mask], linewidth=0.8, color=COLOR_CON, label="Con goteo", alpha=0.7)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Amplitud (mV)")
# ax.set_title("FFT con goteo vs. sin goteo, superpuestas")
# ax.legend()
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 6. Comparación por bandas: amplitud promedio y cociente con/sin
#
# La FFT de un solo registro es "espinosa" (cada frecuencia tiene su propio ruido), y a ojo es
# difícil ver una diferencia sistemática. Para ver la tendencia se promedia la amplitud dentro
# de bandas de 10Hz y se comparan los dos registros banda por banda:
#
# - **Cociente con/sin cercano a 1**: las dos señales tienen lo mismo en esa banda - ahí no hay
#   nada que agregue la gota (es el ruido de fondo, presente en ambos registros).
# - **Cociente mayor a 1**: con goteo hay más amplitud que sin goteo - ese exceso es lo que
#   aporta la gota. Las bandas con cociente más alto son donde conviene dejar pasar la señal.

# %%
f_max = min(F_MAX_HZ, fs / 2)
bins = np.arange(0, f_max + ANCHO_BANDA_HZ, ANCHO_BANDA_HZ)
centros = (bins[:-1] + bins[1:]) / 2
prom_sin = np.array([amp_sin[(freqs >= bins[i]) & (freqs < bins[i + 1])].mean() for i in range(len(bins) - 1)])
prom_con = np.array([amp_con[(freqs >= bins[i]) & (freqs < bins[i + 1])].mean() for i in range(len(bins) - 1)])
cociente = prom_con / prom_sin

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
# ancho_barra = ANCHO_BANDA_HZ * 0.35
# ax1.bar(centros - ancho_barra / 2, prom_sin, width=ancho_barra, color=COLOR_SIN, label="Sin goteo")
# ax1.bar(centros + ancho_barra / 2, prom_con, width=ancho_barra, color=COLOR_CON, label="Con goteo")
# ax1.set_ylabel("Amplitud promedio (mV)")
# ax1.set_title(f"Amplitud promedio por banda de {ANCHO_BANDA_HZ:.0f}Hz")
# ax1.legend()
# ax1.grid(True, alpha=0.3)

# ax2.bar(centros, cociente, width=ANCHO_BANDA_HZ * 0.8, color=[COLOR_CON if c > 1 else COLOR_SIN for c in cociente])
# ax2.axhline(1, color="black", linewidth=1)
# ax2.set_xlabel("Frecuencia (Hz)")
# ax2.set_ylabel("Cociente con/sin")
# ax2.set_title("Cociente con goteo / sin goteo (>1 = más amplitud con goteo)")
# ax2.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

print(f"Bandas de {ANCHO_BANDA_HZ:.0f}Hz ordenadas por cociente con/sin (mayor = más exceso con goteo):")
for i in np.argsort(cociente)[::-1][:8]:
    print(f"  {bins[i]:5.0f}-{bins[i+1]:5.0f}Hz: sin={prom_sin[i]:.3f}mV, con={prom_con[i]:.3f}mV, cociente={cociente[i]:.2f}")

# %% [markdown]
# ## Respaldo: una sola grabación continua, dividida en antes/después de empezar a gotear
#
# La comparación de arriba usa dos archivos tomados en sesiones distintas, y el ruido de fondo
# (luz ambiente, vibraciones, alimentación) puede no ser el mismo en las dos. Esta parte es la
# comprobación más sólida: una **única grabación continua** que arranca sin goteo y en algún
# momento empieza a gotear. Al dividirla en el instante de la transición, las dos mitades
# comparten exactamente el mismo entorno, y la única diferencia entre ellas es la gota.
#
# El instante de transición (`SPLIT_TIME_S`) es aproximado, marcado a mano al tomar el registro.
# Se aplica el mismo método que arriba: misma cantidad de muestras, sin la media, ventana de
# Hann, FFT, amplitud promedio en bandas y cociente con/sin.

# %%
JOINT_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\DatosJuntos.csv")
SPLIT_TIME_S = 15.0  # aproximado: antes de esto sin goteo, después con goteo

x_juntos = np.loadtxt(JOINT_CSV_PATH, delimiter=",", usecols=0)
t_juntos = np.arange(len(x_juntos)) / fs
split_idx = int(SPLIT_TIME_S * fs)

N_j = min(split_idx, len(x_juntos) - split_idx)
x_sin_j = x_juntos[:split_idx][-N_j:]  # las N_j muestras justo antes de la transición
x_con_j = x_juntos[split_idx:][:N_j]   # las N_j muestras justo después

print(f"Grabación: {len(x_juntos)} muestras ({len(x_juntos)/fs:.2f} s), transición en t={SPLIT_TIME_S:.1f}s")
print(f"Cada mitad: {N_j} muestras ({N_j/fs:.2f} s)")
print(f"Sin goteo: media={np.mean(x_sin_j):.1f} mV, desvío={np.std(x_sin_j):.2f} mV")
print(f"Con goteo: media={np.mean(x_con_j):.1f} mV, desvío={np.std(x_con_j):.2f} mV")

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(16, 4))
# ax.plot(t_juntos, x_juntos, linewidth=0.4, color=COLOR_CON, alpha=0.7)
# ax.axvline(SPLIT_TIME_S, color="blue", linestyle="--", linewidth=1.5, label=f"transición (~{SPLIT_TIME_S:.0f}s)")
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Grabación continua, con el momento en que empieza el goteo marcado")
# ax.legend()
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %%
ventana_j = np.hanning(N_j)


def amplitud_j(x):
    x = x - np.mean(x)
    return 2 * np.abs(np.fft.rfft(x * ventana_j)) / ventana_j.sum()


freqs_j = np.fft.rfftfreq(N_j, d=1 / fs)
amp_sin_j = amplitud_j(x_sin_j)
amp_con_j = amplitud_j(x_con_j)
mask_j = freqs_j <= f_max

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 5))
# ax.plot(freqs_j[mask_j], amp_sin_j[mask_j], linewidth=0.8, color=COLOR_SIN, label="Sin goteo (antes de la transición)", alpha=0.9)
# ax.plot(freqs_j[mask_j], amp_con_j[mask_j], linewidth=0.8, color=COLOR_CON, label="Con goteo (después de la transición)", alpha=0.7)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Amplitud (mV)")
# ax.set_title("FFT, misma grabación: antes vs. después de empezar a gotear")
# ax.legend()
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %%
prom_sin_j = np.array([amp_sin_j[(freqs_j >= bins[i]) & (freqs_j < bins[i + 1])].mean() for i in range(len(bins) - 1)])
prom_con_j = np.array([amp_con_j[(freqs_j >= bins[i]) & (freqs_j < bins[i + 1])].mean() for i in range(len(bins) - 1)])
cociente_j = prom_con_j / prom_sin_j

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
# ax1.bar(centros - ancho_barra / 2, prom_sin_j, width=ancho_barra, color=COLOR_SIN, label="Sin goteo")
# ax1.bar(centros + ancho_barra / 2, prom_con_j, width=ancho_barra, color=COLOR_CON, label="Con goteo")
# ax1.set_ylabel("Amplitud promedio (mV)")
# ax1.set_title(f"Misma grabación - amplitud promedio por banda de {ANCHO_BANDA_HZ:.0f}Hz")
# ax1.legend()
# ax1.grid(True, alpha=0.3)

# ax2.bar(centros, cociente_j, width=ANCHO_BANDA_HZ * 0.8, color=[COLOR_CON if c > 1 else COLOR_SIN for c in cociente_j])
# ax2.axhline(1, color="black", linewidth=1)
# ax2.set_xlabel("Frecuencia (Hz)")
# ax2.set_ylabel("Cociente con/sin")
# ax2.set_title("Misma grabación - cociente con goteo / sin goteo (>1 = más amplitud con goteo)")
# ax2.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

print(f"Misma grabación - bandas de {ANCHO_BANDA_HZ:.0f}Hz ordenadas por cociente con/sin:")
for i in np.argsort(cociente_j)[::-1][:8]:
    print(f"  {bins[i]:5.0f}-{bins[i+1]:5.0f}Hz: sin={prom_sin_j[i]:.3f}mV, con={prom_con_j[i]:.3f}mV, cociente={cociente_j[i]:.2f}")

# %% [markdown]
# ### ¿Coinciden las dos comparaciones?
#
# Si las bandas con más exceso son parecidas en la comparación de archivos separados y en la de
# la misma grabación, el resultado no depende de que las sesiones fueran distintas.

# %%
top_sep = set(np.argsort(cociente)[::-1][:8])
top_mis = set(np.argsort(cociente_j)[::-1][:8])
comunes = sorted(top_sep & top_mis)
print(f"Bandas entre las 8 de mayor cociente en AMBAS comparaciones ({len(comunes)} de 8):")
for i in comunes:
    print(f"  {bins[i]:5.0f}-{bins[i+1]:5.0f}Hz: cociente archivos separados={cociente[i]:.2f}, misma grabación={cociente_j[i]:.2f}")

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 4.5))
# ax.plot(centros, cociente, marker="o", color="tab:blue", label="Archivos separados (DataSINGOTA vs DataCONGOTA)")
# ax.plot(centros, cociente_j, marker="s", color="tab:orange", label="Misma grabación (DatosJuntos)")
# ax.axhline(1, color="black", linewidth=1)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Cociente con/sin")
# ax.set_title("Cociente con/sin goteo: las dos comparaciones juntas")
# ax.legend()
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## Señales FILTRADAS con y sin goteo
#
# Registros tomados con `MODE_FILTERED` de `prueba_opt101`: la señal ya pasó por el pasabanda
# digital del firmware (pasaaltos + pasabajos Butterworth) y por un antialias en ~234Hz, así que
# está centrada en 0 y sin ruido fuera de la banda. Se repite el mismo análisis de arriba
# (señal en el tiempo, FFT de cada una, comparación) pero sobre las señales filtradas, para
# ver con menos ruido **en qué frecuencia cae la gota**.

# %%
FILT_SIN_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\FiltradaSINGOTA.csv")
FILT_CON_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\FiltradaCONGOTA.csv")
F_MAX_FILT_HZ = 300.0  # se grafica de 0 a este valor, para ver también la caída del antialias (~234Hz)

xf_sin = np.loadtxt(FILT_SIN_CSV_PATH, delimiter=",", usecols=0)
xf_con = np.loadtxt(FILT_CON_CSV_PATH, delimiter=",", usecols=0)
N_f = min(len(xf_sin), len(xf_con))  # mismo largo, para poder comparar amplitudes
xf_sin = xf_sin[:N_f]
xf_con = xf_con[:N_f]
t_f = np.arange(N_f) / fs

print(f"Muestras por registro filtrado (recortadas al mismo largo): {N_f} ({N_f/fs:.2f} s)")
for nombre, x in (("Filtrada sin goteo", xf_sin), ("Filtrada con goteo", xf_con)):
    print(f"{nombre}: media={np.mean(x):.3f} mV, desvío={np.std(x):.3f} mV, min/max={np.min(x):.2f}/{np.max(x):.2f} mV")

# %% [markdown]
# ## 7. Señal filtrada sin goteo (en el tiempo)

# %%
yf_lim = max(np.abs(xf_sin).max(), np.abs(xf_con).max()) * 1.05  # mismo eje vertical en las dos

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 4))
# ax.plot(t_f, xf_sin, linewidth=0.5, color=COLOR_SIN)
# ax.set_ylim(-yf_lim, yf_lim)
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Señal FILTRADA sin goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 8. Señal filtrada con goteo (en el tiempo)

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 4))
# ax.plot(t_f, xf_con, linewidth=0.5, color=COLOR_CON)
# ax.set_ylim(-yf_lim, yf_lim)
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Señal FILTRADA con goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## FFT de las señales filtradas
#
# Mismo procedimiento que antes: se resta la media, se aplica la ventana de Hann y se
# normaliza para que el eje vertical sea la amplitud en mV.

# %%
ventana_f = np.hanning(N_f)


def amplitud_f(x):
    x = x - np.mean(x)
    return 2 * np.abs(np.fft.rfft(x * ventana_f)) / ventana_f.sum()


freqs_f = np.fft.rfftfreq(N_f, d=1 / fs)
amp_f_sin = amplitud_f(xf_sin)
amp_f_con = amplitud_f(xf_con)

f_max_f = min(F_MAX_FILT_HZ, fs / 2)
mask_f = freqs_f <= f_max_f
amp_f_max = max(amp_f_sin[mask_f].max(), amp_f_con[mask_f].max()) * 1.05  # mismo eje vertical en las dos FFT

# %% [markdown]
# ## 9. FFT de la señal filtrada sin goteo

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 5))
# ax.plot(freqs_f[mask_f], amp_f_sin[mask_f], linewidth=0.8, color=COLOR_SIN)
# ax.set_ylim(0, amp_f_max)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Amplitud (mV)")
# ax.set_title("FFT de la señal FILTRADA sin goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 10. FFT de la señal filtrada con goteo

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 5))
# ax.plot(freqs_f[mask_f], amp_f_con[mask_f], linewidth=0.8, color=COLOR_CON)
# ax.set_ylim(0, amp_f_max)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Amplitud (mV)")
# ax.set_title("FFT de la señal FILTRADA con goteo")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 11. Comparación de las FFT filtradas, superpuestas

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 5))
# ax.plot(freqs_f[mask_f], amp_f_sin[mask_f], linewidth=0.8, color=COLOR_SIN, label="Filtrada sin goteo", alpha=0.9)
# ax.plot(freqs_f[mask_f], amp_f_con[mask_f], linewidth=0.8, color=COLOR_CON, label="Filtrada con goteo", alpha=0.7)
# ax.set_xlabel("Frecuencia (Hz)")
# ax.set_ylabel("Amplitud (mV)")
# ax.set_title("FFT filtrada con goteo vs. sin goteo, superpuestas")
# ax.legend()
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 12. Comparación por bandas de las señales filtradas: amplitud promedio y cociente con/sin
#
# Igual que la comparación de las señales crudas: el cociente con/sin cercano a 1 es ruido que
# está en ambos registros; mayor a 1 es lo que aporta la gota. Como la señal ya está filtrada, el
# ruido de fuera de la banda de interés está atenuado y el contraste debería ser más claro.

# %%
bins_f = np.arange(0, f_max_f + ANCHO_BANDA_HZ, ANCHO_BANDA_HZ)
centros_f = (bins_f[:-1] + bins_f[1:]) / 2
prom_f_sin = np.array([amp_f_sin[(freqs_f >= bins_f[i]) & (freqs_f < bins_f[i + 1])].mean() for i in range(len(bins_f) - 1)])
prom_f_con = np.array([amp_f_con[(freqs_f >= bins_f[i]) & (freqs_f < bins_f[i + 1])].mean() for i in range(len(bins_f) - 1)])
cociente_f = prom_f_con / prom_f_sin

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
# ancho_barra_f = ANCHO_BANDA_HZ * 0.35
# ax1.bar(centros_f - ancho_barra_f / 2, prom_f_sin, width=ancho_barra_f, color=COLOR_SIN, label="Filtrada sin goteo")
# ax1.bar(centros_f + ancho_barra_f / 2, prom_f_con, width=ancho_barra_f, color=COLOR_CON, label="Filtrada con goteo")
# ax1.set_ylabel("Amplitud promedio (mV)")
# ax1.set_title(f"Señales filtradas - amplitud promedio por banda de {ANCHO_BANDA_HZ:.0f}Hz")
# ax1.legend()
# ax1.grid(True, alpha=0.3)

# ax2.bar(centros_f, cociente_f, width=ANCHO_BANDA_HZ * 0.8, color=[COLOR_CON if c > 1 else COLOR_SIN for c in cociente_f])
# ax2.axhline(1, color="black", linewidth=1)
# ax2.set_xlabel("Frecuencia (Hz)")
# ax2.set_ylabel("Cociente con/sin")
# ax2.set_title("Señales filtradas - cociente con goteo / sin goteo (>1 = más amplitud con goteo)")
# ax2.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

print(f"Señales filtradas - bandas de {ANCHO_BANDA_HZ:.0f}Hz ordenadas por cociente con/sin:")
for i in np.argsort(cociente_f)[::-1][:8]:
    print(f"  {bins_f[i]:5.0f}-{bins_f[i+1]:5.0f}Hz: sin={prom_f_sin[i]:.4f}mV, con={prom_f_con[i]:.4f}mV, cociente={cociente_f[i]:.2f}")

# %% [markdown]
# ## Forma de cada gota en el tiempo
#
# Para ver cómo es realmente el pico de una gota, se detectan los eventos, se recorta un pedazo
# de la señal alrededor de cada uno y se grafica con el eje de tiempo en milisegundos (zoom).
# Se hace sobre las dos señales con goteo:
#
# - **Filtrada** (`FiltradaCONGOTA.csv`): los picos son grandes y limpios, fáciles de ubicar. Ojo:
#   el pasabanda del firmware no es "transparente" - un pulso corto pasado por un pasaaltos +
#   pasabajos sale con rebotes (un pico, seguido de un undershoot y un rebote más chico), así
#   que parte de la forma que se ve es la respuesta del filtro, no de la gota sola.
# - **Cruda** (`DataSINGOTA (2).csv`): sin ese efecto del filtro, pero con más ruido, así que se
#   ve mejor promediando varias gotas alineadas en el instante del pico.
#
# Se detecta un evento cuando la señal pasa un umbral y se junta todo lo que ocurre dentro de
# `SEPARACION_MIN_S` como una sola gota.

# %%
UMBRAL_PICO_FILT_MV = 10.0  # la señal filtrada sin goteo nunca pasa de ~2.3 mV, así que 10 mV es un evento claro
UMBRAL_DIP_CRUDA_MV = 10.0  # caída respecto de la mediana de la señal cruda (suavizada 5 muestras)
SEPARACION_MIN_S = 0.1      # eventos más cerca que esto se cuentan como una sola gota
SEMIANCHO_CONTEXTO_MS = 100.0
SEMIANCHO_ZOOM_MS = 15.0


def centros_de_eventos(puntaje, umbral):
    idx = np.where(puntaje > umbral)[0]
    if len(idx) == 0:
        return np.array([], dtype=int)
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_MIN_S * fs))[0] + 1)
    return np.array([g[np.argmax(puntaje[g])] for g in grupos])


def recortes(x, centros, semiancho_ms):
    w = int(semiancho_ms / 1000 * fs)
    validos = [c for c in centros if c - w >= 0 and c + w <= len(x)]
    return np.array([x[c - w:c + w] for c in validos]), np.arange(-w, w) / fs * 1000


def graficar_una_gota(x, centro, titulo, color):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 4.5))
    for ax, semiancho, sub in ((ax1, SEMIANCHO_CONTEXTO_MS, "contexto"), (ax2, SEMIANCHO_ZOOM_MS, "zoom")):
        seg, tt = recortes(x, [centro], semiancho)
        ax.plot(tt, seg[0], marker="o", markersize=3, linewidth=1, color=color)
        ax.axhline(0, color="black", linewidth=0.6)
        ax.axvline(0, color="gray", linestyle=":", linewidth=1)
        ax.set_xlabel("Tiempo relativo al pico (ms)")
        ax.set_ylabel("mV")
        ax.set_title(f"{titulo} - {sub} (±{semiancho:.0f}ms)")
        ax.grid(True, alpha=0.3)
    fig.tight_layout()
    plt.show()


def graficar_gotas_superpuestas(x, centros, titulo, color):
    segs, tt = recortes(x, centros, SEMIANCHO_ZOOM_MS * 2)
    fig, ax = plt.subplots(figsize=(14, 5))
    for s in segs:
        ax.plot(tt, s, linewidth=0.7, color=color, alpha=0.35)
    ax.plot(tt, segs.mean(axis=0), linewidth=2.5, color="black", label=f"Promedio de {len(segs)} gotas")
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_xlabel("Tiempo relativo al pico (ms)")
    ax.set_ylabel("mV")
    ax.set_title(titulo)
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    plt.show()


# %% [markdown]
# ### Señal filtrada con goteo: dónde está cada gota

# %%
xf_con_full = np.loadtxt(FILT_CON_CSV_PATH, delimiter=",", usecols=0)  # registro completo, sin recortar
t_full = np.arange(len(xf_con_full)) / fs
centros_filt = centros_de_eventos(np.abs(xf_con_full), UMBRAL_PICO_FILT_MV)

print(f"Gotas detectadas (pico > {UMBRAL_PICO_FILT_MV:.0f} mV): {len(centros_filt)}")
print(f"Tiempos (s): {[f'{c/fs:.2f}' for c in centros_filt]}")
print(f"Separación entre gotas (s): {[f'{d:.2f}' for d in np.diff(centros_filt) / fs]}")

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(16, 4))
# ax.plot(t_full, xf_con_full, linewidth=0.5, color="tab:green")
# ax.plot(centros_filt / fs, xf_con_full[centros_filt], "rv", markersize=8, label="gota detectada")
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Señal filtrada con goteo, con cada gota marcada")
# ax.legend()
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ### Una gota filtrada, en detalle (la de mayor amplitud)
#
# A la izquierda, ±100ms alrededor del pico (para ver que el resto de la señal está tranquilo);
# a la derecha, ±15ms (donde se ve la forma del pulso, muestra por muestra).

# %%
c_max = centros_filt[np.argmax(np.abs(xf_con_full[centros_filt]))]
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# graficar_una_gota(xf_con_full, c_max, f"Gota filtrada (t={c_max/fs:.2f}s)", "tab:green")

# %% [markdown]
# ### Todas las gotas filtradas alineadas en el pico
#
# Si todas tienen la misma forma, las curvas se apilan y el promedio (negro) es la forma típica.

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# graficar_gotas_superpuestas(xf_con_full, centros_filt, "Gotas filtradas, superpuestas y alineadas en el pico", "tab:green")

# %% [markdown]
# ### Señal cruda con goteo: una gota en detalle
#
# La señal cruda con goteo se suaviza levemente (media de 5 muestras) solo para *detectar* los
# eventos; los gráficos usan la señal cruda sin suavizar. En la cruda la gota es una **caída**
# rápida (pico negativo) respecto del nivel base.

# %%
x_con_cruda = np.loadtxt(CON_CSV_PATH, delimiter=",", usecols=0)  # registro completo
x_con_centrada = x_con_cruda - np.median(x_con_cruda)
x_con_suave = np.convolve(x_con_centrada, np.ones(5) / 5, mode="same")
centros_cruda = centros_de_eventos(-x_con_suave, UMBRAL_DIP_CRUDA_MV)

print(f"Gotas detectadas en la señal cruda (caída > {UMBRAL_DIP_CRUDA_MV:.0f} mV): {len(centros_cruda)}")
print(f"Separación entre gotas (s): media={np.mean(np.diff(centros_cruda) / fs):.2f}, "
      f"min={np.min(np.diff(centros_cruda) / fs):.2f}, max={np.max(np.diff(centros_cruda) / fs):.2f}")

c_cruda = centros_cruda[len(centros_cruda) // 2]
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# graficar_una_gota(x_con_centrada, c_cruda, f"Gota cruda (t={c_cruda/fs:.2f}s)", COLOR_CON)

# %% [markdown]
# ### Todas las gotas crudas alineadas en el pico (y su promedio)
#
# Una gota sola en la señal cruda es ruidosa. Al alinear todas en el instante del pico y promediar,
# el ruido se cancela y queda la forma típica de la gota.

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# graficar_gotas_superpuestas(x_con_centrada, centros_cruda, "Gotas crudas, superpuestas y alineadas en el pico", COLOR_CON)

# %% [markdown]
# ## Correlación cruzada con la plantilla (matched filter)
#
# Idea: si todas las gotas filtradas tienen la misma forma, se puede usar el **promedio de las 8
# gotas como plantilla** y deslizarla a lo largo de toda la señal, calculando en cada instante
# cuánto se parece el pedazo de señal a la plantilla (correlación cruzada). Donde hay una gota,
# la correlación da un pico grande; donde solo hay ruido, da valores chicos. Detectar la gota pasa
# a ser detectar un pico de la salida de la correlación, con un umbral.
#
# La plantilla se normaliza para tener energía 1, así que la salida de la correlación queda en
# las mismas unidades que la señal (mV) y **conserva la amplitud**: una gota grande da un pico
# grande. (La versión totalmente normalizada, que mide solo la *forma*, se probó y no sirve acá:
# como el ruido filtrado también oscila dentro de la banda, pedazos de puro ruido llegan a
# parecerse a la plantilla y dan falsos positivos.)
#
# El umbral se fija a partir de la señal filtrada **sin goteo**: se mide cuánto vale la correlación
# con solo ruido y se pone el umbral bastante por encima de eso.

# %%
SEMIANCHO_PLANTILLA_MS = 20.0
FACTOR_UMBRAL = 12.0  # umbral = FACTOR_UMBRAL x desvío de la salida de la correlación sobre la señal SIN goteo. El ruido llega a ~4.6 desvíos y el evento descartado a ~10; la gota más chica confirmada, a ~13.5

segs_plantilla, t_plantilla = recortes(xf_con_full, centros_filt, SEMIANCHO_PLANTILLA_MS)
plantilla = segs_plantilla.mean(axis=0)
L_plantilla = len(plantilla)
plantilla_unit = plantilla / np.linalg.norm(plantilla)

fig, ax = plt.subplots(figsize=(12, 4.5))
for s in segs_plantilla:
    ax.plot(t_plantilla, s, linewidth=0.7, color="tab:green", alpha=0.3)
ax.plot(t_plantilla, plantilla, linewidth=2.5, color="black", label=f"Plantilla = promedio de {len(segs_plantilla)} gotas")
ax.axhline(0, color="black", linewidth=0.6)
ax.set_xlabel("Tiempo relativo al pico (ms)")
ax.set_ylabel("mV")
ax.set_title(f"Plantilla de la gota filtrada ({L_plantilla} muestras, ±{SEMIANCHO_PLANTILLA_MS:.0f}ms)")
ax.legend()
ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()


def correlacion_con_plantilla(x, plantilla_normalizada):
    # salida[k] compara la plantilla con x[k : k+L]; el eje de tiempo se alinea con el centro de la plantilla
    return np.correlate(x, plantilla_normalizada, mode="valid")


def tiempos_de_correlacion(n_salida):
    return (np.arange(n_salida) + L_plantilla // 2) / fs


def graficar_correlacion(x, corr, umbral, picos, titulo):
    # Se grafica solo la parte positiva: como la plantilla oscila (pico, valle, rebote), la correlación
    # también tiene lóbulos negativos donde la señal está invertida respecto de la plantilla. Eso no
    # es una coincidencia, así que se deja afuera para que se vean solo los picos de coincidencia.
    t_x = np.arange(len(x)) / fs
    t_c = tiempos_de_correlacion(len(corr))
    coincidencia = np.maximum(corr, 0)
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(16, 7), sharex=True)
    ax1.plot(t_x, x, linewidth=0.5, color="tab:green")
    ax1.set_ylabel("mV")
    ax1.set_title(titulo)
    ax1.grid(True, alpha=0.3)
    ax2.plot(t_c, coincidencia, linewidth=0.7, color="tab:purple")
    ax2.axhline(umbral, color="red", linestyle="--", label=f"umbral = {umbral:.1f}")
    ax2.plot(t_c[picos], corr[picos], "rv", markersize=9, label=f"gota detectada ({len(picos)})")
    ax2.set_xlabel("Tiempo (s)")
    ax2.set_ylabel("Coincidencia con la plantilla")
    ax2.set_ylim(bottom=0)
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    fig.tight_layout()
    plt.show()


def graficar_zoom_correlacion(x, corr, k_pico, semiancho_ms=40.0):
    # zoom alrededor de una gota: la señal y, abajo, cómo va cambiando la correlación al deslizar la plantilla
    w = int(semiancho_ms / 1000 * fs)
    c = k_pico + L_plantilla // 2
    i0, i1 = max(0, c - w), min(len(x), c + w)
    tt = (np.arange(i0, i1) - c) / fs * 1000
    k0, k1 = max(0, i0 - L_plantilla // 2), min(len(corr), i1 - L_plantilla // 2)
    tc = (np.arange(k0, k1) + L_plantilla // 2 - c) / fs * 1000
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 6), sharex=True)
    ax1.plot(tt, x[i0:i1], marker="o", markersize=3, linewidth=1, color="tab:green")
    ax1.axhline(0, color="black", linewidth=0.6)
    ax1.set_ylabel("mV")
    ax1.set_title(f"Zoom en una gota (t={c/fs:.2f}s): señal y correlación con la plantilla")
    ax1.grid(True, alpha=0.3)
    ax2.plot(tc, corr[k0:k1], marker="o", markersize=3, linewidth=1, color="tab:purple")
    ax2.axhline(0, color="black", linewidth=0.6)
    ax2.axvline(0, color="red", linestyle=":", label="máxima coincidencia")
    ax2.set_xlabel("Tiempo relativo al pico (ms)")
    ax2.set_ylabel("Correlación")
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    fig.tight_layout()
    plt.show()


# %% [markdown]
# ### Control: señal filtrada SIN goteo (fija el umbral y cuenta los falsos positivos)

# %%
xf_sin_full = np.loadtxt(FILT_SIN_CSV_PATH, delimiter=",", usecols=0)  # registro completo, sin recortar
corr_sin = correlacion_con_plantilla(xf_sin_full, plantilla_unit)
umbral_corr = FACTOR_UMBRAL * np.std(corr_sin)
picos_sin = centros_de_eventos(corr_sin, umbral_corr)

print(f"Sin goteo: desvío de la correlación = {np.std(corr_sin):.2f}, máximo = {np.max(np.abs(corr_sin)):.2f} "
      f"({np.max(np.abs(corr_sin)) / np.std(corr_sin):.1f} desvíos)")
print(f"Umbral ({FACTOR_UMBRAL:.0f} desvíos) = {umbral_corr:.2f}  ->  falsos positivos sin goteo: {len(picos_sin)}")

graficar_correlacion(xf_sin_full, corr_sin, umbral_corr, picos_sin, "Señal filtrada SIN goteo y su correlación con la plantilla")

# %% [markdown]
# ### Señal filtrada CON goteo: picos de la correlación

# %%
corr_con = correlacion_con_plantilla(xf_con_full, plantilla_unit)
picos_con = centros_de_eventos(corr_con, umbral_corr)

print(f"Gotas detectadas por correlación: {len(picos_con)}")
print(f"Tiempos (s): {[f'{t:.2f}' for t in tiempos_de_correlacion(len(corr_con))[picos_con]]}")
print(f"Separación entre gotas (s): {[f'{d:.2f}' for d in np.diff(picos_con) / fs]}")
print(f"Valor de la correlación en cada pico: {[f'{corr_con[k]:.0f}' for k in picos_con]}  (umbral {umbral_corr:.1f})")

graficar_correlacion(xf_con_full, corr_con, umbral_corr, picos_con, "Señal filtrada CON goteo y su correlación con la plantilla")
graficar_zoom_correlacion(xf_con_full, corr_con, picos_con[len(picos_con) // 2])

# %% [markdown]
# ### Comparación con la detección por umbral de amplitud
#
# Se compara cada gota detectada por correlación con las que se marcaron antes mirando solo la
# amplitud de la señal (pico > 10 mV). Si coinciden, la correlación encuentra las mismas gotas.

# %%
t_picos_corr = tiempos_de_correlacion(len(corr_con))[picos_con]
t_picos_amp = centros_filt / fs
print(f"Por amplitud: {len(t_picos_amp)} gotas  |  Por correlación: {len(t_picos_corr)} gotas")
for ta in t_picos_amp:
    tc = t_picos_corr[np.argmin(np.abs(t_picos_corr - ta))]
    print(f"  gota en t={ta:.3f}s (amplitud)  ->  correlación en t={tc:.3f}s  (diferencia {abs(tc - ta) * 1000:.1f} ms)")

# %% [markdown]
# ### Validación sin hacer trampa (leave-one-out)
#
# La plantilla se armó con las mismas 8 gotas en las que después se buscan picos, así que
# encontrarlas es un poco circular. Para chequearlo: se saca una gota, se arma la plantilla con
# las otras 7, y se ve si la correlación igual encuentra la gota que quedó afuera. Se repite para
# cada una de las 8.

# %%
resultados_loo = []
assert len(segs_plantilla) == len(centros_filt)  # ninguna gota quedó fuera por estar en el borde
for i, c_i in enumerate(centros_filt):
    p_i = np.delete(segs_plantilla, i, axis=0).mean(axis=0)
    p_i = p_i / np.linalg.norm(p_i)
    corr_i = correlacion_con_plantilla(xf_con_full, p_i)
    picos_i = centros_de_eventos(corr_i, umbral_corr)
    t_i = tiempos_de_correlacion(len(corr_i))[picos_i]
    encontrada = np.any(np.abs(t_i - c_i / fs) < 0.01)
    resultados_loo.append(encontrada)
    print(f"  gota {i + 1} (t={c_i/fs:.2f}s) fuera de la plantilla: {'encontrada' if encontrada else 'NO encontrada'}  "
          f"({len(picos_i)} gotas detectadas en total)")

print(f"Encontradas: {sum(resultados_loo)} de {len(resultados_loo)}")

# %% [markdown]
# ## Prueba en otro registro: `Datafiltrada.csv`
#
# Este registro filtrado es independiente: la plantilla **no** se armó con estas gotas. Sirve para
# ver si la plantilla generaliza a un registro que no usó. Se usa el mismo umbral que salió de la
# señal sin goteo. Ojo: no se sabe de antemano cuántas gotas tiene, así que se puede comparar el
# resultado contra lo que se ve a ojo en la señal, pero no contra una cuenta "correcta".

# %%
FILT_EXT_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datafiltrada.csv")

xf_ext = np.loadtxt(FILT_EXT_CSV_PATH, delimiter=",", usecols=0)
corr_ext = correlacion_con_plantilla(xf_ext, plantilla_unit)
picos_ext = centros_de_eventos(corr_ext, umbral_corr)
sigma_ext = 1.4826 * np.median(np.abs(corr_ext))  # estimación robusta del ruido (las gotas casi no la afectan)
sigma_sin = 1.4826 * np.median(np.abs(corr_sin))

print(f"Registro: {len(xf_ext)} muestras ({len(xf_ext)/fs:.2f} s)")
print(f"Ruido de la correlación (estimación robusta): este registro = {sigma_ext:.2f}, sin goteo = {sigma_sin:.2f}")
print(f"Gotas detectadas (umbral {umbral_corr:.1f}): {len(picos_ext)}")
print(f"Tiempos (s): {[f'{t:.2f}' for t in tiempos_de_correlacion(len(corr_ext))[picos_ext]]}")
print(f"Valor de la correlación en cada pico: {[f'{corr_ext[k]:.0f}' for k in picos_ext]}")
print(f"Separación entre gotas (s): {[f'{d:.2f}' for d in np.diff(picos_ext) / fs]}")

graficar_correlacion(xf_ext, corr_ext, umbral_corr, picos_ext, "Datafiltrada: señal filtrada y su correlación con la plantilla")

# %% [markdown]
# ## Exportar la plantilla al firmware
#
# Imprime la plantilla (el promedio de las gotas filtradas) como arreglo de C, para pegarlo en
# `prueba_opt101/main/drop_template.h`. Hay que regenerarla si cambia `SAMPLE_PERIOD_US` o los
# cortes del pasabanda del firmware, porque la forma de la gota filtrada depende de ambos.

# %%
print(f"#define DROP_TEMPLATE_LEN {L_plantilla}\n")
print("static const float DROP_TEMPLATE[DROP_TEMPLATE_LEN] = {")
for i in range(0, L_plantilla, 6):
    print("    " + ", ".join(f"{v:.5f}f" for v in plantilla[i:i + 6]) + ",")
print("};")
print(f"\nUmbral usado en el firmware (XCORR_THRESHOLD): {umbral_corr:.1f}")
