# ARM Cortex-M bare-metal toolchain for Arm Tools for Embedded (ATfE) Clang 21
# Targets: Cortex-M4F (STM32F429ZI on the DISCO-F429ZI board)

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_ROOT "C:/Arm/ATfE-21.1.1-Windows-x86_64")
set(TOOLCHAIN_BIN  "${TOOLCHAIN_ROOT}/bin")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_BIN}/clang.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/clang++.exe")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_BIN}/clang.exe")
set(CMAKE_AR           "${TOOLCHAIN_BIN}/llvm-ar.exe")
set(CMAKE_RANLIB       "${TOOLCHAIN_BIN}/llvm-ranlib.exe")

set(CMAKE_OBJCOPY "${TOOLCHAIN_BIN}/llvm-objcopy.exe")
set(CMAKE_OBJDUMP "${TOOLCHAIN_BIN}/llvm-objdump.exe")
set(CMAKE_SIZE    "${TOOLCHAIN_BIN}/llvm-size.exe")

# CPU / FPU flags for Cortex-M4F
set(_CPU_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb")

set(CMAKE_C_FLAGS_INIT          "--target=arm-none-eabi ${_CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT        "--target=arm-none-eabi ${_CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT        "--target=arm-none-eabi ${_CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")

# Prevent CMake from trying to link a test executable against host libs
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Point CMake at the ATfE ARM sysroot so it finds the right runtime libs
set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_ROOT}/lib/clang-runtimes/arm-none-eabi")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
