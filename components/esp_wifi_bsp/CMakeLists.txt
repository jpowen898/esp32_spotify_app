idf_component_register(
  SRCS "esp_wifi_bsp.cpp"

  PRIV_REQUIRES
    "esp_event"
    "nvs_flash"

  REQUIRES
    "esp_wifi"
    "param"

  INCLUDE_DIRS "./")
