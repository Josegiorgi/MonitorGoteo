# prueba_adxl345

Lee el acelerómetro ADXL345 por I2C y muestra los ejes y los ángulos, para ver, según cómo quede colocado en el dispositivo, cuál es el que sigue la inclinación que interesa medir.

## Conexiones

| ADXL345 | ESP32-C3 |
|:-------:|:--------:|
| SCL | GPIO_7 |
| SDA | GPIO_21 |
| GND | GND |
| Alimentación | 3V3 |
| CS | 3V3 (modo I2C) |
| SDO | GND (dirección 0x53) |

- **SDO no puede quedar sin conectar:** de él depende la dirección I2C (a GND, 0x53; a alimentación, 0x1D). El programa prueba las dos.
- **Alimentación:** el chip admite como máximo 3.6 V. Confirmar con el esquema del módulo cuál de sus pines de alimentación llega al chip antes de conectar.
- Si el chip no responde, el programa avisa cada segundo y sigue intentando: se puede corregir el cableado sin volver a cargar.

## Ejes de este módulo

Según la serigrafía del módulo, con el chip horizontal (apaisado): **Y apunta hacia el lado de los pines, X a lo largo del lado más largo y Z sale del chip hacia quien mira.** Con esos tres ejes (sistema derecho), si Y apunta hacia los pines y Z hacia quien mira, X apunta hacia la derecha: **confirmar el sentido de X con la prueba de abajo**, porque no se pudo deducir de la foto.

Con el módulo plano y el chip hacia arriba:

| Inclinación | Cambia |
|:--|:--|
| Bajar el lado hacia donde apunta X | `angX` negativo, `pitch` positivo |
| Subir el lado hacia donde apunta X | `angX` positivo, `pitch` negativo |
| Subir el lado de los pines (Y) | `angY` positivo, `roll` positivo |
| Bajar el lado de los pines (Y) | `angY` negativo, `roll` negativo |

**Confirmar el sentido de X:** apoyá el módulo sobre uno de sus lados cortos, sin moverlo. El lado que queda hacia arriba es hacia donde apunta X si `ax` marca cerca de +1 g (y hacia donde no apunta si marca -1 g).

## Cómo leer la salida

```
ax=+0.012 ay=-0.034 az=+0.998 g | arriba=+Z | angX=-0.7 angY=-1.9 angZ=+88.0 | pitch=+0.7 roll=-2.0 tilt=2.3 deg
```

| Valor | Qué es |
|:--|:--|
| `ax`, `ay`, `az` | Aceleración en g sobre cada eje del chip. Quieto es la gravedad: el eje que apunta hacia arriba marca cerca de +1 g y los otros cerca de 0. |
| `arriba` | Eje que más se acerca a la vertical y hacia dónde apunta (`+Z`: la flecha de Z mira hacia arriba). Dice cómo quedó colocado el dispositivo. |
| `angX`, `angY`, `angZ` | Ángulo de cada eje respecto de la horizontal, con signo: 0° horizontal, +90° vertical hacia arriba. |
| `pitch`, `roll` | Los que usa `MODE_ANGLE` de `prueba_opt101` y `pruebabpw34`. `pitch = -angX` (giro alrededor del eje Y) y `roll` gira alrededor del eje X. Son los buenos con el eje Z hacia arriba. |
| `tilt` | Cuánto se aparta de la vertical el eje que apunta más hacia arriba, sin signo. |

## Cómo elegir el ángulo que interesa

1. Colocá el ADXL345 en el dispositivo como va a quedar y dejalo quieto. Fijate en `arriba`.
2. Inclinalo de a poco en la dirección que interesa (la que hace que se pierda la detección de gotas).
3. Mirá cuál de `angX`, `angY` o `angZ` (o `pitch`/`roll`) cambia y qué signo toma. Ese es el que sigue esa inclinación. Los que casi no se mueven no sirven.
4. Si el que cambia no es `pitch`, avisá cuál es: hay que adaptar `MODE_ANGLE` para que use ese eje.

Los ángulos solo valen con el dispositivo quieto: el movimiento suma aceleración a la gravedad.
