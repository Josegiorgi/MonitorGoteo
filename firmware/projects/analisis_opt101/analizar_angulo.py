# %% [markdown]
# # Ángulo de pérdida de detección - OPT101 + MPU-6000
#
# Este script analiza las tomas hechas con `MODE_ANGLE` de `prueba_opt101` (el firmware transmite
# `deteccion,0,0,pitch`; el análisis de `pruebabpw34` es `../analisis_bpw34/analizar_angulo_bpw34.py`, con el
# mismo método): ve a qué ángulo de pitch el detector de gotas **deja de detectar** y propone
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

SAMPLE_PERIOD_US = 700.0     # SAMPLE_PERIOD_US de prueba_opt101 (el de pruebabpw34 es 1200)
PLOT_DECIMATION = 14         # PLOT_DECIMATION de prueba_opt101 (muestras por línea del CSV; el de pruebabpw34 es 8)
COL_DETECCION = 0            # columna 1 del CSV
COL_PITCH = 3                # columna 4 del CSV

VENTANA_MEDIANA = 5          # líneas del filtro de mediana del pitch (5 líneas ~ 50 ms): saca los picos aislados
FACTOR_CORTE = 2.5           # un "corte" es un intervalo entre gotas mayor a FACTOR_CORTE x el período típico
PITCH_MIN_CORTE_ANGULO = 3.0 # un corte cuenta como "por ángulo" del lado positivo si el pitch llega al menos a esto durante el corte
PITCH_MIN_CORTE_NEGATIVO = 2.0  # y del lado negativo si llega al menos a -esto. Los dos valores salen de mirar los gráficos del BPW34
                             # (el lado negativo empieza a fallar antes que el positivo). Con el OPT101 todavía no hay tomas con pitch negativo
PITCH_ARRANQUE_MIN = 1.0     # además, la última gota antes del corte tiene que estar ya inclinada al menos esto de ese lado
                             # (descarta cortes con el pitch cerca de 0°: ahí la causa es otra, ej. el arranque del goteo)
SUBIDA_MIN_GRADOS = 0.2      # y el pitch tiene que irse al menos esto más allá del de la última gota
MARGEN_AVISO_GRADOS = 0.5    # el aviso se pone esto antes (del lado del reposo) del ángulo de pérdida más cercano al reposo
HISTERESIS_GRADOS = 0.5      # el aviso se apaga esto por debajo del ángulo en que se enciende
EXCLUIR_CORTES = {}          # cortes que no son por ángulo (ej. cortaste el flujo), por toma: {"Toma 1": [4]} (número de la tabla)
INTERVALOS_PITCH = [-5.0, -4.0, -3.0, -2.5, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 14.0]   # rangos para la tasa de detección (incluye pitch negativo por si hay tomas hacia ese lado)

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

if not tomas:
    raise SystemExit("No se cargó ninguna toma: revisar CARPETA y los nombres en TOMAS.")

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
    idx, t, p = d["gotas_idx"], d["t"], d["pitch"]
    limite = FACTOR_CORTE * d["periodo"]
    pares = [(a, b) for a, b in zip(idx[:-1], idx[1:]) if t[b] - t[a] > limite]
    # Una gota suelta dentro de una zona perdida parte el corte en dos: si dos cortes quedan pegados
    # (el final de uno es el principio del siguiente), se unen en uno solo.
    unidos = []
    for a, b in pares:
        if unidos and unidos[-1][1] == a:
            unidos[-1] = (unidos[-1][0], b)
        else:
            unidos.append((a, b))

    cortes = []
    for n, (a, b) in enumerate(unidos, start=1):
        tramo = p[a:b + 1]
        sube = (tramo.max() >= PITCH_MIN_CORTE_ANGULO and p[a] >= PITCH_ARRANQUE_MIN
                and tramo.max() - p[a] >= SUBIDA_MIN_GRADOS)
        baja = (tramo.min() <= -PITCH_MIN_CORTE_NEGATIVO and p[a] <= -PITCH_ARRANQUE_MIN
                and p[a] - tramo.min() >= SUBIDA_MIN_GRADOS)
        lado = "+" if sube and not baja else "-" if baja and not sube else ""
        excluido = n in EXCLUIR_CORTES.get(nombre, [])
        cortes.append({
            "n": n, "t0": t[a], "t1": t[b], "dur": t[b] - t[a],
            "p_ultima": p[a], "p_primera": p[b],
            "p_min": tramo.min(), "p_max": tramo.max(),
            "lado": lado, "por_angulo": bool(lado) and not excluido,
        })
    d["cortes"] = cortes

    print(f"\n{nombre}: cortes (intervalo > {FACTOR_CORTE} x {d['periodo']:.2f} s = {FACTOR_CORTE * d['periodo']:.1f} s)")
    print(" n   desde-hasta [s]   dur[s]   pitch última gota   pitch en el corte   pitch primera después   por ángulo")
    for c in cortes:
        marca = f"sí ({c['lado']})" if c["por_angulo"] else "no"
        print(f"{c['n']:2d}  {c['t0']:6.1f}-{c['t1']:6.1f}   {c['dur']:6.1f}   {c['p_ultima']:12.1f}°    "
              f"{c['p_min']:5.1f}° ..{c['p_max']:5.1f}°     {c['p_primera']:12.1f}°          {marca}")


# %%
# ------------------------------------------------------------------------------------------
# 4. Tasa de detección según el pitch
# ------------------------------------------------------------------------------------------
# Tasa = gotas detectadas / gotas esperadas en ese rango de pitch. Las esperadas salen del tiempo
# que la toma pasó en el rango, entre la primera y la última gota, dividido por el período típico.
# Las esperadas suponen goteo regular al período típico, así que en rangos con pocas gotas la
# cuenta puede dar un poco más de lo esperado: por eso el porcentaje se limita a 100%. Lo importante
# es dónde cae la tasa a casi 0.
def porcentaje(detectadas, esperadas):
    """Gotas detectadas como % de las esperadas, con tope en 100% (nan si el rango no tuvo tiempo)."""
    return min(100.0 * detectadas / esperadas, 100.0) if esperadas > 0.5 else float("nan")


def goteo_regular(d):
    """Primera y última muestra del goteo regular: desde la primera gota que tiene otras dos seguidas al
    ritmo normal hasta la última que tiene otras dos antes. Así el arranque y el final del goteo (gotas
    sueltas, o nada) no cuentan como "gotas esperadas que no se detectaron"."""
    idx, t = d["gotas_idx"], d["t"]
    limite = FACTOR_CORTE * d["periodo"]
    ok = np.diff(t[idx]) <= limite            # ok[k]: la gota k+1 llega a tiempo después de la k
    lo = next((idx[k] for k in range(len(ok) - 1) if ok[k] and ok[k + 1]), idx[0])
    hi = next((idx[k + 2] for k in range(len(ok) - 2, -1, -1) if ok[k] and ok[k + 1]), idx[-1])
    return lo, hi


def tasa_por_pitch(d):
    lo, hi = goteo_regular(d)
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
        tasa = porcentaje(det, esp)
        print(f"   pitch {p0:5.1f}° a {p1:5.1f}°   {tiempo:5.1f} s   detectadas {det:3d}   esperadas {esp:5.1f}   -> {tasa:4.0f}%")

print("\nTodas las tomas juntas:")
for (p0, p1), (det, esp) in zip(zip(INTERVALOS_PITCH[:-1], INTERVALOS_PITCH[1:]), acumulado):
    tasa = porcentaje(det, esp)
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
# Ángulo de pérdida de un corte "por ángulo": pitch de la última gota detectada antes del corte (el
# pitch se aleja del reposo y deja de detectar). Ángulo de recuperación: pitch de la primera gota después.
# Se analiza cada lado por separado: el positivo y el negativo no pierden a la misma inclinación.
avisos = {}   # lado -> (aviso conservador, aviso típico), para el gráfico
for lado, nombre_lado in (("+", "POSITIVO"), ("-", "NEGATIVO")):
    eventos = [(nombre, c) for nombre, d in tomas.items() for c in d["cortes"] if c["por_angulo"] and c["lado"] == lado]
    print(f"\n=== Lado {nombre_lado} ===")
    if not eventos:
        print("  Sin cortes por ángulo de este lado en las tomas cargadas.")
        continue
    print("  toma     corte   desde [s]   pérdida   recuperación")
    for nombre, c in eventos:
        print(f"  {nombre:7s}  {c['n']:4d}   {c['t0']:8.1f}   {c['p_ultima']:6.1f}°   {c['p_primera']:8.1f}°")
    perdidas = np.array([c["p_ultima"] for _, c in eventos])
    recuperaciones = np.array([c["p_primera"] for _, c in eventos])
    print(f"  {len(eventos)} cortes: pérdida mediana {np.median(perdidas):.1f}° (de {perdidas.min():.1f}° a {perdidas.max():.1f}°), "
          f"recuperación mediana {np.median(recuperaciones):.1f}° (de {recuperaciones.min():.1f}° a {recuperaciones.max():.1f}°)")

    # El aviso se pone antes de la pérdida, del lado del reposo: en el positivo es el ángulo menos el margen, en el negativo es el ángulo más el margen.
    signo = 1 if lado == "+" else -1
    cercano_al_reposo = perdidas.min() if lado == "+" else perdidas.max()   # el ángulo de pérdida menos inclinado de los vistos
    on_cons = cercano_al_reposo - signo * MARGEN_AVISO_GRADOS
    on_tip = np.median(perdidas) - signo * MARGEN_AVISO_GRADOS
    avisos[lado] = (on_cons, on_tip)
    print(f"  aviso conservador: enciende en pitch {'>=' if lado == '+' else '<='} {on_cons:.1f}° "
          f"(pérdida menos inclinada {cercano_al_reposo:.1f}°, margen {MARGEN_AVISO_GRADOS}°), apaga {on_cons - signo * HISTERESIS_GRADOS:.1f}°")
    print(f"  aviso típico     : enciende en pitch {'>=' if lado == '+' else '<='} {on_tip:.1f}° "
          f"(mediana {np.median(perdidas):.1f}°, margen {MARGEN_AVISO_GRADOS}°), apaga {on_tip - signo * HISTERESIS_GRADOS:.1f}°")

print("\nPitch en reposo (al principio y al final de cada toma; el 0° del MPU puede no ser el 0° de uso):")
for nombre, d in tomas.items():
    n = int(0.5 / DT_LINEA_S)
    print(f"  {nombre}: al inicio {np.median(d['pitch'][:n]):.1f}°, al final {np.median(d['pitch'][-n:]):.1f}°")
print("OJO: son pocos cortes por lado y las tomas no dan el mismo ángulo de pérdida (ver el resumen por lado).")


# %%
# ------------------------------------------------------------------------------------------
# 7. Tasa de detección según el pitch (gráfico)
# ------------------------------------------------------------------------------------------
centros = [(p0 + p1) / 2 for p0, p1 in zip(INTERVALOS_PITCH[:-1], INTERVALOS_PITCH[1:])]
tasas = [porcentaje(det, esp) for det, esp in acumulado]
anchos = [p1 - p0 for p0, p1 in zip(INTERVALOS_PITCH[:-1], INTERVALOS_PITCH[1:])]
plt.figure(figsize=(10, 4.5))
plt.bar(centros, tasas, width=anchos, color="tab:blue", edgecolor="white")
plt.axhline(100, color="gray", lw=0.8)
for x, tasa, (det, esp) in zip(centros, tasas, acumulado):
    if esp > 0.5:   # sobre cada barra: gotas detectadas / gotas esperadas en ese rango
        plt.text(x, (0 if np.isnan(tasa) else tasa) + 2, f"{int(det)}/{esp:.0f}", ha="center", va="bottom", fontsize=8)
plt.ylim(0, 118)
for lado, (on_cons, on_tip) in avisos.items():
    plt.axvline(on_cons, color="tab:orange", lw=2, label=f"aviso conservador {lado} ({on_cons:.1f}°)")
    plt.axvline(on_tip, color="tab:green", lw=2, ls="--", label=f"aviso típico {lado} ({on_tip:.1f}°)")
if avisos:
    plt.legend(loc="upper right", fontsize=8)
plt.xlabel("pitch [°]")
plt.ylabel("gotas detectadas [% de las esperadas, tope 100%]")
plt.title("Detección según el pitch (todas las tomas); sobre cada barra: detectadas / esperadas")
plt.grid(alpha=0.3)
plt.tight_layout()
if GUARDAR_PNG:
    plt.savefig(SALIDA / "angulo_tasa.png", dpi=120)
plt.show()
