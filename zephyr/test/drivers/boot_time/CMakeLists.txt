target_sources(app PRIVATE
    src/power_common.c
)

add_subdirectory(
    ${PLATFORM_EC}/zephyr/test/drivers/mock_panic_output
    mock_panic_output
)
