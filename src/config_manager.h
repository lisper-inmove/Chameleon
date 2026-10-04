#pragma once

#include <string>

#include <yaml-cpp/yaml.h>

namespace chameleon {

// Loads and provides access to the YAML runtime configuration
// (configs/app.yaml). Missing keys or an unloadable file fall back to
// built-in defaults.
class ConfigManager
{
public:
    // Returns false if the file cannot be opened or parsed; defaults apply.
    bool load(const std::string &path);

    std::string title() const;
    int windowWidth() const;
    int windowHeight() const;

private:
    bool m_loaded = false;
    YAML::Node m_root;
};

} // namespace chameleon
