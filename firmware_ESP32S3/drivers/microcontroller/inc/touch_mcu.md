# touch_mcu

Driver de detección capacitiva (touch) para el ESP32-S3. Es el único driver de este set que no viene de la cátedra — se armó desde cero para el sensor de gota del monitor de goteo.

## Qué hace

Envuelve el periférico de touch integrado del ESP32-S3 con funciones simples:

| Función | Qué hace |
|---|---|
| `TouchInit(canal, threshold)` | Inicializa el periférico (una sola vez) y habilita un canal con un umbral de detección. |
| `TouchReadRaw(canal)` | Lee el valor crudo (raw) del contador capacitivo de un canal. |
| `TouchDetect(canal)` | Devuelve `true` si el canal está "tocado" según el `threshold` configurado. |
| `TouchActivInt(callback, args)` | Registra una interrupción global (el hardware tiene un solo ISR para todos los canales, no uno por canal). |
| `TouchSetSensitivity(nivel)` | Ajusta el swing de voltaje de carga/descarga. Nivel 0 = menos sensible/menos ruido, nivel 2 = más sensible/más ruido. |
| `TouchShieldEnable(canal_guarda, nivel_shield)` / `TouchShieldDisable()` | Prende/apaga el blindaje ("shield") de fábrica contra humedad. |
| `TouchDeinit()` | Apaga el periférico. |

## Por qué existe

Para el monitor de goteo, dos electrodos de cinta de cobre rodean la cámara de goteo (uno activo conectado a un GPIO de touch, el otro a GND), formando un capacitor. Cuando una gota de agua pasa entre ellos, sube la capacitancia que "ve" el canal de touch, y eso aparece como una variación en el valor crudo (`TouchReadRaw`).

El periférico de touch del ESP32 mide **auto-capacitancia**: cada canal mide la capacitancia de su propio electrodo contra la referencia interna del chip, no la capacitancia *entre* dos pines. Por eso el segundo electrodo va a GND y no a otro canal de touch.

## Mapeo de canales a GPIO

El ESP32-S3 tiene 15 canales de touch (T0 a T14):
- **T0** es interno, no tiene pin externo (se usa solo para reducción de ruido, no está expuesto en este driver).
- **T1 a T14 se corresponden 1 a 1 con GPIO1 a GPIO14** (canal N = GPIO N). Confirmado contra el SDK de Espressif (`soc/touch_sensor_channel.h`).

## El shield (blindaje contra humedad)

- El canal de shield está **fijo por hardware en GPIO14 (Touch14)** — no se puede mover a otro pin.
- Reduce falsos positivos causados por una película de humedad/condensación sobre el electrodo sensor, excitando un anillo de guarda al mismo potencial que el sensor.
- **En la placa Super Mini usada en este proyecto, GPIO14 no es un pin de borde soldable normal** — solo existe como una vía interna en el medio de la PCB (una de esas hileras de "circulitos" chiquitos). Se pudo usar soldando un cable fino directo a esa vía; en otras unidades/revisiones puede no ser posible.
- Requiere haber llamado `TouchInit()` en al menos un canal antes de habilitarlo.

## Ajustando la sensibilidad

`TouchSetSensitivity()` controla el swing de voltaje de carga/descarga (a más swing, más reacciona el contador ante cambios chicos de capacitancia, pero también capta más ruido):

| Nivel | Uso sugerido |
|---|---|
| 0 | Punto de partida — menos ruido en reposo |
| 1 | Intermedio |
| 2 | Máxima sensibilidad — usar solo si con nivel 0/1 no se distingue bien la gota del ruido |

Se ajusta de forma empírica: subir de a un nivel mirando cuánto ruido aparece en reposo vs. cuánto se nota el paso de la gota.
