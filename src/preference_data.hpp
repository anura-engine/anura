#ifndef PREFERENCE_DATA
#define PREFERENCE_DATA

#include <string>

struct PreferenceData{
    int resolution_width;
    int resolution_height;
    bool is_full_screen;
    bool error = false;
    std::string error_message = "";
};

#endif