# DtBlkFx
cmake_minimum_required(VERSION 3.22)

# macOS defaults, must be set before project(). Override on the command line if needed, e.g.
#   -DCMAKE_OSX_ARCHITECTURES=arm64            (Apple Silicon only, faster to build)
#   -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"   (Universal, the default)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures to build")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

project(DTBLKFX VERSION 2.0.0 LANGUAGES C CXX)

# Static libraries (FFTW) end up inside a shared plug-in module.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(DTBLKFX_USE_SYSTEM_FFTW
    "Use an installed single-precision FFTW (pkg-config fftw3f, e.g. Homebrew) instead of building it from source"
    OFF)
option(DTBLKFX_COPY_PLUGIN_AFTER_BUILD "Copy the VST3 into the user's plug-in folder after building" OFF)

#--------------------------------------------------------------------------------------------------
# JUCE (git submodule in modules/juce)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/juce/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE is missing. Run: git submodule update --init --recursive")
endif()
add_subdirectory(modules/juce)

#--------------------------------------------------------------------------------------------------
# FFTW (single precision). DtBlkFx uses non power-of-two FFT sizes (e.g. 320, 9600, 80640), so
# Accelerate/vDSP or juce::dsp::FFT can't stand in for it.
#
# By default FFTW is downloaded and built as a static library for every architecture in
# CMAKE_OSX_ARCHITECTURES, which is what makes a one-step Universal build possible. FFTW's own
# SIMD options are x86-only and can't be mixed into a single multi-arch compile, so the scalar
# code paths are used.

if(DTBLKFX_USE_SYSTEM_FFTW)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)
    add_library(dtblkfx_fftw3f INTERFACE)
    target_link_libraries(dtblkfx_fftw3f INTERFACE PkgConfig::FFTW3F)
else()
    include(FetchContent)

    set(DTBLKFX_FFTW_URL "https://www.fftw.org/fftw-3.3.10.tar.gz" CACHE STRING "FFTW source tarball")
    set(DTBLKFX_FFTW_SHA256 "56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467"
        CACHE STRING "SHA256 of the FFTW source tarball")

    FetchContent_Declare(fftw3
        URL "${DTBLKFX_FFTW_URL}"
        URL_HASH "SHA256=${DTBLKFX_FFTW_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

    # FFTW 3.3.10's CMakeLists asks for CMake 3.0, which CMake 4 refuses without this.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    # Let the settings below win over FFTW's option() defaults, and hide its symbols so the
    # plug-in doesn't clash with other copies of FFTW loaded into the same host.
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTS OFF)
    set(ENABLE_FLOAT ON)
    set(ENABLE_THREADS OFF)
    set(ENABLE_OPENMP OFF)
    set(DISABLE_FORTRAN ON)

    FetchContent_MakeAvailable(fftw3)

    unset(CMAKE_POLICY_VERSION_MINIMUM)
    unset(CMAKE_C_VISIBILITY_PRESET)
    unset(BUILD_SHARED_LIBS)

    add_library(dtblkfx_fftw3f INTERFACE)
    target_link_libraries(dtblkfx_fftw3f INTERFACE fftw3f)
    target_include_directories(dtblkfx_fftw3f SYSTEM INTERFACE "${fftw3_SOURCE_DIR}/api")
endif()

#--------------------------------------------------------------------------------------------------
# Plug-in

if(DTBLKFX_COPY_PLUGIN_AFTER_BUILD)
    set_directory_properties(PROPERTIES JUCE_COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

juce_add_plugin(DtBlkFx
    PRODUCT_NAME "DtBlkFx_GUI"          # Name of the .vst3 bundle
    COMPANY_NAME "haiori"
    PLUGIN_NAME "DtBlkFx"               # Name shown in the DAW
    VST3_CATEGORIES Fx Spectral
    PLUGIN_MANUFACTURER_CODE DtBl
    PLUGIN_CODE DtB2
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${DTBLKFX_COPY_PLUGIN_AFTER_BUILD})

juce_add_binary_data(DtBlkFxAssets
    SOURCES
        dtblkfx_src/dtblkfx_logo.png)

target_sources(DtBlkFx PRIVATE
    src/DtBlkFxProcessor.cpp
    src/DtBlkFxEditor.cpp
    src/core/DtBlkFx.cpp
    src/core/FxRun1_0.cpp
    src/core/FxState1_0.cpp
    src/core/GlobalData.cpp
    src/core/fftw_support.cpp
    src/core/rfftw_float.cpp
    src/core/fft_frac_shift.cpp
    src/core/misc_stuff.cpp
    src/core/NoteFreq.cpp
    src/core/sweep1_coeff.cpp
    src/core/sweep2_coeff.cpp
    src/core/sweep3_coeff.cpp
    src/core/sweep4_coeff.cpp
    src/core/sweep5_coeff.cpp)

target_include_directories(DtBlkFx PRIVATE
    src
    src/core)

target_compile_definitions(DtBlkFx
    PRIVATE
        STEREO
    PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_features(DtBlkFx PRIVATE cxx_std_17)

target_link_libraries(DtBlkFx
    PRIVATE
        DtBlkFxAssets
        dtblkfx_fftw3f
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
