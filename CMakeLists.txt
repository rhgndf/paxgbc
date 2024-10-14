cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_COMPILER arm-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabi-g++)

project(paxgbc)

# Add the libpax library as a dependency
include(FetchContent)
FetchContent_Declare(
    libpax
    GIT_REPOSITORY https://github.com/paxdevs/libpax.git
    GIT_TAG main
)
FetchContent_MakeAvailable(libpax)

# Create the shared library target
add_library(paxgbc SHARED
    main.cpp
    emu.c
    state.c
    cpu.c
    mmu.c
    disassembler.c
    lcd.c
    audio.c
    fileio.c
    debugger-dummy.c
)

# Set the optimization level
target_compile_options(paxgbc PRIVATE -O3 -Wall -Wextra -flto)

# Set the link options
target_link_options(paxgbc PRIVATE -static-libstdc++ -static-libgcc)

# Link against the libpax library
target_link_libraries(paxgbc PRIVATE libpax)

# Set the include directories for the libpax library
target_include_directories(paxgbc PRIVATE ${libpax_SOURCE_DIR}/include)
