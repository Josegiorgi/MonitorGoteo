# Componente ESP-IDF para el core de u8g2 (https://github.com/olikraus/u8g2)
# Fuente vendorizada desde csrc/ del repo original, sin modificaciones.
file(GLOB SOURCES "*.c")
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "."
                       )
