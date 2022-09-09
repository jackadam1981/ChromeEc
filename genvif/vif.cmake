# SPDX-License-Identifier: Apache-2.0

# Generates VIF format XML file from zephyr.dts file generated during build.

set(GEN_VIF_SCRIPT          ${ZEPHYR_BASE}/scripts/dts/generate_vif.py)
set(DTS_BINDINGS          ${ZEPHYR_BASE}/dts/bindings)

set(VIF_XML  ${PROJECT_BINARY_DIR}/vif.xml)

message(STATUS "executing gen vif, reading file from loc : ${ZEPHYR_DTS}")

 #
  # Run gen_defines.py to create a header file, zephyr.dts, and edt.pickle.
  #

  set(CMD_GEN_VIF ${PYTHON_EXECUTABLE} ${GEN_VIF_SCRIPT}
  --dts ${ZEPHYR_DTS}
  --vif-out ${VIF_XML}.new
  --bindings-dirs ${DTS_BINDINGS}
  )

  execute_process(
    COMMAND ${CMD_GEN_VIF}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(STATUS "In: ${PROJECT_BINARY_DIR}, command: ${CMD_GEN_VIF}")
    message(FATAL_ERROR "generate_vif.py failed with return code: ${ret}")
  else()
    zephyr_file_copy(${VIF_XML}.new ${VIF_XML} ONLY_IF_DIFFERENT)
    file(REMOVE ${VIF_XML}.new)
    message(STATUS "Generated vif.xml: ${VIF_XML}")
  endif()
