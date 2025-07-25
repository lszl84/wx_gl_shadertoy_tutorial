#pragma once

#include <GL/glew.h> // must be included before glcanvas.h
#include <wx/glcanvas.h>
#include <wx/wx.h>

#include <chrono>

#include "channel.h"
#include "shaderprogram.h"

wxDECLARE_EVENT(wxEVT_OPENGL_INITIALIZED, wxCommandEvent);

class OpenGLCanvas : public wxGLCanvas {
public:
    OpenGLCanvas(wxWindow* parent, const wxGLAttributes& canvasAttrs);
    ~OpenGLCanvas();

    bool InitializeOpenGL();

    bool IsOpenGLInitialized() const { return isOpenGLInitialized; }

    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    void OnTimer(wxTimerEvent& event);

    void CompileCustomFragmentShader(const std::string& fragmentShaderSource);

    std::string GetShaderBuildLog() const
    {
        return shaderProgram.lastBuildLog.str();
    }
    Channel channel[4] {}; // Channels for shader inputs
    void updateChannels();
    void updateChannel(int channelIndex);

private:
    bool InitializeOpenGLFunctions();

    wxGLContext* openGLContext;
    bool isOpenGLInitialized { false };

    ShaderProgram shaderProgram {};

    wxTimer timer;
    std::chrono::high_resolution_clock::time_point openGLInitializationTime {};
    float elapsedSeconds { 0.0f };
    float deltaTime { 0.0f };
    int frameCount { 0 };
};