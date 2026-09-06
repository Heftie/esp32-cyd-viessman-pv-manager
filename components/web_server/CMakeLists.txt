idf_component_register(SRCS "web_server.c" "wifi_provision.c"
                    INCLUDE_DIRS "include"
                    EMBED_TXTFILES "index.html" "portal.html"
                    REQUIRES esp_wifi esp_netif esp_event esp_http_server
                    PRIV_REQUIRES nvs_flash esp_timer lwip espressif__mdns espressif__cjson
                    )
