# prueba_mpu6000

Lee el acelerómetro del módulo GY-521 (MPU-6050, mismo mapa de registros que el MPU-6000) por I2C y calcula pitch, roll y tilt para registrar el ángulo en que el monitor de goteo deja de detectar gotas.

## Conexiones del módulo GY-521

| Pin del módulo | Conexión |
|:--------------:|:---------|
| VCC | 3V3 |
| GND | GND |
| SCL | GPIO_7 |
| SDA | GPIO_21 |
| AD0 | Sin conectar (dirección 0x68) |
| INT | Sin conectar (el firmware lee por polling) |
| XDA | Sin conectar (bus auxiliar del chip) |
| XCL | Sin conectar (bus auxiliar del chip) |

Según la hoja del módulo, la dirección por defecto es 0x68 con AD0 sin conectar. Llevar AD0 a nivel alto, o puentear el jumper de soldadura de la parte trasera, la cambia a 0x69. El firmware usa 0x68.

## Advertencias

- **GPIO_21 es el TX de la UART0**, la consola por defecto del proyecto (`CONFIG_ESP_CONSOLE_UART_DEFAULT`). Usarlo como SDA rompe `printf`/logs por el puerto serie. Hay que pasar la consola a USB-Serial-JTAG (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`) o dejarla sin usar.
- La pantalla con u8g2 (`u8g2_esp32_hal`) crea su propio bus en el puerto I2C 0, igual que `i2c_mcu`. En una misma aplicación, uno de los dos tiene que reutilizar el bus del otro. Este proyecto de prueba no usa la pantalla.
- Los ángulos son válidos solo con el dispositivo quieto. Ajustar `TILT_AXIS` en el código según la orientación en que quede el módulo.
