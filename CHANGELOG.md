# Changelog

Each release lists what changed, what updating means for existing code, and every security fix.

## Unreleased

## 5.8.0 - 2026-05-23

Charts: the canvas learns to draw more than one line of data.

- `pnl_canvas_chart` draws a column chart several rows tall, one column per value, with eighths
  of a row for a smooth top; the newest values stay when there are more than fit
- `pnl_canvas_columns` draws a few labeled columns side by side, as wide as the space allows,
  for comparing things at a glance (cores, disks, queues)
- `pnl_canvas_plot` draws a line through the values with braille dots, two points per column and
  four per row, joining each point to the next so steep changes stay a line
- `PNL_METER_PERCENT` writes the percentage in the middle of a bar, inverted where the bar is
  filled
- `PNL_METER_LEVEL` works on all of them: chart columns shift from green to red as they rise
- the linear mode leaves braille out, as it does with boxes and blocks
- the live screen of `examples/basic.c` shows every one of them

## 5.7.0 - 2026-05-23

Inspector: one key shows what the program is doing inside, while it does it.

- f12 opens the inspector over any screen, and f12 or q closes it. It refreshes four times a
  second and shows:
  - frames: how many were drawn, the last, average and worst time to draw and render one, the
    bytes sent, and a sparkline of the last 60
  - screens: the stack from the menu to the top, dialogs marked
  - parts: actions, status lines, values, timers, watches, running tasks, toasts and mapped keys
  - motion: whether animations are on, whether a spinner turns, whether something is animating
  - terminal: size, colors, background, mouse, focus, links on screen and how fast the terminal
    answered
  - events: the last 16 keys, timer runs, wakes from other threads, ready watches and resizes,
    with how long ago each happened
- `f12` is a key name in the keys file and in `pnl_test_keys`, so the inspector can be moved to
  another key (`ctrl-t = f12`) or opened in a test
- `TUIKIT_INSPECT=off` turns it off; it costs nothing until the panel runs and about 1.5 KB
  after
- key names in help and in the inspector show any key, including non-ASCII letters, instead of
  a broken character

## 5.6.0 - 2026-05-23

Cleanup: what the 5.x releases left behind, with no change in behavior.

- one UTF-8 encoder instead of four (screen, virtual screen, line editor and Windows input)
- one reading of `off` and `0` for every `TUIKIT_*` switch: `TUIKIT_MOUSE`, `TUIKIT_MOTION`,
  `TUIKIT_LINKS`, `TUIKIT_LINEAR`, `TUIKIT_NOTIFY` and `TUIKIT_MEMORY` now accept the same words
- the renderer has two plain paths, cell diff and linear, instead of one loop with both mixed in;
  a frame that fails to reach the terminal is no longer written to the recording
- a correction to the 5.0 notes: under `pnl_test_create` animations are off, so a screen never
  depends on timing; timers and toasts follow the virtual clock

## 5.5.0 - 2026-05-18

Lighter: the same panel holding less, measured with the bench from 5.4.

- an app takes 1.9 KB instead of 10.6 KB: the table of links is allocated with the screen when
  the panel opens, not with every app
- a log keeps its text in one shared ring instead of 240 bytes reserved for every line: a log of
  1000 lines takes 152 KB instead of 264 KB. It keeps up to `capacity` lines, fewer only when they
  average more than 128 bytes; the oldest go first, as before
- a table keeps its row order in 32-bit indexes and frees the sorting buffer when the sort ends:
  a table of a million rows holds 8 MB instead of 24 MB, 12 MB while sorting. Tables are limited
  to 4294967295 rows
- `pnl_value_set` with a value already waiting to be drawn, or with the same text, no longer
  wakes the panel again: 15 ns instead of 830 ns per call, so a value set in a tight loop costs
  next to nothing
- the bench reports the memory of a table of a million rows

## 5.4.0 - 2026-05-18

Measured: before making anything faster or smaller, a way to know.

- `bench/run.sh` builds `bench/bench.c` with optimizations and prints how long things take and
  how much memory they hold: drawing a 200x60 screen, rendering it whole and with one changed
  row (also in linear mode), writing log lines, setting values, opening, sorting and filtering a
  table of a million rows, and the bytes an app, an action, a value and a log take
- the table numbers come from a real panel running under `pnl_test_create`, so they include
  everything a user would wait for
- on the machine it was written on: a full 200x60 frame takes about 0.3 ms and 13 KB, a frame
  with one changed row 24 us and 21 bytes; a log line 34 ns; a million rows sort in about 2 s
  and filter in 0.2 s; an empty app takes 10 KB and an empty log of 1000 lines 264 KB, the
  starting point for the next release

## 5.3.0 - 2026-05-15

Clickable links: an address on the screen opens with a click, even when it does not fit.

- any `http://` or `https://` address drawn by the panel, in a status line, a result, a log, a
  table cell or a pager, becomes an OSC 8 link; terminals that know OSC 8 open it with a click
  (often ctrl-click), the others show plain text
- the link points to the whole address from the text, not to what fits on screen, so a long
  address cut by a narrow column still opens the right page; a period, comma or parenthesis
  right after the address is left out of it
- `pnl_canvas_link(canvas, x, y, style, text, url)` draws any text as a link to `url`
- `TUIKIT_LINKS=off` draws them as plain text; linear mode prints the address itself

## 5.2.0 - 2026-05-15

For everyone: the panel for those who listen to it, who need strong colors or who never touch a
mouse.

- `TUIKIT_LINEAR=on` draws the panel as plain lines instead of a screen: no alternate screen,
  no cursor jumps, no colors, no box lines. Only what changed is printed, so a screen reader
  reads the new selection, the new value or the next line of a log, and not the whole screen
  again. Moving the selection prints just the selected line; a dialog prints only what is inside
  its box; lines that scroll are not repeated; bars become their percentages
- in linear mode animations, the mouse and terminal queries are off
- `TUIKIT_CONTRAST=high`, already there, is now documented: every color, muted text included,
  is pushed to a strong contrast with the background
- every action, list, table, tab and dialog can be reached from the keyboard alone; the mouse
  stays optional

## 5.1.0 - 2026-05-15

Record: a session becomes a file anyone can replay.

- `TUIKIT_RECORD=demo.cast tool` records what the panel draws as an asciicast v2 file, which
  `asciinema play` replays in a terminal and tools like `agg` turn into a GIF; no change to the
  tool is needed
- `pnl_app_record(app, path)` sets the file from code, `NULL` to stop; the variable is used
  when the tool sets nothing
- only the screen is recorded, never what is typed; a resize appears in the file as a resize
  event
- under `pnl_test_create` the recording follows the virtual clock and leaves out the date, so
  a test script produces the same file every run: a demo that never goes stale

## 5.0.0 - 2026-05-11

Headless: a panel runs without a terminal, so a tool can be tested the way it is used.

- `pnl_test_create(app, width, height, &test)` runs the panel on a virtual screen with a virtual
  clock, in a thread of its own; `pnl_test_destroy` quits it
- `pnl_test_keys(test, "down enter \"text\" esc")` types a script: key names as in the keys
  file, quoted text typed letter by letter. Every key waits until the screen settles and no task
  is running, so the next one never races the last
- `pnl_test_wait(test, ms)` moves the virtual clock: timers, toasts and animations advance by
  exactly that much, and the same script always gives the same screen
- `pnl_test_screen(test)` returns the screen as text, one line per row
- `pnl_test_expect(test, path)` compares the screen with a snapshot file: a missing one is
  written, a different screen is saved next to it as `path.new` and the call returns the new
  `PNL_ERR_MISMATCH`; `TUIKIT_SNAPSHOTS=update` rewrites them
- under test the panel skips what depends on the machine: terminal queries, the mouse, the
  remembered choices and the keys files
- inside, drawing and input now go through one layer that leads either to the terminal or to
  the virtual screen; the next releases build on it

### Breaking

- `pnl_app_create(title, allocator, &app)` takes a `pnl_allocator_t` with `alloc_fn`, `free_fn`
  and `ctx` instead of three arguments; most code passed `NULL, NULL, NULL` and now passes `NULL`
- the shared library is now `libtuikit.so.5`

## 4.6.0 - 2026-05-11

Two new examples, each a small tool you might actually keep.

- `examples/tail.c`: `tuikit_tail app.log` follows a file into a live log, guesses the level of
  each line from words like "error" or "warn", and counts the errors in the status;
  `tuikit_tail app.log lines --json` prints what it read
- `examples/csv.c`: `tuikit_csv data.csv` opens a CSV file of any size as a table that reads
  its cells straight from the file, with quoted fields; a million rows sort in a few seconds
  without blocking the screen
- fixed: a large table in a tool without values or logs never showed the result of its sort
  or filter, because nothing woke the panel when the thread finished
- after a sort or a filter, a table still on its first row goes back to the top instead of
  following that row

## 4.5.0 - 2026-05-10

Your keys: whoever uses the tool decides which key does what.

- a plain text file maps keys to other keys, one `key = key` per line, `#` for comments:
  `~/.config/tuikit/keys` for every tool built with c-tuikit and `~/.config/<tool>/keys` for one of
  them (`$XDG_CONFIG_HOME` when set, `%APPDATA%` on Windows). `ctrl-n = down`, `x = q`,
  `q = none` to turn a key off
- names: `up`, `down`, `left`, `right`, `enter`, `esc`, `pgup`, `pgdn`, `home`, `end`,
  `backspace`, `delete`, `tab`, `backtab`, `space`, `ctrl-a` to `ctrl-z`, `none` and any single
  character
- `pnl_app_bind(app, key, to)` sets the same from code, as defaults the files can override
- while you type in a filter or an input, letters and digits keep their meaning; only control
  keys and named keys are mapped there
- `?` lists the mapped keys under "your keys"
- ctrl-c always quits and cannot be mapped

## 4.4.0 - 2026-04-30

Let me know: start something long, switch to another window, and hear about it when it ends.

- the panel asks the terminal to report focus. A task that ran for 5 seconds or more and ends
  while the terminal is in the background rings the bell and sends a desktop notification
  (OSC 9, understood by iTerm2, WezTerm, kitty, foot, Ghostty and Windows Terminal) with the
  task title and its result
- on the command line, a long command does the same on stderr when it is a terminal
- `pnl_notify(app, text)` sends one for anything else; it stays quiet while the terminal has the
  focus
- `TUIKIT_NOTIFY=bell` keeps only the bell, `TUIKIT_NOTIFY=off` turns both off
- the backup in the basic example takes six seconds now, long enough to try it

## 4.3.0 - 2026-04-27

Live log: the screen to keep open while a service runs.

- `pnl_log_create(app, capacity, &log)` keeps the last `capacity` lines; `pnl_log_write(log,
  level, text)` adds one from any thread, with a level (`PNL_LEVEL_DEBUG`, `INFO`, `WARN`,
  `ERROR`) and the time it arrived. A burst of lines wakes the panel once, not once per line
- `pnl_log_open(app, log, title)` shows it and follows the end as lines arrive. Scrolling up
  pauses it and counts the lines that came in meanwhile; end or `f` follows again
- `v` raises the lowest level shown, from debug to error and around
- `/` filters as you type and marks the match in each line
- on the command line the same call prints the lines kept so far, or a JSON array with
  `time`, `level` and `text`
- the basic example logs fake requests on a timer under "Live log"

## 4.2.0 - 2026-04-27

Memory: the panel picks up where you left off.

- `pnl_app_set_memory(app, path)` turns it on; with `NULL` the file goes to
  `$XDG_STATE_HOME/<tool>/memory` (`~/.local/state` when unset) or `%LOCALAPPDATA%\<tool>\memory`
  on Windows. `TUIKIT_MEMORY=off` turns it off for one run
- the menu opens on the action chosen last time
- pick lists open on the item picked last time, per title
- text inputs keep a history per prompt: up and down go through the last 20 answers and back
  to the text you started with
- tables keep their filter and sort order, per title
- the file is plain text, written once when the panel closes, to a temporary file renamed over
  the old one; a missing or unreadable file only means nothing is remembered
- the basic and deploy examples remember

## 4.1.0 - 2026-04-27

Millions: a table no longer has to fit in memory as strings, and it stays responsive while it
sorts a million rows.

- `pnl_table_source(app, title, headers, flags, columns, rows, cell_fn, release_fn, ctx)` reads
  cells through a callback, only for the rows on screen and while sorting or filtering. The
  callback may write the text into the buffer it is given, so cells can be generated on the
  fly; `release_fn` runs when the table closes
- `/` filters the rows of any table as you type; enter keeps the filter, esc clears it, and the
  line under the header shows how many rows match
- tables with more than 20000 rows sort and filter on a thread: the screen shows "sorting..."
  and keeps taking keys, and a new key cancels the work in progress
- the selection stays on the same row after a sort or a filter
- column widths come from the first rows and grow as wider cells scroll into view
- the sort marker sits next to the header, also on columns aligned to the right
- the basic example has a table of a million generated files

## 4.0.0 - 2026-04-17

Data sources: what the panel shows can come from anywhere, and only what changed is redrawn.

- `pnl_value_create(app, label, &value)` adds a status line that lives outside the status
  callback; `pnl_value_set(value, mark, text)` and `pnl_value_set_meter(value, mark, fraction)`
  change it from any thread. The panel wakes up at once, through a pipe on POSIX and an event on
  Windows, and the renderer sends only the cells that changed; no refresh timer is needed
- values also appear in `tool status` and `tool status --json`, after the lines of the callback
- labels form a column, so values line up under each other, in the panel and on the command
  line; JSON status entries carry a `"label"` field
- the highlight on a changed value follows the label: inserting or removing a line above no
  longer makes every line below it flash
- the basic example keeps the chosen profile in a value

### Breaking

- `pnl_status_line(app, mark, label, text)` takes a label before the text; pass `NULL` for a
  line without one
- `pnl_status_meter(app, mark, label, fraction)`: the text before the bar is now a label and
  lines up with the other labels
- the shared library is now `libtuikit.so.4`

## 3.4.0 - 2026-04-11

Narrow terminals, the kind you get on a phone over SSH or in a split pane.

- columns that do not fit side by side stack as rows when there is height for them, before
  any of them is hidden; the dashboard example shows services above load on a narrow, tall
  terminal
- a footer wider than the screen keeps the hints that fit, whole, and ends with `?: help`,
  where the rest can be read (inside dialogs it just ends)

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
