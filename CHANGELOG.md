# Changelog

Each release lists what changed, what updating means for existing code, and every security fix.

## Unreleased

## 1.2.0 - 2026-02-21

Role colors fit the terminal's background, light or dark.

- when the panel opens it asks the terminal for its background color (OSC 11), with a device
  attributes query behind it so terminals that do not answer cost no wait; at most 150 ms
  otherwise
- `TUIKIT_BACKGROUND=light` or `dark` decides without asking; `COLORFGBG` is used when set
- a light background gets its own set of role colors, and in true color every role is darkened or
  lightened until it reaches a contrast of 4.5:1 against the actual background (3:1 for
  `PNL_ROLE_MUTED`)

### Fixed

- a reply to a terminal query arriving late is no longer read as an escape key

## 1.1.0 - 2026-02-19

Colors by meaning instead of by name, adapted to what the terminal can show.

- role colors usable wherever a color goes (`PNL_FG`, `PNL_BG`): `PNL_ROLE_ACCENT`, `PNL_ROLE_OK`,
  `PNL_ROLE_WARN`, `PNL_ROLE_FAIL`, `PNL_ROLE_MUTED`, `PNL_ROLE_INFO`, `PNL_ROLE_SURFACE`
- each role is drawn in true color when `COLORTERM` says so, from the 256-color palette when
  `TERM` has `256color`, and with the 16 basic colors otherwise; the 16 named colors stay as they
  were, following the terminal's own palette
- `NO_COLOR` or `TERM=dumb` turn colors off; backgrounds become reverse video, so bars stay visible
- `pnl_color_depth` tells which of these applies: 0, 16, 256 or 16777216 colors
- the built-in screens use roles: marks, footer, selection marker, progress bars

### Fixed

- the ABI check compares only types reachable from the public header; the 1.0.0 baseline was
  regenerated that way

## 1.0.0 - 2026-02-19

The API is stable. Nothing changes for code written against 0.9; from here on, everything in
`c-tuikit.h` keeps working, source and binary, until a 2.0.0.

- soname `libtuikit.so.1`
- the ABI baseline of the shared library is in `abi/c-tuikit.abi` and CI checks each build against it

## 0.9.0 - 2026-02-19

Polish before 1.0, and a second example.

- `pnl_task_then` registers a function that runs on the panel's thread once the work returns, the
  safe place to update state the status callback reads
- `examples/deploy.c`: a deploy tool with a confirmation, a task with steps, a target picker, a
  table of releases and a pager, usable as `deploy`, `deploy deploy --yes` or `deploy releases
  --json`
- on the command line, a confirmation that could not be asked (no terminal, no `--yes`) makes the
  exit code 1

## 0.8.0 - 2026-02-07

The panel opens on the Windows console and on macOS, not only on Linux.

- Windows 10 or later: the console is switched to virtual terminal mode, so the same screen code
  and key decoder serve both; input and output use UTF-8 and the previous modes come back on exit
  or when the console closes
- tasks use Windows threads there
- `pnl_app_watch_fd` returns `PNL_ERR_UNSUPPORTED` on Windows, as before; ctrl-z does nothing
  there
- marks and progress on the command line are colored in the Windows console too
- macOS builds in CI next to Linux and Windows

## 0.7.0 - 2026-02-07

Nothing to memorize: every screen can say what its keys do, and the actions can be reached in more
than one way.

- `?` opens a help screen built from the key hints of the current screen plus the keys that work
  everywhere
- `pnl_view_set_hints` gives a view of your own its footer and help, as `"r: reload  esc: back"`
- `:` opens a palette with every action, filtered as you type
- in the menu, `1` to `9` run the action with that number, a letter jumps to the next action
  starting with it, home/end and page up/down go to the first and last item
- keys a view does not handle reach the global ones, so the pager and the table also answer `?`

## 0.6.0 - 2026-01-25

Ready-made screens for what most tools end up writing by hand. Each one also works on the command
line, so an action keeps a single code path.

- `pnl_confirm`: yes or no; on the command line it asks on the terminal, or needs `--yes` (`-y`)
- `pnl_input`: one line of text with cursor, home/end, ctrl-u and ctrl-w; reads a line from stdin
  on the command line
- `pnl_pick`: a list filtered as you type; on the command line it takes a number or the exact item
- `pnl_pager`: long text with scrolling and `/` search (`n` and `N` for the next and previous
  match); printed as is on the command line
- `pnl_table`: rows with a sortable column (numbers sort as numbers); aligned text or `--json` on
  the command line
- `pnl_confirm`, `pnl_input` and `pnl_pick` wait for the answer and return it, while timers and
  tasks keep running behind them
- new keys: `PNL_KEY_PAGE_UP`, `PNL_KEY_PAGE_DOWN`, `PNL_KEY_HOME`, `PNL_KEY_END`,
  `PNL_KEY_BACKSPACE`, `PNL_KEY_DELETE`

### Fixed

- a menu taller than the terminal scrolls instead of losing its last items

## 0.5.0 - 2026-01-13

Slow work no longer freezes the panel. An action hands a function to `pnl_task_run`, which runs it
on a thread while the panel shows its progress and log.

- `pnl_task_run`, `pnl_task_step`, `pnl_task_progress`, `pnl_task_log`, `pnl_task_cancelled`,
  `pnl_task_result`
- the task screen shows the current step, a progress bar (or the elapsed time when the total is
  unknown), the last lines of the log and, at the end, the result
- esc or `q` asks the work to stop; it decides when, through `pnl_task_cancelled`
- on the command line the same work runs in the foreground: log lines go to stdout, progress to
  stderr when it is a terminal, the result as usual
- the library now links against the platform threads (`Threads::Threads`, `Libs.private`)

### Fixed

- the footer of a view is drawn after the view, so changes made while drawing show at once

## 0.4.0 - 2026-01-13

Every panel is also a command line tool. The actions declared once become subcommands, so the
same program serves people at a terminal and scripts.

- `pnl_app_run` takes `argc`, `argv` and `out_exit_code`, ready to be returned from `main`
- each action is a command named after its label (`"Watch live"` becomes `watch-live`)
- `status` prints the status lines; it is also what runs when the output is not a terminal
- `--help` is generated from the actions; `--json` prints status and results as JSON
- exit code 1 when a status line or result is `PNL_MARK_FAIL`, or when an action needs a
  screen (`pnl_view_push` returns `PNL_ERR_TTY` there); 2 for usage errors
- marks are colored only when the output is a terminal and `NO_COLOR` is not set

### Upgrading

- `pnl_app_run(app)` becomes `pnl_app_run(app, argc, argv, &exit_code)`; pass `0, NULL, NULL`
  to keep the old behavior of always opening the panel when there is a terminal

## 0.3.0 - 2026-01-13

The panel is alive: it keeps answering keys while timers fire, file descriptors become readable
and custom screens update.

- `pnl_app_add_timer` / `pnl_app_remove_timer`: callbacks every `interval_ms`; the screen is
  redrawn after each one
- `pnl_app_watch_fd` / `pnl_app_unwatch_fd`: a callback when a descriptor has data (POSIX)
- `pnl_view_push` / `pnl_view_pop`: screens of your own on top of the menu, drawn through
  `pnl_canvas_text` and `pnl_canvas_fill`; esc or `q` goes back unless the key callback takes it
- `pnl_app_with_terminal`: gives the terminal back for a while, to print or run another program
- `pnl_app_redraw`, `pnl_app_quit`, `pnl_text_width`
- styles: `PNL_BOLD`, `PNL_DIM`, `PNL_REVERSE`, `PNL_UNDERLINE`, `PNL_FG(color)`, `PNL_BG(color)`
  with 16 colors

### Upgrading

- actions now run inside the panel, with the terminal still in raw mode. An action that prints
  or starts an interactive program must do it inside `pnl_app_with_terminal`
- `pnl_result` can be called at any time while the panel runs, not only inside an action

## 0.2.0 - 2026-01-13

The screen no longer flickers. Nothing changes in the API.

- the panel draws into a cell buffer and sends only the cells that changed since the last frame;
  moving the selection now writes about 50 bytes instead of clearing and repainting everything
- runs on the alternate screen, so the shell scrollback is left as it was
- follows terminal resizes
- the terminal is restored when the program is killed (`SIGTERM`, `SIGHUP`, `SIGQUIT`, `SIGINT`)
- Ctrl-Z suspends the panel and `fg` brings it back
- text is measured in columns: wide characters (CJK, emoji) take two, combining marks none,
  and lines are cut at the edge of the screen instead of wrapping

## 0.1.0 - 2026-01-06

First release: the menu.

- `pnl_app_create` / `pnl_app_destroy`: a panel with a title and an optional allocator
- `pnl_app_set_status` and `pnl_status_line`: status lines with a mark, refreshed every
  `refresh_ms` while waiting for a key
- `pnl_app_add_action` and `pnl_result`: actions picked with the arrow keys, `j`/`k` and enter,
  with an optional result screen
- `pnl_app_run`: the key loop; `q`, esc or Ctrl-C quits
- `pnl_key_decode`: turns raw terminal bytes into keys (arrows, enter, escape, UTF-8)
- POSIX terminals only; on Windows `pnl_app_run` returns `PNL_ERR_TTY`
