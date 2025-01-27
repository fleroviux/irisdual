add_subdirectory(atom)

if(DUAL_ENABLE_JIT)
  set(LUNATIC_USE_EXTERNAL_FMT ON CACHE BOOL "" FORCE)
  add_subdirectory(lunatic)
endif()

set(CAPSTONE_ARCHITECTURE_DEFAULT OFF)
set(CAPSTONE_AARCH64_SUPPORT ON)
set(CAPSTONE_BUILD_MACOS_THIN ON)
add_subdirectory(capstone)

add_subdirectory(oaknut)
