# Stability

What a release of c-tuikit promises to code built on it, and what it does not.

## Versions

c-tuikit follows [Semantic Versioning](https://semver.org). A version is `MAJOR.MINOR.PATCH`:

- **patch** (6.3.0 to 6.3.1): fixes only. Nothing in the header changes.
- **minor** (6.5.0 to 6.6.0): new functions, new macros, new values of existing flags, new error
  codes, new environment variables. Code that compiled and linked against the previous minor
  release compiles, links and runs the same way.
- **major** (5.x to 6.0.0): anything may change. The soname changes with it.

Every release is described in [CHANGELOG.md](../CHANGELOG.md), with a section on what updating
means for existing code. Releases are tagged `vMAJOR.MINOR.PATCH` in git.

## What is stable within a major version

**Source (API).** Every function, typedef and macro in `include/c-tuikit/c-tuikit.h`: its name, its
arguments, its return values and the meaning of each status code it returns. A function may
learn to accept arguments it used to reject with `PNL_ERR_ARG`.

**Binary (ABI).** The shared library keeps the soname `libtuikit.so.MAJOR`. A program linked against any release of a major version runs against any later
release of the same major version without being rebuilt. Every type the header exposes is
either opaque (`pnl_app_t`, `pnl_widget_t`, ...) or a small struct whose layout is frozen
(`pnl_allocator_t`, `pnl_widget_class_t`).

CI checks this on every build: `scripts/abi.sh check` compares the library against the baseline
in `abi/c-tuikit.abi` with libabigail and fails on any removed symbol or changed type. The
baseline is saved again at every release, so it always holds everything published so far.

**Files and settings.** The formats a user writes by hand stay readable: theme files, key files
(`~/.config/tuikit/keys`) and the environment variables in the README. The memory file written by
`pnl_app_set_memory` is read back by later releases; an entry the library no longer knows is
ignored.

**Build.** The CMake package (`find_package(c-tuikit)`, target `c-tuikit::c-tuikit`), the pkg-config
name `c-tuikit`, the options `BUILD_SHARED_LIBS` and `C_TUIKIT_BUILD_TESTS`, `C_TUIKIT_BUILD_EXAMPLES`,
`C_TUIKIT_BUILD_FUZZ`, and the single-file form with `PNL_IMPLEMENTATION` and `PNL_STATIC`.

**Command line.** The subcommands, `--json`, `--yes` and `--help` that `pnl_app_run` gives every
tool, and the shape of the JSON it prints. New fields may be added to the JSON.

## What is not stable

- What a screen looks like: layout, spacing, colors, symbols, animations, the wording of the
  footer, help and messages. Snapshots taken with `pnl_test_expect` may need to be saved again
  after a minor release (`TUIKIT_SNAPSHOTS=update`).
- Anything under `src/`, including `pnl_internal.h` and every name marked internal. The
  single-file form (`dist/c-tuikit/c-tuikit.h`) exposes them to the compiler; they are still not API.
- The order in which callbacks run within one frame, beyond what the documentation states.
- Fixed limits (key bindings, colors, watched descriptors, steps, scenes): a minor release may
  raise them, never lower them. Reaching one returns `PNL_ERR_LIMIT`.
- Performance numbers from `bench/`.
- The fuzz and test programs, the scripts in `scripts/` and the layout of the source tree.

## Deprecation

A function on its way out is marked with `PNL_DEPRECATED` for at least one minor release before
the next major version removes it; compilers then warn where it is used and the warning names
the replacement. Define `PNL_NO_DEPRECATED_WARNINGS` to silence the warnings while migrating.
The CHANGELOG lists every deprecation and every removal.

## Behaviour changes inside a major version

A minor release may change behaviour that was a bug, or that the documentation left open, when
the old behaviour could hide mistakes. The CHANGELOG says so under the release, with what to do.
For example, 6.6.0 made `pnl_test_expect` fail on a missing snapshot instead of writing it and
passing, and made fixed limits return `PNL_ERR_LIMIT` instead of `PNL_ERR_NOMEM`.

## Platforms

Supported: Linux and macOS with a POSIX terminal, and the Windows console on Windows 10 or later.
The library is C99 and needs only the C library and threads. Dropping a platform or raising the
oldest supported Windows version happens only in a major release.

## Security fixes

Only the latest release receives fixes; see [SECURITY.md](../SECURITY.md).
