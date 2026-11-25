set(WEIRD_TEST_TARGETS)

function(weird_add_test_suite TARGET_NAME)
    cmake_parse_arguments(ARG "NEEDS_ASSETS;NEEDS_SAMPLE_HEADERS" "" "SOURCES" ${ARGN})

    add_executable(${TARGET_NAME} ${ARG_SOURCES} TestHarness.h)
    target_link_libraries(${TARGET_NAME} PRIVATE WeirdEngine)
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    if(ARG_NEEDS_SAMPLE_HEADERS)
        target_include_directories(${TARGET_NAME} PRIVATE ../examples/sample-scenes/include)
    endif()

    if(ARG_NEEDS_ASSETS)
        target_compile_definitions(${TARGET_NAME} PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../examples/sample-scenes/assets/")
    endif()

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(${TARGET_NAME} PROPERTIES TIMEOUT 60)

    list(APPEND WEIRD_TEST_TARGETS ${TARGET_NAME})
    set(WEIRD_TEST_TARGETS "${WEIRD_TEST_TARGETS}" PARENT_SCOPE)
endfunction()

weird_add_test_suite(WeirdSnapshotTests
    NEEDS_ASSETS
    NEEDS_SAMPLE_HEADERS
    SOURCES SnapshotTests.cpp
)

weird_add_test_suite(WeirdShapeTests
    SOURCES ShapeTests.cpp
)

weird_add_test_suite(WeirdUITests
    SOURCES UITests.cpp
)

weird_add_test_suite(WeirdPhysicsTests
    NEEDS_ASSETS
    SOURCES PhysicsTests.cpp
)

weird_add_test_suite(WeirdSerializationTests
    NEEDS_ASSETS
    SOURCES SerializationTests.cpp
)

weird_add_test_suite(WeirdSampleSceneTests
    NEEDS_ASSETS
    NEEDS_SAMPLE_HEADERS
    SOURCES
        SampleSceneTests.cpp
        ../examples/sample-scenes/src/InstrumentScene.cpp
        ../examples/sample-scenes/src/MenuScene.cpp
        ../examples/sample-scenes/src/MusicScene.cpp
        ../examples/sample-scenes/src/RopeScene.cpp
        ../examples/sample-scenes/src/TetrominoScene.cpp
        ../examples/sample-scenes/src/ThrustScene.cpp
)

add_custom_target(WeirdTests DEPENDS ${WEIRD_TEST_TARGETS})
