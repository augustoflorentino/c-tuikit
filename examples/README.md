# Examples

Each example is one C file that builds on its own against the library. Build them all with:

```sh
cmake -S . -B build -DC_TUIKIT_BUILD_EXAMPLES=ON
cmake --build build
./build/tuikit_deploy
```

Every example is also a command line tool: run it with `--help` to list its subcommands, or with
its output sent to a pipe to see the plain form.

| Example | Start here if you want to... | Shows |
| --- | --- | --- |
| [basic.c](basic.c) | see everything once | status lines and values, results, tasks with progress and cancel, confirm, input, pick lists, pager, tables (also of a million rows from a callback), live logs, timers, handing the terminal back, charts and spinners on a view of your own, memory between runs |
| [deploy.c](deploy.c) | build a real tool | a deploy as a task with steps, a log and progress, a live diagram of the pipeline as the banner, a scene that follows the deploy, a follow-up after the task, release history in a table |
| [dashboard.c](dashboard.c) | split a screen into panels | layouts in rows, columns and tabs, panels of your own with focus and footer hints, panel sizes with a minimum, mouse position, toasts |
| [monitor.c](monitor.c) | build screens from components | bars, graphs, spinners, a live log and big letters in stacks inside a layout, and a clock written as a component from scratch |
| [form.c](form.c) | ask for several things at once | text fields (one hidden), a choice, a slider, a checkbox and a button, a change callback that keeps a summary up to date |
| [tail.c](tail.c) | follow something outside the tool | a file read on a timer into a live log, a value counting errors, memory between runs |
| [csv.c](csv.c) | show a lot of data | a CSV file of any size opened as a table through `pnl_table_source`, sorted and filtered on a worker thread |
| [gallery.c](gallery.c) | look at what the canvas can draw | random combinations of charts, plots, bars, gradients, spinners, big numbers and motion, regenerated on a timer |

`tail.c` and `csv.c` take a file: `./build/tuikit_tail /var/log/syslog`,
`./build/tuikit_csv data.csv`.

See [../docs/api.md](../docs/api.md) for every function and [../docs/threads.md](../docs/threads.md)
for what may run on other threads.
