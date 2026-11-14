add_executable(WeirdSnapshotTests
    SnapshotTests.cpp
    ../examples/sample-scenes/src/RopeScene.cpp
    ../examples/sample-scenes/src/TetrominoScene.cpp
    ../examples/sample-scenes/src/ThrustScene.cpp
)
target_link_libraries(WeirdSnapshotTests PRIVATE WeirdEngine)
target_include_directories(WeirdSnapshotTests PRIVATE ../examples/sample-scenes/include)
target_compile_definitions(WeirdSnapshotTests PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../examples/sample-scenes/assets/")
add_test(NAME WeirdSnapshotTests COMMAND WeirdSnapshotTests)
set_tests_properties(WeirdSnapshotTests PROPERTIES TIMEOUT 30)
