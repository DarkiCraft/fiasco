# -- Tests ---------------------------------------------------------------------
add_executable(fiasco_tests
    test_sanity.cpp
    test_tcp_transport.cpp
    test_thread_pool.cpp
    test_event_loop.cpp
    test_http.cpp
    test_serialization.cpp
        test_routing.cpp
)

target_link_libraries(fiasco_tests PRIVATE
    fiasco
    Catch2::Catch2WithMain
)

# Register with CTest so `ctest` discovers our tests
include(Catch)
catch_discover_tests(fiasco_tests)
