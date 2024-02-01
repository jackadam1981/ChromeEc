
message("Linking main.cc to ${ZEPHYR_CURRENT_LIBRARY}")
zephyr_library_sources(main.cc)
zephyr_library_link_libraries(
    pw_unit_test.light
    pw_unit_test.logging_event_handler
)
