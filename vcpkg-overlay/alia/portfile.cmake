vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO "" # This is a local overlay, but vcpkg usually expects a fetch. 
            # Actually, for local dev overlays we often use vcpkg_from_path if available or just point to it.
            # However, standard practice for custom overlays being tested locally is often to use the local files.
)

# Since I want to package the CURRENT directory:
set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../..")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DALIA_BUILD_EXAMPLES=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME alia CONFIG_PATH lib/cmake/alia)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/alia" RENAME copyright OPTIONAL)
