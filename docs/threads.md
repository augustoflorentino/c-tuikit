# Threads

c-tuikit runs on one thread: the one that calls `pnl_app_run`. Everything it draws, every callback
it makes and every structure it keeps belongs to that thread, which this page calls the panel
thread. A few calls are made to be used from other threads too; they are listed below, and
nothing else is.

## The panel thread

These callbacks are always called on the panel thread, one at a time, never while another one
is running:

- the status callback (`pnl_app_on_status`)
- actions (`pnl_app_add_action`) and the follow-up of a task (`pnl_task_then`)
- timers (`pnl_app_add_timer`) and watched descriptors (`pnl_app_watch_fd`)
- the draw and key callbacks of views, dialogs and layout panels
- every callback of a component (`pnl_widget_class_t`) and change callbacks
  (`pnl_widget_on_change`)
- the release callback of `pnl_table_source`
- `pnl_app_with_terminal`

Inside them any function of the library may be called. A callback that blocks holds the whole
panel: no keys, no redraws, no timers. Slow work belongs in a task.

## Calls allowed from any thread

| Call | Notes |
| --- | --- |
| `pnl_value_set`, `pnl_value_set_meter` | The panel wakes up and redraws only when the value changed. |
| `pnl_log_write` | Lines are copied; a full log drops its oldest lines. |
| `pnl_task_progress`, `pnl_task_step`, `pnl_task_log`, `pnl_task_cancelled`, `pnl_task_result`, `pnl_task_then`, `pnl_task_show` | Only with the `pnl_task_t` given to the work function, and only until that function returns. |
| `pnl_widget_set_step` | On a flow (`pnl_widget_flow`). Add the steps on the panel thread first. |
| `pnl_widget_set_value` | Only on a scene (`pnl_widget_scene`), where the value is the state. |

Creating and destroying values, logs and components stays on the panel thread. Destroy a value
or a log only when no other thread will touch it again: join or stop the threads that write to
it first.

Every other function, including `pnl_widget_set_value` on any component other than a scene,
`pnl_widget_set_text`, `pnl_toast`, `pnl_app_redraw` and `pnl_app_quit`, must be called from the
panel thread. To act on the panel from another thread, set a value or write a log line and let a
timer or the status callback react to it, or have a task end with `pnl_task_then`.

## Tasks

`pnl_task_run` starts the work function on a thread of its own. While it runs:

- it reports with the `pnl_task_*` calls above, which lock and wake the panel
- `pnl_task_cancelled` turns true when the user presses esc; the function should check it often
  and return
- the work context passed to `pnl_task_run` is shared with the panel thread; guard it yourself if
  both sides use it

When the function returns, the panel thread joins the thread and only then calls the follow-up
set with `pnl_task_then`, so the follow-up sees everything the work function wrote. On the
command line (no terminal, or a subcommand) the same function runs in the foreground on the
calling thread.

## Tables from a callback

The `cell_fn` of `pnl_table_source` is called from the panel thread to draw. For tables of more
than 20 000 rows it is also called from a worker thread while the table sorts or filters, at the
same time as drawing. It must:

- only read data that stays the same while the table is open
- write into the `buf` it is given, or return a pointer to data that stays the same
- never write into shared or static memory

The release callback runs on the panel thread after the worker has stopped.

## Allocators

An allocator given to `pnl_app_create` is called only from the panel thread: the calls allowed
from any thread, the task threads and the table worker never allocate. It does not need to be
safe to call from several threads unless the tool shares it with code of its own.

## Signals

On POSIX, while the panel holds the terminal, the library installs handlers for `SIGWINCH`
(resize) and for `SIGHUP`, `SIGINT`, `SIGQUIT` and `SIGTERM`, and puts the previous handlers back
when it leaves. The resize handler only sets a flag; the others restore the terminal and raise
the signal again with its default action. No callback of the tool runs inside a signal handler.

## Testing

`pnl_test_keys` and `pnl_test_wait` return only when the screen is still and no task or table
sort is running, so a test reads a settled screen. The virtual terminal is built and driven from
the thread that calls `pnl_test_create`.
