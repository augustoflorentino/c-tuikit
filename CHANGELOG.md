# Changelog

Each release lists what changed, what updating means for existing code, and every security fix.

## Unreleased

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
