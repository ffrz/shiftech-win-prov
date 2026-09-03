function(add_shiftech_test test_name source_file)
    qt_add_executable(${test_name} ${source_file})
    set_target_properties(${test_name} PROPERTIES WIN32_EXECUTABLE OFF)
    target_link_libraries(${test_name} PRIVATE shiftech_core Qt6::Test)
    target_compile_definitions(${test_name} PRIVATE
        FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        SHIPPED_PROFILES_DIR="${CMAKE_SOURCE_DIR}/profiles")
    set_shiftech_warnings(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_shiftech_test(test_hardwareid unit/test_hardwareid.cpp)
add_shiftech_test(test_deviceclassify unit/test_deviceclassify.cpp)
add_shiftech_test(test_drivermatch unit/test_drivermatch.cpp)
add_shiftech_test(test_mockprovider unit/test_mockprovider.cpp)
add_shiftech_test(test_targetsystem unit/test_targetsystem.cpp)
add_shiftech_test(test_profileloader unit/test_profileloader.cpp)
add_shiftech_test(test_wingetparse unit/test_wingetparse.cpp)
