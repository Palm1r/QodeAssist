add_executable(QodeAssistBench
    main.cpp
)

target_link_libraries(QodeAssistBench PRIVATE
    Qt::Core
    Session
    Agents
    Providers
    ProvidersConfig
    LLMQore
)

set_target_properties(QodeAssistBench PROPERTIES
    OUTPUT_NAME bench
    FOLDER "qtc_runnable"
)

if(APPLE)
    get_target_property(_qtcCoreLoc QtCreator::Core LOCATION)
    get_filename_component(_qtcCoreDir "${_qtcCoreLoc}" DIRECTORY)
    get_filename_component(QTC_FRAMEWORKS_DIR "${_qtcCoreDir}/../../Frameworks" ABSOLUTE)
    if(EXISTS "${QTC_FRAMEWORKS_DIR}")
        configure_file(run-bench.sh.in "${CMAKE_CURRENT_BINARY_DIR}/run-bench.sh" @ONLY)
        execute_process(COMMAND chmod +x "${CMAKE_CURRENT_BINARY_DIR}/run-bench.sh")
    endif()
endif()
