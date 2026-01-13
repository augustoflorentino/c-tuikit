# Changelog

Each release lists what changed, what updating means for existing code, and every security fix.

## Unreleased

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
