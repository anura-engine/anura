# Building on MacOS

In order build Anura, you will need XCode, [vcpkg](https://github.com/microsoft/vcpkg), and [homebrew](https://brew.sh/) installed.

## 1) Install `vcpkg` dependencies with homebrew (if necessary)

In order to build anura's dependencies, the following packages are required:

```sh
$ brew install pkg-config libtool automake autoconf-archive
```

## 2) Install `vcpkg`

- Clone the [vcpkg repo](https://github.com/microsoft/vcpkg) somewhere outside the anura folder.
- Inside the vcpkg folder, run `bootstrap-vcpkg.sh`.

## 3) Install dependencies

Inside the anura folder, run:

```sh
$ path/to/vcpkg/.vcpkg install
```

This will build all packages listed in `vcpkg.json`. They will be found inside `./vcpkg_installed`.

## 4) Build

Open `Frogatto.xcodeproj` and build.
