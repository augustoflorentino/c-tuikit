# c-tuikit

Interactive panels for command line tools written in C. Instead of printing a numbered menu and
reading a number in a loop, the tool shows a screen: what is going on right now, the actions it
can take, and the outcome of each one.

```
service - demo

  ok  config file found
  ok  service running for 42s

  > Start
    Stop
    Quit

up/down: move  enter: choose  q: quit
```

- status lines with a mark (`ok`, `..`, `!!`) refreshed on a timer while the panel waits for a key
- actions chosen with the arrow keys (or `j`/`k`) and enter
- each action runs with the terminal back to normal, so it can print or start other programs
- an action may leave a result, shown on its own screen until a key is pressed
- no dependencies; POSIX terminals today, Windows console planned

## Build and install

```sh
cmake -S . -B build -DBUILD_SHARED_LIBS=ON
cmake --build build
cmake --install build --prefix /usr/local
```

Use it from CMake:

```cmake
find_package(c-tuikit REQUIRED)
target_link_libraries(app PRIVATE c-tuikit::c-tuikit)
```

Or with pkg-config:

```sh
cc app.c $(pkg-config --cflags --libs c-tuikit)
```

## Single-file form

`scripts/amalgamate.sh` writes `dist/c-tuikit/c-tuikit.h`. Copy it into your project and, in exactly
one `.c` file:

```c
#define PNL_IMPLEMENTATION
#include "c-tuikit/c-tuikit.h"
```

Define `PNL_STATIC` as well to keep every function private to that file.

## Usage

```c
static void show_status(void *ctx, pnl_app_t *app)
{
    pnl_status_line(app, PNL_MARK_OK, "config file found");
}

static void start(void *ctx, pnl_app_t *app)
{
    pnl_result(app, PNL_MARK_OK, "service started");
}

pnl_app_t *app = NULL;
pnl_app_create("service", NULL, NULL, NULL, &app);
pnl_app_set_status(app, show_status, NULL, 1000);
pnl_app_add_action(app, "Start", start, NULL);
pnl_app_run(app);
pnl_app_destroy(app);
```

`pnl_status_line` works only inside the status callback and `pnl_result` only inside an action.
`pnl_app_run` returns `PNL_ERR_TTY` when stdin or stdout is not a terminal.

See [`examples/basic.c`](examples/basic.c). The header `include/c-tuikit/c-tuikit.h` is the
API reference.

Conventions:

- every function that can fail returns `PNL_OK` or an error code; `pnl_strerror` describes it
- what `pnl_x_create` returns is released only by `pnl_x_destroy`, which accepts `NULL`
- functions never take ownership of their arguments; returned pointers belong to the library
- pass your own allocator to `pnl_app_create`, or `NULL` for `malloc`/`free`

## Versioning

Semantic Versioning. Compatible releases bump the minor version; incompatible ones bump the
major version and the soname. See [CHANGELOG.md](CHANGELOG.md).

## License

MIT, see [LICENSE](LICENSE).
