# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\wk_gui_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_gui_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_gui_smoke_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_gui_smoke_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_pc2_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_pc2_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_pc3_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_pc3_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_pc_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_pc_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_pod_datasource_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_pod_datasource_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_pod_handler_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_pod_handler_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_pod_integration_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_pod_integration_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\wk_pod_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\wk_pod_tests_autogen.dir\\ParseCache.txt"
  "wk_gui_autogen"
  "wk_gui_smoke_autogen"
  "wk_pc2_tests_autogen"
  "wk_pc3_tests_autogen"
  "wk_pc_tests_autogen"
  "wk_pod_datasource_tests_autogen"
  "wk_pod_handler_tests_autogen"
  "wk_pod_integration_tests_autogen"
  "wk_pod_tests_autogen"
  )
endif()
