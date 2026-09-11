idf_component_register(
    SRCS "main.cpp" "hardware.cpp" "bluetooth.cpp" "graphics.cpp" "session.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_spi esp_driver_i2c esp_timer bt nvs_flash freertos
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)

if(CONFIG_PRECISIONSHOT_SELF_TEST)
    target_sources(${COMPONENT_LIB} PRIVATE "../tests/session_test.cpp")
    set_source_files_properties("../tests/session_test.cpp" PROPERTIES
        COMPILE_DEFINITIONS PRECISIONSHOT_EMBEDDED_TEST)
endif()
