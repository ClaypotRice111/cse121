idf_component_register(
    SRCS "lab3_1.cpp" "DFRobot_LCD.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_driver_gpio
)