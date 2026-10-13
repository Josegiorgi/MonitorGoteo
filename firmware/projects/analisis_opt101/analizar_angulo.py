# %% [markdown]
# # Ángulo de pérdida de detección - OPT101 + MPU-6000
#
# Este script analiza las tomas hechas con `MODE_ANGLE` de `prueba_opt101` (el firmware transmite
# `deteccion,0,0,pitch`): ve a qué ángulo de pitch el detector de gotas **deja de detectar** y propone
# el ángulo del aviso.
#
# Formato de cada toma (CSV sin encabezado, una línea cada `PLOT_DECIMATION` muestras):
#
# | Columna | Contenido |
# |---|---|
# | 1 | `500` si en esa línea se detectó una gota, `0` si no |
# | 2 y 3 | siempre `0` |
# | 4 | pitch en grados (con un decimal) |
# | 5 en adelante | (si el exportador del plotter las agrega) se ignoran |
#
# Orden del análisis:
# 1. Cargar las tomas, suavizar el pitch (los picos aislados son movimiento de la mano, no inclinación).
# 2. Gotas detectadas y ritmo del goteo de cada toma.
# 3. Cortes de detección (intervalos largos sin gotas) y el pitch en cada uno.
# 4. Tasa de detección según el pitch.
# 5. Gráfico de cada toma.
# 6. Ángulo de pérdida y ángulo de aviso sugerido.
#
# **Cómo correr esto en VS Code:** cada bloque `# %%` es una celda ("Run Cell"); los gráficos salen
# en la Interactive Window. Solo requiere `numpy` y `matplotlib`.
#
# Autor: Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)

# %%
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# ------------------------------------------------------------------------------------------
# Config
# ------------------------------------------------------------------------------------------
CARPETA = Path(r"C:\Users\Jose\OneDrive\Escritorio\Datos OPT101\Comparacion\Filtrada para plantilla")
TOMAS = {
    "Toma 1": CARPETA / "Goteo y angulo.csv",
    "Toma 2": CARPETA / "Goteo y angulo 2.csv",
    # Agregar acá las tomas nuevas: "Toma 3": CARPETA / "Goteo y angulo 3.csv",
}

SAMPLE_PERIOD_US = 700.0     # SAMPLE_PERIOD_US del firmware
PLOT_DECIMATION = 14         # PLOT_DECIMATION del firmware (muestras por línea del CSV)
COL_DETECCION = 0            # columna 1 del CSV
COL_PITCH = 3                # columna 4 del CSV

VENTANA_MEDIANA = 5          # líneas del filtro de mediana del pitch (5 líneas ~ 50 ms): saca los picos aislados
FACTOR_CORTE = 2.5           # un "corte" es un intervalo entre gotas mayor a FACTOR_CORTE x el período típico
PITCH_MIN_CORTE_ANGULO = 3.0 # un corte cuenta como "por ángulo" si el pitch llega al menos a esto durante el corte (descarta
                             # cortes con el pitch siempre bajo: ahí la causa es otra, ej. cambió el flujo). Es un criterio
                             # puesto mirando los gráficos: si cambia el montaje, revisar este valor
SUBIDA_MIN_GRADOS = 0.5      # y además tiene que subir al menos esto sobre el pitch de la última gota
MARGEN_AVISO_GRADOS = 0.5    # el aviso se pone esto por debajo del menor ángulo de pérdida
HISTERESIS_GRADOS = 0.5      # el aviso se apaga esto por debajo del ángulo en que se enciende
EXCLUIR_CORTES = {}          # cortes que no son por ángulo (ej. cortaste el flujo), por toma: {"Toma 1": [4]} (número de la tabla)
INTERVALOS_PITCH = [-1.0, 1.0, 2.0, 2.5, 3.0, 3.5, 4.0, 9.0]   # rangos para la tasa de detección

GUARDAR_PNG = True           # guarda los gráficos como PNG en esta carpeta
SALIDA = Path(__file__).resolve().parent if "__file__" in globals() else Path.cwd()

DT_LINEA_S = PLOT_DECIMATION * SAMPLE_PERIOD_US * 1e-6   # tiempo entre líneas del CSV


# %%
# ------------------------------------------------------------------------------------------
# 1. Cargar las tomas
# ------------------------------------------------------------------------------------------
def mediana_movil(x, n):
    """Mediana móvil centrada de n puntos (n impar). Los bordes se rellenan con el valor del extremo."""
    pad = n // 2
    xp = np.pad(x, pad, mode="edge")
    return np.median(np.lib.stride_tricks.sliding_window_view(xp, n), axis=1)


def cargar_toma(ruta):
    datos = np.genfromtxt(ruta, delimiter=",")
    deteccion = datos[:, COL_DETECCION] > 0
    pitch_crudo = datos[:, COL_PITCH]
    pitch = mediana_movil(pitch_crudo, VENTANA_MEDIANA)
    t = np.arange(len(datos)) * DT_LINEA_S
    return {"t": t, "deteccion": deteccion, "pitch": pitch, "pitch_crudo": pitch_crudo}


tomas = {}
for nombre, ruta in TOMAS.items():
    if not ruta.exists():
        print(f"{nombre}: no encuentro {ruta}")
        continue
    tomas[nombre] = cargar_toma(ruta)
    d = tomas[nombre]
    print(f"{nombre}: {len(d['t'])} líneas, {d['t'][-1]:.1f} s, "
          f"pitch entre {d['pitch'].min():.1f}° y {d['pitch'].max():.1f}° (suavizado)")

# El eje de tiempo asume que cada línea del CSV son PLOT_DECIMATION muestras. Si el goteo da un
# período razonable en el punto 2, esa suposición es coherente; si no, revisar PLOT_DECIMATION.


# %%
# ------------------------------------------------------------------------------------------
# 2. Gotas detectadas y ritmo del goteo
# ------------------------------------------------------------------------------------------
for nombre, d in tomas.items():
    idx = np.where(d["deteccion"])[0]
    d["gotas_idx"] = idx
    d["gotas_t"] = d["t"][idx]
    intervalos = np.diff(d["gotas_t"])
    d["periodo"] = float(np.median(intervalos))   # período típico entre gotas, s
    print(f"{nombre}: {len(idx)} gotas detectadas, período típico {d['periodo']:.2f} s "
          f"({1 / d['periodo']:.1f} gotas/s)")


# %%
# ------------------------------------------------------------------------------------------
# 3. Cortes de detección y pitch en cada uno
# ------------------------------------------------------------------------------------------
for nombre, d in tomas.items():
    cortes = []
    idx, t, p = d["gotas_idx"], d["t"], d["pitch"]
    n = 0
    for a, b in zip(idx[:-1], idx[1:]):
        if t[b] - t[a] > FACTOR_CORTE * d["periodo"]:
            n += 1
            tramo = p[a:b + 1]
            sube = tramo.max() >= PITCH_MIN_CORTE_ANGULO and tramo.max() - p[a] >= SUBIDA_MIN_GRADOS
            excluido = n in EXCLUIR_CORTES.get(nombre, [])
            cortes.append({
                "n": n, "t0": t[a], "t1": t[b], "dur": t[b] - t[a],
                "p_ultima": p[a], "p_primera": p[b],
                "p_min": tramo.min(), "p_max": tramo.max(),
                "por_angulo": sube and not excluido,
            })
    d["cortes"] = cortes

    print(f"\n{nombre}: cortes (intervalo > {FACTOR_CORTE} x {d['periodo']:.2f} s = {FACTOR_CORTE * d['periodo']:.1f} s)")
    print(" n   desde-hasta [s]   dur[s]   pitch última gota   pitch en el corte   pitch primera después   por ángulo")
    for c in cortes:
        print(f"{c['n']:2d}  {c['t0']:6.1f}-{c['t1']:6.1f}   {c['dur']:6.1f}   {c['p_ultima']:12.1f}°    "
              f"{c['p_min']:5.1f}° ..{c['p_max']:5.1f}°     {c['p_primera']:12.1f}°          "
              f"{'sí' if c['por_angulo'] else 'no'}")


# %%
# ------------------------------------------------------------------------------------------
# 4. Tasa de detección según el pitch
# ------------------------------------------------------------------------------------------
# Tasa = gotas detectadas / gotas esperadas en ese rango de pitch. Las esperadas salen del tiempo
# que la toma pasó en el rango, entre la primera y la última gota, dividido por el período típico.
# Más de 100% o menos de 100% con el pitch bajo es ruido de contar pocas gotas: lo importante es
# dónde cae la tasa a casi 0.
def tasa_por_pitch(d):
    lo, hi = d["gotas_idx"][0], d["gotas_idx"][-1]
    filas = []
    for p0, p1 in zip(INTERVALOS_PITCH[:-1], INTERVALOS_PITCH[1:]):
        en_rango = np.zeros(len(d["pitch"]), bool)
        en_rango[lo:hi + 1] = True
        en_rango &= (d["pitch"] >= p0) & (d["pitch"] < p1)
        tiempo = en_rango.sum() * DT_LINEA_S
        detectadas = int((d["deteccion"] & en_rango).sum())
        filas.append((p0, p1, tiempo, detectadas, tiempo / d["periodo"]))
    return filas


acumulado = np.zeros((len(INTERVALOS_PITCH) - 1, 2))   # [detectadas, esperadas] sumadas entre tomas
for nombre, d in tomas.items():
    filas = tasa_por_pitch(d)
    d["tasa"] = filas
    print(f"\n{nombre}: tasa de detección según el pitch")
    for i, (p0, p1, tiempo, det, esp) in enumerate(filas):
        acumulado[i] += (det, esp)
        tasa = 100 * det / esp if esp > 0 else float("nan")
        print(f"   pitch {p0:5.1f}° a {p1:5.1f}°   {tiempo:5.1f} s   detectadas {det:3d}   esperadas {esp:5.1f}   -> {tasa:4.0f}%")

print("\nTodas las tomas juntas:")
for (p0, p1), (det, esp) in zip(zip(INTERVALOS_PITCH[:-1], INTERVALOS_PITCH[1:]), acumulado):
    tasa = 100 * det / esp if esp > 0 else float("nan")
    print(f"   pitch {p0:5.1f}° a {p1:5.1f}°   detectadas {int(det):3d}   esperadas {esp:5.1f}   -> {tasa:4.0f}%")


# %%
# ------------------------------------------------------------------------------------------
# 5. Gráfico de cada toma
# ------------------------------------------------------------------------------------------
fig, ejes = plt.subplots(2, len(tomas), figsize=(6.5 * len(tomas), 6), sharey="row",
                         gridspec_kw={"height_ratios": [3, 1]}, squeeze=False)
for col, (nombre, d) in enumerate(tomas.items()):
    ax_p, ax_g = ejes[0, col], ejes[1, col]
    ax_p.plot(d["t"], d["pitch_crudo"], lw=0.6, color="lightgray", label="pitch (crudo)")
    ax_p.plot(d["t"], d["pitch"], lw=1.2, color="tab:blue", label="pitch (mediana)")
    for c in d["cortes"]:
        ax_p.axvspan(c["t0"], c["t1"], color="tab:red" if c["por_angulo"] else "tab:gray", alpha=0.15)
    ax_p.set_title(f"{nombre}  (rojo: corte por ángulo, gris: otro corte)")
    ax_p.set_ylabel("pitch [°]")
    ax_p.set_ylim(max(-3, d["pitch"].min() - 1), d["pitch"].max() + 1)
    ax_p.grid(alpha=0.3)
    ax_p.legend(loc="upper right", fontsize=8)
    ax_g.vlines(d["gotas_t"], 0, 1, color="tab:red", lw=1)
    ax_g.set_yticks([])
    ax_g.set_xlabel("tiempo [s]")
    ax_g.set_ylabel("gotas")
    ax_g.grid(alpha=0.3)
plt.tight_layout()
if GUARDAR_PNG:
    plt.savefig(SALIDA / "angulo_tomas.png", dpi=120)
plt.show()


# %%
# ------------------------------------------------------------------------------------------
# 6. Ángulo de pérdida y ángulo de aviso sugerido
# ------------------------------------------------------------------------------------------
# Ángulo de pérdida de un corte "por ángulo": pitch de la última gota detectada antes del corte
# (el pitch sube y deja de detectar). Ángulo de recuperación: pitch de la primera gota después
# (el pitch baja y vuelve a detectar).
perdidas, recuperaciones = [], []
for nombre, d in tomas.items():
    for c in d["cortes"]:
        if c["por_angulo"]:
            perdidas.append(c["p_ultima"])
            recuperaciones.append(c["p_primera"])

if not perdidas:
    print("No hay cortes por ángulo en las tomas cargadas.")
else:
    perdidas = np.array(perdidas)
    recuperaciones = np.array(recuperaciones)
    print(f"Cortes por ángulo usados: {len(perdidas)}")
    print(f"  ángulo de pérdida      : {np.round(np.sort(perdidas), 1)}   "
          f"mediana {np.median(perdidas):.1f}°, mín {perdidas.min():.1f}°, máx {perdidas.max():.1f}°")
    print(f"  ángulo de recuperación : {np.round(np.sort(recuperaciones), 1)}   "
          f"mediana {np.median(recuperaciones):.1f}°, mín {recuperaciones.min():.1f}°, máx {recuperaciones.max():.1f}°")

    aviso_on = perdidas.min() - MARGEN_AVISO_GRADOS
    aviso_off = aviso_on - HISTERESIS_GRADOS
    aviso_tipico = np.median(perdidas) - MARGEN_AVISO_GRADOS
    print("\nÁngulo de aviso sugerido (pitch absoluto, el que mide el MPU):")
    print(f"  conservador : enciende a pitch >= {aviso_on:.1f}° (menor ángulo de pérdida {perdidas.min():.1f}° - margen {MARGEN_AVISO_GRADOS}°), "
          f"apaga a pitch < {aviso_off:.1f}° (histéresis {HISTERESIS_GRADOS}°)")
    print(f"  típico      : enciende a pitch >= {aviso_tipico:.1f}° (mediana {np.median(perdidas):.1f}° - margen {MARGEN_AVISO_GRADOS}°), "
          f"apaga a pitch < {aviso_tipico - HISTERESIS_GRADOS:.1f}°")
    print("  El conservador avisa antes en todos los cortes vistos, pero queda cerca del pitch de reposo (ver abajo): puede dar falsas alarmas.")

    # Referencia: el pitch en reposo al principio y al final de cada toma. Si el dispositivo no está
    # perfectamente nivelado, el 0° del MPU no es el 0° de uso.
    for nombre, d in tomas.items():
        n = int(0.5 / DT_LINEA_S)
        print(f"  {nombre}: pitch en reposo al inicio {np.median(d['pitch'][:n]):.1f}°, al final {np.median(d['pitch'][-n:]):.1f}°")
    print("  Si el 0° de uso no es el 0° del MPU, el aviso relativo es el sugerido menos el pitch de reposo.")
    print("  OJO: son pocos cortes. Conviene repetir con más tomas, y también inclinando hacia pitch negativo.")


# %%
# ------------------------------------------------------------------------------------------
# 7. Tasa de detección según el pitch (gráfico)
# ------------------------------------------------------------------------------------------
centros = [(p0 + p1) / 2 for p0, p1 in zip(INTERVALOS_PITCH[:-1], INTERVALOS_PITCH[1:])]
tasas = [100 * det / esp if esp > 0 else np.nan for det, esp in acumulado]
anchos = [p1 - p0 for p0, p1 in zip(INTERVALOS_PITCH[:-1], INTERVALOS_PITCH[1:])]
plt.figure(figsize=(7, 4))
plt.bar(centros, tasas, width=anchos, color="tab:blue", edgecolor="white")
plt.axhline(100, color="gray", lw=0.8)
if len(perdidas):
    plt.axvline(aviso_on, color="tab:orange", lw=2, label=f"aviso conservador ({aviso_on:.1f}°)")
    plt.axvline(aviso_tipico, color="tab:green", lw=2, ls="--", label=f"aviso típico ({aviso_tipico:.1f}°)")
    plt.legend()
plt.xlabel("pitch [°]")
plt.ylabel("detección [% de las gotas esperadas]")
plt.title("Tasa de detección según el pitch (todas las tomas)")
plt.grid(alpha=0.3)
plt.tight_layout()
if GUARDAR_PNG:
    plt.savefig(SALIDA / "angulo_tasa.png", dpi=120)
plt.show()
