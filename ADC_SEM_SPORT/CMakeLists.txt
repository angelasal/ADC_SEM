idf_component_register(
    SRCS "serial_manager.c" "audio_processor.c" "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp-dsp
    REQUIRES esp_adc driver
)