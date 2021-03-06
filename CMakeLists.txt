# @H2_LICENSE_TEXT@

cmake_minimum_required(VERSION 3.14.0)

# Use the OLD policy for CMP0074. The NEW policy picks MPI_ROOT environment variable, which is automatically defined by the Spectrum-MPI module on Lassen. Apparently, it results in a compilation error due to -pthread being added to nvcc device link command. Another workaround is to unset MPI_ROOT when invoking CMake.
cmake_policy(SET CMP0074 OLD)

project(DiHydrogen
  VERSION 0.0.1
  DESCRIPTION "A distributed multilinear algebra interface library."
  HOMEPAGE_URL "https://github.com/llnl/DiHydrogen"
  LANGUAGES CXX)

# Add CMake modules
set(CMAKE_MODULE_DIR "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CONFIG_FILE_DIR "${CMAKE_SOURCE_DIR}/cmake/config")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_DIR})

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELEASE CACHE STRING "CMake build type" FORCE)
endif ()

string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE)

include(GNUInstallDirs)
include(H2CMakeUtils)
include(H2CXXFeatureDetection)
include(SetupCXX)
include(SetupMPI)

#
# It will be treated as a FATAL_ERROR to enable any feature and then
# subsequently fail to find its dependencies.
#

option(H2_ENABLE_OPENMP
  "Enable CPU acceleration with OpenMP threads."
  OFF)

option(H2_ENABLE_FP16
  "Enable FP16 support on the CPU. Requires the Half library."
  OFF)

option(H2_ENABLE_CUDA
  "Search for and enable CUDA language features in DiHydrogen."
  OFF)

option(H2_ENABLE_HIP_ROCM
  "Search for and enable ROCm/HIP language features in DiHydrogen."
  OFF)

option(H2_DEVELOPER_BUILD
  "Enable extra warnings and force tests to be enabled."
  ON)

option(H2_ENABLE_CODE_COVERAGE
  "Enable code coverage instrumentation. Requires compiler support."
  OFF)

option(H2_ENABLE_TESTS
  "Enable test codes. Requires Catch2."
  ${H2_DEVELOPER_BUILD})

option(H2_ENABLE_WERROR
  "Enable the \"-Werror\" flag. Requires compiler support."
  OFF)

# Check option consistency
if (H2_ENABLE_CUDA AND H2_ENABLE_HIP_ROCM)
  message(FATAL_ERROR
    "CUDA and ROCm support are mutually exclusive. Please only enable "
    "one of H2_ENABLE_CUDA and H2_ENABLE_HIP_ROCM and reconfigure.")
endif (H2_ENABLE_CUDA AND H2_ENABLE_HIP_ROCM)

# C++ features

# TODO: h2_cxx_set_cxx_standard(H2_CXX_STANDARD)
h2_cxx_determine_pretty_function(H2_PRETTY_FUNCTION)
h2_cxx_determine_restrict_qualifier(H2_RESTRICT)

if (H2_ENABLE_OPENMP)
  find_package(OpenMP 3.0 REQUIRED COMPONENTS CXX)
  set(H2_HAS_OPENMP ${OPENMP_CXX_FOUND})
endif ()

if (H2_ENABLE_FP16)
  find_package(Half REQUIRED)
  set(H2_HAS_HALF ${HALF_FOUND})
endif ()

if (H2_DEVELOPER_BUILD)
  # It is *not* a FATAL_ERROR to not have clang-format.
  include(H2AddClangFormat)
  h2_cxx_get_developer_warning_flags(H2_EXTRA_CXX_FLAGS)
endif ()

if (H2_ENABLE_WERROR)
  h2_cxx_get_warnings_as_errors_flag(H2_EXTRA_CXX_FLAGS)
endif ()

if (H2_ENABLE_CODE_COVERAGE)
  include(H2CXXCodeCoverage)
  add_custom_target(coverage)
  set(H2_CODE_COVERAGE TRUE)
endif ()

# Required dependencies
find_package(BLASImpl QUIET REQUIRED)
if (BLASImpl_FOUND)
  message(STATUS "Found BLAS: ${BLAS_LIBRARIES}")
endif ()

# DiHydrogen will use MPI-3 features extensively. Until proven
# otherwise, assume that the CMake module version checking is
# accurate.
find_package(MPI 3.0.0 COMPONENTS CXX REQUIRED)

# Other device features
if (H2_ENABLE_CUDA)
  enable_language(CUDA)
  include(SetupCUDAToolkit)
  #find_package(CUBLAS REQUIRED)
  find_package(CUB REQUIRED)
  find_package(cuDNN REQUIRED)
  if (CMAKE_CUDA_COMPILER)
    set(H2_HAS_CUDA TRUE)
  endif ()
  find_package(P2P)
  find_package(NVSHMEM)
endif (H2_ENABLE_CUDA)

# This is a placeholder.
if (H2_ENABLE_HIP_ROCM)
  message(FATAL_ERROR
    "ROCm support is not yet available. Please reconfigure with "
    "-DH2_ENABLE_ROCM=OFF")
  if (HIP_FOUND)
    set(H2_HAS_HIP_ROCM TRUE)
  endif (HIP_FOUND)
endif ()

if (H2_HAS_CUDA OR H2_HAS_HIP_ROCM)
  set(H2_HAS_GPU TRUE)
endif ()

find_package(Aluminum NO_MODULE QUIET
  HINTS ${Aluminum_DIR} ${ALUMINUM_DIR} ${AL_DIR}
  $ENV{Aluminum_DIR} $ENV{ALUMINUM_DIR} $ENV{AL_DIR}
  PATH_SUFFIXES lib64/cmake/aluminum lib/cmake/aluminum
  NO_DEFAULT_PATH)
if (NOT Aluminum_FOUND)
  find_package(Aluminum NO_MODULE REQUIRED)
endif ()

# Decide where to put generated include files.
set(CMAKE_GENERATED_INCLUDE_DIRECTORY
  "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")

# Decide where to put built executable files.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

# Build the library
configure_file(
  "${CONFIG_FILE_DIR}/h2_config.hpp.in"
  "${CMAKE_GENERATED_INCLUDE_DIRECTORY}/h2_config.hpp"
  @ONLY)

add_subdirectory(include/h2)
add_subdirectory(src)

# Setup the testing
if (H2_ENABLE_TESTS)
  include(CTest)
  add_subdirectory(test)
endif ()

# Setup clang format
if (TARGET clang-format)
  add_clang_format_to_all_targets()
endif ()

# Setup the documentation targets
add_subdirectory(doc)

# The legacy distconv code
add_subdirectory(legacy)

# Install target stuff
include (CMakePackageConfigHelpers)

# The version file is the same for install and build directories.
write_basic_package_version_file(
  "${CMAKE_BINARY_DIR}/DiHydrogenConfigVersion.cmake"
  COMPATIBILITY SameMinorVersion)

# Build tree export
export(EXPORT DiHydrogenTargets
  NAMESPACE H2::
  FILE DiHydrogenTargets.cmake)

# Write the configure file for the build tree
set(INCLUDE_INSTALL_DIRS
  "${CMAKE_SOURCE_DIR}/include/h2"
  "${CMAKE_BINARY_DIR}" # h2_config.hpp lives here.
  "${CMAKE_BINARY_DIR}/include/h2")
set(LIB_INSTALL_DIR "${CMAKE_BINARY_DIR}")
set(EXTRA_CMAKE_MODULE_DIR "${CMAKE_MODULE_DIR}")
configure_package_config_file(
  "${CONFIG_FILE_DIR}/DiHydrogenConfig.cmake.in"
  "${CMAKE_BINARY_DIR}/DiHydrogenConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_BINARY_DIR}"
  PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR)

# Now the install tree...
set(INCLUDE_INSTALL_DIRS ${CMAKE_INSTALL_INCLUDEDIR})
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/dihydrogen)
unset(EXTRA_CMAKE_MODULE_DIR)

# Install tree export
install(EXPORT DiHydrogenTargets
  NAMESPACE H2::
  DESTINATION ${CMAKE_INSTALL_DIR})

# Write the configure file for the install tree
configure_package_config_file(
  "${CONFIG_FILE_DIR}/DiHydrogenConfig.cmake.in"
  "${CMAKE_BINARY_DIR}/DiHydrogenConfig.install.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_DIR}
  PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR)

# Install the configuration headers
install(FILES
  "${CMAKE_GENERATED_INCLUDE_DIRECTORY}/h2_config.hpp"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Install the CMake stuff
install(FILES
  "${PROJECT_BINARY_DIR}/DiHydrogenConfig.install.cmake"
  RENAME "DiHydrogenConfig.cmake"
  DESTINATION ${CMAKE_INSTALL_DIR})
install(FILES
  "${PROJECT_BINARY_DIR}/DiHydrogenConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_DIR})
install(DIRECTORY ${CMAKE_MODULE_DIR}
  DESTINATION ${CMAKE_INSTALL_DIR}
  FILES_MATCHING PATTERN "*.cmake")
