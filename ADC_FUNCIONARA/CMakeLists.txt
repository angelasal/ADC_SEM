idf_component_register(
    SRCS "wifi_manager.c" "audio_processor.c" "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp-dsp mqtt
    REQUIRES esp_wifi nvs_flash esp_adc
)