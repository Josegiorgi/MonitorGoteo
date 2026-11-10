# Cambios para la app (contrato v3 con el firmware)

Documento para pasarle a la app Android. Resume las decisiones tomadas entre el firmware y `para_firmware.md` (el contrato original) y **todo lo que la app tiene que cambiar**. Lo marcado como *definido* ya está decidido; lo marcado como *propuesta del firmware* es el formato exacto que implementa el firmware y la app debe adoptar.

**Cambios del 10/11/2026 respecto de la versión anterior de este documento:** el equipo **ya no retoma** la infusión tras un reinicio (queda *interrumpida*), la descarga obligatoria es la **final** y el minuto actual **sigue contando después del fin** hasta que la app manda `OK`. Las secciones 1 y 2 explican los dos cambios.

## Resumen de las decisiones

| # | Tema | Decisión |
|---|---|---|
| 1 | **Descarga final y minuto que sigue contando** | La descarga obligatoria es la **final**. Después del fin el minuto actual **no se detiene**: sigue hasta el `OK`. La app calcula la hora de inicio con `hora del celular − minuto actual` |
| 2 | **Reinicios** | El equipo **no retoma** la infusión. Queda **interrumpida**: se anota `REINICIO`, suena una alarma en el equipo y se cuenta el minuto **desde el reinicio** hasta el `OK`. `ESTADO` informa `2` |
| 3 | Líneas extra | El equipo manda además las **claves** y el **cambio de duración**. No manda las muestras de cada minuto ni otros eventos internos |
| 4 | `PACIENTE=<codigo>` | El equipo lo **acepta y no lo guarda** (responde `ACK`) |
| 5 | Descarga | **Registro completo** de la última infusión, como ya hace la app |
| 6 | Canal BLE | **Una sola característica** para órdenes y registro |
| 7 | Confirmación | La app manda **`OK`** al terminar; el equipo marca el registro como descargado y **deja de contar** |
| 8 | **Hora de inicio** | La app **no** la anota al conectarse: la calcula con `ESTADO` y la enfermera la confirma o la corrige |

## 1. Descarga final y minuto que sigue contando

La descarga **obligatoria** es la que se hace **al terminar la infusión**. La app calcula la hora de inicio como `hora del celular en la descarga − minuto actual del equipo`, así que el equipo sigue contando el tiempo **después** del fin.

**Qué hace el equipo:**
- Cuando se detiene el monitoreo (se escribe la línea `FIN`), el contador de minutos desde el inicio **no se detiene**: sigue corriendo hasta que la app manda `OK`.
- `ESTADO` y `FIN_TX` informan ese minuto actual (el tiempo total desde el inicio). En `ESTADO` el segundo dato vale `0` porque la infusión terminó.
- Al recibir `OK` el equipo marca el registro como descargado y **deja de contar**: el último minuto queda congelado.
- Mientras falte el `OK`, la pantalla del equipo muestra "Registro pendiente de descarga · Conectar la app" (sin sonido).
- Si el equipo se reinicia con un registro terminado y sin descargar, el contador sigue desde el último minuto guardado y se anota `REINICIO` (el tiempo que estuvo apagado no se conoce). Esa línea `REINICIO` queda **después** del `FIN`, así que **no viaja** en la descarga (la descarga termina en `FIN`): la hora calculada es aproximada, con un error igual al tiempo apagado.

**Cómo calcula la app la hora de inicio:**

| Estado (`ESTADO`) | Cálculo |
|---|---|
| `1` en curso | `hora_inicio = hora_celular − minuto_actual` |
| `0` terminada, sin `OK` todavía | `hora_inicio = hora_celular − minuto_actual`. La hora del `FIN` es `hora_inicio + minuto de la línea FIN` |
| `2` interrumpida | Ver la sección 2 |

**Importante:** después del `OK` el minuto queda **congelado**. En una conexión posterior, `hora del celular − minuto` ya **no** da la hora de inicio. Por eso la app tiene que calcular y **guardar** la hora de inicio en la descarga que termina con `OK`.

## 2. Reinicios: la infusión queda interrumpida

El equipo **ya no retoma** la infusión después de un reinicio (corte de batería, apagado por el interruptor, etc.).

**Qué hace el equipo al encender con una infusión sin terminar:**
- Escribe en el registro la línea `<último minuto guardado> min - REINICIO` y da la infusión por terminada como **interrumpida**. **No escribe línea `FIN`.**
- No retoma el monitoreo ni el conteo de gotas.
- Activa en el equipo la alarma **EQUIPO_REINICIADO** (prioridad alta, con buzzer: "El equipo se apagó — llamar a enfermería"). Se re-arma a los 2 minutos si se silencia y termina con el `OK` o cuando la enfermera empieza una infusión nueva. Es una alarma **del equipo**: no viaja por BLE.
- Pide la descarga igual que al finalizar ("Registro pendiente de descarga") hasta recibir el `OK`.
- Cuenta los **minutos desde el reinicio** hasta recibir el `OK`.

**Qué ve la app:**
- `ESTADO` responde con el estado `2`, y el minuto actual de `ESTADO` y de `FIN_TX` son los **minutos desde el reinicio** (no desde el inicio de la infusión).
- `DESCARGAR` termina en la línea `REINICIO` (es la última línea antes de `FIN_TX`). No hay línea `FIN`.
- Si el equipo se reinicia más de una vez sin descargar, la descarga termina en el **último** `REINICIO` y el minuto se cuenta desde ese último reinicio.

```
INICIO, ID=21, FG=60, V=250 mL, T=4 h
0 min - Infusión iniciada
60 min - ALARMA Colocación
72 min - Alarma resuelta Colocación
80 min - REINICIO                      <- último minuto guardado antes del corte; no hay FIN
FIN_TX,6,25                            <- 25 = minutos desde el reinicio
```

**Cómo calcula la app las horas de una infusión interrumpida:**
- `hora_reinicio = hora_celular − minuto_actual` (los minutos desde el reinicio).
- `hora_inicio = hora_reinicio − minuto de la línea REINICIO` (aquí 80). Es **aproximada**: el equipo no sabe cuánto estuvo apagado, y ese tiempo no se cuenta.
- Las horas de los eventos anteriores al `REINICIO` se calculan con ese `hora_inicio` y quedan **estimadas**.
- Mostrar la infusión como **"Interrumpida por un reinicio"**, no como finalizada.

**Infusión nueva sin descargar la anterior:** el equipo pregunta "Hay un registro sin descargar. ¿Continuar igual?". Si la enfermera sigue, la nueva arranca en el minuto 0 con un ID nuevo y la anterior **ya no se puede descargar**.

## 3. Formato del registro (propuesta del firmware)

Texto UTF-8, una línea por evento, terminada en `\n`. Los tiempos son minutos enteros desde el inicio.

```
INICIO, ID=17, FG=20, V=500 mL, T=8 h
0 min - Infusión iniciada
135 min - ALARMA Flujo detenido
137 min - Alarma silenciada Flujo detenido
139 min - Alarma resuelta Flujo detenido
150 min - AVISO Desviación leve alto
158 min - Alarma resuelta Desviación leve alto
200 min - Clave incorrecta
201 min - Clave correcta
220 min - Cambio de datos T=6 h
432 min - AVISO Próximo a finalizar
FIN, 9620 gotas, 481 mL estimados
```

### Líneas que la app ya conoce (sin cambios)
`INICIO, ID=…`, `<n> min - Infusión iniciada`, `<n> min - ALARMA <nombre>`, `<n> min - AVISO <nombre>`, `FIN, <gotas> gotas, <mL> mL estimados`.

### Cambios en líneas existentes
- **`REINICIO` ahora lleva su minuto:** `<n> min - REINICIO`, donde `n` es el último minuto guardado antes del corte (antes era solo `REINICIO`). Solo viaja en las infusiones **interrumpidas**, y es siempre la última línea antes de `FIN_TX`.
- **`Alarma silenciada <nombre>`:** el firmware agrega el nombre de la alarma (igual que ya lee en `Alarma resuelta`). Aceptar el nombre opcional también en `silenciada`.
- **`Alarma resuelta <nombre>`:** siempre lleva el nombre. **También se usa para resolver un aviso** (los avisos se registran activados y resueltos). La app tiene que emparejar por nombre aunque el evento abierto sea un `AVISO`.
- **`T` en `INICIO`:** es la duración que se cargó al inicio. Si hubo `Cambio de datos`, esa duración ya no es la vigente (ver líneas nuevas).

### Líneas NUEVAS que la app tiene que reconocer
| Línea | Significado | Sugerencia |
|---|---|---|
| `<n> min - Clave correcta` | Se ingresó la clave clínica | Mostrar en la auditoría como "Acceso" |
| `<n> min - Clave incorrecta` | Intento fallido | Marcar para revisar: puede indicar un intento de modificar la infusión |
| `<n> min - Acceso técnico` | Ingresó soporte técnico | Idem |
| `<n> min - Cambio de datos T=<h> h` | La duración programada cambió a `<h>` horas (decimal con punto, por ejemplo `7.5`) | Actualizar la duración vigente y mostrar el cambio |

Ninguna de estas debe contarse como "no reconocida".

### Lo que el equipo NO manda
Las muestras de cada minuto, el ajuste confirmado y el evento de descarga. Se guardan en el equipo, pero no viajan por BLE. Tampoco viaja la alarma `Equipo reiniciado` (la descarga de una infusión interrumpida termina en `REINICIO`).

### Nombres exactos de los eventos (tabla actualizada)
| Evento | Texto | Tipo |
|---|---|---|
| VOLUMEN_COMPLETO | `Volumen completo` | Alarma alta |
| FLUJO_DETENIDO | `Flujo detenido` | Alarma alta |
| FIN_SACHET | `Fin de sachet` | Alarma alta |
| COLOCACION | `Colocación` | Alarma alta |
| BATERIA_CRITICA (≤ 5 %) | `Batería crítica` | Alarma alta |
| DESVIACION_FUERTE (**> 30 %**) | `Desviación fuerte alto` / `Desviación fuerte bajo` | Alarma media |
| **GOTEO_SIN_MONITOREO** *(nueva)* | `Goteo sin monitoreo` | **Alarma media** |
| BATERIA_BAJA (≤ 10 %) | `Batería baja` | Alarma media |
| DESVIACION_LEVE (> 10 %) | `Desviación leve alto` / `Desviación leve bajo` | Aviso |
| PROXIMO_A_FINALIZAR (≥ 90 %) | `Próximo a finalizar` | Aviso |
| BATERIA_AVISO (≤ 20 %) | `Batería aviso` | Aviso |

`Falla del equipo` **ya no existe** (se reemplazó por la alarma del equipo EQUIPO_REINICIADO, que no viaja por BLE).

**Qué cambia en la app:**
- Agregar `Goteo sin monitoreo` (**media**, porque hoy la app solo considera media a "Desviación fuerte" y "Batería baja") y elegir su ícono.
- El umbral del desvío fuerte es **30 %** (no 25 %): corregirlo donde la app lo muestre.

### Reglas que el firmware cumple (confirmadas)
- Una alarma alta silenciada **no escribe una nueva línea `ALARMA`** al re-armarse a los 2 minutos.
- Si una alarma se resuelve sola (por ejemplo la colocación), se escribe igual `Alarma resuelta <nombre>`.

## 4. Bluetooth (propuesta del firmware)

### Canal
- Servicio `0xFFE0` con **una sola característica `0xFFE1`**, usada en los dos sentidos: la app **escribe** en ella las órdenes y el equipo **notifica** por ella el registro y las respuestas.
- La app tiene que **activar las notificaciones** de esa característica al conectarse.
- El equipo se anuncia con el nombre **`MonitorGoteo-XX`** (XX = número del equipo, por ejemplo `MonitorGoteo-01`). La app puede filtrar la búsqueda por ese nombre.
- Solo se admite **una conexión a la vez**. El equipo no envía nada si nadie está conectado.

### Límites del driver (importante para la app)
- Las notificaciones van de a **20 bytes**: una línea larga llega **cortada en varios pedazos**. La app debe **juntar los pedazos hasta encontrar el `\n`** y recién ahí interpretar la línea.
- **No** se negocia un MTU de 247: la app no debe depender de eso.
- Velocidad aproximada: **1 KB/s**. Un registro típico baja en pocos segundos.
- Las órdenes que manda la app también deben ir en pedazos de **hasta 20 bytes** y terminar con `\n`. El equipo las junta hasta el `\n`.

### Órdenes de la app al equipo (terminadas en `\n`)
| Orden | Qué hace el equipo | Respuesta |
|---|---|---|
| `HORA=<epoch_s>` | La acepta (de momento no la usa para el registro) | `ACK,HORA` |
| `PACIENTE=<codigo>` | Valida el formato (2 a 20 caracteres: mayúsculas, números y guiones) y **no lo guarda** | `ACK,PACIENTE` o `ERR,PACIENTE` si el formato es inválido |
| `ESTADO` | Informa de la última infusión sin descargar nada (ver más abajo) | `ESTADO,<ID>,<estado>,<minuto_actual>,<líneas>` o `ERR,SIN_REGISTRO` |
| `DESCARGAR` | Envía el registro completo de la última infusión | Las líneas del registro y al final `FIN_TX,…` |
| `OK` | La app confirma que la descarga llegó completa: el equipo marca el registro como descargado y **deja de contar el minuto** | `ACK,OK` |

Cualquier otra orden: `ERR,COMANDO`. Si el equipo nunca tuvo una infusión: `ERR,SIN_REGISTRO`.

### Estado de la infusión (`ESTADO`) y hora de inicio

La app manda `ESTADO` apenas se conecta, **antes** de descargar. Ejemplo de respuesta:
```
ESTADO,17,1,90,23
```
| Campo | Significado |
|---|---|
| `17` | ID de la última infusión (la app lo muestra en la pantalla de descarga, junto al nombre del equipo) |
| `1` | **Estado:** `1` = en curso, `0` = terminada, `2` = interrumpida por un reinicio |
| `90` | **Minuto actual.** En curso o terminada: minutos desde el inicio de la infusión (**sigue contando después del fin hasta el `OK`**). Interrumpida: minutos **desde el reinicio**. Después del `OK` queda congelado |
| `23` | Cantidad de líneas que enviaría una descarga ahora (coincide con el primer número de `FIN_TX`) |

**Criterio para la hora de inicio:**
- La app **no** anota la hora de inicio al conectarse, porque el equipo puede venir corriendo desde antes.
- En curso o terminada: `hora_inicio = hora_del_celular − minuto_actual`. Ejemplo: se conecta a las 11:30 y el equipo dice `90` → la infusión empezó a las 10:00. Para una interrumpida, ver la sección 2.
- Se la **muestra a la enfermera para que la confirme o la corrija** antes de guardarla.
- **"Minuto 0"** es el momento en que la enfermera **confirmó el Ajuste de goteo**, no cuando entró a esa pantalla. El error es de ±1 minuto (los tiempos son minutos enteros).
- Si el ID que informa `ESTADO` no coincide con ninguna infusión abierta en la app, es una infusión que el equipo ya venía haciendo: la app avisa "esta infusión lleva N min en marcha, inició aprox. a las HH:MM" antes de asociarla a un paciente.

### Cierre de la descarga
```
FIN_TX,<cantidad_de_lineas>,<minuto_actual>
```
- `<cantidad_de_lineas>`: cuántas líneas del registro envió antes de esta (incluye `INICIO` y, si terminó, `FIN`; si fue interrumpida, termina en `REINICIO`). La app verifica que coincida con las que recibió.
- `<minuto_actual>`: el mismo minuto que informa `ESTADO` (desde el inicio, o desde el reinicio si está interrumpida). La app lo usa en la fórmula `hora_evento = hora_descarga − (minuto_actual − minuto_evento)` cuando el estado es `1` o `0`.

### Secuencia sugerida
1. La app se conecta y activa notificaciones.
2. Espera ~1 s y manda `HORA=<epoch_s>`. Recibe `ACK,HORA`.
3. Manda `ESTADO`. Recibe `ESTADO,<ID>,<estado>,<minuto_actual>,<líneas>` (o `ERR,SIN_REGISTRO` si el equipo nunca tuvo una infusión). Muestra el nombre del equipo y el ID, el estado (en curso, terminada o interrumpida), y calcula la hora de inicio.
4. Si es una infusión nueva, manda `PACIENTE=<codigo>`. Recibe `ACK,PACIENTE`.
5. La enfermera toca Descargar: la app manda `DESCARGAR` y junta las líneas hasta `FIN_TX`.
6. Si la cuenta coincide, calcula y **guarda la hora de inicio** y manda `OK`. Recibe `ACK,OK` y el equipo deja de contar. Si no coincide, **no** manda `OK`: puede reintentar con `DESCARGAR`.

### ID de infusión (confirmado)
`ID=<n>` es único por infusión. El equipo lo incrementa en cada infusión nueva (contador guardado en flash) y **no cambia** con `Cambio de datos`. La parte del equipo se identifica con el nombre BLE (`MonitorGoteo-XX`): la app debe tratar la infusión como el par **equipo + ID**, porque el contador de dos equipos puede coincidir.

## 5. Correcciones al documento `para_firmware.md`
- Sección 6, "Limitaciones": el registro no está en LittleFS sino en una partición propia de la flash que guarda varias infusiones. Sigue sin haber reanudación automática: tras un reinicio la infusión queda **interrumpida** (sección 2 de este documento).
- Sección 2: la tabla del desvío fuerte es **> 30 %**.
- Sección 2: `REINICIO` se escribe como `<n> min - REINICIO` y los minutos posteriores **no** vuelven a empezar de cero: después de un reinicio ya no hay más líneas de la infusión (queda interrumpida).
- Sección 4: reemplazar las dos características por la única `0xFFE1`, y los comandos por la tabla de este documento.
- Sección 5: los puntos 1, 3, 4 y 5 quedan **resueltos** con este documento.

## 6. Lista de cambios para la app

1. **Parser**
   - [ ] Reconocer `<n> min - REINICIO` (con el minuto). Es la última línea de una infusión **interrumpida**, que **no** tiene línea `FIN`.
   - [ ] Reconocer `Clave correcta`, `Clave incorrecta`, `Acceso técnico` y `Cambio de datos T=…`.
   - [ ] Aceptar el nombre opcional en `Alarma silenciada <nombre>`.
   - [ ] `Alarma resuelta <nombre>` también resuelve un `AVISO`.
   - [ ] Agregar `Goteo sin monitoreo` (media), con su ícono. Quitar `Falla del equipo`.
   - [ ] Leer el tercer dato de `FIN_TX` (minuto actual).
   - [ ] La duración vigente se actualiza con `Cambio de datos`.
2. **Estado y horas**
   - [ ] `ESTADO` con tres estados: `1` en curso, `0` terminada, `2` interrumpida.
   - [ ] Calcular la hora de inicio: `hora del celular − minuto actual` (en curso y terminada).
   - [ ] Para una **interrumpida**: `hora_reinicio = hora del celular − minuto actual` y `hora_inicio = hora_reinicio − minuto de la línea REINICIO`. Marcar las horas como estimadas.
   - [ ] **Guardar la hora de inicio** en la descarga que termina con `OK` (después del `OK` el minuto queda congelado).
   - [ ] Mostrar una infusión interrumpida como "Interrumpida por un reinicio", no como finalizada.
   - [ ] Mostrar la hora de inicio calculada para que la enfermera la confirme o la corrija. No anotar la hora al conectar.
3. **Bluetooth**
   - [ ] Servicio `0xFFE0` y una sola característica `0xFFE1` para escribir y para recibir.
   - [ ] Activar notificaciones al conectar.
   - [ ] Juntar los pedazos de 20 bytes hasta el `\n` antes de interpretar una línea.
   - [ ] Enviar las órdenes en pedazos de hasta 20 bytes terminados en `\n`.
   - [ ] Filtrar por el nombre `MonitorGoteo-`.
   - [ ] No depender de un MTU grande.
4. **Órdenes y respuestas**
   - [ ] `HORA=<epoch_s>` al conectar.
   - [ ] `ESTADO` al conectar, y mostrar el ID, el equipo y el estado en la pantalla de descarga.
   - [ ] `PACIENTE=<codigo>` al iniciar una infusión nueva.
   - [ ] `DESCARGAR` y verificar la cuenta con `FIN_TX`.
   - [ ] Mandar `OK` solo si la cuenta coincide. El equipo deja de contar el minuto.
   - [ ] Manejar `ACK,…` y `ERR,…` (incluidos `ERR,COMANDO` y `ERR,SIN_REGISTRO`).
5. **Identificación**
   - [ ] La infusión se identifica por equipo + ID.
6. **Documentación de la app**
   - [ ] Aplicar las correcciones de la sección 5.

## Pendiente a futuro
- **Precisión de segundos** en los tiempos de respuesta: hoy son minutos enteros. El firmware ya guarda un campo libre en cada línea, así que se podría agregar más adelante sin cambiar el registro, pero habría que cambiar el formato de texto y el parser.
- **Vinculación segura** (passkey) para que solo la app del servicio pueda leer.
- **Reinicio con la infusión ya terminada:** la línea `REINICIO` queda después del `FIN` y no viaja. Si se quisiera que la app marque esa hora como estimada, habría que agregarla a la descarga.
