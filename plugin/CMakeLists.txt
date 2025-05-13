cmake_minimum_required(VERSION 3.22)

# Version is needed by JUCE.
project(AudioPlugin VERSION 0.1.0)

# Adding a directory with the library/application name as a subfolder of the
# include folder is a good practice. It helps avoid name clashes later on.
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/PolyArp")

# Adds a plugin target (that's basically what the Projucer does).
juce_add_plugin(
  ${PROJECT_NAME}
  COMPANY_NAME "KORGES"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  PLUGIN_MANUFACTURER_CODE KGES
  PLUGIN_CODE PLAP
  FORMATS Standalone VST3
  PRODUCT_NAME "PolyArp"
)

# Sets the source files of the plugin project.
set(SOURCE_FILES source/PluginEditor.cpp source/PluginProcessor.cpp)
# Optional; includes header files in the project file tree in Visual Studio
set(HEADER_FILES ${INCLUDE_DIR}/PluginEditor.h ${INCLUDE_DIR}/PluginProcessor.h)
target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES} ${HEADER_FILES})

# Sets the include directories of the plugin project.
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Links to all necessary dependencies. The present ones are recommended by JUCE.
# If you use one of the additional modules, like the DSP module, you need to specify it here.
target_link_libraries_system(${PROJECT_NAME} PUBLIC juce::juce_audio_utils)
target_link_libraries(
  ${PROJECT_NAME} PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
                         juce::juce_recommended_warning_flags
)

# These definitions are recommended by JUCE.
target_compile_definitions(${PROJECT_NAME} PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

# Enables strict C++ warnings and treats warnings as errors.
# This needs to be set up only for your projects, not 3rd party
set_source_files_properties(${SOURCE_FILES} PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS_CXX}")

# In Visual Studio this command provides a nice grouping of source files in "filters".
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/..)

juce_enable_copy_plugin_step(${PROJECT_NAME})