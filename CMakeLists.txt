cmake_minimum_required(VERSION 3.16)
project(samp_native VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Required for the vendored RakNet fork: GCC -O2 deletes null checks before
# sized array-delete (cookie read at p-8 faults for p==0). Upstream assumed
# MSVC semantics. Proven by bisection: only this flag fixes release crashes.
add_compile_options(-fno-delete-null-pointer-checks)

add_library(samp_portable INTERFACE)
target_include_directories(samp_portable INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()

# --- sampraknet (vendored in research clone; built, not copied) ---
# Linux-port delta lives in the clone (see RISET.md): SEH guard in
# RakPeer.cpp + forced `-include cstdint` below (GCC 16 vs MSVC).
set(SAMPRAKNET_DIR /home/iqaruz/Projects/opensamp-riset/native/vendor/sampraknet)
add_subdirectory(${SAMPRAKNET_DIR} ${CMAKE_BINARY_DIR}/vendor/sampraknet)
target_compile_options(raknet PRIVATE -include cstdint)

add_executable(test_encoding tests/test_encoding.cpp)
target_link_libraries(test_encoding PRIVATE samp_portable)
add_test(NAME encoding
  COMMAND test_encoding ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/cp1251.txt)

add_executable(test_color tests/test_color.cpp)
target_link_libraries(test_color PRIVATE samp_portable)
add_test(NAME color COMMAND test_color)

add_executable(test_netlink tests/test_netlink.cpp)
target_link_libraries(test_netlink PRIVATE raknet)
target_include_directories(test_netlink PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME netlink COMMAND test_netlink)

add_executable(test_chat tests/test_chat.cpp)
target_link_libraries(test_chat PRIVATE samp_portable raknet)
target_include_directories(test_chat PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME chat COMMAND test_chat)

add_executable(test_sync tests/test_sync.cpp)
target_link_libraries(test_sync PRIVATE samp_portable raknet)
target_include_directories(test_sync PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME sync COMMAND test_sync)

add_executable(test_sync_in tests/test_sync_in.cpp)
target_link_libraries(test_sync_in PRIVATE samp_portable raknet)
target_include_directories(test_sync_in PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME sync_in COMMAND test_sync_in)

add_executable(test_vehicle tests/test_vehicle.cpp)
target_link_libraries(test_vehicle PRIVATE samp_portable raknet)
target_include_directories(test_vehicle PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME vehicle COMMAND test_vehicle)

add_executable(test_handshake tests/test_handshake.cpp)
target_link_libraries(test_handshake PRIVATE samp_portable raknet)
target_include_directories(test_handshake PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME handshake COMMAND test_handshake)

add_executable(test_twoplayer tests/test_twoplayer.cpp)
target_link_libraries(test_twoplayer PRIVATE samp_portable raknet)
target_include_directories(test_twoplayer PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME twoplayer COMMAND test_twoplayer)

add_executable(test_loopback tests/test_loopback.cpp)
target_link_libraries(test_loopback PRIVATE raknet)
target_include_directories(test_loopback PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
add_test(NAME loopback COMMAND test_loopback)

add_library(samp_native SHARED port/net/samp_native_api.cpp)
target_link_libraries(samp_native PRIVATE raknet)
target_include_directories(samp_native PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
set_target_properties(samp_native PROPERTIES
  CXX_VISIBILITY_PRESET default
  POSITION_INDEPENDENT_CODE ON)

add_executable(test_api tests/test_api.cpp)
target_link_libraries(test_api PRIVATE samp_native)
add_test(NAME api COMMAND test_api)

add_executable(test_api_live tests/test_api_live.cpp)
target_link_libraries(test_api_live PRIVATE samp_native)
add_test(NAME api_live COMMAND test_api_live)

add_executable(chatcli port/cli/chatcli.cpp)
target_link_libraries(chatcli PRIVATE samp_portable raknet)
target_include_directories(chatcli PRIVATE
  ${SAMPRAKNET_DIR}/src ${SAMPRAKNET_DIR}/include)
