cmake_minimum_required(VERSION 3.20)
project(pdf-md-bridge)

set(CMAKE_CXX_STANDARD 20)

# 1. Buscar la herramienta pkg-config
find_package(PkgConfig REQUIRED)

# 2. Localizar el componente poppler-cpp de forma obligatoria
pkg_check_modules(POPPLER_CPP REQUIRED IMPORTED_TARGET poppler-cpp)

# 3. Declarar tu ejecutable
add_executable(pdf-md-bridge main.cpp)

# 4. Vincular la biblioteca cargada a tu objetivo
target_link_libraries(pdf-md-bridge PRIVATE PkgConfig::POPPLER_CPP)
