#pragma once

#include <QString>

class QFile;

// ---------------------------------------------------------------------------
// FileLogger - optionally mirrors every DBG_* line that prints to the
// terminal into a rotating log file under XDG_STATE_HOME, when enabled via
// Settings > App > Logging (AppConfig::logToFile).
//
// Rotation, performed each time a new log file is opened (app startup with
// logging enabled, or the setting being turned on at runtime):
//   - The current (unsuffixed) log file, if any, is renamed to "-1.log".
//   - Existing "-1.log".."-8.log" are shifted up to "-2.log".."-9.log".
//   - If "-9.log" already exists, it is deleted (oldest, dropped).
//   - A new unsuffixed log file is created, named with the current
//     date and time.
//
// In-place size cap: after every write, the current log file's size is
// checked. If it exceeds the cap, the oldest lines are dropped from the
// front so the file shrinks back down (see trimIfNeeded() for why it trims
// to well under the cap rather than exactly to it).
// ---------------------------------------------------------------------------
class FileLogger
{
public:
    // Number of rotated backups kept alongside the current log file (see
    // class comment above). Exposed so callers (e.g. the Settings UI) don't
    // need to hardcode this number.
    static constexpr int MAX_ROTATED_LOGS = 9;

    static FileLogger& instance();

    // Enable or disable file logging. Enabling rotates the previous log (if
    // any) and opens a fresh file for the rest of this session. Disabling
    // closes the current file; nothing on disk is deleted.
    void setEnabled(bool enabled);

    // Appends one already-formatted line (no trailing newline) to the open
    // log file. No-op if logging is disabled.
    void write(const QString& line) const;

private:
    FileLogger() = default;
    ~FileLogger();
    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    void openNewLogFile();
    void closeLogFile();
    void trimIfNeeded() const;

    bool m_enabled = false;
    QFile* m_file = nullptr;
};
