# Building on Windows

In order build Anura, you will need [Visual
Studio](https://visualstudio.microsoft.com/downloads/) 2022.

The solution and project wrap vcpkg for you and you should be simply able to
"press play".

An alternate route is to only grab the [build
tools](https://visualstudio.microsoft.com/downloads/?q=build+tools#build-tools-for-visual-studio-2022)
and to build on the command line with `msbuild`. See the
[CI](https://github.com/anura-engine/anura/blob/trunk/.github/workflows/push-unit-tests-static-windows.yaml)
definition for the current details.
