set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# STM32 VS Code bundles are intentionally not required to be in the global
# Windows PATH. Prefer the extension-managed GNU toolchain, then fall back to
# a regular PATH installation for portable command-line builds.
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(_ARM_GNU_BIN_HINTS)

if(DEFINED ENV{CUBE_BUNDLE_PATH})
    file(GLOB _CUBE_ARM_GNU_BINS LIST_DIRECTORIES true
        "$ENV{CUBE_BUNDLE_PATH}/gnu-tools-for-stm32/*/bin"
    )
    list(SORT _CUBE_ARM_GNU_BINS COMPARE NATURAL ORDER DESCENDING)
    list(APPEND _ARM_GNU_BIN_HINTS ${_CUBE_ARM_GNU_BINS})
endif()

if(CMAKE_HOST_WIN32 AND DEFINED ENV{LOCALAPPDATA})
    file(GLOB _LOCAL_ARM_GNU_BINS LIST_DIRECTORIES true
        "$ENV{LOCALAPPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin"
    )
    list(SORT _LOCAL_ARM_GNU_BINS COMPARE NATURAL ORDER DESCENDING)
    list(APPEND _ARM_GNU_BIN_HINTS ${_LOCAL_ARM_GNU_BINS})
endif()

find_program(ARM_NONE_EABI_GCC
    NAMES ${TOOLCHAIN_PREFIX}gcc.exe ${TOOLCHAIN_PREFIX}gcc
    HINTS ${_ARM_GNU_BIN_HINTS}
)

if(NOT ARM_NONE_EABI_GCC)
    message(FATAL_ERROR
        "arm-none-eabi-gcc was not found. Install the STM32 VS Code GNU tools "
        "bundle or add an Arm GNU toolchain to PATH."
    )
endif()

get_filename_component(ARM_NONE_EABI_BIN_DIR "${ARM_NONE_EABI_GCC}" DIRECTORY)

find_program(ARM_NONE_EABI_GXX
    NAMES ${TOOLCHAIN_PREFIX}g++.exe ${TOOLCHAIN_PREFIX}g++
    PATHS "${ARM_NONE_EABI_BIN_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)
find_program(ARM_NONE_EABI_OBJCOPY
    NAMES ${TOOLCHAIN_PREFIX}objcopy.exe ${TOOLCHAIN_PREFIX}objcopy
    PATHS "${ARM_NONE_EABI_BIN_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)
find_program(ARM_NONE_EABI_SIZE
    NAMES ${TOOLCHAIN_PREFIX}size.exe ${TOOLCHAIN_PREFIX}size
    PATHS "${ARM_NONE_EABI_BIN_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)

set(CMAKE_C_COMPILER   "${ARM_NONE_EABI_GCC}" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_ASM_COMPILER "${ARM_NONE_EABI_GCC}" CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_CXX_COMPILER "${ARM_NONE_EABI_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
set(CMAKE_LINKER       "${ARM_NONE_EABI_GXX}" CACHE FILEPATH "Linker" FORCE)
set(CMAKE_OBJCOPY      "${ARM_NONE_EABI_OBJCOPY}" CACHE FILEPATH "Objcopy" FORCE)
set(CMAKE_SIZE         "${ARM_NONE_EABI_SIZE}" CACHE FILEPATH "Size tool" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections -fstack-usage")

# The cyclomatic-complexity parameter must be defined for the Cyclomatic complexity feature in STM32CubeIDE to work.
# However, most GCC toolchains do not support this option, which causes a compilation error; for this reason, the feature is disabled by default.
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcyclomatic-complexity")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32F407XX_FLASH.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")
