vcpkg_from_gitlab(
    GITLAB_URL https://gitlab.freedesktop.org/
    OUT_SOURCE_PATH SOURCE_PATH
    REPO libdecor/libdecor
    REF ca6e6b68a3ecc4cff2da975580dbe1ae07caf18e
    SHA512 a4749861fff46db0b8ed1f74e0bbc1c3cba9eca0bf1c9861d4e3f49d17447c48530e06573fd796255b606aee2f7029ab7f466a2b2f822e43d8f832314d38ddc7
    HEAD_REF master # branch name
)

if("gtk" IN_LIST FEATURES)
    list(APPEND OPTIONS -Dgtk=enabled)
else()
    list(APPEND OPTIONS -Dgtk=disabled)
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${OPTIONS}
        -Ddemo=false
)
vcpkg_install_meson()
vcpkg_copy_pdbs()
vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)
vcpkg_fixup_pkgconfig()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
