add_subdirectory(external)

add_executable(hello_triangle hello_triangle.cpp)
target_link_libraries(hello_triangle PRIVATE glfw lib_glad lib_gsdf)

add_executable(deferred deferred.cpp)
target_link_libraries(deferred PRIVATE glfw lib_glad lib_gsdf)