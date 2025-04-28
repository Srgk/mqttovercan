idf_component_register(
    SRCS 
      lib/h42_nvmem.c lib/h42_packet_queue.c
      isotp-c/isotp.c
      h42_can.c  h42_can_daemon.c h42_isotp.c
    INCLUDE_DIRS "include" "lib/include" "isotp-c"
    REQUIRES tcp_transport nvs_flash driver)

target_compile_options(${COMPONENT_LIB} PRIVATE -Werror=all) 