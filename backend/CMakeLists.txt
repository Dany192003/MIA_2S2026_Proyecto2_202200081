cmake_minimum_required(VERSION 3.10)
project(EXT2FS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar librerías
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(Crow REQUIRED)

# Archivos fuentes
set(SOURCES
    src/main.cpp
    src/server/http_server.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/commands/command_handler.cpp
    src/commands/mkdisk.cpp
    src/commands/rmdisk.cpp
    src/commands/fdisk.cpp
    src/commands/mount.cpp
    src/commands/mounted.cpp
    src/commands/mkfs.cpp
    src/commands/login.cpp
    src/commands/logout.cpp
    src/commands/mkgrp.cpp
    src/commands/rmgrp.cpp
    src/commands/mkusr.cpp
    src/commands/rmusr.cpp
    src/commands/chgrp.cpp
    src/commands/mkfile.cpp
    src/commands/mkdir.cpp
    src/commands/cat.cpp
    src/commands/rep.cpp
    src/commands/lsdisk.cpp   
    src/commands/lsjson.cpp
    src/commands/lsreports.cpp
    src/commands/reports/ReportUtils.cpp
    src/commands/reports/ReportText.cpp
    src/commands/reports/ReportGraphviz.cpp
    src/utils/json_utils.cpp
    src/utils/ext2_utils.cpp
)
# Crear ejecutable
add_executable(ext2fs ${SOURCES})

# Enlazar librerías
target_link_libraries(ext2fs
    nlohmann_json::nlohmann_json
    Crow::Crow
)

target_include_directories(ext2fs PRIVATE src)

# Compilar con advertencias
target_compile_options(ext2fs PRIVATE -Wall -Wextra)