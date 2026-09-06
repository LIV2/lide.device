# Developing lide.device

This document covers what you need to build and work on lide.device locally.

## Table of Contents
* [Requirements](#requirements)
* [Quick start with Docker](#quick-start-with-docker)
* [VS Code Dev Container](#vs-code-dev-container)
* [Building without Docker](#building-without-docker)
* [Build targets](#build-targets)
* [Repository layout](#repository-layout)

## Requirements

Building this project requires an m68k-amigaos cross-compilation toolchain and a handful of Amiga-specific utilities:
* [Bebbo GCC](https://github.com/AmigaPorts/m68k-amigaos-gcc) — m68k-amigaos GCC toolchain
* [VBCC m68k-amigaos target](http://phoenix.owl.de/vbcc/2022-05-22/vbcc_target_m68k-amigaos.lha) — used for parts of the bootrom (`vasmm68k_mot`/`vlink`)
* [Amitools](https://github.com/cnvogelg/amitools) — provides `xdftool` for building the update ADF
* `lha`, `romtool`, `srec_cat` — used to package ROM images and archives

Rather than installing all of this yourself, you can use the `liv2/amiga-gcc:latest` Docker image, which already has the full toolchain set up. This is also the image used by this project's CI (see [`.github/workflows/release.yml`](.github/workflows/release.yml) and [`.github/workflows/coverity.yml`](.github/workflows/coverity.yml)), so building locally with it closely matches what CI does.

## Quick start with Docker

From the repository root:
```
docker run --rm -it -u $(id -u):$(id -g) -v ${PWD}:${PWD} -w ${PWD} liv2/amiga-gcc:latest make clean all
```

This mounts the repo into the container and runs the build, leaving output artifacts on your host under `build/`.

You can pass any other `make` target the same way, e.g.:
```
docker run --rm -it -u $(id -u):$(id -g) -v ${PWD}:${PWD} -w ${PWD} liv2/amiga-gcc:latest make disk lha
```

## VS Code Dev Container

This repo includes a [`.devcontainer/devcontainer.json`](.devcontainer/devcontainer.json) configured to use `liv2/amiga-gcc:latest`. If you have the "Dev Containers" extension installed in VS Code:

1. Open the repository folder in VS Code.
2. Run **Dev Containers: Reopen in Container** from the command palette.

This gives you an editor session running inside the same environment as CI and the Docker quick-start above, with the toolchain, include paths, and recommended extensions (C/C++ tools, GitLens, m68k-lsp) already configured.

## Building without Docker

If you'd rather install the toolchain natively, you'll need to build/install each of the requirements listed above yourself and ensure `m68k-amigaos-gcc`, `vasmm68k_mot`, `vlink`, `xdftool`, `lha`, `romtool`, and `srec_cat` are all on your `PATH`. This is more involved to set up and keep in sync with what CI uses, so it's only recommended if Docker isn't an option for you.

## Build targets

The top-level [`Makefile`](Makefile) drives the whole build:
* `make all` — builds `lide.device`, ROM images (`lide.rom`, `lide-atbus.rom`, N2630 ROMs, AmigaPCI ROM), `AIDE-lide.device`, and the `lidetool`/`lideflash`/`renamelide` utilities
* `make disk` — builds the `lide-update-<version>.adf` and `aide-boot-<version>.adf` disk images
* `make lha` — packages a release archive (`lide-update-<version>.lha`)
* `make clean` — removes build output
* `make -C lidetool` / `make -C lideflash` / `make -C rename` — build an individual utility on its own

`VERSION` is derived from `git describe --tags`, so tags matter if you want version numbers baked into build output (see the `Release-**`/`Dev-**` tag patterns used in CI).

## Repository layout

* The driver source lives at the repository root
* `bootrom/` — boot ROM loader/assets, built with vasm/vlink
* `lidetool/` — command-line configuration utility, built as a native Amiga binary
* `lideflash/` — flashing utility for updating boards' ROMs, built as a native Amiga binary
* `aide-boot/` — boot disk builder for the AIDE interface
* `dist/` — static files included on the update disk/archive (icons, readme templates, etc.)
* `3rdparty/` — third-party code, see [Third-party notice](README.md#third-party-notice) in the README
