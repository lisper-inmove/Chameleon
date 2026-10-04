#include "config_manager.h"

namespace chameleon {

namespace {

// yaml-cpp 0.9 throws InvalidNode when a key is accessed on an undefined
// node, so every level must be checked before descending.
YAML::Node childNode(const YAML::Node &node, const std::string &key)
{
    if (!node.IsDefined())
        return YAML::Node();
    return node[key];
}

} // namespace

bool ConfigManager::load(const std::string &path)
{
    try {
        m_root = YAML::LoadFile(path);
        m_loaded = m_root.IsDefined();
    } catch (const YAML::Exception &) {
        m_loaded = false;
    }
    return m_loaded;
}

std::string ConfigManager::title() const
{
    if (!m_loaded)
        return "Chameleon";
    const YAML::Node title = childNode(childNode(m_root, "app"), "title");
    return title.IsDefined() ? title.as<std::string>() : "Chameleon";
}

int ConfigManager::windowWidth() const
{
    if (!m_loaded)
        return 640;
    const YAML::Node width = childNode(childNode(childNode(m_root, "app"), "window"), "width");
    return width.IsDefined() ? width.as<int>() : 640;
}

int ConfigManager::windowHeight() const
{
    if (!m_loaded)
        return 480;
    const YAML::Node height = childNode(childNode(childNode(m_root, "app"), "window"), "height");
    return height.IsDefined() ? height.as<int>() : 480;
}

} // namespace chameleon
