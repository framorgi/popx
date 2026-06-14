#pragma once

#include <string>

///--------------------------------------------------------------------------
/// @brief Plain-data struct shared between the app loop and the GUI layer.
///        The GUI writes flags; the app loop reads them.
///--------------------------------------------------------------------------
struct SimControl {
    enum class SaveStatus { None, Success, Failed };

    bool paused = false;
    bool stop_requested = false;
    bool terminate_requested = false;
    bool restart_requested = false;
    bool newgen_requested = false;
    bool show_stats_overlay = true;
    bool save_stats_requested = false;
    bool stats_saved_recently = false;
    SaveStatus last_save_status = SaveStatus::None;
    std::string last_save_timestamp;
    std::string last_saved_json_path;
    std::string last_save_error;
};
