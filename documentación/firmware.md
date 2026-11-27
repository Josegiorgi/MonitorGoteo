# Firmware del monitor de goteo

Este capítulo describe el firmware del prototipo: qué hace, cómo está organizado y por qué se tomaron las decisiones principales. El código completo está en el repositorio [github.com/Josegiorgi/MonitorGoteo](https://github.com/Josegiorgi/MonitorGoteo), dentro de la carpeta *firmware*. Cada sección indica los archivos donde está lo que se explica, para que quien lea el texto pueda ir directamente al código.

## Visión general

El monitor mide la infusión por gravedad en tiempo real. Cuenta las gotas que caen en la cámara de goteo, con ellas calcula el volumen infundido y el caudal, y avisa con alarmas visuales y sonoras cuando algo se aparta de lo programado: flujo detenido, fin del sachet, desvío del caudal, equipo mal colocado o batería baja. Todo lo que ocurre durante la infusión queda guardado en un registro, que la enfermera descarga después con una aplicación Android por Bluetooth.  
El microcontrolador es un ESP32-C3 y el firmware está escrito en C sobre ESP-IDF y FreeRTOS. La interfaz es una pantalla OLED de 128 x 64 píxeles con un encoder rotativo con pulsador. El equipo se arma con dos fotodiodos BPW34 para detectar las gotas, un acelerómetro ADXL345 para medir la inclinación, un buzzer pasivo y un divisor resistivo para leer la batería.  
El firmware se organizó en componentes de ESP-IDF, cada uno con una responsabilidad. El programa principal solo los une. Esta decisión permitió probar cada parte por separado antes de integrarla, con un proyecto de prueba propio por componente (por ejemplo *test\_alarmas*, *test\_bateria*, *test\_ble* y *prueba\_adxl345*), y hace que cada archivo se pueda leer sin conocer el resto.

### Conexiones

| Dispositivo | Pin del ESP32-C3 |
| :---- | :---- |
| Salida del amplificador del BPW34 n°1 | GPIO\_2 (ADC1\_CH2) |
| Salida del amplificador del BPW34 n°2 | GPIO\_3 (ADC1\_CH3) |
| Divisor de tensión de la batería | GPIO\_4 (ADC1\_CH4) |
| OLED y ADXL345 (mismo bus I2C) | SCL: GPIO\_7, SDA: GPIO\_21 |
| Encoder: CLK, DT y pulsador | GPIO\_9, GPIO\_8 y GPIO\_20 |
| Buzzer pasivo | GPIO\_10 |

La consola de depuración sale por el puerto USB Serial/JTAG del ESP32-C3, de modo que los pines de la UART quedan libres para el bus I2C.

## Organización del repositorio

El repositorio parte del firmware base de la cátedra Electrónica Programable de la FIUNER (drivers de la placa ESP-EDU) y agrega el trabajo propio del monitor. Dentro de la carpeta *firmware* se distinguen cuatro niveles.

| Carpeta | Contenido |
| :---- | :---- |
| projects/monitor\_goteo | El firmware final. En *main/*, *monitor\_goteo.c* tiene *app\_main* y el lazo principal, y cada tema está en su módulo (autotest, infusion, gestion\_alarmas, descarga, pendiente, modo\_tecnico y reposo). El archivo *partitions.csv* define la tabla de particiones de la flash. |
| components | Los módulos propios del monitor: optical\_sensor, drop\_detection, sensado, alarmas, buzzer\_alarm, inclinacion, bateria, display, interfaz\_usuario, registro y comunicacion. |
| drivers y middleware | Los drivers de la placa y de los dispositivos (ADXL345, SSD1315, buzzer, I2C, ADC, timers, BLE) y los módulos de procesamiento de señal, entre ellos el filtro IIR y el detector por correlación cruzada. |
| third\_party | La biblioteca gráfica u8g2, que dibuja en la OLED. |
| projects/prueba\_\* y test\_\* | Proyectos de prueba de cada sensor y de cada componente, usados durante el desarrollo. |
| projects/analisis\_bpw34 y analisis\_opt101 | Scripts de Python con los que se analizaron las señales de los sensores y se obtuvieron los filtros, la plantilla de la gota y los umbrales. |

La carpeta *documentación*, en la raíz del repositorio, tiene las instrucciones de instalación y compilación, el contrato con la aplicación (*cambios\_para\_la\_app.md*) y el guion de pruebas del firmware final (*pruebas\_firmware\_final.md*). Cada componente documenta su interfaz en el comentario de su archivo de cabecera, que se puede convertir en documentación HTML con Doxygen.

## Arquitectura del programa

Hay dos líneas de ejecución principales. La primera es una tarea de muestreo del sensor óptico, que corre con prioridad 5 y se despierta con un timer. La segunda es el lazo principal de *app\_main*, que atiende el encoder, las gotas, las alarmas y la pantalla. Una tercera tarea, la de comunicación, atiende el Bluetooth. Estas tareas se comunican por una cola de FreeRTOS: la tarea del sensor pone en la cola la hora de cada gota detectada y el lazo principal las toma y las procesa.  
Esta separación tiene un motivo. El muestreo de los fotodiodos no admite demoras, porque una gota dura unos 10 ms y se describe con apenas diez muestras. Dibujar la pantalla, escribir en la flash o responder por Bluetooth tardan mucho más que eso, así que se hacen fuera de la tarea del sensor. La función que avisa una gota solo guarda la hora en la cola y vuelve.  
El lazo principal espera un evento del encoder hasta 100 ms como máximo y, en cada vuelta, vacía la cola de gotas. Una vez por segundo ejecuta el ciclo de actualización: lee el ángulo y la batería, calcula volumen y caudal, evalúa las alarmas, guarda la muestra del registro cuando corresponde y redibuja la pantalla. Cuando cae una gota se redibuja en el momento, pero no más de una vez cada 250 ms, para no saturar el bus I2C compartido con el acelerómetro. El código de este lazo está repartido por tema en la carpeta *main* del proyecto: *monitor\_goteo.c* solo tiene *app\_main* y el lazo, y el resto está en *autotest* (los chequeos del arranque), *infusion* (cola de gotas, inicio y fin de la infusión y registro de eventos), *gestion\_alarmas* (une el componente alarmas con la pantalla y el buzzer), *descarga* y *pendiente* (Bluetooth y registro pendiente de descarga), *modo\_tecnico* y *reposo*. Lo único que comparten esos módulos son los datos que muestra la interfaz y un indicador de si se están contando gotas.

## Adquisición y detección de gotas

### Sensor óptico

El componente *optical\_sensor* lee los dos canales del ADC con un período fijo de 1200 µs, es decir una frecuencia de muestreo de unos 833 Hz. Un timer de hardware genera la interrupción y la interrupción solo avisa a la tarea de muestreo, que hace la lectura y el filtrado. De esta forma el período no depende de lo que haga el resto del sistema.  
Cada fotodiodo tiene su propio amplificador de transimpedancia y, entre la salida del amplificador y el pin del ADC, un filtro antialias RC de 4,7 kΩ y 100 nF (corte cercano a 339 Hz, por debajo de la frecuencia de Nyquist de 417 Hz). Al arrancar, el firmware mide el nivel base de cada canal promediando 100 lecturas con el haz libre, y lo resta a cada muestra. Esto evita cargar los filtros con una continua de aproximadamente 1 V. Por eso el tubo de goteo debe estar en la cámara y sin gotas al encender el equipo.  
La señal sin continua pasa por un filtro pasabanda Butterworth de cuarto orden, formado por un pasaaltos de 20 Hz y un pasabajos de 260 Hz. Los cortes salieron del espectro de energía de la gota promedio medido en *analisis\_bpw34*: la gota es de banda ancha y casi toda su energía está entre 10 y 260 Hz. El pasaaltos elimina la luz ambiente y las derivas lentas, y el pasabajos saca el ruido blanco que queda hasta Nyquist.  
El componente solo tiene migrado el BPW34. El proyecto conserva las pruebas hechas con el sensor OPT101 (*prueba\_opt101* y *analisis\_opt101*), que se evaluó antes de elegir el fotodiodo.

### Detección por correlación cruzada

Con la señal filtrada, el componente *drop\_detection* decide cuándo cayó una gota. Se eligió la correlación cruzada con una plantilla: en cada muestra se calcula cuánto se parece la última ventana de señal a la forma típica de una gota. Donde hay una gota la correlación da un pico grande y donde hay solo ruido se mantiene baja. En el análisis con registros reales detectó todas las gotas, sin falsos positivos, con la gota unas 14 veces por encima del umbral.  
La plantilla de cada canal es el promedio de más de cien gotas reales (101 en el canal 1 y 126 en el canal 2), alineadas en su valle más profundo, con una ventana de 37 muestras (de 15 ms antes a 30 ms después del valle). Cada canal tiene la suya porque cada fotodiodo ve la gota algo distinta. La forma de la gota filtrada es una caída de varios cientos de milivoltios, un rebote por encima del nivel base y una segunda caída, todo en unos 10 ms. Las plantillas se generaron con *analizar\_correlacion\_bpw34.py* y están en *drop\_template.h*. Como la forma depende de la frecuencia de muestreo, del filtro digital y del antialias, hay que volver a generarlas si cambia cualquiera de los tres.  
La detección tiene cuatro parámetros. El umbral de correlación es de 480 mV en el canal 1 y de 140 mV en el canal 2\. La ventana de pico es de 15 ms: después de cruzar el umbral, el detector espera ese tiempo quedándose con el máximo, porque una plantilla con lóbulos positivos y negativos produce lóbulos laterales en la correlación. Luego sigue un período refractario de 100 ms en el que se ignora la entrada, para no contar dos veces la misma gota. Por último, cuando los dos canales ven la misma gota se avisa una sola vez, porque tras el aviso se ignoran ambos canales durante el período refractario.  
El umbral del canal 1 es más alto que el que salió del análisis (160). Con la OLED redibujando aparecieron falsas detecciones en ese canal, de hasta unos 390 mV de correlación, mientras que la gota más débil del mejor canal da cerca de 594 mV. Se eligió 480, que queda entre ambos valores. Este es un punto de la implementación que conviene validar con gotas reales y con el canal 2, que podría ver el mismo ruido.  
El detector en sí es un módulo genérico del middleware (*xcorr\_detector*) que admite varias instancias, una por canal.

## Volumen y caudal

El componente *sensado* (archivo *goteo.c*) convierte las gotas en magnitudes clínicas. No usa hardware ni relojes propios: todas sus funciones reciben la hora en microsegundos, lo que permite probarlo con gotas simuladas.  
El volumen infundido es el número de gotas dividido por el factor de goteo del equipo de infusión (10, 15, 20 o 60 gotas por mL). El caudal en mL/h es 3600 dividido por el producto entre el tiempo entre gotas, en segundos, y el factor de goteo. Para el tiempo entre gotas se usa la mediana de los últimos 10 intervalos y no el promedio. En las pruebas, el promedio de 8 gotas subía mucho al abrir o cerrar el clamp, cuando las gotas caen muy seguidas, mientras que la mediana no se deja arrastrar por un intervalo atípico. Si pasan más de 30 s sin gotas, el caudal se informa como cero.

## Alarmas

### Eventos y prioridades

El componente *alarmas* implementa la especificación de alarmas del proyecto. Mantiene dos máscaras de bits, una con los eventos activos y otra con los silenciados, y se evalúa una vez por segundo. Los eventos tienen un orden de prioridad fijo, que es también el orden en que se muestran y suenan cuando hay varios a la vez.

| Evento | Tipo | Condición |
| :---- | :---- | :---- |
| Volumen completo | Alta | El volumen infundido alcanzó el programado y siguen cayendo gotas. |
| Flujo detenido | Alta | Sin gotas durante más de T\_ocl y volumen infundido menor al 90 %. |
| Fin de sachet | Alta | Sin gotas durante más de T\_ocl y volumen infundido de 90 % o más. |
| Equipo reiniciado | Alta | El equipo se reinició con una infusión sin terminar y falta la descarga. |
| Colocación | Alta | Inclinación o nivel óptico fuera de rango durante 5 s seguidos. |
| Batería crítica | Alta | Carga de 5 % o menos. |
| Desviación fuerte | Media | Desvío del caudal mayor a 30 % durante 2 min. |
| Goteo sin monitoreo | Media | Sin infusión activa, gotas continuas durante 2 min. |
| Batería baja | Media | Carga de 10 % o menos. |
| Desviación leve | Aviso | Desvío del caudal mayor a 10 % durante 5 min. |
| Próximo a finalizar | Aviso | Volumen infundido de 90 % o más del programado. |
| Aviso de batería | Aviso | Carga de 20 % o menos. |

Las alarmas altas y medias suenan en el buzzer; los avisos solo se muestran en pantalla. Flujo detenido y fin de sachet son excluyentes entre sí, y también lo son los tres escalones de batería, de los que queda solo el peor.

### Cómo se evalúa cada condición

**Flujo detenido y fin de sachet.** Se calcula el período objetivo entre gotas, T\_obj, con el volumen, la duración y el factor de goteo programados. El tiempo máximo sin gotas, T\_ocl, es tres veces T\_obj, de modo que tolera hasta dos gotas seguidas no detectadas, y se limita al rango de 15 a 120 s. Si se supera, la alarma es de flujo detenido o de fin de sachet según el volumen ya infundido. Se resuelve sola cuando vuelven a caer tres gotas seguidas. El Resumen no deja iniciar una infusión de menos de 2 gotas por minuto, porque con un caudal tan bajo T\_ocl superaría el límite permitido.  
**Desvío del caudal.** El desvío es T\_obj dividido por el tiempo medido entre gotas, menos 1, de modo que un valor positivo indica que el goteo es más rápido que el objetivo. Se calcula con la mediana de los últimos 10 intervalos y no se evalúa durante los primeros 2 min del monitoreo, para dar tiempo a que el goteo se estabilice. El aviso leve exige más de 10 % durante 5 min y se apaga por debajo de 8 %. La alarma fuerte exige más de 30 % durante 2 min y se apaga por debajo de 28 %. Esa diferencia entre el umbral de encendido y el de apagado es una histéresis, que evita que la alarma se active y se apague repetidamente cuando el valor oscila alrededor del límite.  
**Colocación.** El componente *inclinacion* lee el ADXL345 una vez por segundo, promedia seis lecturas para filtrar vibraciones y obtiene el ángulo del eje Z, al que se le resta el ángulo del equipo en su posición de referencia dentro del gabinete (11,1°, medido en reposo). La tolerancia es de 8° hacia cualquier lado. Se fijó con las tomas de *analisis\_bpw34*: la detección de gotas empieza a perderse a unos 9,8° de inclinación positiva y a unos 10,8° de inclinación negativa, y se dejaron aproximadamente 2° de margen. La condición debe sostenerse 5 s para activar o apagar la alarma. Si el acelerómetro deja de responder durante el uso, también se activa, porque la colocación ya no se puede verificar. Cada activación y resolución de esta alarma queda anotada en el registro, que sirve para saber si el volumen estimado pudo perder gotas mientras el equipo estuvo desacomodado.  
**Batería.** El componente *bateria* mide la tensión de una celda Li-ion de 18650 con un divisor de dos resistencias de 10 kΩ, de modo que el ADC ve la mitad de la tensión (2,1 V con la batería llena), dentro del rango en que la calibración del ESP32-C3 es precisa. Se toma una lectura cada 100 ms y se promedian 16\. No se mide mientras suena el buzzer, porque sus picos de consumo hacen caer la tensión. Con menos de 2,5 V se considera que no hay batería conectada y se informa 100 %, sin alarma. El porcentaje sale de una curva de descarga típica de Li-ion sin carga, provisoria hasta reemplazarla por la curva medida del equipo. Un cambio de escalón (aviso, baja, crítica o vuelta a normal) solo vale si el nivel nuevo se sostiene 30 s y tiene 2 % de histéresis, porque la curva de descarga es plana y unos pocos milivoltios mueven el porcentaje.  
**Goteo sin monitoreo.** Si no hay una infusión activa y caen gotas de forma continua durante 2 min, se activa una alarma, porque puede haber un paciente recibiendo solución sin que el equipo la esté controlando. La racha se corta si pasan más de 45 s sin gotas.  
**Volumen completo.** Se activa cuando el volumen infundido alcanza el programado y siguen cayendo gotas. Si se corta el goteo, pasa a fin de sachet.

### Silenciado y re-armado

Con la infusión en curso, la enfermera solo puede silenciar la alarma con un click del encoder. El silenciado deja de sonar la alarma que se muestra, pasa a la siguiente si hay otra y la anota en el registro. Las alarmas de prioridad alta se re-arman solas a los 2 min si la condición sigue presente. La alarma de colocación se resuelve sola cuando el ángulo vuelve al rango durante 5 s.

### Sonidos

El componente *buzzer\_alarm* reproduce cada alarma como una secuencia de pasos de frecuencia y duración, que se repite hasta que se la detiene. Corre en una tarea propia, para que iniciar un sonido no bloquee al lazo principal. Los patrones se distinguen por ritmo y no solo por tono, para que se reconozcan sin mirar la pantalla. Todos los tonos están entre 3300 y 4200 Hz, que es la banda en que el buzzer elegido suena más fuerte según su hoja de datos. También hay sonidos de la interfaz: tonos ascendentes al encender, descendentes al apagar, un pitido con el botón y un tic con el giro. Los patrones de las alarmas fueron una decisión de prototipo: una versión más madura debería ajustarse a las normas IEC 60601-1-8 e IEC 60601-2-24.

## Interfaz de usuario

El componente *display* maneja la OLED SSD1315 por I2C con la biblioteca u8g2, y *interfaz\_usuario* contiene las pantallas y la navegación. La interfaz no mide ni decide nada: el programa principal completa una estructura de datos con los valores actuales, incluida la alarma que corresponde mostrar, y la interfaz los dibuja y responde a los eventos del encoder. El encoder entrega giro horario, giro antihorario, click, click largo y click muy largo (3 s).  
La navegación sigue la especificación de inicio y navegación. Al encender hay que mantener apretado el botón durante 3 s, lo que evita encendidos accidentales. Sigue la pantalla de inicio, durante la cual se hace el autotest, y después la clave clínica de cuatro dígitos. Luego se recorren Colocación, Config (factor de goteo, volumen y duración), Resumen, Ajuste de goteo y Monitoreo.

> * El Resumen tiene dos páginas. La primera muestra el factor de goteo en grande con la indicación de verificarlo en el envase, porque un factor mal cargado altera todos los cálculos. La segunda muestra volumen, duración, caudal en mL/h y gotas por minuto, y es desde ahí que se inicia la infusión. No deja iniciar si el objetivo es menor a 2 gotas por minuto ni si el registro no tiene lugar para la duración elegida.  
> * En el Ajuste de goteo la enfermera regula el clamp hasta que el caudal coincida con el objetivo y lo confirma. El volumen se cuenta desde ahí. Si pasan 10 min sin confirmar, el equipo lo recuerda con un beep cada minuto.  
> * Durante el Monitoreo se muestran el volumen, el caudal, el tiempo y la carga de la batería. Una alarma interrumpe la pantalla con su propia vista, y las silenciadas o los avisos quedan en el pie.  
> * Con la infusión en curso, el cuidador solo puede silenciar alarmas. Mantener el botón 3 s abre la clave clínica y, de ahí, el menú de Ajustes: Cambiar datos (solo la duración, sin reiniciar el contador), Detener monitoreo, Apagar y Volver. Tres claves incorrectas seguidas bloquean el ingreso durante 60 s, y la pantalla vuelve sola al Monitoreo tras 60 s sin actividad.  
> * Al detener el monitoreo queda la pantalla Sin infusión activa, desde donde se elige Nueva infusión o Apagar. Se bloquea a los 60 s sin actividad.  
> * El Modo Técnico se abre con un código técnico en la clave del arranque. Ofrece Diagnóstico, Probar alarma, Borrar registro y Salir, y sale solo a los 5 min sin actividad.

Las claves clínica y técnica son provisorias y se definen como constantes en *ui.h*.

## Autotest, reposo y reinicios

Durante la pantalla de inicio, que dura al menos 5 s, el equipo hace un autotest de cinco chequeos: pantalla, acelerómetro, sensor óptico, memoria y batería. El del sensor óptico verifica que el nivel base de cada canal esté entre 50 y 2400 mV, es decir, que el fotodiodo tenga señal sin saturar. El de batería pide más de 10 % de carga o el cargador conectado. Si todo da correcto, pasa directo a la clave. Si un chequeo falla, la pantalla muestra cuál y qué hacer, lo reintenta cada segundo y no deja continuar. Si la pantalla misma falla, no hay cómo mostrar un texto y el equipo avisa con el buzzer.  
Al elegir Apagar, el equipo muestra "Registro guardado" y pasa a reposo: apaga la pantalla, el sensado, el buzzer y el Bluetooth, y el microcontrolador entra en light sleep. Se vuelve a encender manteniendo el botón 3 s. No es un consumo nulo, porque el LED infrarrojo queda encendido por hardware y el divisor de la batería consume unos 210 µA, por lo que para guardar el equipo hay que abrir el interruptor interno.  
Si el equipo se reinicia con una infusión sin terminar (corte de batería, watchdog o brown-out), no la retoma. Se tomó esta decisión porque, sin reloj de tiempo real, se perdería el tiempo que estuvo apagado y el volumen informado dejaría de ser confiable. En su lugar anota el reinicio en el registro, da la infusión por interrumpida, hace sonar la alarma Equipo reiniciado ("El equipo se apagó, llamar a enfermería") y pide la descarga del registro. Esta alarma se re-arma a los 2 min si se silencia y termina cuando la aplicación confirma la descarga o cuando la enfermera empieza una infusión nueva. En este caso el autotest no bloquea el arranque, porque la alarma tiene que sonar aunque algo falle.

## Registro de la infusión

El componente *registro* guarda todo lo que ocurre desde el inicio hasta el fin de la infusión, para poder auditarla. Se guarda en una partición propia de la flash (tipo data, subtipo 0x40), definida en *partitions.csv*, de 917504 bytes. La flash del equipo es de 2 MB y se reparte entre la NVS, la aplicación (1 MB) y el registro.  
El registro es una sucesión de líneas binarias de tamaño fijo, de 16 bytes, que se escriben una detrás de otra en el momento en que ocurren, de modo que un reinicio pierde como mucho la línea en curso. Con 16 bytes por línea entran 57344 líneas, unos 35 días de infusión continua con una muestra por minuto más los eventos. El tamaño fijo permite que la descarga empiece en una línea cualquiera saltando directamente a su posición. Nunca se borra solo: si no hay lugar para la duración programada, el Resumen no deja iniciar, y el borrado se hace desde el Modo Técnico.  
Hay dos tipos de línea. Los eventos incluyen el inicio, la duración, la confirmación del ajuste, el cambio de datos, el fin, el reinicio, las claves (correcta, incorrecta y técnica), la activación y resolución de cada aviso o alarma, los silenciados y la descarga. Las muestras se escriben cada minuto con el contador de gotas, el caudal y el volumen. No se guarda cada gota por separado, ni la navegación entre pantallas, ni el resultado del autotest.  
La memoria no volátil de ESP-IDF (NVS) guarda solo lo pequeño: la configuración de la infusión activa, la línea donde empieza, el identificador de cada infusión (que sube de a uno y no se reinicia al borrar el registro) y el estado de la última infusión y de su descarga.  
Como el equipo no tiene reloj de tiempo real, las líneas llevan el minuto desde el inicio de la infusión y no una hora. La aplicación calcula la hora de inicio restando el minuto actual a la hora del celular al momento de la descarga. Para que ese cálculo sirva, el minuto sigue contando después del fin de la infusión, hasta que la aplicación confirma la descarga. Mientras tanto, la pantalla avisa "Registro pendiente de descarga" sin sonido, porque es un aviso y no una alarma. Si se quiere empezar una infusión nueva o apagar el equipo con un registro sin descargar, el equipo pregunta si se continúa igual. La lógica de ese minuto que sigue corriendo está en *pendiente.c*.

## Comunicación con la aplicación

El componente *comunicacion* es independiente del resto: el programa principal solo lo inicia, lo apaga y le da tres funciones para consultar el estado de la infusión. La aplicación Android de la enfermera descarga el registro por Bluetooth Low Energy, pero no ve el estado en tiempo real ni cambia nada del equipo. Los archivos son *ble\_goteo* (driver BLE), *protocolo* (órdenes y formato de las líneas) y *comunicacion* (la tarea que los une). El contrato completo está en *documentación/cambios\_para\_la\_app.md*.  
El equipo se anuncia mientras está encendido con el nombre *MonitorGoteo-XX*, donde XX es el número del equipo, admite una sola conexión a la vez y no usa emparejamiento. Usa un solo servicio (0xFFE0) con una única característica (0xFFE1) para los dos sentidos: la aplicación escribe las órdenes y el equipo notifica las respuestas y el registro. Todo viaja como texto UTF-8, con líneas terminadas en salto de línea. Las notificaciones son de 20 bytes, así que una línea larga llega en varios pedazos y la aplicación los junta hasta encontrar el salto de línea.

| Orden de la aplicación | Qué hace el equipo |
| :---- | :---- |
| HORA=\<epoch\> | Acepta la hora del celular y responde ACK. |
| PACIENTE=\<código\> | Valida el formato (2 a 20 caracteres) y no lo guarda, para no almacenar datos del paciente en el equipo. |
| ESTADO | Informa el identificador de la última infusión, su estado (1 en curso, 0 terminada, 2 interrumpida por un reinicio), el minuto actual y la cantidad de líneas de la descarga. |
| DESCARGAR | Envía el registro completo de la última infusión y cierra con una línea de fin que trae la cantidad de líneas y el minuto actual, para que la aplicación verifique que recibió todo. |
| OK | La aplicación confirma que la descarga llegó completa. El equipo marca el registro como descargado y deja de contar el minuto. |

La infusión se identifica por el par equipo más identificador, porque el contador de dos equipos puede coincidir. En el registro, la aplicación recibe líneas de texto como "135 min \- ALARMA Flujo detenido", lo que permite reconstruir la historia de la infusión con la hora calculada a partir del minuto. No viajan las muestras de cada minuto ni los eventos internos.

## Compilación y configuraciones de prueba

El proyecto *monitor\_goteo* se compila y se graba con ESP-IDF. En *main/escenarios.h* hay dos interruptores. *USAR\_BLE* compila el firmware con o sin Bluetooth. *ESCENARIO* elige con qué configuración arranca: *ESCENARIO\_NORMAL* es la versión final, y los otros dos precargan una infusión y acortan los tiempos de las alarmas, de la interfaz y del registro para probar sin esperar minutos (por ejemplo, el desvío fuerte de 2 min pasa a 20 s). En un escenario de prueba, cada línea que se guarda en el registro sale además por la consola. El compilador avisa si el firmware quedó compilado fuera de la versión final. El código que está en la rama principal del repositorio queda en la configuración final: Bluetooth activado y *ESCENARIO\_NORMAL*.

## Pruebas

El desarrollo fue incremental y cada componente se probó antes de integrarse: el sensor óptico y el detector con *pruebabpw34*, el acelerómetro con *prueba\_adxl345*, la batería con *test\_bateria*, las alarmas y sus sonidos con *test\_alarmas* y *test\_buzzer\_alarmas*, y el Bluetooth con *test\_ble*. La integración se verifica con el guion *pruebas\_firmware\_final.md*, que tiene cuatro partes: sin Bluetooth y con tiempos cortos (autotest, Resumen, registro, reinicios y reposo), con Bluetooth usando una herramienta genérica de BLE, con los tiempos reales de la especificación, y con la aplicación de la enfermera.

## Limitaciones y trabajo futuro

> * Varios parámetros son provisorios y la especificación indica ajustarlos con ensayos de banco: el 90 % del fin de sachet, el rango del nivel base óptico y la curva de porcentaje de la batería.  
> * El equipo no tiene reloj de tiempo real, por lo que el tiempo que permanece apagado se pierde y las horas que calcula la aplicación después de un reinicio son aproximadas.  
> * La pantalla no distingue el volumen completo del fin de sachet: ambos se muestran como fin de sachet, aunque cada uno tiene su propio patrón sonoro.  
> * La conexión Bluetooth no usa emparejamiento seguro, y las notificaciones son de 20 bytes por no negociarse un MTU mayor. Una vinculación con clave y un MTU más grande quedan como mejoras.  
> * El OPT101 no está migrado al componente de sensado óptico: el firmware final solo trabaja con el BPW34.  
> * Los patrones sonoros y la lógica de alarmas son de prototipo. Una versión para uso clínico debería ajustarse a las normas IEC 60601-1-8 e IEC 60601-2-24.