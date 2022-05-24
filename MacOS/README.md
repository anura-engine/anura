# Building on MacOS

You will need to have `XCode` installed, from the Mac App Store; the current install was done against XCode 11.3.1 on both OS X 10.14.6, and 10.13.6, as well as XCode's command-line tools. Most likely if you're cloning this repo via `git`, you will have those tools and XCode, already.

Building the app itself ideally should be merely a matter of opening the XCode file in this folder, and hitting `Build`. This however will fail unless you install a series of dependencies — Anura requires several external libraries to run. We have elected to build these by using a popular, cross-platform package manager called `vcpkg`. In its default configuration, `vcpkg` will generate a collection of `.a` files which can be statically linked into the final binary.

> For historical reasons, it's worth noting that we used to use a much more complicated method of building our libraries, using `.dylib` and `.framework` files; [this is described here](https://github.com/frogatto/frogatto/wiki/Compiling-Frogatto-on-Mac-OS-X). This was done before we had access to viable, cross-platform package managers, so once one amply demonstrated itself we were happy to leave that manual method behind.


## Collecting Dependencies

We will need to install two command-line apps; [vcpkg](https://github.com/microsoft/vcpkg) is the first one, and can be run to create a set of static libraries (`.a` files) inside the root `anura` folder. However, this will fail unless a few command-line programs, are installed "globally" on your system" (i.e. at the top-level namespace, available in any terminal window you open).

The easiest way to install these, at the time of this writing, is to use [homebrew](https://brew.sh/), which is a widely regarded package manager for unix command-line apps on the mac.


## 1) Install `vcpkg`

- Clone the [vcpkg repo](https://github.com/microsoft/vcpkg) somewhere outside the anura folder. This can honestly go anywhere; you'll execute it by running a shell script from inside of it, via a relative path, and then setting the current working directory to the root `anura` folder.


## 2) Install several dependencies the `vcpkg` script needs, via `homebrew`

We need a few packages which can be installed as follows:

```sh
$ brew install pkg-config libtool automake autoconf-archive
```

## 3) Prepare `vcpkg`

- Inside the vcpkg folder, run `./bootstrap-vcpkg.sh`.

## 4) Run `vcpkg` and install dependencies

Assuming that you cloned `vcpkg` into a folder adjacent to the one you cloned anura into (i.e. you have a folder with both the `anura` and `vcpkg` repos in it), you should `cd` into the `anura` directory, and then run:

```sh
$ ../vcpkg/vcpkg install
```

This will build all packages listed in `vcpkg.json`. They will be found inside `./vcpkg_installed`.

## 5) Build

Open `Frogatto.xcodeproj` and click build.
