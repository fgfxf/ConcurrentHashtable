# 内存泄漏测试目标，作为主项目的子目录构建
set(SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/../Common.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/../Threads.cpp)

add_executable(memory_leak_test ${SRC_LIST} memory_leak_test.cpp)
target_include_directories(memory_leak_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(memory_leak_test "-lrt")

# 注册为测试，便于通过 ctest 运行
add_test(NAME memory_leak_test COMMAND memory_leak_test)
