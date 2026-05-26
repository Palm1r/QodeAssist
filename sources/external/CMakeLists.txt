include(FetchContent)

set(INJA_BUILD_TESTS OFF CACHE INTERNAL "")
set(INJA_INSTALL OFF CACHE INTERNAL "")
set(INJA_EXPORT OFF CACHE INTERNAL "")
set(BUILD_BENCHMARK OFF CACHE INTERNAL "")
set(COVERALLS OFF CACHE INTERNAL "")
FetchContent_Declare(inja
    GIT_REPOSITORY https://github.com/pantor/inja.git
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(inja)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

add_subdirectory(llmqore)
