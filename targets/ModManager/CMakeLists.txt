# ModManagerPlugin - Built-in mod manager plugin
# This plugin is statically linked into PluginHost and always loaded

# Source files
set(MOD_MANAGER_PLUGIN_SOURCES
    ModManagerPlugin.cpp
    ModManagerPlugin.hpp
)

# Add to parent scope for linking into PluginHost
set(MOD_MANAGER_PLUGIN_SOURCES ${MOD_MANAGER_PLUGIN_SOURCES} PARENT_SCOPE)

# Note: This plugin is linked into PluginHost, not built as a separate library
# The sources are included in PluginHost's CMakeLists.txt or Makefile

