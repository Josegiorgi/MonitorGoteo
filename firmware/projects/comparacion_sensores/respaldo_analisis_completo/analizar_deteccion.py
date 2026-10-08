"""Comparación OPT101 vs BPW34 en su capacidad de detectar gotas.

Lee los registros de `datos/` SIN modificarlos y regenera todas las figuras (PNG 300 dpi + SVG)
y tablas (CSV + Markdown) en `resultados/`. Correr con:  python analizar_deteccion.py

Detector común (el mismo de los análisis previos y del firmware, ver xcorr_detector.c):
correlación cruzada con la plantilla de la gota, umbral = K_REF · σ_corr (σ_corr = desvío de la
correlación en tramos sin gota), ventana de pico y período refractario. Cada sensor/canal usa su
propia plantilla (drop_template.h de su proyecto) porque la forma de la gota depende del sensor.

Todos los parámetros arbitrarios están declarados abajo y justificados en METRICAS_JUSTIFICACION.md.
"""
from pathlib import Path
import csv
import re
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# ---------------------------------------------------------------- rutas
AQUI = Path(__file__).resolve().parent   # esta carpeta (respaldo)
RAIZ = AQUI.parent                       # comparacion_sensores/
DATOS = RAIZ / "datos"
SALIDA = AQUI / "resultados"
FIG = SALIDA / "figuras"
TAB = SALIDA / "tablas"
PROYECTOS = RAIZ.parent
CSV_OPT = DATOS / "opt101.csv"       # col 0: señal filtrada (mV), col 1: detección del firmware (500/0)
CSV_BPW = DATOS / "bpw34.csv"        # col 0 y 1: canales filtrados (mV), col 2: detección del firmware (1000/0)
CRUDO_OPT = DATOS / "crudos" / "opt101_crudo_18-09_DatosGoteo.csv"      # MODE_RAW, 18/09
CRUDO_BPW = DATOS / "crudos" / "bpw34_crudo_23-09_SenalconGoteo.csv"    # MODE_RAW, 23/09
PLANT_OPT = PROYECTOS / "prueba_opt101" / "main" / "drop_template.h"
PLANT_BPW = PROYECTOS / "pruebabpw34" / "main" / "drop_template.h"

# ---------------------------------------------------------------- parámetros declarados
DT_OPT_MS = 0.7            # período de muestreo del OPT101 (SAMPLE_PERIOD_US = 700)
DT_BPW_MS = 1.2            # período de muestreo del BPW34 (SAMPLE_PERIOD_US = 1200)
K_REF = 12.0               # umbral de referencia = K_REF · σ_corr (el criterio de los análisis previos)
K_BARRIDO = np.unique(np.r_[np.arange(3, 31, 1), np.arange(35, 201, 5)])  # barrido de robustez
VENTANA_PICO_MS = 15.0     # igual que XCORR_PEAK_WINDOW_MS del firmware
REFRACTARIO_MS = 100.0     # igual que XCORR_REFRACTORY_MS (≈ 8 veces el pulso, < 0.25 del intervalo entre gotas)
EXCL_ANTES_MS = 100.0      # exclusión alrededor de cada detección para medir ruido: antes (pulso + ventana)
EXCL_DESPUES_MS = 50.0     # y después
BASE_VENTANA_S = 1.0       # ventana de la mediana móvil para la línea base local
VENTANA_PULSO_MS = 100.0   # ventana anterior a la detección donde se busca el extremo del pulso
COINCIDENCIA_MS = 15.0     # ventana para emparejar detecciones de los dos canales (= ventana de pico)
UMBRAL_DOBLE = 0.6         # intervalo < 0.6 · mediana  -> sospecha de doble detección
UMBRAL_PERDIDA = 1.6       # intervalo > 1.6 · mediana  -> sospecha de gota perdida
TOL_MESETA = 0.10          # meseta: conteo dentro de ±10 % del conteo a K_REF
TRAMO_OPT_MUESTRAS = 9500  # primeras ~6.6 s del OPT101, donde se ven gotas claras
LSB_MV = 3100.0 / 4096.0   # LSB nominal del ADC (12 bits, atenuación 12 dB, ~0-3.1 V). Aproximado.
DRIFT_MIN_S = 60.0         # duración mínima para considerar concluyente la deriva

COLOR = {"OPT101": "#D55E00", "BPW34 c1": "#0072B2", "BPW34 c2": "#009E73", "BPW34 OR": "#6A3D9A"}


# ---------------------------------------------------------------- utilidades
def leer_plantillas(ruta):
    txt = ruta.read_text(encoding="utf-8")
    por_canal = re.findall(r"DROP_TEMPLATE_CH(\d)\[DROP_TEMPLATE_LEN\] = \{(.*?)\};", txt, re.S)
    if por_canal:
        return {int(n): np.array([float(v) for v in re.findall(r"(-?\d+\.\d+)f", b)]) for n, b in por_canal}
    b = re.search(r"DROP_TEMPLATE\[DROP_TEMPLATE_LEN\] = \{(.*?)\};", txt, re.S).group(1)
    return {1: np.array([float(v) for v in re.findall(r"(-?\d+\.\d+)f", b)])}


def mediana_movil(x, tamano):
    """Mediana móvil centrada de `tamano` muestras (impar), con los bordes extendidos con el valor más cercano.
    Se calcula por bloques para no gastar memoria en registros largos."""
    mitad = tamano // 2
    xp = np.pad(x, mitad, mode="edge")
    out = np.empty(len(x))
    bloque = 2000
    for a in range(0, len(x), bloque):
        b = min(a + bloque, len(x))
        ventanas = np.lib.stride_tricks.sliding_window_view(xp[a:b + 2 * mitad], tamano)
        out[a:b] = np.median(ventanas, axis=1)
    return out


def ms_a_muestras(ms, dt_ms):
    return int((ms * 1000.0) // (dt_ms * 1000.0))  # división entera, igual que MS_TO_SAMPLES del firmware


def correlacion(x, plantilla):
    u = plantilla / np.linalg.norm(plantilla)
    L = len(u)
    c = np.zeros(len(x))
    c[L - 1:] = np.convolve(x, u[::-1], "valid")
    return c


def detectar(corr, umbral, ventana, refractario):
    """Misma lógica que XCorrDetectorProcess (xcorr_detector.c): cruce de umbral, espera la ventana de
    pico y confirma al final de la ventana; después, período refractario."""
    det, refl, buscando, resta = [], 0, False, 0
    for i, c in enumerate(corr):
        if refl > 0:
            refl -= 1
            continue
        if buscando:
            resta -= 1
            if resta == 0:
                buscando = False
                refl = refractario
                det.append(i)
            continue
        if c > umbral:
            buscando, resta = True, ventana
    return np.array(det, dtype=int)


def estimar_sigma_corr(corr, L, ventana, refractario, dt_ms):
    """σ de la correlación en tramos sin gota: arranca con MAD (robusto) y refina excluyendo las detecciones."""
    c = corr[L - 1:]
    s = 1.4826 * np.median(np.abs(c - np.median(c)))
    pre, post = int(EXCL_ANTES_MS / dt_ms), int(EXCL_DESPUES_MS / dt_ms)
    for _ in range(3):
        det = detectar(corr, K_REF * s, ventana, refractario)
        mascara = np.ones(len(corr), bool)
        mascara[:L - 1] = False
        for d in det:
            mascara[max(0, d - pre):d + post] = False
        s = corr[mascara].std()
    return s, mascara


def estadistica(v):
    v = np.asarray(v, float)
    if len(v) == 0:
        return dict(media=np.nan, de=np.nan, minimo=np.nan, p5=np.nan, maximo=np.nan)
    return dict(media=v.mean(), de=v.std(ddof=1) if len(v) > 1 else 0.0, minimo=v.min(),
                p5=np.percentile(v, 5), maximo=v.max())


def emparejar(ta, tb, tol):
    """Empareja cada detección de a con la más cercana de b dentro de ±tol (ms). Devuelve pares, solo_a, solo_b."""
    usados, pares, solo_a = set(), [], []
    for i, t in enumerate(ta):
        if len(tb) == 0:
            solo_a.append(i)
            continue
        j = int(np.argmin(np.abs(tb - t)))
        if abs(tb[j] - t) <= tol and j not in usados:
            usados.add(j)
            pares.append((i, j))
        else:
            solo_a.append(i)
    solo_b = [j for j in range(len(tb)) if j not in usados]
    return pares, solo_a, solo_b


def union_or(ta, tb, tol):
    pares, solo_a, solo_b = emparejar(ta, tb, tol)
    t = [min(ta[i], tb[j]) for i, j in pares] + [ta[i] for i in solo_a] + [tb[j] for j in solo_b]
    return np.sort(np.array(t))


def estadistica_intervalos(t_s):
    """t_s: instantes de detección (s). Devuelve mediana, CV y % de intervalos anómalos."""
    if len(t_s) < 3:
        return dict(n=len(t_s), mediana_s=np.nan, cv=np.nan, pct_doble=np.nan, pct_perdida=np.nan, pct_anomalos=np.nan)
    iv = np.diff(t_s)
    med = np.median(iv)
    doble = int((iv < UMBRAL_DOBLE * med).sum())
    perdida = int((iv > UMBRAL_PERDIDA * med).sum())
    return dict(n=len(t_s), mediana_s=med, cv=iv.std(ddof=1) / iv.mean(),
                pct_doble=100.0 * doble / len(iv), pct_perdida=100.0 * perdida / len(iv),
                pct_anomalos=100.0 * (doble + perdida) / len(iv))


def meseta(ks, cuentas, k_ref, tol):
    """Rango contiguo de k alrededor de k_ref con conteo dentro de ±tol del conteo a k_ref."""
    ks = np.asarray(ks, float)
    cuentas = np.asarray(cuentas, float)
    i0 = int(np.argmin(np.abs(ks - k_ref)))
    ref = cuentas[i0]
    ok = np.abs(cuentas - ref) <= tol * max(ref, 1)
    a = b = i0
    while a - 1 >= 0 and ok[a - 1]:
        a -= 1
    while b + 1 < len(ks) and ok[b + 1]:
        b += 1
    return ks[a], ks[b], ref


# ---------------------------------------------------------------- análisis de un canal
def analizar_canal(nombre, x, dt_ms, plantilla):
    L = len(plantilla)
    ventana = ms_a_muestras(VENTANA_PICO_MS, dt_ms)
    refr = ms_a_muestras(REFRACTARIO_MS, dt_ms)
    corr = correlacion(x, plantilla)
    sigma_c, mascara = estimar_sigma_corr(corr, L, ventana, refr, dt_ms)
    det = detectar(corr, K_REF * sigma_c, ventana, refr)
    base = mediana_movil(x, int(BASE_VENTANA_S * 1000 / dt_ms) | 1)
    sigma_std = x[mascara].std()
    # σ robusto (MAD): en el BPW34 los pulsos están separados ~0.4 s y la cola lenta de cada uno (pasaaltos de
    # 5 Hz) cae dentro de las "ventanas sin gota", lo que infla el desvío estándar. Por eso el ruido de
    # referencia para SNR y normalización es el MAD; el desvío estándar se reporta como dato adicional.
    sigma_x = 1.4826 * np.median(np.abs(x[mascara] - np.median(x[mascara])))
    n_pulso = int(VENTANA_PULSO_MS / dt_ms)
    gotas = []
    for d in det:
        a = max(0, d - n_pulso)
        seg = x[a:d + 1] - base[a:d + 1]
        dv = np.abs(seg).max()
        sobre = np.where(np.abs(seg) >= dv / 2)[0]
        gotas.append(dict(t_s=d * dt_ms / 1000, dV_mV=dv, pp_mV=seg.max() - seg.min(),
                          snr_dB=20 * np.log10(dv / sigma_x), fwhm_ms=(sobre[-1] - sobre[0]) * dt_ms,
                          corr_pico=corr[a:d + 1].max()))
    return dict(nombre=nombre, x=x, dt_ms=dt_ms, plantilla=plantilla, corr=corr, sigma_corr=sigma_c,
                sigma_x=sigma_x, sigma_std=sigma_std, mascara=mascara, det=det, base=base, gotas=gotas,
                ventana=ventana, refr=refr, L=L)


def barrido(canal):
    cuentas = []
    for k in K_BARRIDO:
        cuentas.append(len(detectar(canal["corr"], k * canal["sigma_corr"], canal["ventana"], canal["refr"])))
    return np.array(cuentas)


def barrido_or(c1, c2):
    cuentas = []
    for k in K_BARRIDO:
        t1 = detectar(c1["corr"], k * c1["sigma_corr"], c1["ventana"], c1["refr"]) * c1["dt_ms"]
        t2 = detectar(c2["corr"], k * c2["sigma_corr"], c2["ventana"], c2["refr"]) * c2["dt_ms"]
        cuentas.append(len(union_or(t1, t2, COINCIDENCIA_MS)))
    return np.array(cuentas)


# ---------------------------------------------------------------- registros crudos (contraste, deriva)
def contraste_crudo(x, dt_ms):
    """Contraste sobre el registro crudo: eventos = |x - base| > 10·σ_MAD, agrupados con 100 ms de separación."""
    base = np.median(x)
    sig = 1.4826 * np.median(np.abs(x - base))
    idx = np.where(np.abs(x - base) > 10 * sig)[0]
    sep = int(100 / dt_ms)
    eventos, ini, prev = [], idx[0], idx[0]
    for k in idx[1:]:
        if k - prev > sep:
            eventos.append((ini, prev))
            ini = k
        prev = k
    eventos.append((ini, prev))
    pad = int(30 / dt_ms)
    dv = np.array([np.abs(x[max(0, a - pad):b + pad] - base).max() for a, b in eventos])
    return base, sig, dv, len(eventos)


def deriva_cruda(x, dt_ms, sigma):
    ventana = int(2000 / dt_ms)  # ventanas de 2 s
    n = len(x) // ventana
    med = np.array([np.median(x[i * ventana:(i + 1) * ventana]) for i in range(n)])
    t = (np.arange(n) + 0.5) * ventana * dt_ms / 1000
    pend = np.polyfit(t, med, 1)[0] * 60  # pendiente de la recta de mínimos cuadrados, en mV/min
    return pend, med.max() - med.min(), len(x) * dt_ms / 1000


# ---------------------------------------------------------------- figuras
def guardar(fig, nombre):
    fig.savefig(FIG / f"{nombre}.png", dpi=300, bbox_inches="tight")
    fig.savefig(FIG / f"{nombre}.svg", bbox_inches="tight")
    plt.close(fig)


def fig_ventana(canales, t0, t1, nombre, detecciones):
    fig, axs = plt.subplots(len(canales), 1, sharex=True, figsize=(9, 2.2 * len(canales) + 0.8))
    axs = np.atleast_1d(axs)
    for ax, (clave, c) in zip(axs, canales):
        t = np.arange(len(c["x"])) * c["dt_ms"] / 1000
        m = (t >= t0) & (t < t1)
        y = (c["x"] - c["base"]) / c["sigma_x"]
        ax.plot(t[m], y[m], color=COLOR[clave], lw=0.7)
        for d in detecciones[clave]:
            if t0 <= d < t1:
                ax.axvline(d, color="k", lw=0.6, ls=":")
                ax.plot(d, ax.get_ylim()[1] * 0.9, marker="v", color="k", ms=4)
        ax.set_ylabel(f"{clave}\n(V − V_base)/σ")
        ax.grid(alpha=0.25)
    axs[-1].set_xlabel("Tiempo (s)")
    axs[0].set_title("Señal normalizada por el ruido de cada sensor (escala vertical independiente); ▾ = detección", fontsize=9)
    guardar(fig, nombre)


def fig_pulso_promedio(canales):
    fig, ax = plt.subplots(figsize=(7, 4))
    for clave, c in canales:
        n_antes, n_desp = int(VENTANA_PULSO_MS / c["dt_ms"]), int(20 / c["dt_ms"])
        trozos = []
        for d in c["det"]:
            if d - n_antes >= 0 and d + n_desp < len(c["x"]):
                trozos.append(c["x"][d - n_antes:d + n_desp] - c["base"][d - n_antes:d + n_desp])
        trozos = np.array(trozos)
        dv_medio = np.mean([g["dV_mV"] for g in c["gotas"]])
        t = (np.arange(-n_antes, n_desp)) * c["dt_ms"]
        m, s = trozos.mean(0) / dv_medio, trozos.std(0) / dv_medio
        ax.plot(t, m, color=COLOR[clave], label=f"{clave} (n = {len(trozos)}, {1000 / c['dt_ms']:.0f} Hz)")
        ax.fill_between(t, m - s, m + s, color=COLOR[clave], alpha=0.2)
    ax.axvline(0, color="k", lw=0.6, ls=":")
    ax.set_xlabel("Tiempo respecto de la detección (ms)")
    ax.set_ylabel("Amplitud normalizada por ΔV medio de cada sensor")
    ax.set_title("Pulso promedio de gota ± 1 DE", fontsize=10)
    ax.legend(fontsize=8)
    ax.grid(alpha=0.25)
    guardar(fig, "fig2_pulso_promedio")


def fig_snr(canales):
    fig, ax = plt.subplots(figsize=(6, 4))
    datos = [[g["snr_dB"] for g in c["gotas"]] for _, c in canales]
    bp = ax.boxplot(datos, tick_labels=[k for k, _ in canales], patch_artist=True, widths=0.5)
    for parche, (k, _) in zip(bp["boxes"], canales):
        parche.set_facecolor(COLOR[k])
        parche.set_alpha(0.5)
    for i, d in enumerate(datos, start=1):
        ax.plot(i, np.percentile(d, 5), marker="D", color="k", ms=6, ls="none",
                label="percentil 5" if i == 1 else None)
    ax.set_ylabel("SNR de la gota (dB)")
    ax.set_title("Relación señal-ruido por gota", fontsize=10)
    ax.legend(fontsize=8)
    ax.grid(alpha=0.25, axis="y")
    guardar(fig, "fig3_boxplot_snr")


def fig_intervalos(series):
    fig, axs = plt.subplots(len(series), 1, sharex=True, figsize=(8, 2.0 * len(series) + 0.8))
    for ax, (clave, t) in zip(axs, series):
        if len(t) >= 3:
            iv = np.diff(t)
            med = np.median(iv)
            ax.axhspan(UMBRAL_DOBLE * med, UMBRAL_PERDIDA * med, color="0.9")
            ax.axhline(med, color="0.5", lw=0.6)
            ax.plot(t[1:], iv, "o", color=COLOR[clave], ms=4)
        ax.set_ylabel(f"{clave}\nintervalo (s)")
        ax.grid(alpha=0.25)
    axs[-1].set_xlabel("Tiempo (s)")
    axs[0].set_title("Intervalos entre detecciones (banda gris: 0,6× a 1,6× la mediana)", fontsize=9)
    guardar(fig, "fig4_intervalos")


def fig_robustez(curvas):
    fig, ax = plt.subplots(figsize=(7, 4))
    for clave, estilo, cuentas in curvas:
        ax.plot(K_BARRIDO, cuentas, estilo[0], color=COLOR[estilo[1]], label=clave, lw=1.4)
    ax.axvline(K_REF, color="k", lw=0.7, ls=":")
    ax.set_xscale("log")
    ax.set_xlabel("k (umbral = k · σ de la correlación)")
    ax.set_ylabel("Gotas detectadas")
    ax.set_title("Robustez al umbral (línea punteada: k de referencia)", fontsize=10)
    ax.legend(fontsize=8)
    ax.grid(alpha=0.25, which="both")
    guardar(fig, "fig5_robustez_umbral")


def fig_concordancia(pares, solo1, solo2, offsets_ms):
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(9, 3.6))
    a1.bar(["Ambos", "Solo c1", "Solo c2"], [pares, solo1, solo2], color=[COLOR["BPW34 OR"], COLOR["BPW34 c1"], COLOR["BPW34 c2"]])
    a1.set_ylabel("Detecciones")
    a1.set_title("Concordancia entre canales (BPW34)", fontsize=10)
    a2.hist(offsets_ms, bins=15, color="0.6")
    a2.set_xlabel("Diferencia de instante de detección c2 − c1 (ms)")
    a2.set_ylabel("Pares")
    a2.set_title("Desfase entre canales", fontsize=10)
    for a in (a1, a2):
        a.grid(alpha=0.25)
    guardar(fig, "fig6_concordancia_canales")


# ---------------------------------------------------------------- tablas
def md_tabla(filas, cols):
    enc = "| " + " | ".join(cols) + " |\n|" + "|".join("---" for _ in cols) + "|\n"
    return enc + "\n".join("| " + " | ".join(str(f.get(c, "")) for c in cols) + " |" for f in filas) + "\n"


def csv_tabla(ruta, filas, cols):
    with open(ruta, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(cols)
        for fila in filas:
            w.writerow([fila.get(c, "") for c in cols])


def fmt(v, n=2):
    return "—" if v is None or (isinstance(v, float) and np.isnan(v)) else f"{v:.{n}f}"


# ---------------------------------------------------------------- principal
def main():
    FIG.mkdir(parents=True, exist_ok=True)
    TAB.mkdir(parents=True, exist_ok=True)
    opt = np.genfromtxt(CSV_OPT, delimiter=",")
    bpw = np.genfromtxt(CSV_BPW, delimiter=",")
    pl_opt = leer_plantillas(PLANT_OPT)[1]
    pl_bpw = leer_plantillas(PLANT_BPW)

    canal = {
        "OPT101": analizar_canal("OPT101 (completo)", opt[:, 0], DT_OPT_MS, pl_opt),
        "OPT101 tramo": analizar_canal("OPT101 (primeros 6.6 s)", opt[:TRAMO_OPT_MUESTRAS, 0], DT_OPT_MS, pl_opt),
        "BPW34 c1": analizar_canal("BPW34 canal 1", bpw[:, 0], DT_BPW_MS, pl_bpw[1]),
        "BPW34 c2": analizar_canal("BPW34 canal 2", bpw[:, 1], DT_BPW_MS, pl_bpw[2]),
    }

    # detecciones del firmware registradas en los CSV (referencia)
    fw_opt = int((opt[:, 1] > 0).sum())
    fw_bpw = int((bpw[:, 2] > 0).sum())

    # ---- concordancia entre canales y detección combinada
    t1 = canal["BPW34 c1"]["det"] * DT_BPW_MS
    t2 = canal["BPW34 c2"]["det"] * DT_BPW_MS
    pares, solo1, solo2 = emparejar(t1, t2, COINCIDENCIA_MS)
    offsets = [t2[j] - t1[i] for i, j in pares]
    t_or = union_or(t1, t2, COINCIDENCIA_MS) / 1000

    # ---- barridos de robustez
    cuentas = {k: barrido(c) for k, c in canal.items()}
    cuentas["BPW34 OR"] = barrido_or(canal["BPW34 c1"], canal["BPW34 c2"])

    # ---- filas de métricas por serie
    series_t = {k: c["det"] * c["dt_ms"] / 1000 for k, c in canal.items()}
    series_t["BPW34 OR"] = t_or
    filas = []
    for clave in ["OPT101", "OPT101 tramo", "BPW34 c1", "BPW34 c2", "BPW34 OR"]:
        fila = dict(serie=canal[clave]["nombre"] if clave in canal else "BPW34 combinado (OR)")
        iv = estadistica_intervalos(series_t[clave])
        m1, m2, ref = meseta(K_BARRIDO, cuentas[clave], K_REF, TOL_MESETA)
        fila.update(gotas_detectadas=iv["n"], mediana_intervalo_s=fmt(iv["mediana_s"], 3), cv_intervalo=fmt(iv["cv"], 3),
                    pct_dobles=fmt(iv["pct_doble"], 1), pct_perdidas=fmt(iv["pct_perdida"], 1),
                    pct_anomalos=fmt(iv["pct_anomalos"], 1), k_meseta_min=fmt(m1, 0), k_meseta_max=fmt(m2, 0),
                    ancho_meseta=fmt(m2 - m1, 0))
        if clave in canal:
            c = canal[clave]
            g = c["gotas"]
            snr = estadistica([x["snr_dB"] for x in g])
            dv = estadistica([x["dV_mV"] for x in g])
            fw = estadistica([x["fwhm_ms"] for x in g])
            cp = np.array([x["corr_pico"] for x in g])
            fila.update(sigma_MAD_mV=fmt(c["sigma_x"]), sigma_std_mV=fmt(c["sigma_std"]), sigma_LSB=fmt(c["sigma_x"] / LSB_MV, 1),
                        sigma_corr=fmt(c["sigma_corr"]), umbral_corr=fmt(K_REF * c["sigma_corr"], 1),
                        dV_media_mV=fmt(dv["media"], 1), dV_de_mV=fmt(dv["de"], 1), dV_min_mV=fmt(dv["minimo"], 1),
                        cv_dV=fmt(dv["de"] / dv["media"], 3),
                        snr_media_dB=fmt(snr["media"], 1), snr_de_dB=fmt(snr["de"], 1),
                        snr_min_dB=fmt(snr["minimo"], 1), snr_p5_dB=fmt(snr["p5"], 1),
                        fwhm_media_ms=fmt(fw["media"], 1), fwhm_de_ms=fmt(fw["de"], 1),
                        corr_gota_min=fmt(cp.min(), 1), corr_gota_media=fmt(cp.mean(), 1))
        filas.append(fila)

    # corr máxima del ruido (margen de separación señal / ruido, independiente del umbral)
    for clave in ["OPT101", "BPW34 c1", "BPW34 c2"]:
        c = canal[clave]
        ruido_max = c["corr"][c["mascara"]].max()
        for f in filas:
            if f["serie"] == c["nombre"]:
                f["corr_ruido_max"] = fmt(ruido_max, 1)
                f["margen_corr"] = fmt(float(f["corr_gota_min"]) / ruido_max, 1)

    # ---- cola del registro (gotas perdidas al final, sin referencia): tiempo desde la última detección
    for clave in ["OPT101", "BPW34 c1", "BPW34 c2"]:
        c = canal[clave]
        iv = estadistica_intervalos(series_t[clave])
        fin = len(c["x"]) * c["dt_ms"] / 1000
        cola = (fin - series_t[clave][-1]) / iv["mediana_s"]
        for f in filas:
            if f["serie"] == c["nombre"]:
                f["cola_en_medianas"] = fmt(cola, 1)

    # ---- registros crudos (contraste, deriva)
    crudo = []
    if CRUDO_OPT.exists() and CRUDO_BPW.exists():
        co = np.genfromtxt(CRUDO_OPT, delimiter=",")[:, 0]
        cb = np.genfromtxt(CRUDO_BPW, delimiter=",")
        for nombre, x, dt in [("OPT101 (crudo 18/09)", co, DT_OPT_MS), ("BPW34 c1 (crudo 23/09)", cb[:, 0], DT_BPW_MS),
                              ("BPW34 c2 (crudo 23/09)", cb[:, 1], DT_BPW_MS)]:
            base, sig, dv, n_ev = contraste_crudo(x, dt)
            pend, var, dur = deriva_cruda(x, dt, sig)
            con = 100 * dv / base
            crudo.append(dict(registro=nombre, duracion_s=fmt(dur, 1), base_mV=fmt(base, 0), sigma_crudo_mV=fmt(sig, 2),
                              eventos=n_ev, dV_media_mV=fmt(dv.mean(), 1), contraste_medio_pct=fmt(con.mean(), 2),
                              contraste_de_pct=fmt(con.std(ddof=1), 2), contraste_min_pct=fmt(con.min(), 2),
                              deriva_mV_min=fmt(pend, 2), variacion_max_mV=fmt(var, 1),
                              variacion_en_sigma=fmt(var / sig, 1),
                              deriva_concluyente="sí" if dur >= DRIFT_MIN_S else "no (registro corto)"))

    # ---- guardar tablas
    cols = list(filas[0].keys())
    for f in filas:
        for c in f:
            if c not in cols:
                cols.append(c)
    cols = [c for c in dict.fromkeys(sum([list(f.keys()) for f in filas], []))]
    csv_tabla(TAB / "tabla_metricas_deteccion.csv", filas, cols)
    (TAB / "tabla_metricas_deteccion.md").write_text(md_tabla(filas, cols), encoding="utf-8")
    if crudo:
        cols_c = list(crudo[0].keys())
        csv_tabla(TAB / "tabla_registros_crudos.csv", crudo, cols_c)
        (TAB / "tabla_registros_crudos.md").write_text(md_tabla(crudo, cols_c), encoding="utf-8")
    for clave in ["OPT101", "BPW34 c1", "BPW34 c2"]:
        g = canal[clave]["gotas"]
        csv_tabla(TAB / f"gotas_{clave.replace(' ', '_').lower()}.csv", g, list(g[0].keys()))
    conc = [dict(detecciones_ambos=len(pares), solo_c1=len(solo1), solo_c2=len(solo2),
                 pct_ambos=fmt(100 * len(pares) / (len(pares) + len(solo1) + len(solo2)), 1),
                 desfase_medio_ms=fmt(np.mean(offsets), 2), desfase_max_abs_ms=fmt(np.max(np.abs(offsets)), 2),
                 detecciones_firmware_opt=fw_opt, detecciones_firmware_bpw=fw_bpw)]
    csv_tabla(TAB / "tabla_concordancia.csv", conc, list(conc[0].keys()))
    (TAB / "tabla_concordancia.md").write_text(md_tabla(conc, list(conc[0].keys())), encoding="utf-8")
    barr = [dict(k=k, **{n: int(cuentas[n][i]) for n in cuentas}) for i, k in enumerate(K_BARRIDO)]
    csv_tabla(TAB / "tabla_barrido_umbral.csv", barr, list(barr[0].keys()))

    # ---- figuras
    c_o, c_1, c_2 = canal["OPT101"], canal["BPW34 c1"], canal["BPW34 c2"]
    dets_t = {"OPT101": series_t["OPT101"], "BPW34 c1": series_t["BPW34 c1"], "BPW34 c2": series_t["BPW34 c2"]}
    fig_ventana([("OPT101", c_o), ("BPW34 c1", c_1), ("BPW34 c2", c_2)], 1.0, 6.0, "fig1_ventana_representativa", dets_t)
    fig_ventana([("OPT101", c_o)], 0, 10, "fig_revision_opt101_1", dets_t)
    fig_ventana([("OPT101", c_o)], 10, 19, "fig_revision_opt101_2", dets_t)
    fig_ventana([("BPW34 c1", c_1), ("BPW34 c2", c_2)], 0, 10, "fig_revision_bpw34_1", dets_t)
    fig_ventana([("BPW34 c1", c_1), ("BPW34 c2", c_2)], 10, 13.3, "fig_revision_bpw34_2", dets_t)
    fig_pulso_promedio([("OPT101", c_o), ("BPW34 c1", c_1), ("BPW34 c2", c_2)])
    fig_snr([("OPT101", c_o), ("BPW34 c1", c_1), ("BPW34 c2", c_2)])
    fig_intervalos([("OPT101", series_t["OPT101"]), ("BPW34 c1", series_t["BPW34 c1"]),
                    ("BPW34 c2", series_t["BPW34 c2"]), ("BPW34 OR", t_or)])
    fig_robustez([("OPT101 (completo)", ("-", "OPT101"), cuentas["OPT101"]),
                  ("OPT101 (6,6 s)", ("--", "OPT101"), cuentas["OPT101 tramo"]),
                  ("BPW34 canal 1", ("-", "BPW34 c1"), cuentas["BPW34 c1"]),
                  ("BPW34 canal 2", ("-", "BPW34 c2"), cuentas["BPW34 c2"]),
                  ("BPW34 combinado (OR)", ("-", "BPW34 OR"), cuentas["BPW34 OR"])])
    fig_concordancia(len(pares), len(solo1), len(solo2), offsets)

    # ---- resumen por consola
    print(md_tabla(filas, ["serie", "gotas_detectadas", "sigma_MAD_mV", "sigma_std_mV", "sigma_LSB", "snr_media_dB", "snr_min_dB", "snr_p5_dB",
                           "cv_dV", "pct_anomalos", "ancho_meseta", "margen_corr", "cola_en_medianas"]))
    if crudo:
        print(md_tabla(crudo, list(crudo[0].keys())))
    print(md_tabla(conc, list(conc[0].keys())))


if __name__ == "__main__":
    main()
