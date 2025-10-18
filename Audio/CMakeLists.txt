set(LibName SDL3Audio)
add_library(${LibName} SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
file(GLOB_RECURSE META CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.meta")

target_sources(${LibName} PRIVATE ${SRCS} ${INCL} ${META})

target_include_directories(${LibName}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
    $<INSTALL_INTERFACE:Source>
)

target_link_libraries(${LibName} PRIVATE 
    Tbx::Engine
    SDL3-shared
)

add_custom_command(TARGET ${LibName} POST_BUILD
  COMMENT "Copying plugin meta file to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/${LibName}.meta"
          "$<TARGET_FILE_DIR:${LibName}>/${LibName}.meta")

add_library(Tbx::Plugin::${LibName} ALIAS ${LibName})
