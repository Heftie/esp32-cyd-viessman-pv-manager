idf_component_register(SRCS demo.c
                    INCLUDE_DIRS "."
                    REQUIRES board_config lcd touch rgb_led web_server ui
                    )
target_link_libraries(${COMPONENT_LIB} "-u _printf_float")