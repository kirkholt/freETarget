file(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
# drivers/sockets.c is an old copy of the lwIP socket layer (lwip_socket(),
# lwip_connect(), lwip_select() ...).  Because the GLOB above picks up every
# file under main/, it used to be linked into the app, where its symbols take
# precedence over the ESP-IDF lwIP library.  The copy does not match the lwIP
# headers of the ESP-IDF version in use, and the result was that outgoing TCP
# connections were sent to the wrong port (port 1086 instead of 80 or 443,
# seen in a pfSense packet capture).  Every HTTP POST to the remote server
# therefore ended in "esp-tls: select() timeout".
#
# Leave the file out of the build so that the socket layer from ESP-IDF, which
# matches its own headers, is used.  The file is kept in the repository only
# for reference.
list(FILTER app_sources EXCLUDE REGEX ".*/drivers/sockets\\.c$")
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

