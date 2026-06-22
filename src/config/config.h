#ifndef CONFIG_H
#define CONFIG_H
#include <stdbool.h>

#include <cjson/cJSON.h>

#define CONFIG_PATH     ".config/alwide/config"
#define CONFIG_FOLDER   ".config/alwide/"
#define CONFIG_FILENAME "config"

#define DEFAULT_CONFIG "{\n    \"default_path\": \"~/.config/alwide\"\n}\n"


// TODO implement a config file for theme and parser highlight.scm. Using JSON.
bool configExist();

void touchConfig();

cJSON* loadConfig();

#endif // CONFIG_H
