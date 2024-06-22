add_subdirectory(atom)

if(DUAL_ENABLE_JIT)
  set(LUNATIC_USE_EXTERNAL_FMT ON CACHE BOOL "" FORCE)
  add_subdirectory(lunatic)
endif()
