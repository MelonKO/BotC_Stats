# Invoked at build time by the 'deploy' target:
#   cmake -DSRC_DIR=<dir with built exe> -DVCPKG_BIN_DIR=<vcpkg bin> -DDST_DIR=<dist> -P copy_runtime_dlls.cmake
# vcpkg's applocal step places all third-party runtime DLLs (zlib, pcre2,
# harfbuzz, freetype, openssl, ...) next to the built exe. windeployqt only
# deploys Qt's own DLLs and plugins, so these have to be copied separately.
file(GLOB dlls "${SRC_DIR}/*.dll")
file(COPY ${dlls} DESTINATION "${DST_DIR}")

# DLLs used only by Qt plugins are not applocal'd next to the exe (the exe
# does not link them directly), so they are listed here by hand. Currently
# that is just libjpeg for imageformats/qjpeg.dll. Extend this list if a new
# plugin starts failing to load from dist/.
file(COPY "${VCPKG_BIN_DIR}/jpeg62.dll" DESTINATION "${DST_DIR}")
