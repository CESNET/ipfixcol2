include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

# Subdirectories with components
add_subdirectory(common)
add_subdirectory(lister)
add_subdirectory(aggregator)
add_subdirectory(statistics)

add_executable(fdsdump
    main.cpp
    options.cpp
    $<TARGET_OBJECTS:common_obj>
    $<TARGET_OBJECTS:lister_obj>
    $<TARGET_OBJECTS:aggregator_obj>
    $<TARGET_OBJECTS:statistics_obj>
)

target_include_directories(fdsdump
    PUBLIC
        ${FDS_INCLUDE_DIRS})

target_link_libraries(fdsdump
    PUBLIC
        ${FDS_LIBRARIES}
        Threads::Threads)

# Installation targets
install(
    TARGETS fdsdump
    DESTINATION bin)
