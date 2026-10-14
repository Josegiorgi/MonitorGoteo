# %% [markdown]
# # Plantillas y umbrales del detector de gotas (correlación cruzada) - BPW34, dos canales
#
# Este script arma, con registros reales, lo que necesita el detector de `pruebabpw34`:
#
# - una **plantilla por canal**: la forma típica de una gota en cada fotodiodo;
# - un **umbral por canal**: cuánto tiene que parecerse la señal a la plantilla para contar una gota.
#
# El firmware avisa una gota si **cualquiera** de los dos canales la detecta. Eso importa para el
# umbral: una gota débil en un canal no se pierde si el otro la ve fuerte.
#
# Registros (todos con el pasabanda **20-260 Hz**, Butterworth orden 4, `SAMPLE_PERIOD_US = 1200`):
#
# | Registro | Para qué se usa |
# |---|---|
# | `Filtrada2.csv` | plantillas y prueba. `MODE_FILTERED`, **dispositivo fijo**: goteo regular, la gota se ve bien en los dos canales |
# | `Filtrada.csv` | plantillas y prueba. `MODE_FILTERED`, **moviendo el dispositivo** para que la gota pase por cada canal (casos extremos) |
# | `Filtradasingoteo.csv` | ruido sin goteo. `MODE_FILTERED` |
# | `ModorawSINgotaa.csv` | segundo registro de ruido sin goteo. Es **crudo**: se filtra acá con el mismo filtro que el firmware |
# | `ModorawCONgotas.csv` | **validación**: otra toma con goteo (otra colocación del dispositivo), cruda y filtrada acá. No interviene en las plantillas |
#
# Orden:
# 1. Las señales.
# 2. Las gotas y cómo se reparten entre canales.
# 3. Las plantillas.
# 4. La correlación.
# 5. Los umbrales y por qué esos valores.
# 6. Emulación del detector del firmware (con la lógica "cualquiera de los dos canales").
# 7. Exportar al firmware.
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
CARPETA = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos BPW34\Comparacion")
FIJO_CSV = CARPETA / "Filtrada2.csv"                  # dispositivo fijo
MOVIDO_CSV = CARPETA / "Filtrada.csv"                 # dispositivo movido a propósito
SIN_GOTEO_CSV = CARPETA / "Filtradasingoteo.csv"      # ruido, ya filtrado por el firmware
SIN_GOTEO_CRUDO_CSV = CARPETA / "ModorawSINgotaa.csv"  # ruido, crudo
VALIDACION_CRUDO_CSV = CARPETA / "ModorawCONgotas.csv"

SAMPLE_PERIOD_US = 1200.0    # el del firmware al tomar los registros
CORTE_PASAALTOS_HZ = 20.0    # los del firmware (CUTOFF_HIGHPASS_HZ / CUTOFF_LOWPASS_HZ)
CORTE_PASABAJOS_HZ = 260.0
BASELINE_SAMPLES = 100       # el firmware resta el promedio de las primeras muestras antes de filtrar
T_TRANSITORIO_S = 1.0        # se descarta el arranque del filtro
UMBRAL_EVENTO_MV = 100.0     # un evento es todo lo que pasa de esto en la señal filtrada (el ruido llega a ~46)
SEPARACION_EVENTO_S = 0.15   # eventos más cerca que esto cuentan como una sola gota
PP_MIN_PLANTILLA_MV = 400.0  # solo las gotas de más de esto (en ese canal) entran en la plantilla de ese canal
ANTES_MS, DESPUES_MS = 15.0, 30.0   # ventana de la plantilla alrededor del valle
FACTOR_RUIDO = 3.0           # umbral = FACTOR_RUIDO x el ruido máximo de la correlación en ese canal
PEAK_WINDOW_MS = 15          # ventana de búsqueda del máximo del detector (XCORR_PEAK_WINDOW_MS); la gota llega al máximo a los 13 ms como mucho
REFRACTORY_MS = 100          # tiempo que el detector ignora la señal tras detectar (XCORR_REFRACTORY_MS)

COLOR_CAN = ("tab:blue", "tab:orange")
COLOR_SIN = "tab:gray"

fs = 1_000_000.0 / SAMPLE_PERIOD_US
sk = int(T_TRANSITORIO_S * fs)
na, nd = int(ANTES_MS / 1000 * fs), int(DESPUES_MS / 1000 * fs)
L = na + nd

# %% [markdown]
# ## El filtro del firmware
#
# Los registros crudos se pasan por el mismo filtro que el firmware: pasaaltos y pasabajos
# Butterworth orden 4 en cascada de biquads (fórmulas de `esp_dsp`, Q de `iir_filter.c`), restando
# antes el promedio de las primeras `BASELINE_SAMPLES` muestras, como hace `pruebabpw34.c`. Así
# los registros crudos quedan en las mismas condiciones que los filtrados por el firmware.

# %%
Q_BUTTERWORTH_4 = [1 / 0.765, 1 / 1.848]


def biquad(tipo, f_corte, q):
    w0 = 2 * np.pi * f_corte / fs
    alfa, c = np.sin(w0) / (2 * q), np.cos(w0)
    if tipo == "lp":
        b = np.array([(1 - c) / 2, 1 - c, (1 - c) / 2])
    else:
        b = np.array([(1 + c) / 2, -(1 + c), (1 + c) / 2])
    a = np.array([1 + alfa, -2 * c, 1 - alfa])
    return b / a[0], a / a[0]


SECCIONES = [biquad("hp", CORTE_PASAALTOS_HZ, q) for q in Q_BUTTERWORTH_4] + \
            [biquad("lp", CORTE_PASABAJOS_HZ, q) for q in Q_BUTTERWORTH_4]


def filtrar_como_el_firmware(v):
    y = np.asarray(v, float) - np.mean(v[:BASELINE_SAMPLES])
    for b, a in SECCIONES:
        out = np.empty_like(y)
        z1 = z2 = 0.0
        for n_, val in enumerate(y):
            o = b[0] * val + z1
            z1 = b[1] * val - a[1] * o + z2
            z2 = b[2] * val - a[2] * o
            out[n_] = o
        y = out
    return y


def cargar(ruta):
    return np.loadtxt(ruta, delimiter=",", usecols=(0, 1))


def cargar_y_filtrar(ruta):
    crudo = cargar(ruta)
    return np.column_stack([filtrar_como_el_firmware(crudo[:, ch]) for ch in range(2)])


REGISTROS = {
    "fijo": cargar(FIJO_CSV),
    "movido": cargar(MOVIDO_CSV),
    "sin": cargar(SIN_GOTEO_CSV),
    "sin_crudo": cargar_y_filtrar(SIN_GOTEO_CRUDO_CSV),
    "val": cargar_y_filtrar(VALIDACION_CRUDO_CSV),
}
TITULOS = {"fijo": "Dispositivo fijo (Filtrada2.csv)", "movido": "Dispositivo movido (Filtrada.csv)",
           "sin": "Sin goteo (Filtradasingoteo.csv)", "sin_crudo": "Sin goteo (crudo, filtrado acá)",
           "val": "Validación (crudo, filtrado acá)"}
CON_GOTEO = ("fijo", "movido", "val")   # registros que tienen gotas
SIN_GOTEO = ("sin", "sin_crudo")        # registros de solo ruido
print(f"fs = {fs:.1f} Hz | plantilla de {L} muestras (-{ANTES_MS:.0f} a +{DESPUES_MS:.0f} ms)")

# %% [markdown]
# ## 1. Las señales
#
# Todas ya filtradas: oscilan alrededor de 0 mV. En `Filtrada2.csv` (dispositivo fijo) las gotas
# son parejas y se ven en los dos canales. En `Filtrada.csv` se movió el dispositivo a propósito,
# así que la gota se reparte de forma distinta entre los canales a lo largo del registro. En los
# registros sin goteo solo hay ruido (desvío de 3-4 mV).

# %%
fig, axs = plt.subplots(len(REGISTROS), 2, figsize=(16, 16), sharey="row")
for fila, nombre in enumerate(REGISTROS):
    x = REGISTROS[nombre]
    for ch in range(2):
        axs[fila, ch].plot(np.arange(len(x)) / fs, x[:, ch], linewidth=0.4,
                           color=COLOR_SIN if nombre in SIN_GOTEO else COLOR_CAN[ch])
        axs[fila, ch].set_title(f"{TITULOS[nombre]} - canal {ch + 1}")
        axs[fila, ch].set_ylabel("mV")
        axs[fila, ch].grid(True, alpha=0.3)
axs[-1, 0].set_xlabel("Tiempo (s)")
axs[-1, 1].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

for nombre, x in REGISTROS.items():
    for ch in range(2):
        print(f"{nombre:9s} canal {ch + 1}: {len(x)/fs:5.1f} s, desvío = {x[sk:, ch].std():6.1f} mV, mín/máx = {x[sk:, ch].min():.0f}/{x[sk:, ch].max():.0f}")

# %% [markdown]
# ## 2. Las gotas y cómo se reparten entre canales
#
# Un evento es todo lo que pasa `UMBRAL_EVENTO_MV` (en valor absoluto) en un canal; de cada
# evento se toma como **centro el valle más profundo** (se alinea en el valle y no en el
# máximo del valor absoluto, porque en la gota filtrada el valle y el pico positivo son parecidos).
# Esta detección por amplitud es solo para armar las plantillas; el detector del firmware es
# el de correlación.
#
# Las gotas de los dos canales se unen en una lista de **gotas distintas** (un mismo evento visto
# por los dos canales cuenta una vez).

# %%


def centros_de_eventos(v, umbral=UMBRAL_EVENTO_MV):
    idx = np.where(np.abs(v[sk:]) > umbral)[0] + sk
    if len(idx) == 0:
        return np.array([], dtype=int)
    grupos = np.split(idx, np.where(np.diff(idx) > int(SEPARACION_EVENTO_S * fs))[0] + 1)
    c = np.array([g[0] - 8 + np.argmin(v[g[0] - 8:g[-1] + 9]) for g in grupos])
    return c[(c >= na) & (c + nd <= len(v))]


def gotas_distintas(x):
    todos = np.sort(np.concatenate([centros_de_eventos(x[:, 0]), centros_de_eventos(x[:, 1])]))
    unicos = []
    for c in todos:
        if not unicos or c - unicos[-1] > int(0.05 * fs):
            unicos.append(c)
    return np.array(unicos)


def pico_a_pico(x, centros):
    return np.array([[x[c - 8:c + 12, ch].max() - x[c - 8:c + 12, ch].min() for ch in range(2)] for c in centros])


GOTAS = {n: gotas_distintas(REGISTROS[n]) for n in CON_GOTEO}
PP = {n: pico_a_pico(REGISTROS[n], GOTAS[n]) for n in CON_GOTEO}
for n in CON_GOTEO:
    pp, g = PP[n], GOTAS[n]
    sep = np.diff(g) / fs
    print(f"{n:7s}: {len(g):2d} gotas distintas, separación media {sep.mean():.2f} s (mín {sep.min():.2f}, máx {sep.max():.2f}) | pico a pico: "
          f"canal 1 mediana {np.median(pp[:, 0]):.0f}, canal 2 mediana {np.median(pp[:, 1]):.0f} mV; mejor canal: mín {pp.max(axis=1).min():.0f}, "
          f"mediana {np.median(pp.max(axis=1)):.0f} | <250 mV: canal 1 = {(pp[:, 0] < 250).sum()}, canal 2 = {(pp[:, 1] < 250).sum()}, ambos = {((pp < 250).all(axis=1)).sum()}")
for n in SIN_GOTEO:
    print(f"{n:9s}: eventos de más de {UMBRAL_EVENTO_MV:.0f} mV = {len(gotas_distintas(REGISTROS[n]))} (debe ser 0)")

COLOR_REG = {"fijo": "tab:green", "movido": "black", "val": "tab:purple"}
fig, axs = plt.subplots(1, 2, figsize=(16, 4.8))
for n in CON_GOTEO:
    axs[1].plot(PP[n][:, 1], PP[n][:, 0], "o", color=COLOR_REG[n], markersize=4, label=TITULOS[n])
for ch in range(2):
    axs[0].plot(GOTAS["movido"] / fs, PP["movido"][:, ch], "o-", color=COLOR_CAN[ch], linewidth=0.5, markersize=4, label=f"Canal {ch + 1}")
axs[0].set_xlabel("Tiempo (s)")
axs[0].set_ylabel("Pico a pico (mV)")
axs[0].set_title("Dispositivo movido: amplitud de cada gota en cada canal")
axs[0].legend()
axs[0].grid(True, alpha=0.3)
axs[1].set_xlabel("Canal 2: pico a pico (mV)")
axs[1].set_ylabel("Canal 1: pico a pico (mV)")
axs[1].set_title("Cada punto = una gota. Fijo: parejas; movido: una débil en un canal es fuerte en el otro")
axs[1].legend(fontsize=8)
axs[1].grid(True, alpha=0.3)
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 3. Las plantillas
#
# Una plantilla por canal (cada fotodiodo ve la gota algo distinta). Cada una es el **promedio de
# las gotas de más de `PP_MIN_PLANTILLA_MV` en ese canal**, de los registros con el dispositivo
# fijo y movido, alineadas en el valle. Antes de promediar, cada gota se normaliza a energía 1,
# para que las más grandes no dominen. Después la plantilla se normaliza a energía 1, que es como
# la usa el detector.
#
# La validación no entra: sirve para probar las plantillas en gotas que no las armaron.

# %%


def unitaria(v):
    return v / np.linalg.norm(v)


def segmentos_para_plantilla(nombre, ch):
    x, segs = REGISTROS[nombre], []
    for c, p in zip(GOTAS[nombre], PP[nombre][:, ch]):
        if p > PP_MIN_PLANTILLA_MV:
            c = c - 8 + np.argmin(x[c - 8:c + 12, ch])
            if c >= na and c + nd <= len(x):
                segs.append(x[c - na:c + nd, ch])
    return segs


plantillas, t_ms = [], np.arange(-na, nd) / fs * 1000
fig, axs = plt.subplots(1, 3, figsize=(17, 4.8))
for ch in range(2):
    s_fijo, s_mov = segmentos_para_plantilla("fijo", ch), segmentos_para_plantilla("movido", ch)
    p = unitaria(np.mean([unitaria(s) for s in s_fijo + s_mov], axis=0))
    plantillas.append(p)
    for s in s_fijo + s_mov:
        axs[ch].plot(t_ms, unitaria(s), color="tab:gray", linewidth=0.5, alpha=0.25)
    axs[ch].plot(t_ms, p, color="black", linewidth=2.6, label=f"Plantilla: promedio de {len(s_fijo) + len(s_mov)} gotas")
    axs[ch].axhline(0, color="black", linewidth=0.6)
    axs[ch].set_title(f"Canal {ch + 1}")
    axs[ch].set_xlabel("Tiempo relativo al valle (ms)")
    axs[ch].set_ylabel("Amplitud normalizada")
    axs[ch].legend()
    axs[ch].grid(True, alpha=0.3)
    p_fijo, p_mov = unitaria(np.mean([unitaria(s) for s in s_fijo], axis=0)), unitaria(np.mean([unitaria(s) for s in s_mov], axis=0))
    parecido = [unitaria(s) @ p for s in s_fijo + s_mov]
    print(f"Canal {ch + 1}: plantilla con {len(s_fijo)} gotas del registro fijo y {len(s_mov)} del movido; parecido de cada gota con ella (coseno): "
          f"media = {np.mean(parecido):.2f}, mín = {np.min(parecido):.2f}")
    print(f"   plantilla solo con el registro fijo contra solo con el movido: coseno = {p_fijo @ p_mov:.2f} (1 = misma forma)")
axs[2].plot(t_ms, plantillas[0], color=COLOR_CAN[0], label="Canal 1")
axs[2].plot(t_ms, plantillas[1], color=COLOR_CAN[1], label="Canal 2")
axs[2].axhline(0, color="black", linewidth=0.6)
axs[2].set_title("Las dos plantillas")
axs[2].set_xlabel("Tiempo relativo al valle (ms)")
axs[2].legend()
axs[2].grid(True, alpha=0.3)
fig.tight_layout()
plt.show()
print(f"Parecido (coseno) entre la plantilla del canal 1 y la del canal 2: {plantillas[0] @ plantillas[1]:.2f}")
print(f"Con {SAMPLE_PERIOD_US / 1000:.1f} ms entre muestras, la gota queda descripta con pocas muestras: la forma cambia de gota a gota según "
      f"en qué instante cae cada muestra.")

# %% [markdown]
# ## 4. La correlación con la plantilla
#
# `corr[k]` es la plantilla (energía 1) multiplicada muestra a muestra por la señal en
# `x[k : k+L]`, sumada. Es grande si la señal tiene la forma de la plantilla **y** gran amplitud
# (queda en mV). Donde hay una gota hace un pico; con solo ruido queda chica.

# %%


def correlacion(x, ch):
    return np.correlate(x[:, ch], plantillas[ch], mode="valid")


def mascara_ruido(n, centros, mitad_s=0.15):
    """Muestras de 'solo ruido': sin el arranque del filtro y a más de mitad_s de cualquier gota."""
    m = np.ones(n, bool)
    m[:sk] = False
    for c in centros:
        m[max(0, c - int(mitad_s * fs)):c + int(mitad_s * fs)] = False
    return m


def correlacion_de_ruido(corr, centros, n):
    m = mascara_ruido(n, centros)
    return corr[np.array([m[k:k + L].all() for k in range(len(corr))])]


def pico_por_gota(corr, centros):
    return np.array([corr[max(0, c - na - 6):c - na + 7].max() for c in centros])


CORR = {n: [correlacion(REGISTROS[n], ch) for ch in range(2)] for n in REGISTROS}
fig, axs = plt.subplots(len(REGISTROS), 2, figsize=(16, 16))
for fila, n in enumerate(REGISTROS):
    for ch in range(2):
        c = CORR[n][ch]
        axs[fila, ch].plot((np.arange(len(c)) + na) / fs, c, linewidth=0.5, color=COLOR_SIN if n in SIN_GOTEO else COLOR_CAN[ch])
        axs[fila, ch].set_title(f"Correlación - {TITULOS[n]} - canal {ch + 1}")
        axs[fila, ch].set_ylabel("Coincidencia (mV)")
        axs[fila, ch].grid(True, alpha=0.3)
axs[-1, 0].set_xlabel("Tiempo (s)")
axs[-1, 1].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 5. Los umbrales
#
# El umbral de cada canal tiene que quedar **por encima del ruido más grande** (si no, el ruido
# cuenta como gota) y lo más abajo posible para no perder gotas.
#
# Se mide el **ruido máximo de la correlación** en cada canal en todos los lugares donde no hay gota:
# los dos registros sin goteo y los tramos sin gota de los tres registros con goteo.
#
# **Cómo se elige el valor.** Con un solo canal, el umbral iría en el punto medio entre el ruido y
# la gota más débil. Acá el firmware detecta si **cualquiera** de los dos canales pasa su umbral,
# así que perder una gota débil en un canal no importa si el otro la ve. Entonces se prioriza
# **no tener falsas detecciones** (en pruebas anteriores, otra computadora en el mismo enchufe
# metió ruido que disparó el detector): se usa `FACTOR_RUIDO` veces el ruido máximo de cada canal.
# Se comprueba más abajo que, con ese umbral, se siguen viendo todas las gotas de los registros
# con el dispositivo fijo y de la validación.

# %%
ruido_max, detalle = [], {}
for ch in range(2):
    valores = {n: np.abs(CORR[n][ch][sk:]).max() for n in SIN_GOTEO}
    for n in CON_GOTEO:
        valores[n] = np.abs(correlacion_de_ruido(CORR[n][ch], GOTAS[n], len(REGISTROS[n]))).max()
    ruido_max.append(max(valores.values()))
    detalle[ch] = valores
    print(f"Canal {ch + 1}: ruido máximo de la correlación: " + ", ".join(f"{k} = {v:.0f}" for k, v in valores.items()) + f"  ->  {ruido_max[ch]:.0f}")

UMBRALES = [float(np.round(FACTOR_RUIDO * r / 5) * 5) for r in ruido_max]
print(f"\nUmbrales ({FACTOR_RUIDO:g} veces el ruido máximo): canal 1 = {UMBRALES[0]:.0f}, canal 2 = {UMBRALES[1]:.0f}")

PICOS = {n: np.column_stack([pico_por_gota(CORR[n][ch], GOTAS[n]) for ch in range(2)]) for n in CON_GOTEO}
for n in CON_GOTEO:
    p = PICOS[n]
    print(f"{n:7s}: pico de la correlación en cada gota - canal 1: mediana {np.median(p[:, 0]):.0f}, mín {p[:, 0].min():.0f} | "
          f"canal 2: mediana {np.median(p[:, 1]):.0f}, mín {p[:, 1].min():.0f} | del mejor canal: mín {p.max(axis=1).min():.0f}")

fig, axs = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
for ch in range(2):
    ax = axs[ch]
    for k, n in enumerate(CON_GOTEO):
        ax.plot(PICOS[n][:, ch], np.full(len(PICOS[n]), 0.2 + 0.3 * k), "|", color=COLOR_REG[n], markersize=16, label=TITULOS[n])
    ax.axvline(ruido_max[ch], color="gray", linestyle=":", linewidth=1.8, label=f"ruido máximo ({ruido_max[ch]:.0f})")
    ax.axvline(UMBRALES[ch], color="red", linewidth=2.5, label=f"umbral ({UMBRALES[ch]:.0f})")
    ax.set_xscale("symlog", linthresh=10)
    ax.set_yticks([])
    ax.set_ylim(0, 1)
    ax.set_title(f"Canal {ch + 1}: pico de la correlación de cada gota contra el ruido máximo y el umbral")
    ax.legend(loc="upper left", fontsize=8, ncol=2)
    ax.grid(True, alpha=0.3, which="both")
axs[1].set_xlabel("Valor de la correlación (mV)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 6. Emulación del detector del firmware
#
# Se reproduce `XCorrDetectorProcess` muestra a muestra en cada canal: al pasar el umbral espera
# `PEAK_WINDOW_MS` quedándose con el máximo y confirma la gota; luego ignora la señal
# `REFRACTORY_MS`. Las detecciones de los dos canales se unen (una gota vista por los dos
# canales cuenta una vez), como hace el firmware con el LED. Se cuenta cuántas gotas distintas
# detecta en cada registro (sin contar el primer segundo, el arranque del filtro), con distintos
# umbrales (el mismo valor en los dos canales, y al final los elegidos).

# %%


def emular_canal(x, ch, umbral, ventana_ms=PEAK_WINDOW_MS, refractario_ms=REFRACTORY_MS):
    c = correlacion(x, ch)
    ventana, refractario = int(ventana_ms / 1000 * fs), int(refractario_ms / 1000 * fs)
    detecciones, buscando, resto, refrac = [], False, 0, 0
    for k, v in enumerate(c):
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
    return detecciones


def emular(x, umbrales):
    todas = sorted(emular_canal(x, 0, umbrales[0]) + emular_canal(x, 1, umbrales[1]))
    unicas = []
    for t in todas:
        if not unicas or t - unicas[-1] > REFRACTORY_MS / 1000:
            unicas.append(t)
    return [t for t in unicas if t >= T_TRANSITORIO_S]  # el arranque del filtro no cuenta (igual que en las gotas de referencia)


def falsas(umbrales):
    return sum(len(emular(REGISTROS[n], umbrales)) for n in SIN_GOTEO)


barrido = [60, 100, 160, 200, 300, 400]
reales = {n: len(GOTAS[n]) for n in CON_GOTEO}
print(f"{'umbral':>8s} | " + " | ".join(f"{n + f' (de {reales[n]})':>16s}" for n in CON_GOTEO) + " | falsas sin goteo (2 registros)")
for u in barrido:
    cuentas = [len(emular(REGISTROS[n], (u, u))) for n in CON_GOTEO]
    print(f"{u:8.0f} | " + " | ".join(f"{c:16d}" for c in cuentas) + f" | {falsas((u, u))}")
cuentas = [len(emular(REGISTROS[n], UMBRALES)) for n in CON_GOTEO]
print(f"{'elegidos':>8s} | " + " | ".join(f"{c:16d}" for c in cuentas) + f" | {falsas(UMBRALES)}"
      f"   (canal 1 = {UMBRALES[0]:.0f}, canal 2 = {UMBRALES[1]:.0f})")

# ventana de búsqueda: cuánto pasa entre cruzar el umbral y llegar al máximo, en gotas fuertes
adelantos = []
for n in CON_GOTEO:
    for ch in range(2):
        c = CORR[n][ch]
        for g, p in zip(GOTAS[n], PICOS[n][:, ch]):
            tramo = c[max(0, g - na - 45):g - na + 30]
            if p > 3 * UMBRALES[ch] and len(tramo):
                cruces = np.where(tramo > UMBRALES[ch])[0]
                if len(cruces):
                    adelantos.append((np.argmax(tramo) - cruces[0]) / fs * 1000)
print(f"\nTiempo entre cruzar el umbral y llegar al máximo (gotas fuertes): mediana = {np.median(adelantos):.0f} ms, máx = {np.max(adelantos):.0f} ms "
      f"-> ventana de búsqueda elegida: {PEAK_WINDOW_MS} ms")

fig, axs = plt.subplots(len(REGISTROS), 2, figsize=(16, 16))
for fila, n in enumerate(REGISTROS):
    x = REGISTROS[n]
    det_canales = [emular_canal(x, ch, UMBRALES[ch]) for ch in range(2)]
    for ch in range(2):
        ax = axs[fila, ch]
        ax.plot(np.arange(len(x)) / fs, x[:, ch], linewidth=0.4, color=COLOR_SIN if n in SIN_GOTEO else COLOR_CAN[ch])
        if det_canales[ch]:
            ax.plot(det_canales[ch], np.full(len(det_canales[ch]), x[sk:, ch].max() * 1.05), "rv", markersize=5)
        ax.set_title(f"{TITULOS[n]} - canal {ch + 1}: {len(det_canales[ch])} detecciones (umbral {UMBRALES[ch]:.0f})")
        ax.set_ylabel("mV")
        ax.grid(True, alpha=0.3)
axs[-1, 0].set_xlabel("Tiempo (s)")
axs[-1, 1].set_xlabel("Tiempo (s)")
fig.tight_layout()
plt.show()

# %% [markdown]
# ## 7. Exportar al firmware
#
# Pegar el contenido en `pruebabpw34/main/drop_template.h` (las dos plantillas, con el mismo
# largo) y los umbrales en `XCORR_THRESHOLD_CH1` / `XCORR_THRESHOLD_CH2`. Valen para
# `SAMPLE_PERIOD_US = 1200`, el pasabanda 20-260 Hz ORDER_4 y el antialias de 4k7 + 100 nF.

# %%
print(f"#define DROP_TEMPLATE_LEN {L}\n")
for ch in range(2):
    print(f"/* BPW34 n°{ch + 1} (GPIO_{2 + ch}) */")
    print(f"static const float DROP_TEMPLATE_CH{ch + 1}[DROP_TEMPLATE_LEN] = {{")
    for i in range(0, L, 6):
        print("    " + ", ".join(f"{v:.5f}f" for v in plantillas[ch][i:i + 6]) + ",")
    print("};\n")
print(f"XCORR_THRESHOLD_CH1  {UMBRALES[0]:.1f}f")
print(f"XCORR_THRESHOLD_CH2  {UMBRALES[1]:.1f}f")
print(f"XCORR_PEAK_WINDOW_MS {PEAK_WINDOW_MS}")
print(f"XCORR_REFRACTORY_MS  {REFRACTORY_MS}")
