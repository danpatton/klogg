add_executable(tail_demo main.cpp)

target_link_libraries(tail_demo PRIVATE klogg_logdata klogg_settings)
