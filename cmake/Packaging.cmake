# Runtime-only ZIPs. Development tools, source, personal settings and texture
# caches are deliberately excluded; caches can be regenerated after extraction.
install(TARGETS foodrun RUNTIME DESTINATION .)
install(DIRECTORY "${SUBWAY_ASSET_DIR}/" DESTINATION assets
    PATTERN ".cooked" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE
    PATTERN "Thumbs.db" EXCLUDE)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION .)
install(FILES "${PROJECT_SOURCE_DIR}/docs/PLAYING.md" DESTINATION . RENAME README.txt)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/external/licenses/" DESTINATION licenses)

if(WIN32 AND SUBWAY_SDL_TARGET)
    get_target_property(sdl_type ${SUBWAY_SDL_TARGET} TYPE)
    if(sdl_type STREQUAL "SHARED_LIBRARY")
        install(FILES "$<TARGET_FILE:${SUBWAY_SDL_TARGET}>" DESTINATION .)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(foodrun_package_platform windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(foodrun_package_platform linux)
else()
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" foodrun_package_platform)
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" foodrun_package_arch)
if(foodrun_package_arch MATCHES "^(amd64|x64|x86_64)$")
    set(foodrun_package_arch x86_64)
endif()

set(CPACK_PACKAGE_NAME foodrun)
set(CPACK_PACKAGE_VENDOR "Food Run contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Food Run - a C++ lane runner")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "foodrun-${PROJECT_VERSION}-${foodrun_package_platform}-${foodrun_package_arch}")
set(CPACK_GENERATOR ZIP)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_SOURCE_GENERATOR ZIP)
# Use git archive for source releases so ignored local files cannot leak into them.
set(CPACK_SOURCE_IGNORE_FILES "/[.]git/;/[.]idea/;/[.]cooked/;/build[^/]*/;/cmake-build[^/]*/;/dist/;/out/;/releases/;/game[.]cfg")
include(CPack)
