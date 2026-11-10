# Guion de pruebas del firmware final (monitor_goteo)

Guion para probar en el equipo, **en el orden en que se ejecuta**. Cada paso tiene dos partes:

- **▶ Hacer:** lo que tenés que hacer con el equipo (o con el celular).
- **✔ Debe pasar:** lo que tenés que ver, oír o leer por consola.

Tildá lo que pase y anotá lo que falle (con el log de la consola).

## Resumen del guion

| Parte | Configuración | Qué se prueba | Tiempo |
|---|---|---|---|
| **A. Sin Bluetooth, tiempos cortos** | `USAR_BLE = 0` + `ESCENARIO_DESVIO_COLOCACION` | Autotest, Resumen, registro en la flash, descarga final pendiente, reinicios, goteo sin monitoreo, reposo y Modo Técnico | ~1 h 15 min |
| **B. Bluetooth con nRF Connect** | `USAR_BLE = 1` + `ESCENARIO_DESVIO_COLOCACION` | Conexión, órdenes, descarga, minuto que sigue contando, infusión interrumpida, efecto sobre las gotas | ~45 min |
| **C. Tiempos reales** | `USAR_BLE = 1` + `ESCENARIO_NORMAL` | Lo esencial con los tiempos de la especificación | ~30 min |
| **D. Con la app** | `USAR_BLE = 1` + `ESCENARIO_NORMAL` | La app de la enfermera con el equipo | aparte |
| **Apéndice** | cualquiera | Repaso de las etapas 1 a 3 (ya probadas) | opcional |

**Estado:** las etapas 1 a 3 (alarmas, batería, sonidos, clave, Ajustes y el paso a paso) ya están probadas. Todo lo demás está pendiente.

**Al terminar:** dejar `USAR_BLE = 1` y `ESCENARIO_NORMAL` antes de commitear.

## Antes de empezar

### Los dos interruptores (al principio de `monitor_goteo.c`)
Después de **cada** cambio hay que volver a compilar y grabar con `idf.py flash` (flash completo: hay una tabla de particiones nueva).

| Qué querés probar | `USAR_BLE` | `ESCENARIO` |
|---|---|---|
| Todo menos el BLE, sin esperar minutos | `0` | `ESCENARIO_DESVIO_COLOCACION` |
| El BLE | `1` | `ESCENARIO_DESVIO_COLOCACION` |
| Versión final / tiempos reales | `1` | `ESCENARIO_NORMAL` |

- `USAR_BLE = 0`: no anuncia nada y no gasta la memoria del BLE. Al compilar sale un aviso y la consola dice `SIN Bluetooth`.
- Con un escenario de prueba los tiempos se acortan y **el registro se muestra por la consola**: cada línea que se guarda aparece al momento (por ejemplo `#57 MUESTRA min=3 gotas=62 ...` o `#58 ALARMA ACTIVA ...`).
- Con `ESCENARIO_NORMAL` esos mensajes del registro no salen.
- **Las primeras líneas del arranque se pierden en la consola USB.** Por eso el estado del registro sale **después del autotest**, junto a `SIN Bluetooth`: `registro: N lineas escritas, M minutos de muestras libres` y, en los escenarios de prueba, el volcado de las últimas 60 líneas.

### Tiempos
| Qué | Real | Escenario de prueba |
|---|---|---|
| Recordatorio del Ajuste de goteo | 10 min (beep cada 1 min) | 30 s (beep cada 10 s) |
| Muestra del registro durante la infusión | 1 min | 15 s |
| Re-armado de una alarma alta silenciada | 2 min | 20 s |
| Goteo sin monitoreo (y se resuelve) | 2 min (45 s) | 20 s (15 s) |
| Inactividad de clave, Ajustes y Sin infusión | 60 s | 20 s |
| Inactividad del Modo Técnico | 5 min | 30 s |
| Bloqueo de la clave | 60 s | 15 s |

**Ojo:** el minuto que sigue contando después del fin (o de un reinicio) va en **minutos reales** en todos los modos: hay que esperar de verdad un par de minutos para ver que sube.

### Preparación
- Consola USB abierta durante todas las pruebas.
- **Tubo seco** al encender (el sensor mide su nivel base).
- Anotar los valores que aparezcan por consola (niveles base, líneas del registro).

### Claves
- Clave de enfermera: **1234**.
- Código técnico: **4321**.
- "Mantener 3 s" = mantener el botón del encoder apretado 3 segundos.

### No se puede probar
"Memoria llena" (la partición tarda ~35 días en llenarse) y la curva de batería medida.

---

# Parte A. Sin Bluetooth, tiempos cortos

**Configuración:** `USAR_BLE = 0` y `ESCENARIO = ESCENARIO_DESVIO_COLOCACION`. Compilar y grabar con flash completo.

## A1. Arranque y autotest
- [ ] **A1.1** ▶ Hacer: tubo seco, encender el equipo y mantener el botón 3 s.
  ✔ Debe pasar: suena el tono **ascendente** y aparece "Iniciando" con la animación.
- [ ] **A1.2** ▶ Hacer: mirar la consola (esperar ~15 s).
  ✔ Debe pasar: `sensor optico: nivel base del canal 1 = ... mV` y del canal 2, ambos entre 50 y 2400 mV. **Anotar los valores.**
- [ ] **A1.3** ▶ Hacer: mirar la consola después de `SIN Bluetooth`.
  ✔ Debe pasar: `registro: N lineas escritas, ...`. En un equipo nuevo `N` es 0. Además se imprime el volcado de las últimas líneas guardadas.
  *(El mensaje "borrando la particion del registro" solo sale la primera vez que se graba el firmware con esta partición: la pantalla queda en negro unos 10 s y puede perderse en la consola. Si ya la habías grabado, no sale.)*
- [ ] **A1.4** ▶ Hacer: no tocar nada.
  ✔ Debe pasar: a los ~5 s pasa **directo a la clave**, sin pantalla de OK.
- [ ] **A1.5** ▶ Hacer: repetir con batería conectada (más de 10 %) y después solo con USB y sin batería.
  ✔ Debe pasar: en los dos casos pasa el autotest.
- [ ] **A1.6** *(opcional)* ▶ Hacer: reiniciar con luz fuerte sobre el sensor.
  ✔ Debe pasar: `Falla: Sensor optico` y `No usar`, reintenta cada segundo y no deja seguir.

## A2. Clave, Resumen y recordatorio del Ajuste
- [ ] **A2.1** ▶ Hacer: en la clave, poner un código incorrecto.
  ✔ Debe pasar: los dígitos se ven con asterisco y dice "Clave incorrecta".
- [ ] **A2.2** ▶ Hacer: equivocarse 3 veces seguidas.
  ✔ Debe pasar: "Espere N s" (15 s en escenario) y después deja intentar de nuevo.
- [ ] **A2.3** ▶ Hacer: poner **1234** y cargar una infusión hasta el Resumen (factor 20, 500 mL, 8 h).
  ✔ Debe pasar: **Resumen página 1** con `FG 20 gtt/mL` en grande y "Verificar en el envase de la guia".
- [ ] **A2.4** ▶ Hacer: girar el encoder; después click en la página 1; después pulsación larga en la 2 y otra vez en la 1.
  ✔ Debe pasar: el giro cambia de página; el click en la 1 pasa a la **página 2** (volumen, duración, mL/h y gtt/min) y recién ahí se puede iniciar; la pulsación larga en la 2 vuelve a la 1 y en la 1 vuelve a Config.
- [ ] **A2.5** ▶ Hacer: cargar 50 mL en 24 h con factor 10 y llegar al Resumen.
  ✔ Debe pasar: el pie dice "Menos de 2 gtt/min: no" y no deja iniciar.
- [ ] **A2.6** ▶ Hacer: cargar una infusión válida, avanzar hasta la pantalla **Ajuste de goteo** y **esperar 30 s sin confirmar**. (El recordatorio es solo en esa pantalla, no en Config ni en el menú de Ajustes.)
  ✔ Debe pasar: el pie dice "Confirmar ajuste" y suena un beep corto cada 10 s.
- [ ] **A2.7** ▶ Hacer: confirmar el ajuste.
  ✔ Debe pasar: el pie y los beeps se van.
- [ ] **A2.8** ▶ Hacer: con goteo real, quedarse en la pantalla de la clave del arranque (sin tocar) más de 30 s y después pasar por Colocación, Config y Resumen.
  ✔ Debe pasar: **no salta ninguna alarma** de "Sin monitoreo" y no queda ningún sonido colgado.

## A3. Una infusión con alarma, clave y cambio de datos: el registro se guarda
- [ ] **A3.1** ▶ Hacer: iniciar una infusión con goteo real y dejarla correr.
  ✔ Debe pasar: por consola aparecen `INICIO`, `DURACION`, `AJUSTE_OK` y una `MUESTRA` cada 15 s.
- [ ] **A3.2** ▶ Hacer: cortar el goteo (cerrar el clamp) hasta que salte el flujo detenido, silenciar con un click y volver a abrir el goteo.
  ✔ Debe pasar: `ALARMA ACTIVA`, `SILENCIADO` y `ALARMA resuelta` en el orden en que ocurrieron.
- [ ] **A3.3** ▶ Hacer: en el Monitoreo mantener 3 s y poner una clave **incorrecta**.
  ✔ Debe pasar: línea `CLAVE_ERROR` en la consola.
- [ ] **A3.4** ▶ Hacer: Ajustes > Cambiar datos > otra duración > confirmar.
  ✔ Debe pasar: línea `CAMBIO_DATOS` y la misma infusión sigue (no se reinicia el contador).
- [ ] **A3.5** ▶ Hacer: contar las gotas a mano durante dos muestras y comparar con el conteo del equipo.
  ✔ Debe pasar: no faltan gotas alrededor de cada muestra (cuando se escribe en la flash).

## A4. Detener: registro pendiente de descarga (descarga final)
- [ ] **A4.1** ▶ Hacer: Ajustes (3 s + clave) > Detener monitoreo > Sí.
  ✔ Debe pasar: queda **"Sin infusion activa / Registro pendiente de descarga / Conectar la app"**, **sin sonido**.
- [ ] **A4.2** ▶ Hacer: esperar 2 minutos mirando la consola.
  ✔ Debe pasar: aparece `FIN` y después una `MUESTRA` **por minuto**, con el minuto creciendo.
- [ ] **A4.3** ▶ Hacer: mantener 3 s y poner 1234.
  ✔ Debe pasar: el menú "Sin infusion activa" (Nueva infusión / Apagar) tiene el pie **"Registro sin descargar"**.
- [ ] **A4.4** ▶ Hacer: elegir Nueva infusión y, en la pregunta, elegir **No**.
  ✔ Debe pasar: muestra "Continuar igual? / Hay un registro sin / descargar" y "No" vuelve al menú. **No elegir "Sí" todavía.**
- [ ] **A4.5** ▶ Hacer: elegir Apagar y en la pregunta elegir **No**.
  ✔ Debe pasar: muestra "Registro sin descargar / Apagar igual?" y "No" vuelve al menú.

## A5. Reposo con el registro pendiente
- [ ] **A5.1** ▶ Hacer: Apagar > "Apagar igual?" > Sí.
  ✔ Debe pasar: tono **descendente**, "Registro guardado" 2 s y la pantalla queda en negro.
- [ ] **A5.2** ▶ Hacer: dar una pulsación corta y girar el encoder.
  ✔ Debe pasar: la pantalla **no** enciende.
- [ ] **A5.3** ▶ Hacer: mantener el botón 3 s.
  ✔ Debe pasar: reinicia (tono ascendente, "Iniciando") y arranca directo en **"Sin infusion activa" con el aviso** de registro pendiente, **sin** alarma de reinicio y **sin pedir otros 3 s**.
- [ ] **A5.4** ▶ Hacer: mirar la consola.
  ✔ Debe pasar: `registro pendiente de descarga` y una línea `REINICIO min=<último minuto guardado>`; el contador sigue desde ahí.
- [ ] **A5.5** ▶ Hacer: repetir el apagado y encendido con el **USB conectado** y después solo con batería.
  ✔ Debe pasar: funciona en los dos casos (el light sleep con el USB es lo más delicado; si se corta el puerto, reconectalo).

## A6. Nueva infusión: "Continuar igual"
- [ ] **A6.1** ▶ Hacer: mantener 3 s + clave > Nueva infusión > "Continuar igual?" > **Sí**.
  ✔ Debe pasar: el aviso desaparece y sigue por Colocación, Config, Resumen y Ajuste.
- [ ] **A6.2** ▶ Hacer: iniciar la infusión.
  ✔ Debe pasar: por consola `INICIO` con un **ID nuevo** (sube de a uno).

## A7. Reinicio con una infusión sin terminar (el equipo ya NO la retoma)
- [ ] **A7.1** ▶ Hacer: con la infusión de A6 en el Monitoreo y 2 muestras escritas, **abrir el interruptor y volver a cerrarlo**.
  ✔ Debe pasar: arranca **sin pedir el botón 3 s**, con el tono ascendente y "Iniciando" (el autotest no bloquea).
- [ ] **A7.2** ▶ Hacer: mirar la pantalla.
  ✔ Debe pasar: **no** retoma (no va al Monitoreo ni cuenta gotas).
- [ ] **A7.3** ▶ Hacer: escuchar y mirar.
  ✔ Debe pasar: suena la alarma EQUIPO_REINICIADO enseguida, con "ALARMA ALTA / SE APAGO / Llamar a enfermeria".
- [ ] **A7.4** ▶ Hacer: mirar la consola.
  ✔ Debe pasar: `registro pendiente de descarga (infusion interrumpida por un reinicio)` y `REINICIO min=<último minuto guardado>`.
- [ ] **A7.5** ▶ Hacer: dar un click para silenciar.
  ✔ Debe pasar: deja de sonar y queda "Sin infusion activa / Registro pendiente de descarga", sin sonido.
- [ ] **A7.6** ▶ Hacer: esperar 20 s sin tocar.
  ✔ Debe pasar: **vuelve a sonar** (re-armado, mientras falte el `OK` de la app).
- [ ] **A7.7** ▶ Hacer: esperar 2 minutos mirando la consola.
  ✔ Debe pasar: una `MUESTRA` por minuto (los minutos se cuentan **desde el reinicio**).
- [ ] **A7.8** ▶ Hacer: empezar otra infusión, entrar al Monitoreo y cortar la alimentación **enseguida** (antes de la primera muestra).
  ✔ Debe pasar: al volver a encender, `REINICIO min=0`.
- [ ] **A7.9** ▶ Hacer: cortar la alimentación mientras estás en Config o Resumen (todavía sin infusión).
  ✔ Debe pasar: al encender pide el botón 3 s y va a la clave, sin alarma ni aviso nuevo.

## A8. Goteo sin monitoreo
- [ ] **A8.1** ▶ Hacer: llegar a "Sin infusion activa" (por ejemplo deteniendo una infusión; el aviso de registro pendiente no suena y no molesta), con goteo real y el clamp abierto, y esperar 20 s.
  ✔ Debe pasar: salta **SIN MONIT.** (prioridad media, "Hay goteo: llamar") con su sonido, y el título y el detalle se ven completos en la pantalla blanca.
- [ ] **A8.2** ▶ Hacer: dar un click.
  ✔ Debe pasar: se silencia y vuelve a la pantalla anterior.
- [ ] **A8.3** ▶ Hacer: cerrar el clamp y esperar 15 s.
  ✔ Debe pasar: la alarma se resuelve sola.
- [ ] **A8.4** ▶ Hacer: con goteo real, pasar por la **clave del arranque**, Colocación, Config y Resumen.
  ✔ Debe pasar: **no debe saltar** en ninguna de esas pantallas aunque caigan gotas.

## A9. Modo Técnico
- [ ] **A9.1** ▶ Hacer: en "Sin infusion activa" mantener 3 s y poner **4321** (también entra con 4321 en la clave del arranque tras un reinicio normal).
  ✔ Debe pasar: entra al Modo Técnico.
- [ ] **A9.2** ▶ Hacer: abrir **Diagnóstico**, inclinar el equipo y comparar la batería con un tester.
  ✔ Debe pasar: base de los dos canales (50 a 2400 mV), ángulo que cambia al inclinar, batería en mV y %, `FW 0.1` y `Reg N` (`*` = sin descargar). `Reg N` coincide con el número de la última línea de la consola.
- [ ] **A9.3** ▶ Hacer: abrir **Probar alarma** y probar las 9 opciones: click para que suene, click o giro para cortarla, pulsación larga para salir.
  ✔ Debe pasar: las 9 suenan; el click suena y para; el giro la corta; la pulsación larga sale.
- [ ] **A9.4** ▶ Hacer: con una infusión en curso, en el Monitoreo mantener 3 s y poner 4321.
  ✔ Debe pasar: se toma como clave incorrecta.
- [ ] **A9.5** ▶ Hacer: entrar al Modo Técnico y no tocar nada 30 s.
  ✔ Debe pasar: reinicia solo.
- [ ] **A9.6** ▶ Hacer: entrar al Modo Técnico y elegir Salir.
  ✔ Debe pasar: reinicia **sin pedir el botón 3 s**.
- [ ] **A9.7** ▶ Hacer: mirar el volcado al arrancar después del reinicio.
  ✔ Debe pasar: `registro: N lineas ...` y las mismas líneas que viste antes del reinicio (el registro sobrevive).
- [ ] **A9.8** ▶ Hacer (**al final**, así el registro queda vacío para la Parte B): Modo Técnico > Borrar registro.
  ✔ Debe pasar: avisa "El registro no fue descargado / Borrar igual?"; con Sí muestra "Borrando registro..." unos 10 s y **sale del Modo Técnico solo**: reinicia (logo, autotest) y va a la clave, sin aviso ni alarma de registro pendiente. Entrar de nuevo al Modo Técnico (4321) y mirar el Diagnóstico: `Reg 0`.

## A10. Regresión: la detección de gotas y la pantalla
- [ ] **A10.1** ▶ Hacer: dejar el equipo ~10 min **sin goteo** con la pantalla encendida.
  ✔ Debe pasar: no aparecen gotas falsas.
- [ ] **A10.2** ▶ Hacer: con goteo real, comparar con el conteo a mano.
  ✔ Debe pasar: el conteo coincide.
- [ ] **A10.3** ▶ Hacer: mirar la pantalla durante toda la prueba.
  ✔ Debe pasar: no se "borronea" ni queda en negro.
- [ ] **A10.4** ▶ Hacer: revisar la consola.
  ✔ Debe pasar: si aparece `fallaron envios a la OLED por I2C`, anotar cuántos y cuándo.
- [ ] **A10.5** ▶ Hacer: mirar la línea `pila libre minima` (sale cada 30 s).
  ✔ Debe pasar: no baja de ~1000 bytes.

---

# Parte B. Bluetooth con nRF Connect

**Configuración:** `USAR_BLE = 1` y `ESCENARIO = ESCENARIO_DESVIO_COLOCACION`. Compilar y grabar (el registro quedó vacío de la Parte A). En el celular, la app **nRF Connect**.

**Nota sobre las órdenes:** la app real termina cada orden en un salto de línea (`\n`). En nRF Connect elegí el formato **Text** y escribí la orden; si no podés agregar el salto de línea no pasa nada: el equipo ejecuta la orden por su cuenta a los 0,5 s sin recibir más datos. Las notificaciones llegan de a 20 bytes, así que **cada línea aparece cortada en varios pedazos**: es lo esperado (la app las junta hasta el salto de línea).

## B1. Conexión y órdenes básicas (registro vacío)
- [ ] **B1.1** ▶ Hacer: encender el equipo y mirar la consola.
  ✔ Debe pasar: `anunciandose como MonitorGoteo-01` y ningún error de Bluetooth.
- [ ] **B1.2** ▶ Hacer: en nRF Connect, escanear.
  ✔ Debe pasar: aparece **`MonitorGoteo-01`** con el servicio `0xFFE0`.
- [ ] **B1.3** ▶ Hacer: tocar CONNECT.
  ✔ Debe pasar: **conecta sin pedir emparejar** (ninguna ventana de PIN) y la consola dice `app conectada`.
- [ ] **B1.4** ▶ Hacer: abrir el servicio `0xFFE0`, buscar la característica `0xFFE1` y activar las **notificaciones**.
  ✔ Debe pasar: la característica existe y las notificaciones quedan activas.
- [ ] **B1.5** ▶ Hacer: escribir `HORA=1762400000` y después `HORA=abc`.
  ✔ Debe pasar: `ACK,HORA` y `ERR,HORA`.
- [ ] **B1.6** ▶ Hacer: escribir `PACIENTE=PAC-0123`; después uno con minúsculas y otro con un solo carácter.
  ✔ Debe pasar: `ACK,PACIENTE` y, los otros dos, `ERR,PACIENTE`.
- [ ] **B1.7** ▶ Hacer: escribir `HOLA`.
  ✔ Debe pasar: `ERR,COMANDO`.
- [ ] **B1.8** ▶ Hacer: escribir `DESCARGAR` y `ESTADO` sin ninguna infusión.
  ✔ Debe pasar: `ERR,SIN_REGISTRO` en los dos.
- [ ] **B1.9** ▶ Hacer: escribir `OK` sin haber descargado antes.
  ✔ Debe pasar: `ERR,OK`.

## B2. Infusión en curso y descarga
- [ ] **B2.1** ▶ Hacer: iniciar una infusión corta, provocar un flujo detenido (silenciarlo y resolverlo), poner una clave incorrecta y hacer un Cambiar datos.
  ✔ Debe pasar: la infusión sigue normal.
- [ ] **B2.2** ▶ Hacer: desde nRF Connect escribir `ESTADO`, esperar un par de minutos y repetirlo.
  ✔ Debe pasar: `ESTADO,<ID>,1,<minuto>,<líneas>` (estado **1**) y el minuto sube.
- [ ] **B2.3** ▶ Hacer: Ajustes > Detener monitoreo > Sí y después escribir `DESCARGAR`.
  ✔ Debe pasar: llegan líneas como estas y cierra con `FIN_TX`:
```
INICIO, ID=1, FG=20, V=500 mL, T=8 h
0 min - Infusión iniciada
12 min - ALARMA Flujo detenido
13 min - Alarma silenciada Flujo detenido
15 min - Alarma resuelta Flujo detenido
16 min - Clave incorrecta
FIN, 640 gotas, 32 mL estimados
FIN_TX,7,18
```
- [ ] **B2.4** ▶ Hacer: contar las líneas recibidas.
  ✔ Debe pasar: el primer número de `FIN_TX` coincide con tu conteo y con el `<líneas>` de `ESTADO`. El segundo es el minuto actual.
- [ ] **B2.5** ▶ Hacer: si hiciste un Cambiar datos, buscar su línea; si hubo un desvío, mirar su texto.
  ✔ Debe pasar: `Cambio de datos T=...` en horas; en un desvío, "alto" o "bajo" correcto.

## B3. El minuto sigue contando después del FIN
(Seguir con la infusión detenida de B2, **sin mandar `OK` todavía**.)
- [ ] **B3.1** ▶ Hacer: escribir `ESTADO`.
  ✔ Debe pasar: `ESTADO,<ID>,0,<minuto>,<líneas>` (estado **0**, terminada) y la pantalla del equipo dice "Registro pendiente de descarga".
- [ ] **B3.2** ▶ Hacer: esperar unos minutos y repetir `ESTADO`.
  ✔ Debe pasar: **el minuto sigue aumentando** (el equipo cuenta el tiempo desde el inicio hasta el OK).
- [ ] **B3.3** ▶ Hacer: escribir `DESCARGAR` y comparar con un `ESTADO` enseguida.
  ✔ Debe pasar: el segundo número de `FIN_TX` es el mismo minuto que informa `ESTADO`.
- [ ] **B3.4** ▶ Hacer: escribir **`OK`** y después `ESTADO` varias veces.
  ✔ Debe pasar: `ACK,OK`; el minuto **ya no cambia** (quedó congelado) y el aviso del equipo **desapareció**.
- [ ] **B3.5** ▶ Hacer: Modo Técnico > Borrar registro.
  ✔ Debe pasar: **ya no avisa** que no fue descargado.

## B4. Infusión interrumpida por un reinicio
- [ ] **B4.1** ▶ Hacer: iniciar una infusión, esperar 2 muestras, **cortar la alimentación** y volver a encender.
  ✔ Debe pasar: suena la alarma de reinicio.
- [ ] **B4.2** ▶ Hacer: conectar nRF Connect y escribir `ESTADO`; esperar un par de minutos y repetirlo.
  ✔ Debe pasar: `ESTADO,<ID>,2,<minuto>,<líneas>` con el estado **2** y los **minutos desde el reinicio** (no desde el inicio); el minuto sube.
- [ ] **B4.3** ▶ Hacer: escribir `DESCARGAR`.
  ✔ Debe pasar: la **última línea antes de `FIN_TX` es `<n> min - REINICIO`** (con el último minuto guardado) y **no hay línea `FIN`**.
- [ ] **B4.4** ▶ Hacer: escribir `OK`.
  ✔ Debe pasar: `ACK,OK`; la alarma EQUIPO_REINICIADO y el aviso del equipo se apagan y el minuto se congela.
- [ ] **B4.5** ▶ Hacer: comparar el ID con el que tenía la infusión antes del corte.
  ✔ Debe pasar: es el mismo.

## B5. Infusión nueva con un registro sin descargar
- [ ] **B5.1** ▶ Hacer: detener una infusión (queda el registro pendiente) y empezar otra con "Continuar igual".
  ✔ Debe pasar: la nueva arranca en el minuto 0 con un **ID nuevo** (sube de a uno).
- [ ] **B5.2** ▶ Hacer: escribir `ESTADO`.
  ✔ Debe pasar: informa la **nueva** infusión; la anterior ya no se puede descargar.

## B6. Robustez
- [ ] **B6.1** ▶ Hacer: pedir `DESCARGAR`, desconectar a mitad, volver a conectar y pedir `DESCARGAR` otra vez.
  ✔ Debe pasar: no se cuelga y la descarga nueva funciona.
- [ ] **B6.2** ▶ Hacer: mandar una orden sin salto de línea.
  ✔ Debe pasar: se ejecuta a los 0,5 s.

## B7. Efecto sobre la detección de gotas (lo más importante)
- [ ] **B7.1** ▶ Hacer: con el BLE anunciando y **sin** conexión, 10 min sin goteo.
  ✔ Debe pasar: no aparecen gotas falsas.
- [ ] **B7.2** ▶ Hacer: con goteo normal, contar a mano antes, durante y después de conectar.
  ✔ Debe pasar: el conteo coincide con el real.
- [ ] **B7.3** ▶ Hacer: con goteo en curso, pedir una `DESCARGAR`.
  ✔ Debe pasar: no se pierden gotas y no aparecen falsas.
- [ ] **B7.4** Si pasa cualquiera de las dos cosas, anotar cuándo ocurre y avisar (se puede bajar la prioridad del Bluetooth).

## B8. Reposo y BLE
- [ ] **B8.1** ▶ Hacer: Apagar > Sí (con el celular mirando el escaneo).
  ✔ Debe pasar: el equipo deja de anunciarse (desaparece de nRF Connect).
- [ ] **B8.2** ▶ Hacer: mantener 3 s para reiniciar.
  ✔ Debe pasar: vuelve a aparecer.

---

# Parte C. Tiempos reales

**Configuración:** `USAR_BLE = 1` y `ESCENARIO = ESCENARIO_NORMAL` (la versión final). Compilar y grabar. Repetir lo esencial con los tiempos de la especificación:
- [ ] **C1** ▶ Hacer: iniciar una infusión y provocar una alarma de prioridad alta y un desvío leve.
  ✔ Debe pasar: aviso a los 5 min (desvío leve) o alarma a los 2 min, re-armado a los 2 min.
- [ ] **C2** ▶ Hacer: con la infusión en curso, cortar la alimentación y volver a encender.
  ✔ Debe pasar: suena EQUIPO_REINICIADO, se re-arma a los **2 min** y queda el aviso de registro pendiente.
- [ ] **C3** ▶ Hacer: llegar al Ajuste de goteo y esperar 10 min sin confirmar.
  ✔ Debe pasar: el pie dice "Confirmar ajuste" con un beep por minuto.
- [ ] **C4** ▶ Hacer: 10 min sin goteo con la pantalla encendida.
  ✔ Debe pasar: sin gotas falsas.
- [ ] **C5** ▶ Hacer: mirar la consola.
  ✔ Debe pasar: **no** imprime las líneas del registro ni el volcado.

---

# Parte D. Con la app

**Configuración:** `USAR_BLE = 1` y `ESCENARIO = ESCENARIO_NORMAL`. La app descarga siempre el **registro completo de la última infusión** y reemplaza al anterior.
- [ ] **D1** ▶ Hacer: conectar la app al equipo.
  ✔ Debe pasar: la pantalla de descarga muestra el nombre del equipo, el **ID de la infusión** y su estado (en curso, terminada o interrumpida), a partir de `ESTADO`.
- [ ] **D2** ▶ Hacer: con la infusión andando desde hace unos minutos, conectar.
  ✔ Debe pasar: la app propone la hora de inicio como `hora del celular − minuto actual` y no como la hora de conexión.
- [ ] **D3** ▶ Hacer: descargar y revisar el INICIO.
  ✔ Debe pasar: factor de goteo, volumen y duración coinciden con lo cargado en el Resumen, y `0 min - Infusión iniciada`.
- [ ] **D4** ▶ Hacer: revisar las alarmas y avisos.
  ✔ Debe pasar: cada activación, silenciado (con el nombre) y resolución, con el minuto correcto. En los desvíos, "alto" o "bajo" correctos.
- [ ] **D5** ▶ Hacer: revisar las claves.
  ✔ Debe pasar: `Clave correcta`, `Clave incorrecta` y `Acceso técnico`, con la cantidad de intentos que hiciste.
- [ ] **D6** ▶ Hacer: revisar el cambio de datos.
  ✔ Debe pasar: `Cambio de datos T=...` con la duración nueva.
- [ ] **D7** ▶ Hacer: detener la infusión y descargar.
  ✔ Debe pasar: el FIN con las gotas y los mL estimados.
- [ ] **D8** ▶ Hacer: descargar una infusión interrumpida por un reinicio.
  ✔ Debe pasar: termina en `<n> min - REINICIO` sin `FIN`; la app la muestra como "Interrumpida por un reinicio" y las horas como estimadas (`hora del reinicio − minuto de la línea REINICIO`).
- [ ] **D9** ▶ Hacer: descargar dos veces la misma infusión.
  ✔ Debe pasar: la segunda trae de nuevo el registro completo (con lo nuevo al final) y la app reemplaza al anterior sin duplicar la infusión.
- [ ] **D10** ▶ Hacer: descargar con `OK` al final.
  ✔ Debe pasar: la app guarda la hora de inicio en esa descarga.
- [ ] **D11** ▶ Hacer: después del `OK`, Modo Técnico > Borrar registro.
  ✔ Debe pasar: ya no avisa que no fue descargado.
- [ ] **D12** ▶ Hacer: provocar "Goteo sin monitoreo" y descargar.
  ✔ Debe pasar: se ve con su ícono (prioridad media).

**Lo que se guarda en el equipo pero NO viaja por BLE** (se verifica por consola con un escenario de prueba): `AJUSTE_OK`, las muestras de cada minuto, la línea `DESCARGA` y la alarma `Equipo reiniciado`.

---

## Qué anotar si algo falla
- Qué pantalla estabas viendo y qué hiciste justo antes.
- El log de la consola de ese momento.
- La configuración (`USAR_BLE` y `ESCENARIO`) con la que estabas.

## Apéndice. Repaso de las etapas 1 a 3 (ya probadas)

Opcional, para confirmar con la versión final que sigue todo igual.

**Flujo básico:**
- [ ] ▶ Mantener el botón 3 s. ✔ Tono ascendente, "Iniciando" y clave.
- [ ] ▶ Seguir el paso a paso. ✔ Colocación (el click solo con el equipo derecho), Config, Resumen, Ajuste de goteo y Monitoreo.
- [ ] ▶ Girar y hacer click. ✔ Tic corto con el giro y pitido con el click.
- [ ] ▶ Ajustes (3 s + clave). ✔ Cambiar datos (solo la duración, sin reiniciar el volumen), Detener monitoreo con confirmación, Apagar y Volver.
- [ ] ▶ Quedarse en "Sin infusión activa" 60 s. ✔ Se bloquea y se desbloquea con 3 s + clave; ofrece Nueva infusión o Apagar.

**Alarmas:**
- [ ] ✔ Flujo detenido, fin de sachet y volumen completo.
- [ ] ✔ Desvío: porcentaje arriba a la derecha y `Obj: ... gtt/min` abajo. Aviso a más de 10 % y alarma a más de 30 %.
- [ ] ✔ Colocación: el click en su pantalla la silencia y se resuelve sola al volver al rango.
- [ ] ✔ Silenciado con un click y re-armado a los 2 min en las de prioridad alta.
- [ ] ✔ Batería: aviso a 20 %, alarma media a 10 % y alta a 5 % (se probó con `test_bateria`).

## Pendiente (no se prueba todavía)
- Vinculación segura del BLE (passkey): el prototipo no la usa.
- Precisión de segundos en los tiempos de respuesta de las alarmas (hoy minutos enteros).
- Barra superior del Monitoreo con la batería y el factor de goteo, y el ícono de BLE.
- Curva de batería medida y autonomía.
- Límites definitivos del nivel base del sensor óptico (hoy 50 y 2400 mV, provisorios).
