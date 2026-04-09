# Changelog

Each release lists what changed, what updating means for existing code, and every security fix.

## Unreleased

## 3.3.0 - 2026-04-08

Mouse: click and scroll, for when a hand is already on it.

- the panel turns on SGR mouse reporting; `TUIKIT_MOUSE=off` leaves the mouse to the terminal,
  and shift-drag still selects text in most terminals
- new keys `PNL_KEY_CLICK`, `PNL_KEY_WHEEL_UP` and `PNL_KEY_WHEEL_DOWN`; `pnl_mouse_position`
  gives the position inside the screen, dialog or panel receiving the event
- a view that does not handle the wheel gets three up or down keys instead, so pagers, tables
  and custom lists scroll without changes
- click an action to run it, a table row to select it, a table header to sort by that column,
  an item of a pick list to choose it, yes or no in a confirmation, a panel to focus it and a
  tab to open it; clicks outside an open dialog are ignored

## 3.2.0 - 2026-04-02

On top: some screens should not replace the one below them.

- `pnl_view_push_dialog` opens a framed box centered over the current screen; the screen below
  keeps drawing and updating, only the dialog takes keys
- confirmation, input and pick lists open as dialogs instead of taking the whole screen
- `pnl_toast(app, mark, text)` shows a short notification in the lower right corner for three
  seconds, up to three at once; with motion on it slides in. On the command line it goes to
  stderr, and it is silent with `--json`
- the dashboard example restarts a service with enter, asking first and confirming with a toast

## 3.1.0 - 2026-04-02

Focus: it is always clear which panel listens, and what it listens to.

- the focused panel's title, or the tab bar holding it, gets a mark in the accent color
- `pnl_layout_hints(layout, panel, hints)` gives a panel its own key hints; the footer shows those
  of the focused panel followed by the layout's own keys, and `?` lists them in the help screen
- the footer offers tab only when more than one panel is visible and `[ ]` only when the layout
  has tabs

## 3.0.0 - 2026-03-30

Layouts: a screen can be made of panels, and what does not fit folds away.

- `pnl_layout_create(app, kind, &layout)` starts a tree whose root splits its children into rows,
  columns or tabs (`PNL_SPLIT_ROWS`, `PNL_SPLIT_COLUMNS`, `PNL_SPLIT_TABS`);
  `pnl_layout_split` nests another split and `pnl_layout_panel` adds a panel with its own title,
  draw function and key function
- `pnl_layout_size(layout, node, size, min_size)` gives a node a fixed size along its parent's
  direction (0 shares the space left) and the least it can live with; when a split runs out of
  room its last children are hidden first
- `pnl_view_push_layout(app, title, layout)` opens it as a screen; the screen keeps its own copy,
  so the layout can be destroyed right away or pushed again. Tab and shift-tab move the focus
  between visible panels, `[` and `]` switch tabs, other keys go to the focused panel, and
  `pnl_canvas_focused(canvas)` tells a panel whether it has the focus
- `pnl_layout_focus` picks the panel that starts focused
- new example: `examples/dashboard.c`

### Breaking

- `pnl_table` takes per-column flags after the headers: `PNL_COLUMN_RIGHT` aligns a column to the
  right, also on the command line, and `PNL_COLUMN_OPTIONAL` lets the table hide it, rightmost
  first, when the terminal is too narrow; the header then shows how many are hidden. Pass `NULL`
  for no flags
- tab now arrives as `PNL_KEY_TAB` instead of `'\t'`, and shift-tab as `PNL_KEY_BACKTAB`
- the shared library is now `libtuikit.so.3`

### Fixed

- the panel no longer passes a null pointer to `memcpy` when no file descriptor is watched

## 2.6.0 - 2026-03-30

Motion turns itself off over a slow link.

- inside an SSH session (`SSH_CONNECTION` set) the panel times the terminal's reply to the query it
  already sends at start; when the reply takes more than 60 ms, or never comes, all motion is off
  for that run, so a remote panel does not stream frames through a slow connection
- when the background query is skipped (`TUIKIT_BACKGROUND`, `COLORFGBG` or no colors) a lone
  device attributes query measures the link instead, only over SSH
- `TUIKIT_MOTION=on` keeps motion on regardless, and `TUIKIT_MOTION=all` implies it

## 2.5.0 - 2026-03-23

Optional motion, third part: failures pulse and results type themselves out.

- with `PNL_MOTION_PULSE` every `!!` mark breathes slowly between the fail color and a dimmer
  shade, once every 1.2 s, so a failure on a busy panel is hard to miss; 256 colors or more
- with `PNL_MOTION_TYPEWRITER` the text of a result screen appears letter by letter, 12 ms each and
  never more than 400 ms in all; a key pressed meanwhile acts at once
- `TUIKIT_MOTION=all` includes both

## 2.4.0 - 2026-03-16

Optional motion, second part: screens slide in.

- with `PNL_MOTION_TRANSITION` a screen that opens slides in from the right over 160 ms, starting a
  third of the width away, and the screen you return to settles in from a sixth of the width; the
  title and the footer stay put
- `TUIKIT_MOTION=all` includes it

## 2.3.0 - 2026-03-12

Optional motion, first part: the selection slides and pages scroll smoothly.

- `pnl_app_enable_motion(app, PNL_MOTION_SLIDE)` turns on the first optional animation: the menu's
  `>` marker travels to the new item over 140 ms instead of jumping, and the pager scrolls a page
  down or up over the same time
- `TUIKIT_MOTION=all` turns on every optional animation from the terminal; `TUIKIT_MOTION=off`
  still turns off all motion, default or optional
- a key pressed while something slides acts at once, and the slide continues from where it was
  toward the new place

## 2.2.0 - 2026-03-12

Changes catch the eye.

- when a status line's text or mark changes between refreshes, its text gets a tint of the accent
  color that fades out over 600 ms; lines are matched by position and nothing is highlighted on the
  first draw
- a result screen opens with a short flash behind its mark, in the mark's own color
- in 16 colors the highlight is bold for the first half of the fade; with motion off there is no
  highlight and no flash

## 2.1.0 - 2026-03-12

Things that wait now show it, and bars fill smoothly.

- a `..` mark turns into a spinner while the panel is open, and a running task shows one next to
  its step; with motion off the marks stay as they were
- bars, status meters and the task progress bar fill in eighths of a cell, so a 30-cell bar has
  240 steps instead of 30
- frames drawn only to move a spinner reuse the status lines from the last refresh instead of
  calling the status callback again

## 2.0.0 - 2026-03-07

A frame clock: the panel can now animate, and the deprecated status call is gone.

- while something is animating the panel redraws about 30 times a second, and goes back to waiting
  for keys, timers and tasks when it stops; keys are always read before a frame is drawn, so an
  animation never holds one back
- `pnl_app_animate(app, ms)` asks for frames for that long, `pnl_app_frame_ms(app)` gives the time
  of the frame being drawn, and `pnl_app_tween(app, start_ms, duration_ms)` returns how far along,
  from 0 to 1, an animation that started at `start_ms` is, asking for frames until it reaches 1
- `pnl_app_motion(app)` tells whether motion is on; it is off outside the terminal and with
  `TUIKIT_MOTION=off`, and then `pnl_app_tween` returns 1 right away so every animation shows its
  final state

### Breaking

- `pnl_app_set_status` is removed; call `pnl_app_on_status(app, fn, ctx)` and
  `pnl_app_set_refresh(app, ms)` instead
- the shared library is now `libtuikit.so.2`; programs built against 1.x need to be rebuilt

## 1.5.0 - 2026-03-07

The status callback and its refresh interval are set apart; contrast can be raised.

- `pnl_app_on_status(app, fn, ctx)` sets the status callback and `pnl_app_set_refresh(app, ms)`
  how often the panel redraws on its own (0, the default, redraws only after a key, a timer, a
  task or `pnl_app_redraw`); the interval can now change while the panel is open without touching
  the callback
- `TUIKIT_CONTRAST=high` asks for 7:1 on role colors and 4.5:1 on muted text in true color

### Deprecated

- `pnl_app_set_status` still works but warns at compile time; it goes away in 2.0. Define
  `PNL_NO_DEPRECATED_WARNINGS` to silence the warning meanwhile

## 1.4.0 - 2026-02-26

Color that measures: values can be drawn, not only printed.

- `pnl_canvas_bar(canvas, x, y, width, fraction, flags)` draws a horizontal bar
- `pnl_canvas_sparkline(canvas, x, y, width, values, values_len, max, flags)` draws the most recent
  values as a one-line chart
- `pnl_status_meter(app, mark, text, fraction)` adds a status line with a bar and a percentage; the
  command line prints `disk  83%` and `--json` adds a `"meter"` field
- with `PNL_METER_LEVEL` the color follows the value, from the theme's ok color through warn to
  fail; `PNL_METER_ACCENT` keeps the accent color
- the basic example shows a load sparkline and a memory meter

## 1.3.0 - 2026-02-21

Themes: the role colors can come from a built-in set or from a file.

- built-in themes `default`, `mono`, `contrast` and `warm`
- `pnl_app_set_theme(app, name_or_path)` picks the tool's theme; `TUIKIT_THEME` lets the person at
  the terminal override it with a name or a path
- a theme file has one `role = #rrggbb` per line (`accent`, `ok`, `warn`, `fail`, `muted`, `info`,
  `surface`), `light.role = #rrggbb` for light backgrounds, and `#` comments; roles left out keep
  the default
- the minimum contrast of 1.2 applies to every theme, and the 256-color index of each role now
  comes from its RGB value, so themes look right in 256-color terminals too

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
