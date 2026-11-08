# Logger

Logger is a native Haiku application for viewing, following and filtering
the system log (`syslog`) and other log files.

## Features

- **Opens the system log at launch.** It also opens any file passed on the
  command line, dropped onto a window, or chosen with *File → Open…*. Each
  file gets its own window.
- **Follows the file live.** New lines appear as they are written. Logger
  notices when a file is truncated or replaced, as happens when syslog
  rotates. Following pauses when you scroll up and resumes when you scroll
  back to the end (*View → Jump to latest*, `Alt+J`).
- **Parses lines into columns:** Line, Time, Host, Facility, Severity,
  Source, PID and Message. It understands:
  - Haiku's syslog (`KERN: …`, `AUTH 'app'[pid]: …`)
  - RFC 5424 (`<PRI>1 TIMESTAMP HOST APP PROCID MSGID SD MSG`)
  - BSD syslog (`<PRI>Mmm dd hh:mm:ss host tag[pid]: …`)
  - ISO or bracketed time stamps followed by a level (`ERROR`, `[warn]`)
    or by free text

  Lines in any other format show up whole in the Message column. Columns
  that no line fills are hidden. Columns can be moved, resized, and shown
  or hidden with their pop-up menu.
- **Color-codes lines by severity**, in both light and dark themes.
- **Search and filters.** The search field (`Alt+F`) matches any column.
  The filter bar (`Shift+Alt+F`) combines rules on any column, applying all
  of them or any of them. The operators are *contains*, *is*, *starts with*,
  *ends with* and *matches* (regular expression), the negation of each, and
  *less than* / *greater than*, which compare numbers as numbers and
  severities by how severe they are. Each rule can be switched off or made
  case-sensitive.
- **Context menu on a line.** Show only, or hide, lines with the same value
  in the clicked column. Show only this severity and more severe ones. Copy
  the value or the lines.
- **Details pane** (`Alt+D`) with the parsed fields and raw text of the
  selected line.
- **Status bar** that shows how many lines pass the filters and whether
  Logger is following the file.
- **Replicant.** The log list can be dragged onto the Desktop by its
  dragger. It keeps following the file there, with the window's columns and
  filters, and shows the latest 1000 lines.

## Building

Logger needs a current Haiku (developed on R1/beta6 nightlies) with the
development tools (`haiku_devel`). It links against `libbe`, `libtracker` and
`libcolumnlistview`, and uses the private `BColumnListView` and
`BAboutWindow` headers.

### Command line

```sh
make                  # release build
make DEBUGGER=TRUE    # build with debug information
make install          # copy to ~/config/non-packaged/apps
make clean
```

The application is built as `objects.<arch>-<compiler>-<mode>/Logger`, for
example `objects.x86_64-cc13-release/Logger`.

### HiLo Developer Studio

Open `Logger.hiloproj` and build the `Logger` scheme. The build products go
to `build/`.

## Usage

```sh
Logger                         # open the system log
Logger /var/log/syslog.old     # open one or more log files
```

## Source layout

| File | Purpose |
| --- | --- |
| `src/main.cpp` | Entry point |
| `src/LoggerApp.*` | Application: opens windows, the open panel, the About window |
| `src/LogWindow.*` | A window that shows, follows and filters one log file |
| `src/LogListView.*` | The column list of log lines, archivable as a replicant |
| `src/LogColumns.*` | Columns, fields and rows, with severity colors |
| `src/FilterView.*` | The filter rule bar |
| `src/LogFilter.*` | Filter rules and rule sets |
| `src/LogParser.*` | Splits log lines into records |
| `src/LogRecord.*` | A parsed log line: fields and severities |
| `src/LogTailer.*` | Reads lines appended to a file, and detects truncation and rotation |
| `Logger.rdef` | Signature, version, file types and icon |

## License

Copyright © 2025-2026 Dru Satori @ Druware Software Designs.

Logger is released under the MIT License. See [LICENSE](LICENSE).
