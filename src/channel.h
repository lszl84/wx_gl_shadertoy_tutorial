#pragma once
#include <GL/glew.h>
#include <wx/wx.h>
enum class ChannelSource {
    None,
    File,
    Microphone,
    Camera
};

enum class ChannelType {
    None,
    Image2D,
    ImageCube,
    Audio
};

struct Channel {
    ChannelSource source;
    ChannelType type;
    float speed { 1.0f }; // Playback speed, 1.0 for normal speed
    std::string name; // Name of the channel, e.g., "iChannel0"
    std::string filePath; // Path to the file if source is File
    int width { 0 }; // Width of the image or audio data
    int height { 0 }; // Height of the image data, 0 for audio
    int depth { 0 }; // Depth for cube maps, 0 for non-cube images
    wxImage texture;
    GLuint textureId = 0;
    bool isStatic { true }; // True if the channel data is static (not changing over time)
    bool isReady { false }; // True if the channel is ready to be used in the shader
};