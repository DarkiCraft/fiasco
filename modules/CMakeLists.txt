add_library(fiasco_modules)
target_sources(fiasco_modules
  PUBLIC FILE_SET CXX_MODULES FILES
    fiasco.cppm
    fiasco-common.cppm
    fiasco-response.cppm
    fiasco-server.cppm
    fiasco-router.cppm
)
target_link_libraries(fiasco_modules PUBLIC fiasco)
