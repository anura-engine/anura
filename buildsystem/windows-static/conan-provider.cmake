set(conan_provider "${CMAKE_BINARY_DIR}/_deps/conan_provider.cmake")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
file(DOWNLOAD
    "https://raw.githubusercontent.com/conan-io/cmake-conan/0.19.0/conan_provider.cmake"
    "${conan_provider}"
    EXPECTED_HASH SHA256=d574ac4f1ad0784e743304fde4af6e6692191783afae4898974903b622928546
    TLS_VERIFY ON
)
include("${conan_provider}")
