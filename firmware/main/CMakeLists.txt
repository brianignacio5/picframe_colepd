idf_component_register(SRCS "main.c" "epd.c"
                    INCLUDE_DIRS ".")

# Embed the icons file
target_add_binary_data(${COMPONENT_TARGET} "icons.bmp" TEXT)
