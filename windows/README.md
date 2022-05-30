# Building on Windows

In order build Anura, you will need Visual Studio 2022 and [vcpkg](https://github.com/microsoft/vcpkg) installed.

If you do not have vcpkg set up:
- Clone the [vcpkg repo](https://github.com/microsoft/vcpkg) somewhere outside the anura folder.
- Inside the vcpkg folder, run `bootstrap-vcpkg.bat`, then `vcpkg integrate install`.

Once vcpkg is integrated with Visual Studio:
- Open `anura.sln` and build. The executable will be in `./$(Target)-$(Configuration)/` (`./x64-Release`, for example).
