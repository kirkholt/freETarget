file(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
idf_component_register(SRCS ${app_sources} 
                    INCLUDE_DIRS "."
                    "." 
                    "./tcpip" 
                    "./drivers"
                    "./html"
                    "${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos"
                    "${IDF_PATH}/components/hal/include/hal"
                    "${IDF_PATH}/components/esp_adc/include/esp_adc"
                    "${IDF_PATH}/components/esp_http_server/include"
                    
                    EMBED_FILES 
                    "html/FreeEtarget.html"
                    "html/FreeEtargetIcon.png"
                    "html/help.html"
                    "html/menu.html"
                    "html/remote.html"
                    "tcpip/ca_cert.pem"
                    )

