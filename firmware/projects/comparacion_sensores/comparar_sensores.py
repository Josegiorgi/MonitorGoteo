"""Comparación simple OPT101 vs BPW34: ¿cuál detecta mejor las gotas?

Para cada registro hace lo mismo, en 5 pasos:
  1. Lee la señal ya filtrada (mV) que sacó el firmware en MODE_DETECTOR o MODE_FILTERED.
  2. La correlaciona con la plantilla de la gota del sensor (la misma que usa el firmware).
  3. Fija el umbral: UMBRAL = K_RUIDO x (desvío del ruido de la correlación).
  4. Detecta las gotas (cruza el umbral, se queda con el máximo en una ventana, período refractario).
  5. Mide: gotas detectadas, falsos positivos, relación gota/ruido y margen entre la gota más débil y el ruido.

Cómo se usa con las pruebas nuevas (ver LEEME_COMPARACION_SIMPLE.md):
  copiar los CSV a datos/ensayo_actual/ con el nombre   <sensor>_r<ronda>_<tipo>.csv
  (sensor = opt101 o bpw34, tipo = goteo o sin_goteo) y correr:  python comparar_sensores.py
Si esa carpeta está vacía se usan los registros provisorios del 08/10 (solo con goteo).
"""
from pathlib import Path
import csv
import re
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ------------------------------------------------------------------ configuración
RAIZ = Path(__file__).resolve().parent
ENSAYO = RAIZ / "datos" / "ensayo_actual"   # registros nuevos (mismo día, ambos sensores)
PREVIOS = RAIZ / "datos" / "previos"        # registros de otros días, solo para el umbral
SALIDA = RAIZ / "resultados_simple"
PROYECTOS = RAIZ.parent

SENSORES = {
    "opt101": dict(nombre="OPT101", dt_ms=0.7, canales=["OPT101"], color={"OPT101": "#D55E00"},
                   plantillas=PROYECTOS / "prueba_opt101" / "main" / "drop_template.h"),
    "bpw34": dict(nombre="BPW34", dt_ms=1.2, canales=["BPW34 c1", "BPW34 c2"],
                  color={"BPW34 c1": "#0072B2", "BPW34 c2": "#009E73"},
                  plantillas=PROYECTOS / "pruebabpw34" / "main" / "drop_template.h"),
}
SENSOR_ELEGIDO = "bpw34"      # para el bloque del umbral

# registros provisorios (08/10), se usan solo si datos/ensayo_actual/ está vacía
PROVISORIOS = [("opt101", 1, RAIZ / "datos" / "opt101.csv", None),
               ("bpw34", 1, RAIZ / "datos" / "bpw34.csv", None)]
# registros del 23/09 (otro día) para calcular el umbral del BPW34 con más datos
PREVIOS_UMBRAL = [("bpw34", "23/09", PREVIOS / "bpw34_23-09_goteo.csv", PREVIOS / "bpw34_23-09_sin_goteo.csv")]

K_RUIDO = 12.0            # umbral = K_RUIDO x desvío del ruido de la correlación (criterio de los análisis previos)
VENTANA_PICO_MS = 15.0    # tras cruzar el umbral se espera esto y se queda con el máximo (igual que el firmware)
REFRACTARIO_MS = 100.0    # después de detectar una gota se ignora este tiempo (igual que el firmware)
VENTANA_GOTA_MS = 100.0   # se busca el pico a pico de la gota en los 100 ms anteriores a la detección
K_BARRIDO = np.unique(np.r_[np.arange(2, 31, 1), np.arange(35, 201, 5)])  # para el gráfico de conteo vs umbral


# ------------------------------------------------------------------ funciones
def leer_plantillas(ruta):
    """Lee las plantillas de drop_template.h. Devuelve {1: plantilla, 2: ...}."""
    txt = ruta.read_text(encoding="utf-8")
    por_canal = re.findall(r"DROP_TEMPLATE_CH(\d)\[DROP_TEMPLATE_LEN\] = \{(.*?)\};", txt, re.S)
    if por_canal:
        return {int(n): np.array([float(v) for v in re.findall(r"(-?\d+\.\d+)f", b)]) for n, b in por_canal}
    b = re.search(r"DROP_TEMPLATE\[DROP_TEMPLATE_LEN\] = \{(.*?)\};", txt, re.S).group(1)
    return {1: np.array([float(v) for v in re.findall(r"(-?\d+\.\d+)f", b)])}


def correlacion(x, plantilla):
    """Correlación de las últimas muestras con la plantilla (normalizada a energía 1, queda en mV)."""
    u = plantilla / np.linalg.norm(plantilla)
    c = np.zeros(len(x))
    c[len(u) - 1:] = np.convolve(x, u[::-1], "valid")
    return c


def detectar(corr, umbral, ventana, refractario):
    """Misma lógica que el firmware (xcorr_detector.c): al cruzar el umbral espera `ventana` muestras,
    confirma la gota y ignora `refractario` muestras. Devuelve los índices donde se confirma cada gota."""
    det, ignorar, buscando, resta = [], 0, False, 0
    for i, c in enumerate(corr):
        if ignorar > 0:
            ignorar -= 1
            continue
        if buscando:
            resta -= 1
            if resta == 0:
                buscando = False
                ignorar = refractario
                det.append(i)
            continue
        if c > umbral:
            buscando, resta = True, ventana
    return np.array(det, dtype=int)


def ruido_entre_gotas(x, corr, L, ventana, refr, dt):
    """Sin registro sin goteo: mide el ruido en los tramos entre gotas (se saltea 100 ms antes y 50 ms
    después de cada detección). Se usa el desvío robusto (MAD) para que la cola del pulso no lo infle."""
    c = corr[L - 1:]
    s = 1.4826 * np.median(np.abs(c - np.median(c)))
    for _ in range(3):
        det = detectar(corr, K_RUIDO * s, ventana, refr)
        m = np.ones(len(corr), bool)
        m[:L - 1] = False
        for d in det:
            m[max(0, d - int(100 / dt)):d + int(50 / dt)] = False
        s = corr[m].std()
    xm = x[m]
    return s, 1.4826 * np.median(np.abs(xm - np.median(xm))), corr[m].max()


def analizar(canal, x_con, x_sin, plantilla, dt):
    """Analiza un canal de un registro. x_sin puede ser None (entonces el ruido se mide entre gotas)."""
    L = len(plantilla)
    ventana, refr = int(VENTANA_PICO_MS // dt), int(REFRACTARIO_MS // dt)
    corr = correlacion(x_con, plantilla)
    if x_sin is not None:
        corr_sin = correlacion(x_sin, plantilla)
        sigma_corr = corr_sin[L - 1:].std()
        sigma_x = x_sin.std()
        ruido_max = corr_sin[L - 1:].max()
        falsos = len(detectar(corr_sin, K_RUIDO * sigma_corr, ventana, refr))
        origen = "registro sin goteo"
    else:
        sigma_corr, sigma_x, ruido_max = ruido_entre_gotas(x_con, corr, L, ventana, refr, dt)
        falsos = None
        origen = "entre gotas"
    umbral = K_RUIDO * sigma_corr
    det = detectar(corr, umbral, ventana, refr)
    n = int(VENTANA_GOTA_MS / dt)
    pp = np.array([x_con[max(0, d - n):d + 1].max() - x_con[max(0, d - n):d + 1].min() for d in det])
    c_gota = np.array([corr[max(0, d - n):d + 1].max() for d in det])
    snr = 20 * np.log10(pp / sigma_x) if len(pp) else np.array([])
    curva = [len(detectar(corr, k * sigma_corr, ventana, refr)) for k in K_BARRIDO]
    return dict(canal=canal, x=x_con, dt=dt, det=det, umbral=umbral, sigma_x=sigma_x, sigma_corr=sigma_corr,
                ruido_max=ruido_max, falsos=falsos, origen_ruido=origen, pp=pp, c_gota=c_gota, snr=snr, curva=curva)


def buscar_registros():
    """Devuelve [(sensor, ronda, archivo_con_goteo, archivo_sin_goteo_o_None)] y si son provisorios."""
    por_clave = {}
    for f in sorted(ENSAYO.glob("*.csv")):
        m = re.fullmatch(r"(opt101|bpw34)_r(\d+)_(goteo|sin_goteo)\.csv", f.name)
        if m:
            por_clave.setdefault((m.group(1), int(m.group(2))), {})[m.group(3)] = f
    regs = [(s, r, d["goteo"], d.get("sin_goteo")) for (s, r), d in sorted(por_clave.items()) if "goteo" in d]
    return (regs, False) if regs else (PROVISORIOS, True)


def referencia_esperadas():
    """Opcional: datos/ensayo_actual/referencia.csv con columnas archivo,gotas_esperadas (conteo de referencia)."""
    ruta = ENSAYO / "referencia.csv"
    if not ruta.exists():
        return {}
    with open(ruta, encoding="utf-8") as f:
        return {fila["archivo"]: int(fila["gotas_esperadas"]) for fila in csv.DictReader(f)}


def columnas(sensor, d):
    """Columnas de señal filtrada del CSV: 1 para el OPT101, 2 para el BPW34."""
    return [d[:, i] for i in range(len(SENSORES[sensor]["canales"]))]


def fmt(v, n=1):
    return "" if v is None or (isinstance(v, float) and np.isnan(v)) else f"{v:.{n}f}"


def md_tabla(filas, cols):
    return ("| " + " | ".join(cols) + " |\n|" + "|".join("---" for _ in cols) + "|\n"
            + "\n".join("| " + " | ".join(str(f.get(c, "")) for c in cols) + " |" for f in filas) + "\n")


def guardar_tabla(nombre, filas, cols):
    with open(SALIDA / "tablas" / f"{nombre}.csv", "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(cols)
        for fila in filas:
            w.writerow([fila.get(c, "") for c in cols])
    (SALIDA / "tablas" / f"{nombre}.md").write_text(md_tabla(filas, cols), encoding="utf-8")


def guardar_fig(fig, nombre):
    fig.savefig(SALIDA / "figuras" / f"{nombre}.png", dpi=300, bbox_inches="tight")
    fig.savefig(SALIDA / "figuras" / f"{nombre}.svg", bbox_inches="tight")
    plt.close(fig)


# ------------------------------------------------------------------ principal
def main():
    (SALIDA / "figuras").mkdir(parents=True, exist_ok=True)
    (SALIDA / "tablas").mkdir(parents=True, exist_ok=True)
    registros, provisorio = buscar_registros()
    esperadas = referencia_esperadas()
    plantillas = {s: leer_plantillas(c["plantillas"]) for s, c in SENSORES.items()}

    resultados = []   # un elemento por canal y ronda
    for sensor, ronda, f_con, f_sin in registros:
        d_con = np.genfromtxt(f_con, delimiter=",")
        d_sin = np.genfromtxt(f_sin, delimiter=",") if f_sin else None
        cfg = SENSORES[sensor]
        for i, canal in enumerate(cfg["canales"]):
            x_sin = d_sin[:, i] if d_sin is not None else None
            r = analizar(canal, d_con[:, i], x_sin, plantillas[sensor][i + 1], cfg["dt_ms"])
            r.update(sensor=sensor, ronda=ronda, archivo=f_con.name, esperadas=esperadas.get(f_con.name))
            resultados.append(r)

    # ---- tabla comparativa
    filas = []
    for r in resultados:
        filas.append({
            "Sensor / canal": r["canal"], "Ronda": r["ronda"],
            "Gotas detectadas": len(r["det"]),
            "Gotas esperadas": "" if r["esperadas"] is None else r["esperadas"],
            "Falsos positivos (sin goteo)": "" if r["falsos"] is None else r["falsos"],
            "Ruido (mV)": fmt(r["sigma_x"], 2),
            "Amplitud gota media (mV)": fmt(r["pp"].mean(), 0),
            "Amplitud gota mínima (mV)": fmt(r["pp"].min(), 0),
            "Gota / ruido media (dB)": fmt(r["snr"].mean()),
            "Gota / ruido mínima (dB)": fmt(r["snr"].min()),
            "Margen gota más débil / ruido máx.": fmt(r["c_gota"].min() / r["ruido_max"]),
            "Ruido medido": r["origen_ruido"],
        })
    cols = list(filas[0].keys())
    guardar_tabla("tabla_comparacion", filas, cols)

    # ---- figura 1: señal con las gotas detectadas (5 s desde la primera gota de la primera ronda de cada sensor)
    vistos, panel = set(), []
    for r in resultados:
        if r["canal"] not in vistos:
            vistos.add(r["canal"])
            panel.append(r)
    fig, axs = plt.subplots(len(panel), 1, figsize=(9, 2.2 * len(panel) + 0.8), sharex=True)
    for ax, r in zip(np.atleast_1d(axs), panel):
        t = np.arange(len(r["x"])) * r["dt"] / 1000
        t0 = max(0.0, r["det"][0] * r["dt"] / 1000 - 0.5) if len(r["det"]) else 0.0
        m = (t >= t0) & (t < t0 + 5)
        color = next(c["color"][r["canal"]] for c in SENSORES.values() if r["canal"] in c["color"])
        ax.plot(t[m] - t0, r["x"][m], color=color, lw=0.7)
        for d in r["det"]:
            if t0 <= d * r["dt"] / 1000 < t0 + 5:
                ax.plot(d * r["dt"] / 1000 - t0, r["x"][m].max() * 0.95, "v", color="k", ms=4)
        ax.set_ylabel(f"{r['canal']}\n(mV)")
        ax.grid(alpha=0.25)
    np.atleast_1d(axs)[-1].set_xlabel("Tiempo (s)")
    np.atleast_1d(axs)[0].set_title("Señal filtrada de cada sensor (▾ = gota detectada; cada panel con su escala)", fontsize=9)
    guardar_fig(fig, "fig1_senales_con_gotas")

    # ---- figura 2: relación gota/ruido por canal (promedio entre rondas; puntos = cada ronda)
    canales = list(dict.fromkeys(r["canal"] for r in resultados))
    fig, ax = plt.subplots(figsize=(6.5, 4))
    for i, canal in enumerate(canales):
        rs = [r for r in resultados if r["canal"] == canal]
        color = next(c["color"][canal] for c in SENSORES.values() if canal in c["color"])
        media = [r["snr"].mean() for r in rs]
        minima = [r["snr"].min() for r in rs]
        ax.bar(i - 0.18, np.mean(media), 0.34, color=color, alpha=0.85, label="media de las gotas" if i == 0 else None)
        ax.bar(i + 0.18, np.mean(minima), 0.34, color=color, alpha=0.45, hatch="//",
               label="gota más débil" if i == 0 else None)
        ax.plot([i - 0.18] * len(media), media, "k.", ms=4)
        ax.plot([i + 0.18] * len(minima), minima, "k.", ms=4)
    ax.set_xticks(range(len(canales)))
    ax.set_xticklabels(canales)
    ax.set_ylabel("Relación gota / ruido (dB)")
    ax.set_title("Qué tan separada está la gota del ruido (puntos = cada ronda)", fontsize=10)
    ax.legend(fontsize=8)
    ax.grid(alpha=0.25, axis="y")
    guardar_fig(fig, "fig2_relacion_gota_ruido")

    # ---- figura 3: gotas detectadas según el umbral (primera ronda de cada canal)
    fig, ax = plt.subplots(figsize=(7, 4))
    for r in panel:
        color = next(c["color"][r["canal"]] for c in SENSORES.values() if r["canal"] in c["color"])
        ax.plot(K_BARRIDO, r["curva"], color=color, label=r["canal"], lw=1.5)
    ax.axvline(K_RUIDO, color="k", lw=0.7, ls=":")
    ax.set_xscale("log")
    ax.set_xlabel(f"Umbral, en veces el ruido de la correlación (punteada: {K_RUIDO:.0f})")
    ax.set_ylabel("Gotas detectadas")
    ax.set_title("Si el conteo no cambia al mover el umbral, la detección es robusta", fontsize=10)
    ax.legend(fontsize=8)
    ax.grid(alpha=0.25, which="both")
    guardar_fig(fig, "fig3_conteo_vs_umbral")

    # ---- umbral del sensor elegido: punto medio entre el peor ruido y la gota más débil
    cfg = SENSORES[SENSOR_ELEGIDO]
    fuentes = [r for r in resultados if r["sensor"] == SENSOR_ELEGIDO]
    previos = []
    for sensor, etiqueta, f_con, f_sin in PREVIOS_UMBRAL:
        if sensor != SENSOR_ELEGIDO or not f_con.exists():
            continue
        d_con = np.genfromtxt(f_con, delimiter=",")
        d_sin = np.genfromtxt(f_sin, delimiter=",") if f_sin.exists() else None
        for i, canal in enumerate(cfg["canales"]):
            r = analizar(canal, d_con[:, i], d_sin[:, i] if d_sin is not None else None,
                         plantillas[sensor][i + 1], cfg["dt_ms"])
            r.update(sensor=sensor, ronda=f"previo {etiqueta}")
            previos.append(r)
    filas_u, filas_det = [], []
    for canal in cfg["canales"]:
        todos = [r for r in fuentes + previos if r["canal"] == canal]
        for r in todos:
            filas_det.append({"Canal": canal, "Registro": r["ronda"], "Ruido máx. correlación": fmt(r["ruido_max"]),
                              "Gota más débil (correlación)": fmt(r["c_gota"].min(), 0)})
        peor_ruido = max(r["ruido_max"] for r in todos)
        gota_min = min(r["c_gota"].min() for r in todos)
        umbral = float(np.sqrt(peor_ruido * gota_min))
        filas_u.append({"Canal": canal, "Peor ruido": fmt(peor_ruido), "Gota más débil": fmt(gota_min, 0),
                        "Umbral propuesto (punto medio)": fmt(umbral, 0),
                        "Veces sobre el ruido": fmt(umbral / peor_ruido), "Veces bajo la gota más débil": fmt(gota_min / umbral)})
    guardar_tabla("tabla_umbral_detalle", filas_det, list(filas_det[0].keys()))
    guardar_tabla("tabla_umbral", filas_u, list(filas_u[0].keys()))

    # ---- consola
    print("Registros:", "PROVISORIOS (08/10, sin registro sin goteo)" if provisorio else "datos/ensayo_actual")
    print(md_tabla(filas, cols))
    print("Umbral del", cfg["nombre"])
    print(md_tabla(filas_u, list(filas_u[0].keys())))


if __name__ == "__main__":
    main()
