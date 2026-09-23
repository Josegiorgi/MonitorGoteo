# %% [markdown]
# # Análisis espectral - pruebabpw34
#
# Compara un registro **sin goteo** contra uno **con goteo** tomados con `pruebabpw34` (CSV con
# una línea por muestra: `canal1,canal2,0,0,...`, en mV) para ver cómo es la gota con el sensor
# nuevo (dos BPW34 + transimpedancia MCP6004) y elegir con datos reales los cortes del
# pasaaltos (bajas frecuencias) y del pasabajos (altas frecuencias) del firmware.
#
# Orden del análisis (el mismo que `analisis_opt101`):
# 1. Señal sin goteo y señal con goteo, en el tiempo.
# 2. FFT sin goteo, FFT con goteo y las dos superpuestas.
# 3. Comparación por bandas de 10 Hz (amplitud promedio y cociente con/sin).
# 4. Promedio de las gotas (forma típica de la gota).
# 5. Relación señal/ruido y prueba del pasabanda (simulado en Python).
# 6. Señales FILTRADAS por el firmware: el mismo análisis y la plantilla de la gota para la
#    correlación cruzada (como en `analisis_opt101`).
#
# Los gráficos de las secciones 1 a 9 (señales crudas) están desactivados (comentados) para no
# llenar la Interactive Window; las cuentas y los print siguen corriendo.
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
SIN_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos BPW34\SeñalsinnGoteo.csv")
CON_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos BPW34\SeñalconGoteo.csv")
SAMPLE_PERIOD_US = 1200.0  # tiene que coincidir con el SAMPLE_PERIOD_US del firmware al tomar los registros
ANCHO_BANDA_HZ = 10.0      # ancho de las bandas para promediar la amplitud en la comparación

CANALES = ("BPW34 n°1 (GPIO_2)", "BPW34 n°2 (GPIO_3)")
COLOR_SIN = "tab:gray"
COLOR_CON = "tab:red"
COLORES_CANAL = ("tab:blue", "tab:orange")

# %% [markdown]
# ## Cargar los dos registros
#
# Se usan las dos primeras columnas del CSV (una por fotodiodo); el resto son ceros fijos de
# canales sin usar del plotter. Para poder comparar amplitudes, los dos registros se recortan al
# mismo largo.

# %%
x_sin = np.loadtxt(SIN_CSV_PATH, delimiter=",", usecols=(0, 1))
x_con = np.loadtxt(CON_CSV_PATH, delimiter=",", usecols=(0, 1))

fs = 1_000_000.0 / SAMPLE_PERIOD_US
N = min(len(x_sin), len(x_con))
x_sin = x_sin[:N]
x_con = x_con[:N]
t = np.arange(N) / fs

print(f"Frecuencia de muestreo (fs): {fs:.1f} Hz  |  Nyquist: {fs/2:.1f} Hz")
print(f"Muestras por registro (recortadas al mismo largo): {N} ({N/fs:.2f} s)")
for nombre, x in (("Sin goteo", x_sin), ("Con goteo", x_con)):
    print(f"{nombre}:")
    for c in range(2):
        print(f"   {CANALES[c]}: media={np.mean(x[:, c]):.1f} mV, desvío={np.std(x[:, c]):.2f} mV, "
              f"min/max={np.min(x[:, c]):.0f}/{np.max(x[:, c]):.0f} mV")

# %% [markdown]
# ## 1. Señal sin goteo (en el tiempo)

# %%
y_min = x_sin.min() if x_sin.min() < x_con.min() else x_con.min()
y_max = x_sin.max() if x_sin.max() > x_con.max() else x_con.max()
y_min, y_max = y_min - 20, y_max + 20  # mismo eje vertical en las dos, para poder compararlas a ojo

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 4))
# for c in range(2):
#     ax.plot(t, x_sin[:, c], linewidth=0.5, color=COLORES_CANAL[c], label=CANALES[c])
# ax.set_ylim(y_min, y_max)
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Señal SIN goteo")
# ax.legend(loc="lower right")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 2. Señal con goteo (en el tiempo)

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, ax = plt.subplots(figsize=(14, 4))
# for c in range(2):
#     ax.plot(t, x_con[:, c], linewidth=0.5, color=COLORES_CANAL[c], label=CANALES[c])
# ax.set_ylim(y_min, y_max)
# ax.set_xlabel("Tiempo (s)")
# ax.set_ylabel("mV")
# ax.set_title("Señal CON goteo")
# ax.legend(loc="lower right")
# ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## FFT de cada registro
#
# A cada registro se le resta la media (la continua, que si no domina el espectro en 0 Hz) y se
# lo multiplica por una ventana de Hann (que suaviza los bordes, para que el corte de la señal
# no invente frecuencias falsas). Después se hace la FFT y se normaliza para que el eje
# vertical sea la amplitud en mV de cada componente de frecuencia. Se grafica de 0 Hz a Nyquist.

# %%
ventana_hann = np.hanning(N)


def espectro_amplitud(x):
    x = x - np.mean(x)
    return 2 * np.abs(np.fft.rfft(x * ventana_hann)) / ventana_hann.sum()


freqs = np.fft.rfftfreq(N, d=1 / fs)
amp_sin = np.array([espectro_amplitud(x_sin[:, c]) for c in range(2)])
amp_con = np.array([espectro_amplitud(x_con[:, c]) for c in range(2)])
amp_max = max(amp_sin.max(), amp_con.max()) * 1.05  # mismo eje vertical en todas las FFT


def graficar_fft(freqs_, amp, titulo, y_max):
    fig, axs = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
    for c, ax in enumerate(axs):
        ax.plot(freqs_, amp[c], linewidth=0.8, color=COLORES_CANAL[c])
        ax.set_ylim(0, y_max)
        ax.set_ylabel("Amplitud (mV)")
        ax.set_title(f"{titulo} - {CANALES[c]}")
        ax.grid(True, alpha=0.3)
    axs[-1].set_xlabel("Frecuencia (Hz)")
    fig.tight_layout()
    plt.show()


# %% [markdown]
# ## 3. FFT sin goteo

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# graficar_fft(freqs, amp_sin, "FFT SIN goteo", amp_max)

print("Componentes más grandes sin goteo:")
for c in range(2):
    top = np.argsort(amp_sin[c][freqs > 1])[::-1][:3] + np.count_nonzero(freqs <= 1)
    print(f"   {CANALES[c]}: " + ", ".join(f"{freqs[i]:.1f} Hz ({amp_sin[c][i]:.2f} mV)" for i in top))

# %% [markdown]
# ## 4. FFT con goteo

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# graficar_fft(freqs, amp_con, "FFT CON goteo", amp_max)

# %% [markdown]
# ## 5. Comparación: los dos espectros superpuestos

# %%
# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, axs = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
# for c, ax in enumerate(axs):
#     ax.plot(freqs, amp_sin[c], linewidth=0.8, color=COLOR_SIN, label="Sin goteo", alpha=0.9)
#     ax.plot(freqs, amp_con[c], linewidth=0.8, color=COLOR_CON, label="Con goteo", alpha=0.7)
#     ax.set_ylabel("Amplitud (mV)")
#     ax.set_title(f"FFT con goteo vs. sin goteo, superpuestas - {CANALES[c]}")
#     ax.legend()
#     ax.grid(True, alpha=0.3)
# axs[-1].set_xlabel("Frecuencia (Hz)")
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## 6. Comparación por bandas: amplitud promedio y cociente con/sin
#
# La FFT de un solo registro es "espinosa" (cada frecuencia tiene su propio ruido), y a ojo es
# difícil ver una diferencia sistemática. Para ver la tendencia se promedia la amplitud dentro
# de bandas de 10 Hz y se comparan los dos registros banda por banda:
#
# - **Cociente con/sin cercano a 1**: las dos señales tienen lo mismo en esa banda - ahí no hay
#   nada que agregue la gota (es el ruido de fondo, presente en ambos registros).
# - **Cociente mayor a 1**: con goteo hay más amplitud que sin goteo - ese exceso es lo que
#   aporta la gota. Las bandas con cociente más alto son donde conviene dejar pasar la señal.

# %%
bins = np.arange(0, fs / 2, ANCHO_BANDA_HZ)
bins = np.append(bins, fs / 2)
centros = (bins[:-1] + bins[1:]) / 2


def promedio_por_banda(amp, freqs_=None):
    freqs_ = freqs if freqs_ is None else freqs_
    return np.array([amp[(freqs_ >= bins[i]) & (freqs_ < bins[i + 1])].mean() for i in range(len(bins) - 1)])


prom_sin = np.array([promedio_por_banda(amp_sin[c]) for c in range(2)])
prom_con = np.array([promedio_por_banda(amp_con[c]) for c in range(2)])
cociente = prom_con / prom_sin

for c in range(2):
    # [gráfico desactivado: descomentar las líneas de abajo para verlo]
    # fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
    # ancho_barra = ANCHO_BANDA_HZ * 0.35
    # ax1.bar(centros - ancho_barra / 2, prom_sin[c], width=ancho_barra, color=COLOR_SIN, label="Sin goteo")
    # ax1.bar(centros + ancho_barra / 2, prom_con[c], width=ancho_barra, color=COLOR_CON, label="Con goteo")
    # ax1.set_ylabel("Amplitud promedio (mV)")
    # ax1.set_title(f"Amplitud promedio por banda de {ANCHO_BANDA_HZ:.0f} Hz - {CANALES[c]}")
    # ax1.legend()
    # ax1.grid(True, alpha=0.3)

    # ax2.bar(centros, cociente[c], width=ANCHO_BANDA_HZ * 0.8, color=[COLOR_CON if q > 1 else COLOR_SIN for q in cociente[c]])
    # ax2.axhline(1, color="black", linewidth=1)
    # ax2.set_xlabel("Frecuencia (Hz)")
    # ax2.set_ylabel("Cociente con/sin")
    # ax2.set_title("Cociente con goteo / sin goteo (>1 = más amplitud con goteo)")
    # ax2.grid(True, alpha=0.3)
    # fig.tight_layout()
    # plt.show()

    print(f"{CANALES[c]} - bandas de {ANCHO_BANDA_HZ:.0f} Hz ordenadas por cociente con/sin:")
    for i in np.argsort(cociente[c])[::-1][:8]:
        print(f"   {bins[i]:5.0f}-{bins[i+1]:5.0f} Hz: sin={prom_sin[c, i]:.3f} mV, con={prom_con[c, i]:.3f} mV, cociente={cociente[c, i]:.1f}")

# %% [markdown]
# ## 7. Promedio de las gotas
#
# Recién acá se detectan las gotas: un evento es cuando la señal se aparta de su mediana más de
# `UMBRAL_GOTA_MV`, y todo lo que ocurre dentro de `SEPARACION_MIN_S` cuenta como una sola gota.
# Se recorta ±`SEMIANCHO_GOTA_MS` alrededor de cada una (alineadas en la muestra más baja), se
# resta el nivel base y se superponen. La curva negra es el promedio: la forma típica de la gota.
#
# La detección también se corre sobre el registro "sin goteo", para revisar que no tenga ninguna
# gota (si tiene, ese tramo no es ruido de fondo y conviene tenerlo en cuenta al leer la sección 6).

# %%
UMBRAL_GOTA_MV = 150.0     # el ruido sin goteo tiene un desvío de unos pocos mV; las gotas bajan cientos de mV
SEPARACION_MIN_S = 0.1     # eventos más cerca que esto se cuentan como una sola gota
SEMIANCHO_GOTA_MS = 30.0   # ventana alrededor de cada gota para ver su forma


def centros_de_eventos(x, umbral=UMBRAL_GOTA_MV):
    """Índice de la muestra más alejada de la mediana en cada evento."""
    desvio = np.abs(x - np.median(x))
    idx = np.where(desvio > umbral)[0]
    if len(idx) == 0:
        return np.array([], dtype=int)
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_MIN_S * fs))[0] + 1)
    return np.array([g[np.argmax(desvio[g])] for g in grupos])


gotas_con = [centros_de_eventos(x_con[:, c]) for c in range(2)]
eventos_sin = [centros_de_eventos(x_sin[:, c]) for c in range(2)]

for c in range(2):
    periodo = np.mean(np.diff(gotas_con[c])) / fs
    print(f"{CANALES[c]}:")
    print(f"   con goteo: {len(gotas_con[c])} gotas, una cada {periodo:.3f} s ({60 / periodo:.0f} gotas/min)")
    print(f"   sin goteo: {len(eventos_sin[c])} eventos, en t = {[f'{e / fs:.2f}s' for e in eventos_sin[c]]}")
if len(gotas_con[0]) == len(gotas_con[1]):
    print(f"Diferencia de tiempo entre los dos canales para la misma gota: máx "
          f"{np.max(np.abs(gotas_con[0] - gotas_con[1])) / fs * 1000:.1f} ms")

# %%
w_gota = int(SEMIANCHO_GOTA_MS / 1000 * fs)
t_gota = np.arange(-w_gota, w_gota) / fs * 1000


def recortes(x, centros_idx):
    base = np.median(x)
    return np.array([x[k - w_gota:k + w_gota] - base for k in centros_idx if k - w_gota >= 0 and k + w_gota <= len(x)])


segs = [recortes(x_con[:, c], gotas_con[c]) for c in range(2)]
promedio_gota = [s.mean(axis=0) for s in segs]

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, axs = plt.subplots(1, 2, figsize=(15, 5), sharey=True)
# for c, ax in enumerate(axs):
#     for s in segs[c]:
#         ax.plot(t_gota, s, linewidth=0.7, color=COLORES_CANAL[c], alpha=0.3)
#     ax.plot(t_gota, promedio_gota[c], marker="o", markersize=3, linewidth=2.2, color="black",
#             label=f"Promedio de {len(segs[c])} gotas")
#     ax.axhline(0, color="black", linewidth=0.6)
#     ax.set_xlabel("Tiempo relativo al mínimo (ms)")
#     ax.set_title(f"Gotas superpuestas y alineadas - {CANALES[c]}")
#     ax.legend()
#     ax.grid(True, alpha=0.3)
# axs[0].set_ylabel("mV respecto del nivel base")
# fig.tight_layout()
# plt.show()

for c in range(2):
    minimos = segs[c].min(axis=1)
    print(f"{CANALES[c]}: mínimo de cada gota = {minimos.mean():.0f} ± {minimos.std():.0f} mV, "
          f"máximo del rebote = {segs[c].max(axis=1).mean():.0f} mV")

# %% [markdown]
# ## 8. Relación señal/ruido
#
# El ruido de fondo se mide sobre el tramo más largo del registro sin goteo que no tenga ningún
# evento (si el registro tiene una gota, se la deja afuera). Se compara con la amplitud de cada
# gota: es la medida directa de qué tan "limpia" es la señal.


# %%
def mascara_sin_eventos(n, centros_idx, semiancho_s=0.1):
    """True en las muestras que quedan a más de semiancho_s de cualquier evento."""
    m = np.ones(n, dtype=bool)
    w = int(semiancho_s * fs)
    for k in centros_idx:
        m[max(0, k - w):k + w] = False
    return m


todos_eventos_sin = np.unique(np.concatenate(eventos_sin)).astype(int)
limpio = mascara_sin_eventos(N, todos_eventos_sin)
bordes = np.flatnonzero(np.diff(np.r_[0, limpio.astype(int), 0]))
inicios, fines = bordes[::2], bordes[1::2]
k_tramo = np.argmax(fines - inicios)
x_ruido = x_sin[inicios[k_tramo]:fines[k_tramo]]
ruido_std = x_ruido.std(axis=0)

print(f"Tramo sin eventos usado como ruido de fondo: {inicios[k_tramo]/fs:.2f} s a {fines[k_tramo]/fs:.2f} s")
for c in range(2):
    snr = np.abs(segs[c].min(axis=1)) / ruido_std[c]
    print(f"{CANALES[c]}: desvío del ruido = {ruido_std[c]:.2f} mV (≈ {ruido_std[c] / 0.8:.1f} cuentas del ADC), "
          f"gota / ruido = {snr.min():.0f} a {snr.max():.0f} veces (peor gota: {20 * np.log10(snr.min()):.0f} dB)")

# %% [markdown]
# ## 9. Prueba del pasabanda sobre los registros
#
# Se aplica a los dos registros un pasabanda igual al del firmware (pasaaltos + pasabajos
# Butterworth de 4° orden en cascada, cada uno como dos biquads). Está implementado acá con numpy
# (sin scipy) usando las fórmulas de biquad por transformación bilineal, que dan exactamente un
# Butterworth cuando los Q de las dos etapas son los de 4° orden (0.5412 y 1.3066).
#
# Editar `CORTE_PASAALTOS_HZ` y `CORTE_PASABAJOS_HZ` para probar otros cortes.

# %%
CORTE_PASAALTOS_HZ = 5.0
CORTE_PASABAJOS_HZ = 260.0
Q_BUTTER_4 = (0.5412, 1.3066)


def biquad(tipo, f_corte, q):
    w0 = 2 * np.pi * f_corte / fs
    alfa = np.sin(w0) / (2 * q)
    cw = np.cos(w0)
    if tipo == "lp":
        b = np.array([(1 - cw) / 2, 1 - cw, (1 - cw) / 2])
    else:
        b = np.array([(1 + cw) / 2, -(1 + cw), (1 + cw) / 2])
    a = np.array([1 + alfa, -2 * cw, 1 - alfa])
    return b / a[0], a / a[0]


def filtrar(x, etapas):
    y = np.asarray(x, dtype=float)
    for b, a in etapas:
        salida = np.zeros_like(y)
        x1 = x2 = y1 = y2 = 0.0
        for n, xn in enumerate(y):
            yn = b[0] * xn + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
            x2, x1, y2, y1 = x1, xn, y1, yn
            salida[n] = yn
        y = salida
    return y


etapas = [biquad("hp", CORTE_PASAALTOS_HZ, q) for q in Q_BUTTER_4] + [biquad("lp", CORTE_PASABAJOS_HZ, q) for q in Q_BUTTER_4]
TRANSITORIO_S = 0.5  # se descarta el arranque del filtro (parte de estado cero y tarda en asentarse)
n0 = int(TRANSITORIO_S * fs)

xsim_sin = np.array([filtrar(x_sin[:, c] - x_sin[0, c], etapas) for c in range(2)]).T
xsim_con = np.array([filtrar(x_con[:, c] - x_con[0, c], etapas) for c in range(2)]).T

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, axs = plt.subplots(2, 1, figsize=(14, 7), sharey=True)
# for ax, x, titulo in ((axs[0], xsim_sin, "SIN goteo"), (axs[1], xsim_con, "CON goteo")):
#     for c in range(2):
#         ax.plot(t[n0:], x[n0:, c], linewidth=0.5, color=COLORES_CANAL[c], label=CANALES[c])
#     ax.set_xlabel("Tiempo (s)")
#     ax.set_ylabel("mV")
#     ax.set_title(f"Señal FILTRADA {titulo} (pasabanda {CORTE_PASAALTOS_HZ:.0f}-{CORTE_PASABAJOS_HZ:.0f} Hz)")
#     ax.legend(loc="lower right")
#     ax.grid(True, alpha=0.3)
# fig.tight_layout()
# plt.show()

# %%
# Forma de la gota antes y después de filtrar, y relación señal/ruido en cada caso
limpio_sim = limpio.copy()
limpio_sim[:n0] = False
seg_sim = [recortes(xsim_con[:, c], gotas_con[c][gotas_con[c] > n0]) for c in range(2)]
for c in range(2):
    ruido_sim = xsim_sin[limpio_sim, c].std()
    snr_cruda = np.abs(segs[c].min(axis=1)).mean() / ruido_std[c]
    snr_sim = np.abs(seg_sim[c].min(axis=1)).mean() / ruido_sim
    print(f"{CANALES[c]}: ruido {ruido_std[c]:.2f} -> {ruido_sim:.2f} mV | gota / ruido: cruda {snr_cruda:.0f}, "
          f"filtrada {snr_sim:.0f} ({20 * np.log10(snr_sim / snr_cruda):+.1f} dB)")

# [gráfico desactivado: descomentar las líneas de abajo para verlo]
# fig, axs = plt.subplots(1, 2, figsize=(15, 5), sharey=True)
# for c, ax in enumerate(axs):
#     ax.plot(t_gota, promedio_gota[c], color="gray", linewidth=2, label="Cruda (promedio)")
#     ax.plot(t_gota, seg_sim[c].mean(axis=0), color=COLORES_CANAL[c], linewidth=2, label="Filtrada (promedio)")
#     ax.axhline(0, color="black", linewidth=0.6)
#     ax.set_xlabel("Tiempo relativo al mínimo de la cruda (ms)")
#     ax.set_title(CANALES[c])
#     ax.legend()
#     ax.grid(True, alpha=0.3)
# axs[0].set_ylabel("mV")
# fig.tight_layout()
# plt.show()

# %% [markdown]
# ## Señales FILTRADAS con y sin goteo
#
# Registros tomados con `MODE_FILTERED` de `pruebabpw34`: cada canal ya pasó por el antialias
# analógico (RC de 4.7 kΩ + 100 nF antes del ADC, corte ~339 Hz) y por el pasabanda digital del
# firmware (Butterworth de 4° orden, 5-260 Hz), así que está centrado en 0 y sin la continua. Se
# repite el mismo análisis de arriba (señal en el tiempo, FFT de cada una, comparación por bandas)
# pero sobre las señales filtradas, y después se arma la plantilla de la gota para la
# correlación cruzada.

# %%
FILT_SIN_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos BPW34\SeñalFiltradasinGoteo.csv")
FILT_CON_CSV_PATH = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos BPW34\SeñalFiltradaconGoteo.csv")

xfilt_sin_full = np.loadtxt(FILT_SIN_CSV_PATH, delimiter=",", usecols=(0, 1))  # registros completos
xfilt_con_full = np.loadtxt(FILT_CON_CSV_PATH, delimiter=",", usecols=(0, 1))
N_f = min(len(xfilt_sin_full), len(xfilt_con_full))  # mismo largo, para poder comparar amplitudes
xfilt_sin = xfilt_sin_full[:N_f]
xfilt_con = xfilt_con_full[:N_f]
t_f = np.arange(N_f) / fs

print(f"Muestras por registro filtrado (recortadas al mismo largo): {N_f} ({N_f/fs:.2f} s)")
for nombre, x in (("Filtrada sin goteo", xfilt_sin), ("Filtrada con goteo", xfilt_con)):
    print(f"{nombre}:")
    for c in range(2):
        print(f"   {CANALES[c]}: media={np.mean(x[:, c]):.2f} mV, desvío={np.std(x[:, c]):.2f} mV, "
              f"min/max={np.min(x[:, c]):.0f}/{np.max(x[:, c]):.0f} mV")

# %% [markdown]
# ## 10. Señal filtrada sin goteo (en el tiempo)

# %%
yf_lim = max(np.abs(xfilt_sin).max(), np.abs(xfilt_con).max()) * 1.05  # mismo eje vertical en las dos

fig, ax = plt.subplots(figsize=(14, 4))
for c in range(2):
    ax.plot(t_f, xfilt_sin[:, c], linewidth=0.5, color=COLORES_CANAL[c], label=CANALES[c])
ax.set_ylim(-yf_lim, yf_lim)
ax.set_xlabel("Tiempo (s)")
ax.set_ylabel("mV")
ax.set_title("Señal FILTRADA sin goteo")
ax.legend(loc="lower right")
ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 11. Señal filtrada con goteo (en el tiempo)

# %%
fig, ax = plt.subplots(figsize=(14, 4))
for c in range(2):
    ax.plot(t_f, xfilt_con[:, c], linewidth=0.5, color=COLORES_CANAL[c], label=CANALES[c])
ax.set_ylim(-yf_lim, yf_lim)
ax.set_xlabel("Tiempo (s)")
ax.set_ylabel("mV")
ax.set_title("Señal FILTRADA con goteo")
ax.legend(loc="lower right")
ax.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

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
amp_f_sin = np.array([amplitud_f(xfilt_sin[:, c]) for c in range(2)])
amp_f_con = np.array([amplitud_f(xfilt_con[:, c]) for c in range(2)])
amp_f_max = max(amp_f_sin.max(), amp_f_con.max()) * 1.05  # mismo eje vertical en todas las FFT

# %% [markdown]
# ## 12. FFT de la señal filtrada sin goteo

# %%
graficar_fft(freqs_f, amp_f_sin, "FFT de la señal FILTRADA sin goteo", amp_f_max)

# %% [markdown]
# ## 13. FFT de la señal filtrada con goteo

# %%
graficar_fft(freqs_f, amp_f_con, "FFT de la señal FILTRADA con goteo", amp_f_max)

# %% [markdown]
# ## 14. Comparación de las FFT filtradas, superpuestas

# %%
fig, axs = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
for c, ax in enumerate(axs):
    ax.plot(freqs_f, amp_f_sin[c], linewidth=0.8, color=COLOR_SIN, label="Filtrada sin goteo", alpha=0.9)
    ax.plot(freqs_f, amp_f_con[c], linewidth=0.8, color=COLOR_CON, label="Filtrada con goteo", alpha=0.7)
    ax.set_ylabel("Amplitud (mV)")
    ax.set_title(f"FFT filtrada con goteo vs. sin goteo, superpuestas - {CANALES[c]}")
    ax.legend()
    ax.grid(True, alpha=0.3)
axs[-1].set_xlabel("Frecuencia (Hz)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 15. Comparación por bandas de las señales filtradas: amplitud promedio y cociente con/sin
#
# Igual que con las señales crudas: cociente cercano a 1 es ruido que está en ambos registros;
# mayor a 1 es lo que aporta la gota. Fuera de la banda del pasabanda (debajo de 5 Hz y encima de
# 260 Hz) las dos señales deberían estar atenuadas.

# %%
prom_f_sin = np.array([promedio_por_banda(amp_f_sin[c], freqs_f) for c in range(2)])
prom_f_con = np.array([promedio_por_banda(amp_f_con[c], freqs_f) for c in range(2)])
cociente_f = prom_f_con / prom_f_sin

for c in range(2):
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
    ancho_barra = ANCHO_BANDA_HZ * 0.35
    ax1.bar(centros - ancho_barra / 2, prom_f_sin[c], width=ancho_barra, color=COLOR_SIN, label="Filtrada sin goteo")
    ax1.bar(centros + ancho_barra / 2, prom_f_con[c], width=ancho_barra, color=COLOR_CON, label="Filtrada con goteo")
    ax1.set_ylabel("Amplitud promedio (mV)")
    ax1.set_title(f"Señales filtradas - amplitud promedio por banda de {ANCHO_BANDA_HZ:.0f} Hz - {CANALES[c]}")
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2.bar(centros, cociente_f[c], width=ANCHO_BANDA_HZ * 0.8, color=[COLOR_CON if q > 1 else COLOR_SIN for q in cociente_f[c]])
    ax2.axhline(1, color="black", linewidth=1)
    ax2.set_xlabel("Frecuencia (Hz)")
    ax2.set_ylabel("Cociente con/sin")
    ax2.set_title("Señales filtradas - cociente con goteo / sin goteo (>1 = más amplitud con goteo)")
    ax2.grid(True, alpha=0.3)
    fig.tight_layout()
    plt.show()

    print(f"{CANALES[c]} - señales filtradas, bandas de {ANCHO_BANDA_HZ:.0f} Hz ordenadas por cociente con/sin:")
    for i in np.argsort(cociente_f[c])[::-1][:8]:
        print(f"   {bins[i]:5.0f}-{bins[i+1]:5.0f} Hz: sin={prom_f_sin[c, i]:.3f} mV, con={prom_f_con[c, i]:.3f} mV, cociente={cociente_f[c, i]:.1f}")

# %% [markdown]
# ## Forma de la gota filtrada
#
# Se detectan las gotas en la señal filtrada con goteo (registro completo) y se recorta un pedazo
# alrededor de cada una. En la señal filtrada la gota es una caída, un rebote positivo y otra
# caída; en el canal 2 el rebote llega casi a la misma altura que la caída, así que las gotas se
# alinean en su **mínimo** (no en el máximo del valor absoluto, que podría caer a veces en el
# rebote y desalinear el promedio).

# %%
UMBRAL_GOTA_FILT_MV = 100.0  # la señal filtrada sin goteo no pasa de ~15 mV; las gotas bajan cientos de mV
SEMIANCHO_PLANTILLA_MS = 20.0


def minimos_de_eventos(x, umbral):
    """Índice del mínimo de cada evento (grupo de muestras con |x| > umbral)."""
    idx = np.where(np.abs(x) > umbral)[0]
    if len(idx) == 0:
        return np.array([], dtype=int)
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_MIN_S * fs))[0] + 1)
    return np.array([g[np.argmin(x[g])] for g in grupos])


def picos_positivos(x, umbral):
    """Índice del máximo de cada tramo donde x > umbral (para los picos de la correlación)."""
    idx = np.where(x > umbral)[0]
    if len(idx) == 0:
        return np.array([], dtype=int)
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_MIN_S * fs))[0] + 1)
    return np.array([g[np.argmax(x[g])] for g in grupos])


def recortes_filt(x, centros_idx, semiancho_ms):
    w = int(semiancho_ms / 1000 * fs)
    validos = [k for k in centros_idx if k - w >= 0 and k + w <= len(x)]
    return np.array([x[k - w:k + w] for k in validos]), np.arange(-w, w) / fs * 1000


gotas_filt = [minimos_de_eventos(xfilt_con_full[:, c], UMBRAL_GOTA_FILT_MV) for c in range(2)]
eventos_filt_sin = [minimos_de_eventos(xfilt_sin_full[:, c], UMBRAL_GOTA_FILT_MV) for c in range(2)]
t_full_f = np.arange(len(xfilt_con_full)) / fs

for c in range(2):
    periodo = np.mean(np.diff(gotas_filt[c])) / fs
    print(f"{CANALES[c]}: {len(gotas_filt[c])} gotas, una cada {periodo:.2f} s | "
          f"eventos en la filtrada sin goteo: {len(eventos_filt_sin[c])}")
    print(f"   tiempos (s): {[f'{k / fs:.2f}' for k in gotas_filt[c]]}")

# %% [markdown]
# ### Señal filtrada con goteo: dónde está cada gota

# %%
fig, axs = plt.subplots(2, 1, figsize=(16, 7), sharex=True)
for c, ax in enumerate(axs):
    ax.plot(t_full_f, xfilt_con_full[:, c], linewidth=0.5, color=COLORES_CANAL[c])
    ax.plot(gotas_filt[c] / fs, xfilt_con_full[gotas_filt[c], c], "rv", markersize=8, label="gota detectada")
    ax.set_ylabel("mV")
    ax.set_title(f"Señal filtrada con goteo, con cada gota marcada - {CANALES[c]}")
    ax.legend()
    ax.grid(True, alpha=0.3)
axs[-1].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Todas las gotas filtradas alineadas en el mínimo (y su promedio)
#
# Si todas tienen la misma forma, las curvas se apilan y el promedio (negro) es la forma típica.
# Ese promedio es la **plantilla** que se usa después en la correlación.

# %%
segs_plantilla = []
plantillas = []
for c in range(2):
    segs_c, t_plantilla = recortes_filt(xfilt_con_full[:, c], gotas_filt[c], SEMIANCHO_PLANTILLA_MS)
    segs_plantilla.append(segs_c)
    plantillas.append(segs_c.mean(axis=0))
L_plantilla = len(t_plantilla)

fig, axs = plt.subplots(1, 2, figsize=(15, 5), sharey=True)
for c, ax in enumerate(axs):
    for s_ in segs_plantilla[c]:
        ax.plot(t_plantilla, s_, linewidth=0.7, color=COLORES_CANAL[c], alpha=0.35)
    ax.plot(t_plantilla, plantillas[c], marker="o", markersize=3, linewidth=2.5, color="black",
            label=f"Plantilla = promedio de {len(segs_plantilla[c])} gotas")
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_xlabel("Tiempo relativo al mínimo (ms)")
    ax.set_title(f"Plantilla de la gota filtrada ({L_plantilla} muestras, ±{SEMIANCHO_PLANTILLA_MS:.0f} ms) - {CANALES[c]}")
    ax.legend()
    ax.grid(True, alpha=0.3)
axs[0].set_ylabel("mV")
fig.tight_layout()
plt.show()

for c in range(2):
    print(f"{CANALES[c]}: mínimo de cada gota = {segs_plantilla[c].min(axis=1).mean():.0f} ± "
          f"{segs_plantilla[c].min(axis=1).std():.0f} mV, rebote = {segs_plantilla[c].max(axis=1).mean():.0f} mV")

# %% [markdown]
# ## Correlación cruzada con la plantilla (matched filter)
#
# Igual que en `analisis_opt101`: la plantilla (promedio de las gotas) se desliza a lo largo de
# toda la señal, calculando en cada instante cuánto se parece el pedazo de señal a la plantilla.
# Donde hay una gota, la correlación da un pico grande; donde solo hay ruido, da valores chicos.
#
# La plantilla se normaliza para tener energía 1 (igual que hace `xcorr_detector` en el firmware),
# así que la salida de la correlación queda en mV y conserva la amplitud. Se arma **una plantilla
# por canal**, porque la forma de la gota no es igual en los dos fotodiodos (el rebote del canal 2
# es mucho más grande).
#
# El umbral se fija a partir de la señal filtrada **sin goteo**: se mide el desvío de la
# correlación con solo ruido y se pone el umbral `FACTOR_UMBRAL` veces por encima.

# %%
FACTOR_UMBRAL = 12.0  # mismo criterio que en el OPT101: umbral = 12 desvíos de la correlación sin goteo

plantillas_unit = [p_ / np.linalg.norm(p_) for p_ in plantillas]


def correlacion_con_plantilla(x, plantilla_normalizada):
    # salida[k] compara la plantilla con x[k : k+L]; el eje de tiempo se alinea con el centro de la plantilla
    return np.correlate(x, plantilla_normalizada, mode="valid")


def tiempos_de_correlacion(n_salida):
    return (np.arange(n_salida) + L_plantilla // 2) / fs


def graficar_correlacion(x, corr, umbral, picos, titulo, color):
    # Solo la parte positiva: la plantilla oscila (caída, rebote, caída), así que la correlación tiene
    # lóbulos negativos donde la señal está invertida respecto de la plantilla. Eso no es una coincidencia.
    t_x = np.arange(len(x)) / fs
    t_c = tiempos_de_correlacion(len(corr))
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(16, 7), sharex=True)
    ax1.plot(t_x, x, linewidth=0.5, color=color)
    ax1.set_ylabel("mV")
    ax1.set_title(titulo)
    ax1.grid(True, alpha=0.3)
    ax2.plot(t_c, np.maximum(corr, 0), linewidth=0.7, color="tab:purple")
    ax2.axhline(umbral, color="red", linestyle="--", label=f"umbral = {umbral:.1f}")
    ax2.plot(t_c[picos], corr[picos], "rv", markersize=9, label=f"gota detectada ({len(picos)})")
    ax2.set_xlabel("Tiempo (s)")
    ax2.set_ylabel("Coincidencia con la plantilla")
    ax2.set_ylim(bottom=0)
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    fig.tight_layout()
    plt.show()


def graficar_zoom_correlacion(x, corr, k_pico, color, semiancho_ms=40.0):
    # zoom alrededor de una gota: la señal y, abajo, cómo va cambiando la correlación al deslizar la plantilla
    w = int(semiancho_ms / 1000 * fs)
    c_ = k_pico + L_plantilla // 2
    i0, i1 = max(0, c_ - w), min(len(x), c_ + w)
    tt = (np.arange(i0, i1) - c_) / fs * 1000
    k0, k1 = max(0, i0 - L_plantilla // 2), min(len(corr), i1 - L_plantilla // 2)
    tc = (np.arange(k0, k1) + L_plantilla // 2 - c_) / fs * 1000
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 6), sharex=True)
    ax1.plot(tt, x[i0:i1], marker="o", markersize=3, linewidth=1, color=color)
    ax1.axhline(0, color="black", linewidth=0.6)
    ax1.set_ylabel("mV")
    ax1.set_title(f"Zoom en una gota (t={c_/fs:.2f}s): señal y correlación con la plantilla")
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
corr_sin = []
umbral_corr = []
for c in range(2):
    corr_sin.append(correlacion_con_plantilla(xfilt_sin_full[:, c], plantillas_unit[c]))
    umbral_corr.append(FACTOR_UMBRAL * np.std(corr_sin[c]))
    picos_sin = picos_positivos(corr_sin[c], umbral_corr[c])
    print(f"{CANALES[c]}: desvío de la correlación sin goteo = {np.std(corr_sin[c]):.2f}, máximo = "
          f"{np.max(corr_sin[c]):.2f} ({np.max(corr_sin[c]) / np.std(corr_sin[c]):.1f} desvíos)")
    print(f"   umbral ({FACTOR_UMBRAL:.0f} desvíos) = {umbral_corr[c]:.1f}  ->  falsos positivos sin goteo: {len(picos_sin)}")
    graficar_correlacion(xfilt_sin_full[:, c], corr_sin[c], umbral_corr[c], picos_sin,
                         f"Señal filtrada SIN goteo y su correlación con la plantilla - {CANALES[c]}", COLORES_CANAL[c])

# %% [markdown]
# ### Señal filtrada CON goteo: picos de la correlación

# %%
corr_con = []
picos_con = []
for c in range(2):
    corr_con.append(correlacion_con_plantilla(xfilt_con_full[:, c], plantillas_unit[c]))
    picos_con.append(picos_positivos(corr_con[c], umbral_corr[c]))
    print(f"{CANALES[c]}: gotas detectadas por correlación: {len(picos_con[c])}")
    print(f"   tiempos (s): {[f'{t_:.2f}' for t_ in tiempos_de_correlacion(len(corr_con[c]))[picos_con[c]]]}")
    print(f"   valor de la correlación en cada pico: {[f'{corr_con[c][k]:.0f}' for k in picos_con[c]]}  "
          f"(umbral {umbral_corr[c]:.1f}; la gota más chica da {np.min(corr_con[c][picos_con[c]]) / umbral_corr[c]:.0f} veces el umbral)")
    graficar_correlacion(xfilt_con_full[:, c], corr_con[c], umbral_corr[c], picos_con[c],
                         f"Señal filtrada CON goteo y su correlación con la plantilla - {CANALES[c]}", COLORES_CANAL[c])

graficar_zoom_correlacion(xfilt_con_full[:, 0], corr_con[0], picos_con[0][len(picos_con[0]) // 2], COLORES_CANAL[0])

# %% [markdown]
# ### Comparación con la detección por umbral de amplitud
#
# Se compara cada gota detectada por correlación con las que se marcaron mirando solo la amplitud
# de la señal (|x| > `UMBRAL_GOTA_FILT_MV`). Si coinciden, la correlación encuentra las mismas gotas.

# %%
for c in range(2):
    t_picos_corr = tiempos_de_correlacion(len(corr_con[c]))[picos_con[c]]
    t_picos_amp = gotas_filt[c] / fs
    print(f"{CANALES[c]}: por amplitud {len(t_picos_amp)} gotas | por correlación {len(t_picos_corr)} gotas")
    for ta in t_picos_amp:
        tc = t_picos_corr[np.argmin(np.abs(t_picos_corr - ta))]
        print(f"   gota en t={ta:.3f}s (amplitud) -> correlación en t={tc:.3f}s (diferencia {abs(tc - ta) * 1000:.1f} ms)")

# %% [markdown]
# ### Validación sin hacer trampa (leave-one-out)
#
# La plantilla se armó con las mismas gotas en las que después se buscan picos, así que
# encontrarlas es un poco circular. Para chequearlo: se saca una gota, se arma la plantilla con
# las otras, y se ve si la correlación igual encuentra la gota que quedó afuera.

# %%
for c in range(2):
    resultados = []
    for i, k_i in enumerate(gotas_filt[c]):
        p_i = np.delete(segs_plantilla[c], i, axis=0).mean(axis=0)
        p_i = p_i / np.linalg.norm(p_i)
        corr_i = correlacion_con_plantilla(xfilt_con_full[:, c], p_i)
        t_i = tiempos_de_correlacion(len(corr_i))[picos_positivos(corr_i, umbral_corr[c])]
        resultados.append(np.any(np.abs(t_i - k_i / fs) < 0.01))
    print(f"{CANALES[c]}: gotas encontradas dejándolas fuera de la plantilla: {sum(resultados)} de {len(resultados)}")

# %% [markdown]
# ## Exportar las plantillas al firmware
#
# Imprime la plantilla de cada canal (el promedio de las gotas filtradas) como arreglo de C, y el
# umbral de cada uno. `xcorr_detector` normaliza la plantilla por dentro, así que se exporta tal
# cual. Hay que regenerarlas si cambia `SAMPLE_PERIOD_US`, los cortes del pasabanda o el antialias
# RC, porque la forma de la gota filtrada depende de todos ellos.

# %%
print(f"#define DROP_TEMPLATE_LEN {L_plantilla}\n")
for c in range(2):
    print(f"/* {CANALES[c]} */")
    print(f"static const float DROP_TEMPLATE_CH{c + 1}[DROP_TEMPLATE_LEN] = {{")
    for i in range(0, L_plantilla, 6):
        print("    " + ", ".join(f"{v:.5f}f" for v in plantillas[c][i:i + 6]) + ",")
    print("};\n")
for c in range(2):
    print(f"Umbral {CANALES[c]} (XCORR_THRESHOLD): {umbral_corr[c]:.1f}")

# %% [markdown]
# ## Prueba con una señal ruidosa inventada
#
# Probando el firmware, al conectar otra computadora en el mismo enchufe apareció ruido y el
# detector lo tomó como gotas. Acá se simula esa situación: a los registros filtrados reales (sin
# goteo y con goteo) se les suma un ruido **inventado** y se ve cómo responde la correlación.
#
# El ruido se genera como si entrara al ADC (antes del filtro) y se pasa por el **mismo pasabanda
# del firmware** (sección 9), así que llega a la correlación igual que llegaría en la placa. Se
# suma **el mismo ruido a los dos canales**, porque una interferencia que viene por el enchufe
# (alimentación, masa) afecta a los dos a la vez. Hay tres tipos, que se pueden combinar:
#
# - **Blanco:** ruido de banda ancha (desvío `blanco_mV`).
# - **Red:** 50 Hz y sus armónicos hasta 250 Hz (amplitud `red_mV` en 50 Hz, y la mitad, un
#   tercio... en los armónicos). Es lo típico de un problema de masa o de fuente.
# - **Picos:** impulsos cortos (2 muestras) en instantes al azar (`picos_por_s` por segundo, de
#   hasta `pico_mV`), como los que mete una fuente conmutada o un equipo que se prende y apaga.
#
# **Los niveles son inventados.** Sirven para ver cómo reacciona cada estrategia de umbral, no para
# fijar valores: para eso hay que grabar el ruido real (`MODE_FILTERED` con la otra computadora
# enchufada) y repetir el análisis con ese registro.

# %%
SEMILLA_RUIDO = 1  # misma semilla -> mismo ruido cada vez que se corre
rng = np.random.default_rng(SEMILLA_RUIDO)

ESCENARIOS_RUIDO = {
    "Sin ruido agregado": dict(),
    "Blanco (15 mV)": dict(blanco_mV=15),
    "Red 50 Hz (40 mV)": dict(red_mV=40),
    "Picos (3/s, 300 mV)": dict(picos_por_s=3, pico_mV=300),
    "Todo junto": dict(blanco_mV=15, red_mV=40, picos_por_s=3, pico_mV=300),
}
ESCENARIO_EJEMPLO = "Todo junto"  # el que se grafica en detalle


def ruido_inventado(n, blanco_mV=0.0, red_mV=0.0, picos_por_s=0.0, pico_mV=0.0):
    """Ruido a la entrada del ADC, ya pasado por el pasabanda del firmware (sección 9)."""
    t_r = np.arange(n) / fs
    r = blanco_mV * rng.standard_normal(n)
    for k in range(1, 6):  # 50 Hz y armónicos (100, 150, 200, 250 Hz), con fase al azar
        r += red_mV / k * np.sin(2 * np.pi * 50.03 * k * t_r + rng.uniform(0, 2 * np.pi))
    for p in rng.integers(0, n - 2, rng.poisson(picos_por_s * n / fs)):  # picos en instantes al azar
        r[p:p + 2] += rng.uniform(0.5, 1.0) * pico_mV * rng.choice([-1, 1])
    return filtrar(r, etapas)


# El mismo ruido se suma a los dos canales (columna repetida)
ruidos = {nombre: (ruido_inventado(len(xfilt_sin_full), **kw), ruido_inventado(len(xfilt_con_full), **kw))
          for nombre, kw in ESCENARIOS_RUIDO.items()}

r_sin_ej, r_con_ej = ruidos[ESCENARIO_EJEMPLO]
xr_sin_ej = xfilt_sin_full + r_sin_ej[:, None]
xr_con_ej = xfilt_con_full + r_con_ej[:, None]

fig, axs = plt.subplots(2, 1, figsize=(16, 7), sharex=True, sharey=True)
for c, ax in enumerate(axs):
    ax.plot(t_full_f, xr_con_ej[:, c], linewidth=0.5, color="tab:red", label="Con el ruido inventado")
    ax.plot(t_full_f, xfilt_con_full[:, c], linewidth=0.5, color=COLORES_CANAL[c], label="Registro real")
    ax.set_ylabel("mV")
    ax.set_title(f"Señal filtrada con goteo + ruido \"{ESCENARIO_EJEMPLO}\" - {CANALES[c]}")
    ax.legend(loc="lower right")
    ax.grid(True, alpha=0.3)
axs[-1].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Tres estrategias de umbral
#
# 1. **Umbral fijo** (lo que hace hoy el firmware): 12 desvíos de la correlación medidos sobre la
#    señal sin goteo **limpia** (59.7 y 51.4). Si el ruido crece, el umbral no se entera.
# 2. **Umbral adaptativo:** el detector estima el desvío de la correlación mientras funciona (con
#    un promedio exponencial de la correlación al cuadrado, de ~`TAU_ADAPTATIVO_S` segundos) y usa
#    `FACTOR_ADAPTATIVO` veces ese desvío. Si aparece ruido, el umbral **sube solo**. Para que las
#    gotas no inflen la estimación, cada valor de la correlación se limita a ±3 desvíos antes de
#    usarlo (las gotas dan valores de decenas de desvíos); y el umbral nunca baja del fijo. Es implementable en el firmware (una multiplicación y una suma
#    por muestra).
# 3. **Adaptativo + forma:** además del umbral, se pide que el pedazo de señal se **parezca** a la
#    plantilla: la similitud de forma es la correlación dividida por la energía del pedazo (1 =
#    misma forma, 0 = nada que ver), y tiene que ser al menos `FORMA_MINIMA`. Un pico de ruido
#    puede dar una correlación grande solo por ser grande, pero su forma no es la de una gota.
#
# La detección se simula con la **misma lógica del firmware** (ventana de 15 ms quedándose con el
# máximo y 100 ms de período refractario), y los dos canales se combinan como en el firmware: el
# LED se prende si **cualquiera** de los dos detecta.

# %%
FACTOR_ADAPTATIVO = 10.0
TAU_ADAPTATIVO_S = 1.0
FORMA_MINIMA = 0.6
PEAK_WINDOW = int(15 * 1000 / SAMPLE_PERIOD_US)  # igual que XCORR_PEAK_WINDOW_MS en el firmware
REFRACTARIO = int(100 * 1000 / SAMPLE_PERIOD_US)  # igual que XCORR_REFRACTORY_MS


def detector_firmware(corr, umbral, forma=None, forma_min=None):
    """Misma lógica que XCorrDetectorProcess: al pasar el umbral espera PEAK_WINDOW muestras quedándose con
    el máximo, detecta, e ignora REFRACTARIO muestras. Si se pasa `forma`, descarta las detecciones cuya
    similitud de forma en el pico sea menor a forma_min."""
    umbral = np.broadcast_to(umbral, corr.shape)
    detecciones = []
    buscando, quedan, refractario, maximo, k_max = False, 0, 0, 0.0, 0
    for n, v in enumerate(corr):
        if refractario > 0:
            refractario -= 1
            continue
        if buscando:
            if v > maximo:
                maximo, k_max = v, n
            quedan -= 1
            if quedan == 0:
                buscando = False
                refractario = REFRACTARIO
                if forma is None or forma[k_max] >= forma_min:
                    detecciones.append(k_max)
            continue
        if v > umbral[n]:
            buscando, quedan, maximo, k_max = True, PEAK_WINDOW, v, n
    return np.array(detecciones, dtype=int)


def umbral_adaptativo(corr, umbral_fijo, factor=FACTOR_ADAPTATIVO, tau_s=TAU_ADAPTATIVO_S):
    """Umbral = factor x desvío de la correlación, estimado en línea (promedio exponencial de corr²)."""
    alfa = 1 / (tau_s * fs)
    varianza = (umbral_fijo / FACTOR_UMBRAL) ** 2  # arranca con el desvío medido sin ruido
    umbral = np.empty(len(corr))
    for n, v in enumerate(corr):
        desvio = np.sqrt(varianza)
        umbral[n] = max(factor * desvio, umbral_fijo)
        # Cada muestra se limita a ±3 desvíos antes de usarla: así las gotas (y sus lóbulos, que también
        # son grandes, positivos y negativos) casi no mueven la estimación del ruido
        v_limitado = min(max(v, -3 * desvio), 3 * desvio)
        varianza += alfa * (v_limitado * v_limitado - varianza)
    return umbral


def similitud_de_forma(x, plantilla_normalizada):
    """Correlación dividida por la energía de cada pedazo de señal: 1 = misma forma que la plantilla."""
    energia = np.sqrt(np.convolve(x * x, np.ones(L_plantilla), mode="valid"))
    return correlacion_con_plantilla(x, plantilla_normalizada) / np.maximum(energia, 1e-9)


def combinar_canales(det_1, det_2, separacion_s=0.2):
    """El LED se prende si cualquiera de los dos canales detecta; detecciones cercanas cuentan una vez."""
    eventos = []
    for k in np.sort(np.concatenate([det_1, det_2])):
        if not eventos or k - eventos[-1] > int(separacion_s * fs):
            eventos.append(k)
    return np.array(eventos, dtype=int)


def evaluar(x_sin, x_con, estrategia):
    """Devuelve (falsas sin goteo, falsas con goteo, gotas detectadas) con los dos canales combinados."""
    det = {"sin": [], "con": []}
    for c in range(2):
        for clave, x in (("sin", x_sin[:, c]), ("con", x_con[:, c])):
            corr = correlacion_con_plantilla(x, plantillas_unit[c])
            if estrategia == "Umbral fijo":
                det[clave].append(detector_firmware(corr, umbral_corr[c]))
            else:
                umbral = umbral_adaptativo(corr, umbral_corr[c])
                forma = similitud_de_forma(x, plantillas_unit[c]) if estrategia == "Adaptativo + forma" else None
                det[clave].append(detector_firmware(corr, umbral, forma, FORMA_MINIMA))
    ev_sin = combinar_canales(*det["sin"])
    ev_con = combinar_canales(*det["con"]) + L_plantilla // 2  # índice de la correlación -> índice de la señal
    aciertos = sum(np.any(np.abs(ev_con - k) < 0.03 * fs) for k in gotas_filt[0])
    return len(ev_sin), len(ev_con) - aciertos, aciertos


# %% [markdown]
# ### El escenario de ejemplo en detalle (canal 1)
#
# Arriba, la correlación con el umbral fijo (rojo) y el adaptativo (verde): con ruido, la
# correlación sin goteo pasa el umbral fijo muchas veces, mientras que el adaptativo sube y la deja
# por debajo. Abajo, la similitud de forma en los instantes detectados.

# %%
c = 0
corr_ej = correlacion_con_plantilla(xr_con_ej[:, c], plantillas_unit[c])
umbral_ej = umbral_adaptativo(corr_ej, umbral_corr[c])
forma_ej = similitud_de_forma(xr_con_ej[:, c], plantillas_unit[c])
det_fijo = detector_firmware(corr_ej, umbral_corr[c])
det_adapt = detector_firmware(corr_ej, umbral_ej, forma_ej, FORMA_MINIMA)
t_c = tiempos_de_correlacion(len(corr_ej))

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(16, 8), sharex=True)
ax1.plot(t_c, np.maximum(corr_ej, 0), linewidth=0.6, color="tab:purple", label="Correlación (parte positiva)")
ax1.axhline(umbral_corr[c], color="red", linestyle="--", label=f"Umbral fijo = {umbral_corr[c]:.0f} -> {len(det_fijo)} detecciones")
ax1.plot(t_c, umbral_ej, color="tab:green", linewidth=1.5, label=f"Umbral adaptativo -> {len(det_adapt)} detecciones (con forma)")
ax1.plot(t_c[det_fijo], corr_ej[det_fijo], "rx", markersize=7)
ax1.plot(t_c[det_adapt], corr_ej[det_adapt], "gv", markersize=10)
ax1.plot(gotas_filt[c] / fs, np.full(len(gotas_filt[c]), corr_ej.max() * 1.05), "kv", markersize=8, label="Gotas reales")
ax1.set_ylabel("Correlación")
ax1.set_ylim(bottom=0)
ax1.set_title(f"\"{ESCENARIO_EJEMPLO}\", señal con goteo - {CANALES[c]}")
ax1.legend(loc="upper right")
ax1.grid(True, alpha=0.3)
ax2.plot(t_c[det_fijo], forma_ej[det_fijo], "rx", markersize=7, label="Detecciones con umbral fijo")
ax2.plot(t_c[det_adapt], forma_ej[det_adapt], "gv", markersize=10, label="Detecciones adaptativo + forma")
ax2.axhline(FORMA_MINIMA, color="black", linestyle=":", label=f"Forma mínima = {FORMA_MINIMA}")
ax2.set_ylim(-1, 1.05)
ax2.set_xlabel("Tiempo (s)")
ax2.set_ylabel("Similitud de forma")
ax2.legend(loc="lower right")
ax2.grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Resumen: todos los escenarios y las tres estrategias
#
# Para cada escenario se cuentan, con los dos canales combinados como en el firmware: las
# detecciones **falsas** en la señal sin goteo, las falsas en la señal con goteo y las **gotas
# detectadas** (de 7). Lo ideal es 0 / 0 / 7.

# %%
ESTRATEGIAS = ("Umbral fijo", "Adaptativo", "Adaptativo + forma")
resumen = {}
print(f"{'Escenario':22s} | " + " | ".join(f"{e:>20s}" for e in ESTRATEGIAS))
print(f"{'':22s} | " + " | ".join(f"{'falsas sin/con, gotas':>20s}" for _ in ESTRATEGIAS))
for nombre in ESCENARIOS_RUIDO:
    r_sin, r_con = ruidos[nombre]
    x_sin_r = xfilt_sin_full + r_sin[:, None]
    x_con_r = xfilt_con_full + r_con[:, None]
    resumen[nombre] = [evaluar(x_sin_r, x_con_r, e) for e in ESTRATEGIAS]
    print(f"{nombre:22s} | " + " | ".join(f"{fs_:>6d} /{fc:>3d}, {g}/7".rjust(20) for fs_, fc, g in resumen[nombre]))

fig, axs = plt.subplots(1, 2, figsize=(15, 5))
x_pos = np.arange(len(ESCENARIOS_RUIDO))
ancho = 0.27
for i, e in enumerate(ESTRATEGIAS):
    falsas = [resumen[n][i][0] + resumen[n][i][1] for n in ESCENARIOS_RUIDO]
    gotas = [resumen[n][i][2] for n in ESCENARIOS_RUIDO]
    axs[0].bar(x_pos + (i - 1) * ancho, falsas, width=ancho, label=e)
    axs[1].bar(x_pos + (i - 1) * ancho, gotas, width=ancho, label=e)
axs[0].set_title("Detecciones falsas (sin goteo + con goteo)")
axs[1].set_title("Gotas detectadas (de 7)")
axs[1].axhline(len(gotas_filt[0]), color="black", linewidth=0.8)
for ax in axs:
    ax.set_xticks(x_pos)
    ax.set_xticklabels(list(ESCENARIOS_RUIDO), rotation=20, ha="right")
    ax.legend()
    ax.grid(True, axis="y", alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ### Qué muestra la prueba
#
# - **El umbral fijo no aguanta ruido nuevo:** se calculó con el ruido de la señal limpia (~4 mV),
#   así que cualquier ruido agregado lo pasa. Con 50 Hz, la correlación queda por encima del umbral
#   casi todo el tiempo y el detector dispara cada ~115 ms (15 ms de ventana + 100 ms de
#   refractario): el LED quedaría prendido.
# - **El umbral adaptativo resuelve el ruido "constante"** (blanco y 50 Hz): el umbral sube hasta
#   quedar por encima del ruido y las gotas, que dan una correlación mucho más grande, siguen
#   pasándolo. El límite es cuando el ruido es tan grande que `FACTOR_ADAPTATIVO` x su desvío
#   llega a la altura de las gotas (~850): ahí se empiezan a perder gotas. Pasa primero en el
#   **canal 1**: su plantilla tiene una cola lenta (la respuesta del pasaaltos) con mucho contenido
#   en bajas frecuencias, así que el 50 Hz le sube mucho la correlación. En el ejemplo, su umbral
#   adaptativo queda en ~800, justo debajo de las gotas, y el canal 1 solo pierde algunas; el LED
#   igual las marca porque el canal 2 sí las detecta.
# - **Los picos aislados son lo más difícil:** como son esporádicos, casi no suben el desvío, así
#   que el umbral adaptativo apenas se mueve (las falsas detecciones casi no bajan respecto del
#   umbral fijo); y un pico grande pasado por el pasabanda da una correlación grande. El control
#   de forma descarta más o menos la mitad, pero no todos: un impulso filtrado se parece bastante a
#   una gota (la gota también es un evento corto).
# - **Combinar los canales no ayuda con este ruido:** como entra por el enchufe, llega igual a los
#   dos canales. Pedir que detecten los dos a la vez sirve contra ruido que afecta a un solo canal.
#
# Conclusión: el procesamiento puede **mitigar** el ruido (el umbral adaptativo es la mejora más
# útil para el firmware), pero la solución de fondo es que el ruido no entre: alimentación
# filtrada o a batería, masa en estrella, capacitores de desacople junto al MCP6004, cables cortos
# o blindados. Y para ajustar los valores, grabar el ruido real con la otra computadora enchufada.
