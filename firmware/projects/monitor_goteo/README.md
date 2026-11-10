# monitor_goteo

Firmware final del monitor de goteo (ESP32-C3): detección de gotas con BPW34, pantalla OLED y encoder, alarmas con buzzer, ángulo con ADXL345, batería, registro de la infusión en la flash y Bluetooth con la app de la enfermera. Las especificaciones (alarmas e inicio y navegación) están en el Drive; el contrato con la app está en `documentación/cambios_para_la_app.md` y la lista de pruebas en `documentación/pruebas_firmware_final.md`.

Interruptores al principio de `main/monitor_goteo.c`: `USAR_BLE` (1 = con Bluetooth) y `ESCENARIO` (`ESCENARIO_NORMAL` = versión final; los demás acortan los tiempos para probar). Para commitear: `USAR_BLE = 1` y `ESCENARIO_NORMAL`.

## Decisiones de diseño vigentes

### Descarga final obligatoria y minuto que sigue contando (10/11/2026)

La descarga **obligatoria** del registro es la **final**, cuando termina la infusión. La app calcula la hora de inicio como `hora del celular en la descarga − minuto actual del equipo`, así que el equipo sigue contando el tiempo después del fin.

- **Después del FIN** (Detener monitoreo o Apagar con infusión): el contador de minutos desde el inicio **no se detiene**. Sigue hasta que la app manda `OK` (registro descargado). Mientras tanto se guarda una muestra por minuto en el registro, para poder continuar si el equipo se reinicia.
- **`ESTADO` y `FIN_TX`** informan ese minuto actual (tiempo total desde el inicio). El estado vale `0` si la infusión terminó.
- **Al recibir `OK`:** se marca el registro como descargado y se deja de contar (el último minuto queda congelado).
- **Reinicio con un registro terminado y sin descargar:** el contador sigue desde el último minuto guardado y se agrega la línea `REINICIO` (como no hay RTC, el tiempo apagado se pierde).
- **Aviso "Registro pendiente de descarga · Conectar la app"** en la OLED, desde que se detiene el monitoreo hasta recibir el `OK`. Sin sonido: es un aviso, no una alarma.
- **Infusión nueva sin descargar la anterior:** "Hay un registro sin descargar. ¿Continuar igual?". Si sigue, la infusión anterior deja de estar pendiente y ya no se puede descargar.
- **Apagar con el registro sin descargar:** el mismo aviso antes de apagar. Si igual se apaga, el registro se conserva y el tiempo apagado se trata como un `REINICIO`.
- Sin cambios: el formato de las líneas, el servicio `0xFFE0`/`0xFFE1` y las órdenes `HORA`, `ESTADO`, `PACIENTE`, `DESCARGAR` y `OK`. Solo cambia cuándo deja de correr el minuto actual.

### El equipo ya NO retoma la infusión tras un reinicio (10/11/2026)

Se quitó la reanudación automática.

- **Al encender con una infusión sin terminar:**
  - Se agrega al registro la línea `<último minuto guardado> min - REINICIO` y la infusión se da por terminada como **INTERRUMPIDA** (no se escribe línea `FIN`). No se retoma el monitoreo ni el conteo de gotas.
  - Se activa la alarma **EQUIPO_REINICIADO**: prioridad alta, con buzzer, mensaje "El equipo se apagó — llamar a enfermería". Se re-arma a los 2 min si se silencia. Está en el mismo nivel de prioridad que FLUJO_DETENIDO y FIN_SACHET. Reemplaza a la alarma FALLA_EQUIPO.
  - La alarma sigue activa hasta que la app manda `OK` o la enfermera elige "Continuar igual" y empieza una infusión nueva.
  - Se pide la descarga igual que al finalizar ("Registro pendiente de descarga") hasta recibir el `OK`.
  - El equipo cuenta los **minutos desde el reinicio** hasta recibir el `OK`.
  - El autotest no bloquea el arranque en este caso (la alarma tiene que sonar aunque algo falle) y el equipo queda en "Sin infusión activa" (pantalla bloqueada; para operar hace falta la clave).
- **Protocolo BLE:** en `ESTADO` el campo de estado tiene tres valores: `1` = en curso, `0` = terminada, `2` = interrumpida por reinicio. Si está interrumpida, el minuto actual de `ESTADO` y de `FIN_TX` son los **minutos desde el reinicio** (no desde el inicio de la infusión). `DESCARGAR` de una infusión interrumpida termina en la línea `REINICIO`. El resto del protocolo no cambia.
- **Infusión nueva:** si se intenta configurar sin descargar la interrumpida, "Hay un registro sin descargar. ¿Continuar igual?". La nueva arranca en el minuto 0 con un número de infusión nuevo.

Decisiones tomadas con la usuaria el 10/11/2026: FALLA_EQUIPO se reemplaza por EQUIPO_REINICIADO; la alarma termina con el `OK` o con una infusión nueva; tras un reinicio el equipo va a "Sin infusión activa" con la alarma; el minuto después del FIN se guarda con una muestra por minuto en el registro.
