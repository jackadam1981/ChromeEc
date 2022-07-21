# Common sources
target_sources(app PRIVATE
  src/main.c
  src/test_mocks.c
  src/test_rules.c
  src/utils.c
  src/stubs.c
)

target_sources_ifdef(CONFIG_MOCK_POWER_SEQUENCE
                      app PRIVATE src/mock_power.c)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PLATFORM_EC}/driver/ppc/
)
