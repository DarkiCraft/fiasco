add_executable(main main.cpp)
target_link_libraries(main PRIVATE fiasco)

add_executable(module module.cpp)
target_link_libraries(module PRIVATE fiasco_modules)
