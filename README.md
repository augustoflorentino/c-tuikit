# c-tuikit

Interactive panels for command line tools written in C. Instead of printing a numbered menu and
reading a number in a loop, the tool shows a screen: what is going on right now, the actions it
can take, and the outcome of each one.

```
service - demo

  ok  config   file found
  ok  service  running for 42s

  > 1 Start
    2 Stop
    q Quit

up/down: move  enter: choose  ?: help  q: quit
```

- status lines with a mark (`ok`, `..`, `!!`), a label and a value, refreshed on a timer while the
  panel waits for a key; a value that changes is highlighted for a moment
- values that live outside the status callback (`pnl_value_t`), set from any thread; the panel
  wakes up and redraws only what changed
- actions chosen with the arrow keys (or `j`/`k`) and enter, by number, by first letter, or by
  name with `:`; `?` lists the keys of the current screen
- an action may leave a result, open a live screen of its own, or hand the terminal back
  (`pnl_app_with_terminal`) to print or start other programs
- slow work runs on a thread with progress, a log and esc to cancel (`pnl_task_run`)
- ready-made screens: confirmation, text input, filterable pick list, pager with search and a
  table you can sort and filter; each one degrades to plain stdin and stdout on the command line
- tables of millions of rows read from a callback (`pnl_table_source`), sorted and filtered on a
  thread without blocking keys
- optional memory between runs (`pnl_app_set_memory`): the last action, the last item picked,
  input history and the filter and order of each table
- a live log fed from any thread (`pnl_log_*`): follows the end, pauses while you scroll, filters
  by level and by text
- a bell and a desktop notification when a long task ends while you are in another window
  (`pnl_notify`; `TUIKIT_NOTIFY=bell` or `off`)
- keys the user can remap in `~/.config/tuikit/keys` or per tool (`ctrl-n = down`, `q = none`),
  or from code with `pnl_app_bind`
- the same actions are subcommands: `tool start`, `tool status --json`, `tool --help`
- colors by role (accent, ok, warn, fail, muted...) fitted to the terminal: true color, 256 or
  16 colors, light or dark background, `NO_COLOR`; themes built in or from a file
  (`TUIKIT_THEME=warm`, `TUIKIT_THEME=~/.config/mytool/theme.txt`)
- charts for the canvas: bars (with the percentage inside if you like), sparklines, column
  charts several rows tall, labeled columns side by side and line plots drawn with braille dots;
  each one can shift from green to red as the value rises
- waiting indicators: five spinners (`pnl_canvas_spinner`: dots, line, arc, blocks, bounce) and
  a bar that sweeps back and forth when there is no end to measure (`pnl_canvas_busy`), which
  tasks without a total now show
- a frame clock for small time-based animations: spinners, smooth bars, a fading highlight on
  values that change; optional sliding selection, screen transitions, pulse, typewriter and
  entrances (`PNL_MOTION_ENTER`: dialogs grow in and shrink away, notifications slide out, menu
  items arrive one after the other) and feedback (`PNL_MOTION_FEEDBACK`: a dialog shakes at a
  key it does not take, the chosen action flashes, a light runs along progress bars) and live
  numbers (`PNL_MOTION_COUNT`: a number in a status line counts up or down to its new value, a
  meter glides to its new level)
  (`TUIKIT_MOTION=all`). Animations never hold back a key and turn off with `TUIKIT_MOTION=off`
  or on their own over a slow SSH link
- components (`pnl_widget_t`) you put together into screens: text, bars, graphs, spinners and the
  live log come ready, stacks arrange them in rows or columns, and a component of your own is a
  draw callback away; see [Components](#components)
- screens split into panels side by side, stacked or in tabs (`pnl_layout_*`); tab moves the
  focus, `[` and `]` switch tabs, and panels and table columns marked optional fold away when the
  terminal gets narrow
- dialogs drawn over the current screen (`pnl_view_push_dialog`; confirmation, input and pick
  lists use them) and short notifications in the corner that go away on their own (`pnl_toast`)
- the mouse works too: click an action, a row, a table header, a panel or a tab; the wheel
  scrolls. `TUIKIT_MOUSE=off` leaves the mouse to the terminal
- web addresses anywhere on screen become clickable links (OSC 8), pointing to the whole address
  even when the screen cuts it short; `pnl_canvas_link` links any text, `TUIKIT_LINKS=off` turns
  them off
- f12 opens the inspector, an X-ray of the running program: frame times with a sparkline, the
  stack of screens, how many actions, values, timers, watches and tasks are alive, the state of
  animations and of the terminal, and the last events (keys, timers, wakes, watches, resizes);
  `TUIKIT_INSPECT=off` turns it off
- for everyone: `TUIKIT_LINEAR=on` turns the panel into plain lines a screen reader can follow,
  `TUIKIT_CONTRAST=high` pushes every color to a strong contrast, and every action works from the
  keyboard alone
- no dependencies besides threads; Linux, macOS and the Windows console (Windows 10 or later)

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
    pnl_status_line(app, PNL_MARK_OK, "config", "file found");
}

static void start(void *ctx, pnl_app_t *app)
{
    pnl_result(app, PNL_MARK_OK, "service started");
}

pnl_app_t *app = NULL;
pnl_app_create("service", NULL, &app);
pnl_app_on_status(app, show_status, NULL);
pnl_app_set_refresh(app, 1000);
pnl_app_add_action(app, "Start", start, NULL);
pnl_app_run(app, argc, argv, &exit_code);
pnl_app_destroy(app);
return exit_code;
```

`pnl_status_line` works only inside the status callback; `pnl_value_set` works anywhere, from any
thread; `pnl_result` and `pnl_task_run` while
the panel or a command runs. Without a command and with a terminal, `pnl_app_run` opens the
panel; otherwise it prints the status lines, which makes the tool usable in scripts and pipes.
Work given to `pnl_task_run` runs on a thread in the panel and in the foreground on the command
line, so the same function serves both.

See [`examples/basic.c`](examples/basic.c), a tour of every feature, and
[`examples/deploy.c`](examples/deploy.c), a small tool built the way a real one would be, and
[`examples/dashboard.c`](examples/dashboard.c), a screen made of panels and tabs,
[`examples/tail.c`](examples/tail.c), which follows a log file, and
[`examples/csv.c`](examples/csv.c), which opens a CSV file of any size as a table, and
[`examples/monitor.c`](examples/monitor.c), a screen made only of components, and
[`examples/gallery.c`](examples/gallery.c), which keeps generating new combinations of charts,
bars, spinners and motion. The
header `include/c-tuikit/c-tuikit.h` is the API reference.

## Components

A screen can be built from components instead of drawn by hand. Each one draws itself in the
space it gets, may take keys, may say how many rows it wants and which keys it answers to:

```c
pnl_widget_t *cpu, *load, *events, *side;
pnl_widget_bar(app, "cpu", PNL_METER_LEVEL | PNL_METER_PERCENT, &cpu);
pnl_widget_graph(app, "load", PNL_GRAPH_CHART, 0, PNL_METER_LEVEL, 300, &load);
pnl_widget_log(app, logbook, &events);
pnl_widget_stack(app, PNL_STACK_ROWS, &side);
pnl_widget_add(side, cpu, 0);
pnl_widget_add(side, load, 0);

pnl_layout_widget(layout, PNL_LAYOUT_ROOT, "machine", side, NULL);
pnl_layout_widget(layout, PNL_LAYOUT_ROOT, "events", events, NULL);

pnl_widget_set_value(cpu, 0.42);
pnl_widget_set_value(load, 42.0);
```

`pnl_widget_set_value` moves a bar (gliding with `PNL_MOTION_COUNT`) or adds a point to a graph;
`pnl_widget_set_text` changes text and labels. A component goes in a layout panel
(`pnl_layout_widget`), fills a screen of its own (`pnl_view_push_widget`), sits in a stack or is
drawn by another component (`pnl_widget_draw`). Tab, mouse clicks, the footer and `?` work the
same for every one of them.

A component of your own is a class of callbacks:

```c
static const pnl_widget_class_t clock_class = {clock_draw, clock_key, clock_height, clock_hints, NULL};
pnl_widget_create(app, &clock_class, &clock_state, &clock);
```

Only `draw_fn` is required. `height_fn` returns the rows it wants for a width (0 to take what is
left), `hints_fn` the keys for the footer, and `release_fn` runs on `pnl_widget_destroy`.
Components are changed from the panel's own thread (actions, timers, keys) and must outlive the
screens that show them. See [`examples/monitor.c`](examples/monitor.c), with a clock made from
scratch.

## Testing a tool

A panel runs without a terminal too. `pnl_test_create` starts it on a virtual screen with a
virtual clock; `pnl_test_keys` types a script of keys and waits until the screen settles, and
`pnl_test_expect` compares the screen with a snapshot file:

```c
pnl_test_t *test = NULL;
pnl_test_create(app, 60, 20, &test);
pnl_test_keys(test, "down enter \"release-1.2\" enter");
pnl_test_wait(test, 2000);
if (pnl_test_expect(test, "tests/snapshots/release.txt") != PNL_OK)
    return 1;
pnl_test_destroy(test);
```

A missing snapshot is written on the first run; a different screen is saved next to it as
`.new` and the call returns `PNL_ERR_MISMATCH`. `TUIKIT_SNAPSHOTS=update` rewrites them all.
Timers and toasts follow the virtual clock, so the same script gives the same screen.

`TUIKIT_RECORD=demo.cast tool` records a session as an asciicast file, ready for
`asciinema play` or for turning into a GIF; `pnl_app_record(app, path)` does the same from code,
and under test it records the scripted session with the virtual clock.

Conventions:

- every function that can fail returns `PNL_OK` or an error code; `pnl_strerror` describes it
- what `pnl_x_create` returns is released only by `pnl_x_destroy`, which accepts `NULL`
- functions never take ownership of their arguments; returned pointers belong to the library
- pass a `pnl_allocator_t` to `pnl_app_create`, or `NULL` for `malloc`/`free`

`bench/run.sh` measures drawing, rendering, logs, values, a table of a million rows and the
memory each part takes.

## Versioning

Semantic Versioning. Compatible releases bump the minor version; incompatible ones bump the
major version and the soname. Since 1.0.0 the API and the ABI of the shared library hold within a
major version: CI compares every build against the baseline in `abi/`. A function on its way out
is marked deprecated for at least one minor release before the next major removes it. See
[CHANGELOG.md](CHANGELOG.md).

## License

MIT, see [LICENSE](LICENSE).
