# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "BTP_GUI_autogen"
  "CMakeFiles\\BTP_GUI_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\BTP_GUI_autogen.dir\\ParseCache.txt"
  )
endif()
