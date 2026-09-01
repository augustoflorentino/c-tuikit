# API reference

This page lists every function, type and macro in `include/c-tuikit/c-tuikit.h`. For an
overview and examples see [../README.md](../README.md). [threads.md](threads.md) says which
functions may be called from which thread, and [stability.md](stability.md) says what a release
promises about the API and the ABI.

Conventions used by every function:

- Functions that can fail return an `int` status: `PNL_OK` (0) on success or one of the
  `PNL_ERR_*` codes below. `pnl_strerror` turns a status into text.
- Out parameters are set to `NULL` or 0 when a function fails. Most functions do this
  before they check their arguments.
- Every `*_destroy` function accepts `NULL` and does nothing.
- Strings are UTF-8. Strings passed in are copied unless the function says otherwise, so
  the caller may free or reuse them after the call returns.

## Building

| Macro | Effect |
| --- | --- |
| `PNL_STATIC` | Declares every function `static`, marked unused on GCC. |
| `PNL_SHARED` | Define when using the shared library. On Windows the functions are then imported with `__declspec(dllimport)`. The CMake target and `c-tuikit.pc` add it for the shared build. |
| `PNL_IMPLEMENTATION` | With the single-file form (`dist/c-tuikit/c-tuikit.h`, made by `scripts/amalgamate.sh`), define it in exactly one `.c` file before the include to compile the implementation there. |
| `PNL_NO_DEPRECATED_WARNINGS` | Turns off the compiler warnings that `PNL_DEPRECATED` adds to deprecated functions. |

## Contents

- [Status codes](#status-codes)
- [Application](#application)
- [Status lines and values](#status-lines-and-values)
- [Actions, results and running](#actions-results-and-running)
- [Timers, watches and the terminal](#timers-watches-and-the-terminal)
- [Views and dialogs](#views-and-dialogs)
- [Tasks](#tasks)
- [Ready-made screens](#ready-made-screens)
- [Layouts](#layouts)
- [Components](#components)
- [Forms](#forms)
- [Flow and scene](#flow-and-scene)
- [Canvas drawing](#canvas-drawing)
- [Colors and gradients](#colors-and-gradients)
- [Big letters and art](#big-letters-and-art)
- [Motion](#motion)
- [Logs](#logs)
- [Keys and mouse](#keys-and-mouse)
- [Testing](#testing)
- [Text utilities](#text-utilities)
- [Environment variables](#environment-variables)

## Status codes

| Macro | Value | `pnl_strerror` text |
| --- | --- | --- |
| `PNL_OK` | 0 | ok |
| `PNL_ERR_ARG` | 1 | invalid argument |
| `PNL_ERR_NOMEM` | 2 | out of memory |
| `PNL_ERR_TTY` | 3 | no interactive terminal |
| `PNL_ERR_IO` | 4 | terminal read or write failed |
| `PNL_ERR_NOT_FOUND` | 5 | not found |
| `PNL_ERR_UNSUPPORTED` | 6 | not supported on this platform |
| `PNL_ERR_MISMATCH` | 7 | screen differs from the snapshot |
| `PNL_ERR_LIMIT` | 8 | fixed limit reached |

### pnl_strerror

```c
const char *pnl_strerror(int status);
```

Returns a short, static description of a status code. Any value not in the table above
gives "unknown status".

Returns: a static string; never `NULL`.

## Application

```c
typedef void *(*pnl_alloc_fn)(void *ctx, size_t size);
typedef void (*pnl_free_fn)(void *ctx, void *ptr);

typedef struct pnl_allocator {
    pnl_alloc_fn alloc_fn;
    pnl_free_fn free_fn;
    void *ctx;
} pnl_allocator_t;
```

All memory the library allocates for an app goes through its allocator. `ctx` is passed to
both functions.

### pnl_app_create

```c
int pnl_app_create(const char *title, const pnl_allocator_t *allocator, pnl_app_t **out_app);
```

Creates an app. The title is copied and is also used to derive the tool name for command
names, the memory file and the key file. A `NULL` allocator means `malloc` and `free`.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `out_app` or `title` is `NULL`, or the allocator lacks
`alloc_fn` or `free_fn`; `PNL_ERR_NOMEM`.

### pnl_app_destroy

```c
void pnl_app_destroy(pnl_app_t *app);
```

Frees the app and everything it owns. Accepts `NULL`.

Returns: nothing.

### pnl_app_set_theme

```c
int pnl_app_set_theme(pnl_app_t *app, const char *theme);
```

Chooses the colors of the roles. `theme` is a built-in name (`default`, `mono`, `contrast`,
`warm`) or, when it contains `/`, `\` or `.`, the path of a theme file. A theme file has lines
`role = #rrggbb` for the roles `accent`, `ok`, `warn`, `fail`, `muted`, `info` and `surface`;
`light.role = #rrggbb` sets the color used on a light background, and lines starting with `#`
are comments. Roles not in the file keep the `default` colors. `TUIKIT_THEME` overrides the
theme chosen here.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a line that cannot be read;
`PNL_ERR_NOT_FOUND` for an unknown name or a file that cannot be opened. On failure the
theme is unchanged.

### pnl_app_set_memory

```c
int pnl_app_set_memory(pnl_app_t *app, const char *path);
```

Turns on memory: the last menu action, the last pick and input history of each prompt, and
table sort and filter are saved to a file and restored on the next run. A `NULL` path uses
`$XDG_STATE_HOME/<tool>/memory` (when absolute) or `$HOME/.local/state/<tool>/memory`, or
`%LOCALAPPDATA%\<tool>\memory` on Windows; if none is available memory stays off. Missing
directories are created when the file is written. `TUIKIT_MEMORY=off` leaves memory off.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`, `path` is empty, or `path` is 1019 bytes
or longer; `PNL_ERR_NOMEM`.

### pnl_app_record

```c
int pnl_app_record(pnl_app_t *app, const char *path);
```

Records the next interactive run to `path` as an asciicast v2 file. A `NULL` path clears a
path set earlier. When no path is set from code, `TUIKIT_RECORD` is used.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL` or `path` is empty; `PNL_ERR_NOMEM`.

## Status lines and values

The main screen shows a list of status lines above the actions. Each line has a mark, an
optional label and a text or a meter.

| Macro | Value | Shown as |
| --- | --- | --- |
| `PNL_MARK_OK` | 0 | `ok` |
| `PNL_MARK_WAIT` | 1 | `..` |
| `PNL_MARK_FAIL` | 2 | `!!` |

```c
typedef void (*pnl_status_fn)(void *ctx, pnl_app_t *app);
```

The status callback is called whenever the status lines are needed and adds them with
`pnl_status_line` and `pnl_status_meter`.

### pnl_app_on_status

```c
int pnl_app_on_status(pnl_app_t *app, pnl_status_fn status_fn, void *status_ctx);
```

Sets the status callback. A `NULL` callback removes it.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`.

### pnl_app_set_refresh

```c
int pnl_app_set_refresh(pnl_app_t *app, int refresh_ms);
```

Redraws the panel, and so calls the status callback again, every `refresh_ms` milliseconds.
0 turns the refresh off.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL` or `refresh_ms` is negative;
`PNL_ERR_NOMEM`.

### pnl_status_line

```c
int pnl_status_line(pnl_app_t *app, int mark, const char *label, const char *text);
```

Adds one status line. Only valid inside the status callback. A `NULL` or empty label means
no label; labels are shown up to 24 columns wide.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `text` is `NULL`, the mark is not a `PNL_MARK_*`
value, or the call is made outside the status callback; `PNL_ERR_NOMEM`.

### pnl_status_meter

```c
int pnl_status_meter(pnl_app_t *app, int mark, const char *label, double fraction);
```

Adds a status line that shows a meter instead of text. `fraction` is clamped to 0..1 and NaN
counts as 0.

Returns: the same as `pnl_status_line`.

### pnl_value_create

```c
int pnl_value_create(pnl_app_t *app, const char *label, pnl_value_t **out_value);
```

Creates a value: a status line that keeps its state and can be updated from any thread.
Values are shown after the lines from the status callback. A new value has the mark
`PNL_MARK_WAIT` and no text.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app`, `label` or `out_value` is `NULL`; `PNL_ERR_NOMEM`;
`PNL_ERR_IO` if the wake-up channel cannot be opened.

### pnl_value_destroy

```c
void pnl_value_destroy(pnl_value_t *value);
```

Removes the value from its app and frees it. Accepts `NULL`.

Returns: nothing.

### pnl_value_set

```c
int pnl_value_set(pnl_value_t *value, int mark, const char *text);
```

Sets the mark and text. Safe to call from any thread; the panel is woken only when something
changed. Text longer than 255 bytes is cut at a UTF-8 boundary.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `value` or `text` is `NULL` or the mark is invalid.

### pnl_value_set_meter

```c
int pnl_value_set_meter(pnl_value_t *value, int mark, double fraction);
```

Sets the mark and shows a meter instead of text. `fraction` is clamped to 0..1. Safe to call
from any thread.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `value` is `NULL`, the mark is invalid or `fraction` is
NaN.

## Actions, results and running

```c
typedef void (*pnl_action_fn)(void *ctx, pnl_app_t *app);
```

An action callback runs on the panel thread when its action is chosen.

### pnl_app_add_action

```c
int pnl_app_add_action(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx);
```

Adds an action to the menu, in the order of the calls. The label is copied. On the command
line the action becomes a command whose name is the label in lower case, with every character
other than letters, digits, `.` and `_` turned into `-`.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app`, `label` or `action_fn` is `NULL`; `PNL_ERR_NOMEM`.

### pnl_app_run

```c
int pnl_app_run(pnl_app_t *app, int argc, char **argv, int *out_exit_code);
```

Runs the app. With no arguments and an interactive terminal it opens the panel and returns
when the user quits; the exit code is then 0 if the status is `PNL_OK` and 1 otherwise.
Otherwise it runs as a command: `-h`/`--help` prints usage, `--json` prints JSON,
`--yes`/`-y` answers confirmations, and one command name runs that action; `status` or no
command prints the status lines as `[ok] text` lines or as
`{"status":[{"mark","label","text","meter"}]}`. Command exit codes are 0, 1 if any status
line or result has a fail mark or the action needs an interactive terminal, and 2 for a usage
error. `out_exit_code` may be `NULL`.

Returns: `PNL_OK` in command mode; in the panel, the status the loop ended with, such as
`PNL_ERR_TTY` or `PNL_ERR_IO`; `PNL_ERR_ARG` if `app` is `NULL`, `argc` is negative, or `argc`
is positive and `argv` is `NULL`.

### pnl_app_quit

```c
int pnl_app_quit(pnl_app_t *app);
```

Asks the running panel to quit.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`.

### pnl_app_redraw

```c
int pnl_app_redraw(pnl_app_t *app);
```

Marks the screen to be drawn again on the next turn of the loop.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`.

### pnl_result

```c
int pnl_result(pnl_app_t *app, int mark, const char *text);
```

Shows the outcome of an action. In the panel it opens a screen with the text that any key
closes. On the command line it prints one line, and a fail mark makes the exit code 1.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, an invalid mark, or when the app is
neither running nor in command mode; `PNL_ERR_NOMEM`.

### pnl_toast

```c
int pnl_toast(pnl_app_t *app, int mark, const char *text);
```

Shows a short message over the panel for 3 seconds. At most 3 are shown; a fourth
pushes out the oldest. Text is cut at 119 bytes. On the command line it prints `[mark] text`
to stderr, and nothing with `--json`.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `text` is `NULL` or the mark is invalid.

### pnl_notify

```c
int pnl_notify(pnl_app_t *app, const char *text);
```

Sends a desktop notification while the terminal does not have focus: a bell and an OSC 9
message. `TUIKIT_NOTIFY=bell` sends only the bell and `TUIKIT_NOTIFY=off` sends nothing. On the
command line it writes to stderr only when stderr is a terminal. Tasks that ran for 5 seconds
or more send `title - outcome` this way on their own.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `text` is `NULL`, or the app is neither running
nor in command mode.

## Timers, watches and the terminal

```c
typedef void (*pnl_timer_fn)(void *ctx, pnl_app_t *app);
typedef void (*pnl_fd_fn)(void *ctx, pnl_app_t *app, int fd);
typedef void (*pnl_terminal_fn)(void *ctx);
```

These callbacks run on the panel thread.

### pnl_app_add_timer

```c
int pnl_app_add_timer(pnl_app_t *app, int interval_ms, pnl_timer_fn timer_fn, void *timer_ctx, int *out_timer);
```

Calls `timer_fn` every `interval_ms` milliseconds while the panel runs. Timers may be added
before `pnl_app_run`; they start counting when the run starts. `out_timer` may be `NULL`;
otherwise it receives an id, starting at 1, for `pnl_app_remove_timer`.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `timer_fn` is `NULL` or `interval_ms` is not
positive; `PNL_ERR_NOMEM`.

### pnl_app_remove_timer

```c
int pnl_app_remove_timer(pnl_app_t *app, int timer);
```

Removes a timer. Safe to call from inside a timer callback.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`; `PNL_ERR_NOT_FOUND` for an unknown id.

### pnl_app_watch_fd

```c
int pnl_app_watch_fd(pnl_app_t *app, int fd, pnl_fd_fn fd_fn, void *fd_ctx);
```

Calls `fd_fn` when `fd` has data to read. Watching an fd again replaces its callback. At most
64 descriptors can be watched.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `fd_fn` is `NULL` or `fd` is negative;
`PNL_ERR_LIMIT`; `PNL_ERR_UNSUPPORTED` on Windows.

### pnl_app_unwatch_fd

```c
int pnl_app_unwatch_fd(pnl_app_t *app, int fd);
```

Stops watching `fd`.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`; `PNL_ERR_NOT_FOUND` if `fd` is not
watched.

### pnl_app_with_terminal

```c
int pnl_app_with_terminal(pnl_app_t *app, pnl_terminal_fn terminal_fn, void *terminal_ctx);
```

Gives the terminal back to the normal screen, calls `terminal_fn`, and takes it again, so
the callback can run an editor or another program. If the panel does not hold the terminal,
it only calls `terminal_fn`.

Returns: `PNL_OK`, or the status of taking the terminal back; `PNL_ERR_ARG` if `app` or
`terminal_fn` is `NULL`.

## Views and dialogs

```c
typedef void (*pnl_draw_fn)(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas);
typedef int (*pnl_key_fn)(void *ctx, pnl_app_t *app, int key);
```

A view draws itself in `draw_fn` and receives keys in `key_fn`. `key_fn` returns non-zero
when it handled the key. When it returns 0 the default handling runs: `?` opens help, `:` opens
the action palette, and `q` or escape closes the view; a mouse wheel key not handled is turned
into three up or down keys. Key bindings are applied before the key reaches `key_fn`; ctrl-c
always quits and F12 opens the inspector.

### pnl_view_push

```c
int pnl_view_push(pnl_app_t *app, const char *title, pnl_draw_fn draw_fn, pnl_key_fn key_fn, void *view_ctx);
```

Opens a full-screen view on top of the current one. The title may be `NULL`; `key_fn` may be
`NULL`. In command mode the command then exits 1 with "this action needs an interactive
terminal".

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `draw_fn` is `NULL`; `PNL_ERR_TTY` in command
mode; `PNL_ERR_NOMEM`.

### pnl_view_push_dialog

```c
int pnl_view_push_dialog(pnl_app_t *app, const char *title, int width, int height, pnl_draw_fn draw_fn, pnl_key_fn key_fn, void *view_ctx);
```

Opens a view in a box of `width` by `height` cells over the current screen.

Returns: as `pnl_view_push`, and `PNL_ERR_ARG` if `width` or `height` is not positive.

### pnl_view_pop

```c
int pnl_view_pop(pnl_app_t *app);
```

Closes the top view.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`; `PNL_ERR_NOT_FOUND` if no view is open.

### pnl_view_shake

```c
int pnl_view_shake(pnl_app_t *app);
```

Shakes the top view sideways to signal a rejected input. It does something only when
`PNL_MOTION_FEEDBACK` is on.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`; `PNL_ERR_NOT_FOUND` if no view is open.

### pnl_view_set_hints

```c
int pnl_view_set_hints(pnl_app_t *app, const char *hints);
```

Sets the key hints of the top view, shown at the bottom and in the help screen.

Returns: `PNL_OK`; `PNL_ERR_ARG` if an argument is `NULL`; `PNL_ERR_NOT_FOUND` if no view is
open; `PNL_ERR_NOMEM`.

## Tasks

```c
typedef void (*pnl_work_fn)(void *ctx, pnl_task_t *task);
```

`work_fn` runs on its own thread in the panel and on the calling thread in command mode.
The `pnl_task_*` functions that take a task may be called from it.

### pnl_task_run

```c
int pnl_task_run(pnl_app_t *app, const char *title, pnl_work_fn work_fn, void *work_ctx);
```

Runs `work_fn` and shows its progress: the current step, a progress bar (or a moving bar and
elapsed seconds when the total is 0), the result and the last log lines. Escape or `q` asks
the task to cancel; after it finishes any key goes back. In command mode it runs in the
foreground and writes progress to stderr when stderr is a terminal. A task that sets no
result ends with "finished" (marked ok) or "cancelled" (marked as failed).

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app`, `title` or `work_fn` is `NULL`, or the app is
neither running nor in command mode; `PNL_ERR_NOMEM`.

### pnl_task_progress

```c
void pnl_task_progress(pnl_task_t *task, unsigned long long done, unsigned long long total);
```

Reports progress as `done` of `total`. A total of 0 means unknown. A `NULL` task is ignored.

Returns: nothing.

### pnl_task_step

```c
int pnl_task_step(pnl_task_t *task, const char *text);
```

Sets the text of the current step, cut at 255 bytes.

Returns: `PNL_OK`; `PNL_ERR_ARG` if an argument is `NULL`.

### pnl_task_log

```c
int pnl_task_log(pnl_task_t *task, const char *line);
```

Adds a line to the task log. The last 100 lines are kept.

Returns: `PNL_OK`; `PNL_ERR_ARG` if an argument is `NULL`.

### pnl_task_cancelled

```c
int pnl_task_cancelled(pnl_task_t *task);
```

Tells the work function whether the user asked to cancel.

Returns: 1 if cancelled, otherwise 0; always 0 in command mode and for `NULL`.

### pnl_task_result

```c
int pnl_task_result(pnl_task_t *task, int mark, const char *text);
```

Sets the outcome shown when the task finishes.

Returns: `PNL_OK`; `PNL_ERR_ARG` if an argument is `NULL` or the mark is invalid.

### pnl_task_then

```c
int pnl_task_then(pnl_task_t *task, pnl_action_fn then_fn, void *then_ctx);
```

Calls `then_fn` on the panel thread after the work function returns. In command mode it is
called right after.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `task` or `then_fn` is `NULL`.

### pnl_task_show

```c
int pnl_task_show(pnl_task_t *task, pnl_widget_t *widget);
```

Shows a widget in the task screen, drawn only when its height plus 6 rows fits. A `NULL`
widget removes it. Safe to call from the work function; does nothing in command mode.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `task` is `NULL` or the widget belongs to another app.

## Ready-made screens

All of these work in the panel and in command mode, and return `PNL_ERR_ARG` when the app is
neither running nor in command mode.

### pnl_confirm

```c
int pnl_confirm(pnl_app_t *app, const char *question, int *out_yes);
```

Asks a yes/no question. Keys: `y`, `n`, left, right, tab, `h`, `l`, enter; escape means no. In
command mode `--yes` answers yes; without a terminal on stdin it prints that the action was
not confirmed and to pass `--yes`, and counts as a failure; with a terminal it asks `[y/N]`.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument. `*out_yes` is 1 for yes and 0
otherwise.

### pnl_input

```c
int pnl_input(pnl_app_t *app, const char *prompt, char *text, size_t text_len, int *out_ok);
```

Asks for a line of text. `text` holds the initial value and receives the answer only if the
user accepts it. With memory on, up and down walk the last 20 answers to the same prompt. In
command mode it reads a line from stdin of at most 4096 bytes; end of input sets `*out_ok` to 0.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, `text_len` of 0, or `text` not
terminated within `text_len`; `PNL_ERR_NOMEM`. `*out_ok` is 1 if accepted.

### pnl_pick

```c
int pnl_pick(pnl_app_t *app, const char *title, const char *const *items, size_t items_len, size_t *out_index, int *out_ok);
```

Lets the user pick one item. Typing filters the list, ignoring ASCII case. With memory on the
last pick for the same title is selected first. In command mode it prints a numbered list to
stderr when it is a terminal and reads a number or the exact text of an item.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or item, or `items_len` of 0;
`PNL_ERR_NOMEM`. `*out_index` is the chosen index and `*out_ok` is 1 if an item was chosen;
when nothing was chosen `*out_ok` and `*out_index` are 0.

### pnl_pager

```c
int pnl_pager(pnl_app_t *app, const char *title, const char *text, size_t text_len);
```

Shows `text_len` bytes of text in a scrolling view. Tabs become 4 spaces and `\r` and NUL bytes
are dropped. Keys: `/` search, `n`/`N` next and previous match, `g`/`G` top and bottom, space
and page keys. The text is copied. In command mode it writes the text to stdout.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `title` is `NULL`, or `text` is `NULL` with
`text_len` above 0; `PNL_ERR_NOMEM`.

### pnl_table

```c
int pnl_table(pnl_app_t *app, const char *title, const char *const *headers, const int *column_flags, size_t columns, const char *const *cells, size_t rows);
```

Shows a table. `cells` holds `rows * columns` strings row by row and is copied; a `NULL` cell
shows as empty. `column_flags` may be `NULL`. Columns are at most 40 wide. Keys: `s` sorts by
the current column (again reverses), `/` filters, left and right change column, and clicking a
header sorts. Sort and filter are remembered per title. With more than 20000 rows sorting and
filtering run on a thread. In command mode it prints aligned columns, or a JSON array of
objects with `--json`.

| Macro | Value | Meaning |
| --- | --- | --- |
| `PNL_COLUMN_RIGHT` | 1 | Align the column to the right. |
| `PNL_COLUMN_OPTIONAL` | 2 | Hide the column, from the right, when the table is too narrow. |

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app`, `title` or `headers` is `NULL`, `columns` is 0,
`rows` is above 0xffffffff, or `cells` is `NULL` with rows; `PNL_ERR_NOMEM`.

### pnl_table_source

```c
int pnl_table_source(pnl_app_t *app, const char *title, const char *const *headers, const int *column_flags, size_t columns, size_t rows, pnl_cell_fn cell_fn, pnl_release_fn release_fn, void *ctx);
```

Like `pnl_table`, but cells are read on demand from `cell_fn`. `release_fn`, which may be
`NULL`, is called when the table closes, or after printing in command mode; it is not called
when this function fails.

```c
typedef const char *(*pnl_cell_fn)(void *ctx, size_t row, size_t column, char *buf, size_t buf_len);
typedef void (*pnl_release_fn)(void *ctx);
```

`cell_fn` returns the text of one cell. It may write it into `buf`, which is 256 bytes, and
return `buf`. A `NULL` return shows as empty.

Returns: as `pnl_table`, with `PNL_ERR_ARG` if `cell_fn` is `NULL`.

## Layouts

A layout is a tree of nodes. Node 0 is the root split; splits contain splits and panels.

| Macro | Value | Meaning |
| --- | --- | --- |
| `PNL_SPLIT_ROWS` | 0 | Children stacked top to bottom. |
| `PNL_SPLIT_COLUMNS` | 1 | Children side by side. Shown as rows when the height allows 5 rows per child. |
| `PNL_SPLIT_TABS` | 2 | One child at a time; `[` and `]` switch. |
| `PNL_LAYOUT_ROOT` | 0 | The root node. |

Tab and backtab move the focus between panels; the focused panel's title is marked.

### pnl_layout_create

```c
int pnl_layout_create(pnl_app_t *app, int kind, pnl_layout_t **out_layout);
```

Creates a layout whose root is a split of `kind`.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `out_layout` or `app` is `NULL` or `kind` is not a
`PNL_SPLIT_*` value; `PNL_ERR_NOMEM`.

### pnl_layout_destroy

```c
void pnl_layout_destroy(pnl_layout_t *layout);
```

Frees a layout. Accepts `NULL`.

Returns: nothing.

### pnl_layout_split

```c
int pnl_layout_split(pnl_layout_t *layout, int parent, int kind, const char *title, int *out_node);
```

Adds a split under `parent`. The title is used as a tab label inside a tabs split.

Returns: `PNL_OK`; `PNL_ERR_ARG` for an invalid parent, a parent that is a panel, or an
invalid kind; `PNL_ERR_NOMEM`.

### pnl_layout_panel

```c
int pnl_layout_panel(pnl_layout_t *layout, int parent, const char *title, pnl_draw_fn draw_fn, pnl_key_fn key_fn, void *panel_ctx, int *out_node);
```

Adds a panel under `parent`. The first panel gets the focus. `key_fn` receives keys while
the panel has focus.

Returns: `PNL_OK`; `PNL_ERR_ARG` for an invalid parent or a `NULL` `draw_fn`; `PNL_ERR_NOMEM`.

### pnl_layout_widget

```c
int pnl_layout_widget(pnl_layout_t *layout, int parent, const char *title, pnl_widget_t *widget, int *out_node);
```

Adds a panel that draws a widget and sends it keys through `pnl_widget_key`.

Returns: as `pnl_layout_panel`, with `PNL_ERR_ARG` if `layout` or `widget` is `NULL`.

### pnl_layout_size

```c
int pnl_layout_size(pnl_layout_t *layout, int node, int size, int min_size);
```

Gives a node a fixed size in columns or rows, depending on its parent; 0 shares the space
left with the other children. `min_size` is the size below which the node is left out; 0
keeps the default of 16 columns or 3 rows.

Returns: `PNL_OK`; `PNL_ERR_ARG` for an invalid node, the root, or a negative size.

### pnl_layout_focus

```c
int pnl_layout_focus(pnl_layout_t *layout, int node);
```

Gives the focus to a panel.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `node` is not a panel.

### pnl_layout_hints

```c
int pnl_layout_hints(pnl_layout_t *layout, int node, const char *hints);
```

Sets the key hints shown while the panel has focus.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `node` is not a panel; `PNL_ERR_NOMEM`.

### pnl_view_push_layout

```c
int pnl_view_push_layout(pnl_app_t *app, const char *title, const pnl_layout_t *layout);
```

Opens a view that shows the layout. The layout is copied, so it may be destroyed after the
call.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a layout from another app;
`PNL_ERR_TTY` in command mode; `PNL_ERR_NOMEM`.

## Components

A widget is a drawable part with its own state, used in stacks, layouts, views, the task
screen and the banner. The generic functions come first, then the built-in widgets.

```c
typedef int (*pnl_height_fn)(void *ctx, pnl_app_t *app, int width);
typedef const char *(*pnl_hints_fn)(void *ctx);

typedef struct pnl_widget_class {
    pnl_draw_fn draw_fn;
    pnl_key_fn key_fn;
    pnl_height_fn height_fn;
    pnl_hints_fn hints_fn;
    pnl_release_fn release_fn;
} pnl_widget_class_t;
```

`height_fn` returns the rows the widget wants at a given width. `hints_fn` returns the key
hints shown while the widget has focus. `release_fn` is called when the widget is destroyed.
Only `draw_fn` is required.

### pnl_widget_create

```c
int pnl_widget_create(pnl_app_t *app, const pnl_widget_class_t *widget_class, void *widget_ctx, pnl_widget_t **out_widget);
```

Creates a widget from a class. The class is copied; `widget_ctx` is not owned.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app`, `widget_class`, its `draw_fn` or `out_widget` is
`NULL`; `PNL_ERR_NOMEM`.

### pnl_widget_destroy

```c
void pnl_widget_destroy(pnl_widget_t *widget);
```

Calls `release_fn` and frees the widget. Accepts `NULL`.

Returns: nothing.

### pnl_widget_ctx

```c
void *pnl_widget_ctx(const pnl_widget_t *widget);
```

Returns: the context given at creation; `NULL` for a `NULL` widget.

### pnl_widget_draw

```c
int pnl_widget_draw(pnl_widget_t *widget, pnl_canvas_t *canvas, int x, int y, int width, int height);
```

Draws the widget into an area of a canvas, clipped to the canvas.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a negative value.

### pnl_widget_key

```c
int pnl_widget_key(pnl_widget_t *widget, int key);
```

Passes a key to the widget.

Returns: the value of its `key_fn`; 0 if it has none.

### pnl_widget_height

```c
int pnl_widget_height(pnl_widget_t *widget, int width);
```

Returns: the rows the widget wants at `width`; 0 if it has no `height_fn` or `width` is not
positive.

### pnl_view_push_widget

```c
int pnl_view_push_widget(pnl_app_t *app, const char *title, pnl_widget_t *widget);
```

Opens a view that shows the widget and sends it keys.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a widget from another app;
`PNL_ERR_TTY` in command mode; `PNL_ERR_NOMEM`.

### pnl_widget_set_text

```c
int pnl_widget_set_text(pnl_widget_t *widget, const char *text);
```

Sets the text of a widget that has one: text, bar and graph labels, spinner, big letters, and
the form widgets (see [Forms](#forms)).

Returns: `PNL_OK`; `PNL_ERR_ARG` if `text` is `NULL` or the widget has no text;
`PNL_ERR_NOMEM`.

### pnl_widget_get_text

```c
const char *pnl_widget_get_text(const pnl_widget_t *widget);
```

Returns: the widget's text; `NULL` if the widget has none.

### pnl_widget_set_value

```c
int pnl_widget_set_value(pnl_widget_t *widget, double value);
```

Sets the value of a bar, graph, check box, choice, slider or scene.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `value` is NaN or the widget has no value.

### pnl_widget_get_value

```c
int pnl_widget_get_value(const pnl_widget_t *widget, double *out_value);
```

Reads the value of a widget.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a widget with no value;
`PNL_ERR_NOT_FOUND` for a graph with no values.

### pnl_widget_set_range

```c
int pnl_widget_set_range(pnl_widget_t *widget, double min, double max);
```

Sets the range of a graph or a slider.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `max` is below `min`, for a slider if `max` equals `min`,
or if the widget is neither.

The built-in widgets use these macros:

| Macro | Value | Used by |
| --- | --- | --- |
| `PNL_METER_ACCENT` | 0 | Bars: the accent color. |
| `PNL_METER_LEVEL` | 1 | Bars: green to red by value. |
| `PNL_METER_PERCENT` | 2 | Bars: the percentage inside the bar. |
| `PNL_METER_GRADIENT(g)` | `g << 16` | Bars and graphs: color along a gradient. |
| `PNL_SPINNER_DOTS` | 0 | Spinner of 10 frames. |
| `PNL_SPINNER_LINE` | 1 | Spinner of 4 frames. |
| `PNL_SPINNER_ARC` | 2 | Spinner of 6 frames. |
| `PNL_SPINNER_BLOCKS` | 3 | Spinner of 4 frames. |
| `PNL_SPINNER_BOUNCE` | 4 | Spinner of 6 frames. |
| `PNL_GRAPH_SPARKLINE` | 0 | One row, one cell per value. |
| `PNL_GRAPH_CHART` | 1 | Vertical bars, one column per value. |
| `PNL_GRAPH_PLOT` | 2 | Braille line, two values per cell. |
| `PNL_STACK_ROWS` | 0 | Children top to bottom. |
| `PNL_STACK_COLUMNS` | 1 | Children side by side. |

### pnl_widget_text

```c
int pnl_widget_text(pnl_app_t *app, const char *text, int style, pnl_widget_t **out_widget);
```

Creates a widget that shows text in a style. `NULL` text means empty. Its height is the
number of lines; each line is cut at 255 bytes.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `out_widget` is `NULL`; `PNL_ERR_NOMEM`.

### pnl_widget_bar

```c
int pnl_widget_bar(pnl_app_t *app, const char *label, int flags, pnl_widget_t **out_widget);
```

Creates a progress bar with an optional label. `pnl_widget_set_value` sets the fraction and,
with `PNL_MOTION_COUNT`, the bar glides to it; `pnl_widget_get_value` gives the target.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `out_widget` is `NULL`; `PNL_ERR_NOMEM`.

### pnl_widget_graph

```c
int pnl_widget_graph(pnl_app_t *app, const char *label, int kind, int rows, int flags, size_t capacity, pnl_widget_t **out_widget);
```

Creates a graph that keeps the last `capacity` values. `pnl_widget_set_value` adds a value and
`pnl_widget_get_value` returns the last one. A sparkline is one row; for the others `rows` is
the height, and 0 fills the space given. The range is automatic (from 0 unless values are
negative; a plot starts at its smallest value) until `pnl_widget_set_range` fixes it;
setting `min` equal to `max` returns to the automatic range.

Returns: `PNL_OK`; `PNL_ERR_ARG` for an invalid kind, `rows` outside 0..256, or `capacity`
of 0 or above 4096; `PNL_ERR_NOMEM`.

### pnl_widget_spinner

```c
int pnl_widget_spinner(pnl_app_t *app, int kind, const char *text, pnl_widget_t **out_widget);
```

Creates a spinner with optional text beside it.

Returns: `PNL_OK`; `PNL_ERR_ARG` for an invalid kind; `PNL_ERR_NOMEM`.

### pnl_widget_log

```c
int pnl_widget_log(pnl_app_t *app, pnl_log_t *logbook, pnl_widget_t **out_widget);
```

Creates a widget that shows a log. Keys: end follows new lines, `v` changes the lowest level
shown, `/` filters.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a log from another app;
`PNL_ERR_NOMEM`.

### pnl_widget_stack

```c
int pnl_widget_stack(pnl_app_t *app, int kind, pnl_widget_t **out_widget);
```

Creates a stack that lays out child widgets. Tab, backtab and enter, and up/down for rows
or left/right for columns, move the focus between children.

Returns: `PNL_OK`; `PNL_ERR_ARG` for an invalid kind; `PNL_ERR_NOMEM`.

### pnl_widget_add

```c
int pnl_widget_add(pnl_widget_t *stack, pnl_widget_t *child, int size);
```

Adds a child to a stack. `size` is fixed rows or columns; 0 uses the child's `height_fn` in a
row stack, or an equal share.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, a `stack` that is not a stack, a child
from another app, a negative size, or a child that contains the stack; `PNL_ERR_NOMEM`.

### pnl_widget_set_focusable

```c
int pnl_widget_set_focusable(pnl_widget_t *widget, int focusable);
```

Sets whether a widget can take the focus in a stack.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `widget` is `NULL` or a stack.

### pnl_widget_focus

```c
int pnl_widget_focus(pnl_widget_t *stack, pnl_widget_t *child);
```

Gives the focus to a child of the stack, looking into nested stacks.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, a `stack` that is not a stack, or a
child that cannot take the focus; `PNL_ERR_NOT_FOUND` if the child is not in the stack.

## Forms

Form widgets are focusable widgets for stacks and layouts. Their labels are optional except
for the button. The change callback runs only when the user changes the widget, not when code
calls `pnl_widget_set_*`.

```c
typedef void (*pnl_change_fn)(void *ctx, pnl_app_t *app, pnl_widget_t *widget);
```

| Macro | Value | Meaning |
| --- | --- | --- |
| `PNL_INPUT_SECRET` | 1 | Input hides what is typed. |

### pnl_widget_input

```c
int pnl_widget_input(pnl_app_t *app, const char *label, int flags, pnl_widget_t **out_widget);
```

Creates a one-line text field that holds up to 255 bytes. `pnl_widget_get_text` and
`pnl_widget_set_text` read and set the text.

Returns: `PNL_OK`; `PNL_ERR_ARG` for flags other than `PNL_INPUT_SECRET`; `PNL_ERR_NOMEM`.
`pnl_widget_set_text` returns `PNL_ERR_ARG` for text longer than the field holds.

### pnl_widget_check

```c
int pnl_widget_check(pnl_app_t *app, const char *label, int checked, pnl_widget_t **out_widget);
```

Creates a check box. Space or a click toggles it. Its value is 1 or 0; its text is the label.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `out_widget` is `NULL`; `PNL_ERR_NOMEM`.

### pnl_widget_choice

```c
int pnl_widget_choice(pnl_app_t *app, const char *label, const char *const *options, size_t count, pnl_widget_t **out_widget);
```

Creates a choice between options, which are copied. Left, right, space or a click change
it. The value is the index of the chosen option. `pnl_widget_get_text` returns the chosen
option and `pnl_widget_set_text` chooses the option with that text.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `options` is `NULL`, `count` is 0 or an option is `NULL`;
`PNL_ERR_NOMEM`. `pnl_widget_set_value` returns `PNL_ERR_ARG` for an index out of range and
`pnl_widget_set_text` returns `PNL_ERR_NOT_FOUND` for text that is not one of the options.

### pnl_widget_slider

```c
int pnl_widget_slider(pnl_app_t *app, const char *label, double min, double max, double step, pnl_widget_t **out_widget);
```

Creates a slider that starts at `min`. Values set are clamped to the range and snapped to
`step`; a step of 0 is continuous and keys then move 1/20 of the range. Home and end go to the
ends.

Returns: `PNL_OK`; `PNL_ERR_ARG` unless `max` is above `min` and `step` is not negative;
`PNL_ERR_NOMEM`.

### pnl_widget_button

```c
int pnl_widget_button(pnl_app_t *app, const char *label, pnl_action_fn action_fn, void *action_ctx, pnl_widget_t **out_widget);
```

Creates a button that calls `action_fn` on enter, space or a click. `action_fn` may be `NULL`.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `label` is `NULL`; `PNL_ERR_NOMEM`.

### pnl_widget_on_change

```c
int pnl_widget_on_change(pnl_widget_t *widget, pnl_change_fn change_fn, void *change_ctx);
```

Sets the change callback. A `NULL` callback removes it.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `widget` is `NULL`.

## Flow and scene

| Macro | Value |
| --- | --- |
| `PNL_STEP_WAIT` | 0 |
| `PNL_STEP_RUN` | 1 |
| `PNL_STEP_OK` | 2 |
| `PNL_STEP_FAIL` | 3 |
| `PNL_STEP_SKIP` | 4 |

### pnl_widget_flow

```c
int pnl_widget_flow(pnl_app_t *app, pnl_widget_t **out_widget);
```

Creates a flow: a row of steps with a state and a detail each. It takes 3 rows, plus a row of
details when they fit across; when the steps do not fit across they are drawn down, two rows
per step.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `out_widget` is `NULL`; `PNL_ERR_NOMEM`.

### pnl_widget_add_step

```c
int pnl_widget_add_step(pnl_widget_t *flow, const char *label, int *out_step);
```

Adds a step in the `PNL_STEP_WAIT` state. A flow holds up to 16 steps; labels are cut at 31
bytes.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a widget that is not a flow;
`PNL_ERR_LIMIT`. `*out_step` is the step index, or -1 on failure.

### pnl_widget_set_step

```c
int pnl_widget_set_step(pnl_widget_t *flow, int step, int state, const char *detail);
```

Sets the state and detail of a step. Safe to call from any thread. A `NULL` detail means
empty; details are cut at 47 bytes.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a widget that is not a flow or an invalid state;
`PNL_ERR_NOT_FOUND` for an invalid step.

### pnl_widget_scene

```c
int pnl_widget_scene(pnl_app_t *app, pnl_widget_t **out_widget);
```

Creates a scene: art that changes with a state and animates between frames. The state starts
at 0. `pnl_widget_set_value` sets the state from any thread and `pnl_widget_get_value` reads
it. Nothing is drawn in linear mode.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `out_widget` is `NULL`; `PNL_ERR_NOMEM`.
`pnl_widget_set_value` returns `PNL_ERR_ARG` for a value outside the `int` range.

### pnl_widget_add_scene

```c
int pnl_widget_add_scene(pnl_widget_t *scene, int state, const char *frames, int style, int gradient, int frame_ms);
```

Sets the art for a state. `frames` holds up to 16 frames separated by `\f`; more are dropped.
Each frame shows for `frame_ms` milliseconds; 0 means 500. Adding a state again replaces it;
a scene holds up to 16 states.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `frames` is `NULL`, the widget is not a scene or `frame_ms`
is negative; `PNL_ERR_LIMIT`; `PNL_ERR_NOMEM`.

## Canvas drawing

A canvas is the area a draw callback draws into. Coordinates are relative to it, and drawing
is clipped to it.

| Macro | Value | Meaning |
| --- | --- | --- |
| `PNL_BOLD` | 0x01 | Bold. |
| `PNL_DIM` | 0x02 | Dim. |
| `PNL_REVERSE` | 0x04 | Swap foreground and background. |
| `PNL_UNDERLINE` | 0x08 | Underline. |
| `PNL_FG(c)` | `c << 8` | Foreground color. |
| `PNL_BG(c)` | `c << 16` | Background color. |

### pnl_canvas_focused

```c
int pnl_canvas_focused(const pnl_canvas_t *canvas);
```

Returns: 1 if the panel or widget being drawn has the focus; 0 otherwise or for `NULL`.

### pnl_canvas_width

```c
int pnl_canvas_width(const pnl_canvas_t *canvas);
```

Returns: the width in cells; 0 for `NULL`.

### pnl_canvas_height

```c
int pnl_canvas_height(const pnl_canvas_t *canvas);
```

Returns: the height in cells; 0 for `NULL`.

### pnl_canvas_text

```c
int pnl_canvas_text(pnl_canvas_t *canvas, int x, int y, int style, const char *text);
```

Writes text at a position. `http://` and `https://` URLs in the text become links.

Returns: the column after the text; `x` if `canvas` or `text` is `NULL` or the position is
outside the canvas.

### pnl_canvas_link

```c
int pnl_canvas_link(pnl_canvas_t *canvas, int x, int y, int style, const char *text, const char *url);
```

Writes text that links to `url` in terminals that support links. An empty `url` writes the
text with no link. `TUIKIT_LINKS=off` turns links off.

Returns: as `pnl_canvas_text`; `x` also if `url` is `NULL`.

### pnl_canvas_fill

```c
void pnl_canvas_fill(pnl_canvas_t *canvas, int x, int y, int width, int height, int style);
```

Fills a rectangle with blanks in a style.

Returns: nothing.

### pnl_canvas_bar

```c
void pnl_canvas_bar(pnl_canvas_t *canvas, int x, int y, int width, double fraction, int flags);
```

Draws a horizontal bar `width` cells long. `fraction` is clamped to 0..1. Flags are the
`PNL_METER_*` values. A `width` of 0 or less draws nothing.

Returns: nothing.

### pnl_canvas_busy

```c
void pnl_canvas_busy(pnl_canvas_t *canvas, int x, int y, int width, int flags);
```

Draws a block that moves back and forth, for work of unknown length. Draws nothing when
`width` is not larger than the block.

Returns: nothing.

### pnl_canvas_spinner

```c
int pnl_canvas_spinner(pnl_canvas_t *canvas, int x, int y, int style, int kind);
```

Draws the current frame of a spinner, or its first frame when motion is off.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `canvas` is `NULL` or `kind` is invalid.

### pnl_canvas_sparkline

```c
void pnl_canvas_sparkline(pnl_canvas_t *canvas, int x, int y, int width, const double *values, size_t values_len, double max, int flags);
```

Draws the last `width` values as one row of bars, one cell per value, scaled to `max`.
Draws nothing unless `max` is positive.

Returns: nothing.

### pnl_canvas_chart

```c
void pnl_canvas_chart(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values, size_t values_len, double max, int flags);
```

Draws the last `width` values as vertical bars `height` rows tall, one column per value,
scaled to `max`. Draws nothing unless `max` is positive.

Returns: nothing.

### pnl_canvas_columns

```c
void pnl_canvas_columns(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values, const char *const *labels, size_t count, double max, int flags);
```

Draws `count` bars, each `width / count` columns wide, scaled to `max`. When `labels` is not
`NULL` they are written on the last row. Draws nothing if `width` is less than `count`.

Returns: nothing.

### pnl_canvas_plot

```c
void pnl_canvas_plot(pnl_canvas_t *canvas, int x, int y, int width, int height, const double *values, size_t values_len, double min, double max, int flags);
```

Draws the last `width * 2` values as a line in braille dots, two values per cell, between
`min` and `max`. Draws nothing unless `max` is above `min`, or when `height` is above 256.

Returns: nothing.

## Colors and gradients

| Macro | Value |
| --- | --- |
| `PNL_COLOR_DEFAULT` | 0 |
| `PNL_COLOR_BLACK` | 1 |
| `PNL_COLOR_RED` | 2 |
| `PNL_COLOR_GREEN` | 3 |
| `PNL_COLOR_YELLOW` | 4 |
| `PNL_COLOR_BLUE` | 5 |
| `PNL_COLOR_MAGENTA` | 6 |
| `PNL_COLOR_CYAN` | 7 |
| `PNL_COLOR_WHITE` | 8 |
| `PNL_COLOR_GRAY` | 9 |
| `PNL_COLOR_BRIGHT_RED` | 10 |
| `PNL_COLOR_BRIGHT_GREEN` | 11 |
| `PNL_COLOR_BRIGHT_YELLOW` | 12 |
| `PNL_COLOR_BRIGHT_BLUE` | 13 |
| `PNL_COLOR_BRIGHT_MAGENTA` | 14 |
| `PNL_COLOR_BRIGHT_CYAN` | 15 |
| `PNL_COLOR_BRIGHT_WHITE` | 16 |

Roles are colors chosen by the theme:

| Macro | Value |
| --- | --- |
| `PNL_ROLE_ACCENT` | 32 |
| `PNL_ROLE_OK` | 33 |
| `PNL_ROLE_WARN` | 34 |
| `PNL_ROLE_FAIL` | 35 |
| `PNL_ROLE_MUTED` | 36 |
| `PNL_ROLE_INFO` | 37 |
| `PNL_ROLE_SURFACE` | 38 |

### pnl_color_depth

```c
int pnl_color_depth(void);
```

Detects the colors the terminal shows from `NO_COLOR`, `TERM` and `COLORTERM`.

Returns: 0, 16, 256 or 16777216.

### pnl_app_color

```c
int pnl_app_color(pnl_app_t *app, unsigned long rgb, int *out_color);
```

Gives a color value for `0xrrggbb`, usable with `PNL_FG` and `PNL_BG`. The same `rgb` gives the
same value. An app has 56 such colors.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or `rgb` above 0xffffff;
`PNL_ERR_LIMIT` when no color is left. On failure `*out_color` is `PNL_COLOR_DEFAULT`.

### pnl_app_gradient

```c
int pnl_app_gradient(pnl_app_t *app, const unsigned long *stops, size_t count, int steps, int *out_gradient);
```

Makes a gradient of `steps` colors through `count` stops given as `0xrrggbb`. The steps use
colors from the same 56 as `pnl_app_color`.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, `count` below 2, `steps` outside 2..24
or a stop above 0xffffff; `PNL_ERR_LIMIT`. On failure `*out_gradient` is 0.

### pnl_gradient_color

```c
int pnl_gradient_color(int gradient, double fraction);
```

Returns: the color at `fraction` (clamped to 0..1) along the gradient. A plain color passed
as `gradient` is returned unchanged.

### pnl_canvas_gradient_text

```c
int pnl_canvas_gradient_text(pnl_canvas_t *canvas, int x, int y, int style, int gradient, const char *text);
```

Writes text colored left to right along a gradient.

Returns: the column after the text; `x` if `canvas` or `text` is `NULL`.

### pnl_canvas_gradient_fill

```c
void pnl_canvas_gradient_fill(pnl_canvas_t *canvas, int x, int y, int width, int height, int gradient);
```

Fills a rectangle with a background colored left to right along a gradient.

Returns: nothing.

## Big letters and art

| Macro | Value | Height |
| --- | --- | --- |
| `PNL_BIG_SMALL` | 0 | 3 rows |
| `PNL_BIG_LARGE` | 1 | 5 rows |

### pnl_big_width

```c
int pnl_big_width(int size, const char *text);
```

Measures text in big letters. Each glyph is 3 columns (space is 2, `.,:;!'|` are 1) with one
column between glyphs, and twice that at `PNL_BIG_LARGE`. Lower case is drawn as upper case and
any other character, one `?` per character.

Returns: the width in columns; 0 for `NULL` text or an invalid size.

### pnl_big_height

```c
int pnl_big_height(int size);
```

Returns: 5 for `PNL_BIG_LARGE`, otherwise 3.

### pnl_canvas_big

```c
int pnl_canvas_big(pnl_canvas_t *canvas, int x, int y, int size, int style, int gradient, const char *text);
```

Draws text in big letters, colored along `gradient` when it is not 0. In linear mode it writes
the plain text.

Returns: the column after the letters; `x` for invalid arguments.

### pnl_canvas_art

```c
int pnl_canvas_art(pnl_canvas_t *canvas, int x, int y, int style, int gradient, const char *art);
```

Draws multi-line text art. Spaces leave the cells under them untouched. A gradient colors it
left to right.

Returns: the number of lines; 0 for `NULL` art or in linear mode.

### pnl_app_set_banner

```c
int pnl_app_set_banner(pnl_app_t *app, const char *art, int gradient);
```

Shows art above the status lines on the main screen, only when it fits together with the
status lines and actions and never in linear mode. `NULL` art shows the title in big letters.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL`; `PNL_ERR_NOMEM`.

### pnl_app_set_banner_widget

```c
int pnl_app_set_banner_widget(pnl_app_t *app, pnl_widget_t *widget);
```

Shows a widget as the banner; it takes the place of the art from `pnl_app_set_banner`. `NULL`
removes it.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL` or the widget belongs to another app.

### pnl_widget_big

```c
int pnl_widget_big(pnl_app_t *app, const char *text, int size, int style, int gradient, pnl_widget_t **out_widget);
```

Creates a widget that shows text in big letters. `pnl_widget_set_text` changes the text.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `text` or `out_widget` is `NULL` or the size is invalid;
`PNL_ERR_NOMEM`.

### pnl_widget_art

```c
int pnl_widget_art(pnl_app_t *app, const char *art, int style, int gradient, pnl_widget_t **out_widget);
```

Creates a widget that shows text art. Its height is 0 in linear mode.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `art` is `NULL`; `PNL_ERR_NOMEM`.

## Motion

Animation is allowed while the panel runs, except when `TUIKIT_MOTION` is `off` or `0`, or
over a slow SSH link. Spinners and other frames drawn with `pnl_app_tween` follow that
setting. The kinds below are off until turned on with `pnl_app_enable_motion` or
`TUIKIT_MOTION=all`:

| Macro | Value | What moves |
| --- | --- | --- |
| `PNL_MOTION_SLIDE` | 1 | The menu selection and the pager scroll glide. |
| `PNL_MOTION_TRANSITION` | 2 | Full-screen views slide when opened and closed; toasts slide in. |
| `PNL_MOTION_PULSE` | 4 | Fail marks pulse, with 256 or 24-bit colors. |
| `PNL_MOTION_TYPEWRITER` | 8 | Result text appears one character at a time. |
| `PNL_MOTION_ENTER` | 16 | Menu items, dialogs and toasts come in and leave. |
| `PNL_MOTION_FEEDBACK` | 32 | `pnl_view_shake` shakes the view. |
| `PNL_MOTION_COUNT` | 64 | Numbers and meters in status lines, bars and sliders glide to new values. |

### pnl_app_enable_motion

```c
int pnl_app_enable_motion(pnl_app_t *app, int flags);
```

Turns on the given kinds of motion, added to those already on.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL` or `flags` has unknown bits.

### pnl_app_motion

```c
int pnl_app_motion(const pnl_app_t *app);
```

Returns: 1 if animations are on, which is only while the panel runs; otherwise 0.

### pnl_app_frame_ms

```c
long long pnl_app_frame_ms(const pnl_app_t *app);
```

Returns: the time in milliseconds of the frame being drawn, or the current time when not
drawing; 0 for `NULL`.

### pnl_app_animate

```c
int pnl_app_animate(pnl_app_t *app, int duration_ms);
```

Keeps redrawing about every 33 ms for `duration_ms` milliseconds. Does nothing when motion is
off.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` is `NULL` or `duration_ms` is negative.

### pnl_app_tween

```c
double pnl_app_tween(pnl_app_t *app, long long start_ms, int duration_ms);
```

Gives the progress of an animation that started at `start_ms` (a value from
`pnl_app_frame_ms`) and lasts `duration_ms`, and keeps frames coming until it ends.

Returns: a value from 0 to 1; 1 if `app` is `NULL`, motion is off or `duration_ms` is not
positive.

## Logs

| Macro | Value |
| --- | --- |
| `PNL_LEVEL_DEBUG` | 0 |
| `PNL_LEVEL_INFO` | 1 |
| `PNL_LEVEL_WARN` | 2 |
| `PNL_LEVEL_ERROR` | 3 |

### pnl_log_create

```c
int pnl_log_create(pnl_app_t *app, size_t capacity, pnl_log_t **out_log);
```

Creates a log that keeps the last `capacity` lines. Text is kept in a pool of
`capacity * 128 + 240` bytes; the oldest lines are dropped when either the lines or the pool
are full.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `app` or `out_log` is `NULL` or `capacity` is 0 or too
large; `PNL_ERR_NOMEM`.

### pnl_log_destroy

```c
void pnl_log_destroy(pnl_log_t *logbook);
```

Frees a log. Accepts `NULL`.

Returns: nothing.

### pnl_log_write

```c
int pnl_log_write(pnl_log_t *logbook, int level, const char *text);
```

Adds a line with the current time. Safe to call from any thread. Only the text up to the
first newline is kept, cut at 239 bytes, with tabs turned into spaces.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or an invalid level.

### pnl_log_open

```c
int pnl_log_open(pnl_app_t *app, pnl_log_t *logbook, const char *title);
```

Opens a view that shows the log, with the keys of `pnl_widget_log`. In command mode it prints
each line as the time, the level and the text, or as `[{"time","level","text"}]` with `--json`.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, a log from another app, or when the
app is neither running nor in command mode; `PNL_ERR_NOMEM`.

## Keys and mouse

Keys are Unicode code points for characters, 1 to 26 for ctrl-a to ctrl-z, and these values
for other keys:

| Macro | Value |
| --- | --- |
| `PNL_KEY_NONE` | 0 |
| `PNL_KEY_UP` | 0x110000 |
| `PNL_KEY_DOWN` | 0x110001 |
| `PNL_KEY_LEFT` | 0x110002 |
| `PNL_KEY_RIGHT` | 0x110003 |
| `PNL_KEY_ENTER` | 0x110004 |
| `PNL_KEY_ESCAPE` | 0x110005 |
| `PNL_KEY_PAGE_UP` | 0x110006 |
| `PNL_KEY_PAGE_DOWN` | 0x110007 |
| `PNL_KEY_HOME` | 0x110008 |
| `PNL_KEY_END` | 0x110009 |
| `PNL_KEY_BACKSPACE` | 0x11000a |
| `PNL_KEY_DELETE` | 0x11000b |
| `PNL_KEY_TAB` | 0x11000c |
| `PNL_KEY_BACKTAB` | 0x11000d |
| `PNL_KEY_CLICK` | 0x11000e |
| `PNL_KEY_WHEEL_UP` | 0x11000f |
| `PNL_KEY_WHEEL_DOWN` | 0x110010 |

On the main screen: up, down, `j` and `k` move; home and end jump; enter runs the selected
action; 1-9 run that action; a letter jumps to the next action starting with it; `q` or
escape quits.

### pnl_app_bind

```c
int pnl_app_bind(pnl_app_t *app, const char *key, const char *to);
```

Makes the key `key` act as the key `to`. Key names are `up`, `down`, `left`, `right`,
`enter`, `esc` or `escape`, `pgup` or `pageup`, `pgdn` or `pagedown`, `home`, `end`,
`backspace`, `delete`, `tab`, `backtab`, `space`, `f12`, `ctrl-a` to `ctrl-z`, or one printable
ASCII character; `to` may be `none` to turn a key off. Binding a key again replaces it.
Bindings are not applied while typing in a field. Users can add bindings as `key = to` lines,
with `#` comments, in `tuikit/keys` and `<tool>/keys` under `$XDG_CONFIG_HOME` (when absolute)
or `$HOME/.config`, or `%APPDATA%` on Windows.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, an unknown name, `key` of `none` or
`ctrl-c`; `PNL_ERR_LIMIT` at 64 bindings.

### pnl_mouse_position

```c
int pnl_mouse_position(const pnl_app_t *app, int *out_x, int *out_y);
```

Gives the position of the last mouse event, relative to the top-left of the panel or view
being handled. Use it when `key_fn` receives `PNL_KEY_CLICK` or a wheel key. `TUIKIT_MOUSE=off`
turns the mouse off.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument.

### pnl_key_decode

```c
size_t pnl_key_decode(const unsigned char *data, size_t data_len, int *out_key);
```

Decodes one key from terminal input: UTF-8 characters, escape sequences and SGR mouse
reports (the position is discarded). `\r` and `\n` give enter, `\t` tab, and 0x7f and 0x08
backspace. Unknown sequences give `PNL_KEY_NONE`.

Returns: the number of bytes used; 0 if `out_key` is `NULL` or there is no data.

## Testing

A test runs an app on a virtual terminal on its own thread, with a virtual clock. Memory, key
files, motion and the mouse are off during a test. Only one test can exist at a time.

### pnl_test_create

```c
int pnl_test_create(pnl_app_t *app, int width, int height, pnl_test_t **out_test);
```

Starts the app on a virtual screen of `width` by `height` cells and waits until it settles.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, `width` outside 10..1000, `height`
outside 4..1000, an app that is already running or in command mode, or another test still
open; `PNL_ERR_NOMEM`; `PNL_ERR_IO` if the app does not settle within 30 seconds.

### pnl_test_destroy

```c
void pnl_test_destroy(pnl_test_t *test);
```

Sends ctrl-c to the app, waits for it to stop and frees the test. Accepts `NULL`.

Returns: nothing.

### pnl_test_keys

```c
int pnl_test_keys(pnl_test_t *test, const char *keys);
```

Sends keys separated by spaces and waits until the app settles. Each token is a key name as in
`pnl_app_bind`, text in double quotes, `click:X,Y`, `wheelup` or `wheeldown`.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument, an unknown token or a click outside the
screen; `PNL_ERR_IO` if the app does not settle.

### pnl_test_wait

```c
int pnl_test_wait(pnl_test_t *test, int ms);
```

Moves the virtual clock forward by `ms` milliseconds, running timers and animations.

Returns: `PNL_OK`; `PNL_ERR_ARG` if `test` is `NULL` or `ms` is negative.

### pnl_test_screen

```c
const char *pnl_test_screen(const pnl_test_t *test);
```

Returns: the screen as text, one line per row with trailing spaces removed; `""` for `NULL`.

### pnl_test_expect

```c
int pnl_test_expect(pnl_test_t *test, const char *path);
```

Compares the screen with the snapshot file at `path`. When the file is missing or differs, the
screen is written to `path.new`; on a match `path.new` is removed. With
`TUIKIT_SNAPSHOTS=update` the snapshot is written instead.

Returns: `PNL_OK`; `PNL_ERR_ARG` for a `NULL` argument or a path longer than 1018 bytes;
`PNL_ERR_NOT_FOUND` if the file is missing; `PNL_ERR_MISMATCH` if it differs; `PNL_ERR_IO` if
an update cannot be written.

## Text utilities

### pnl_text_width

```c
int pnl_text_width(const char *text);
```

Measures UTF-8 text in terminal columns: wide characters count 2 and combining marks 0.

Returns: the width; 0 for `NULL`.

## Environment variables

A value of `off` or `0` turns a setting off where the table says so.

| Variable | Effect |
| --- | --- |
| `TUIKIT_MOTION` | `off` or `0` turns animations off, `on` turns them on even over a slow SSH link, `all` also turns on every kind. |
| `TUIKIT_THEME` | A theme name or file, as in `pnl_app_set_theme`; overrides the theme set in code. |
| `TUIKIT_CONTRAST` | `high` raises the contrast of the colors. |
| `TUIKIT_BACKGROUND` | `light` or `dark` sets the background instead of asking the terminal. |
| `TUIKIT_LINEAR` | Any value other than empty, `off` or `0` draws plain lines instead of a full screen, with no mouse, art or banner. |
| `TUIKIT_LINKS` | `off` or `0` turns links off. |
| `TUIKIT_MOUSE` | `off` or `0` turns the mouse off. |
| `TUIKIT_INSPECT` | `off` or `0` turns the F12 inspector off. |
| `TUIKIT_NOTIFY` | `bell` sends only the bell; `off` or `0` sends nothing. |
| `TUIKIT_MEMORY` | `off` or `0` keeps memory off. |
| `TUIKIT_RECORD` | A file to record the run to, when no path was set with `pnl_app_record`. |
| `TUIKIT_SNAPSHOTS` | `update` makes `pnl_test_expect` write snapshots. |
| `NO_COLOR` | Turns colors off. |
| `TERM` | `dumb` turns colors off; names ending in `256color` or `direct` choose the color depth. |
| `COLORTERM` | `truecolor` or `24bit` turns on 24-bit colors. |
| `COLORFGBG` | Tells whether the background is light or dark. |
| `SSH_CONNECTION` | When set, the link speed is measured and animations are turned off if replies take more than 60 ms. |
| `XDG_STATE_HOME`, `HOME`, `LOCALAPPDATA` | Where the memory file goes. |
| `XDG_CONFIG_HOME`, `HOME`, `APPDATA` | Where key files are read. |
