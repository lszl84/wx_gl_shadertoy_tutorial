#include "openglcanvas.h"
#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#endif

wxDEFINE_EVENT(wxEVT_OPENGL_INITIALIZED, wxCommandEvent);

constexpr auto VertexShaderSource = R"(#version 330 core

    layout(location = 0) in vec3 inPosition;

    void main()
    {
        gl_Position = vec4(inPosition, 1.0);
    }
)";

OpenGLCanvas::OpenGLCanvas(wxWindow* parent, const wxGLAttributes& canvasAttrs)
    : wxGLCanvas(parent, canvasAttrs)
{
    wxGLContextAttrs ctxAttrs;
    ctxAttrs.PlatformDefaults().CoreProfile().OGLVersion(3, 3).EndList();
    openGLContext = new wxGLContext(this, nullptr, &ctxAttrs);

    if (!openGLContext->IsOK()) {
        wxMessageBox("This sample needs an OpenGL 3.3 capable driver.",
            "OpenGL version error", wxOK | wxICON_INFORMATION, this);
        delete openGLContext;
        openGLContext = nullptr;
    }

    Bind(wxEVT_PAINT, &OpenGLCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &OpenGLCanvas::OnSize, this);

    timer.SetOwner(this);
    this->Bind(wxEVT_TIMER, &OpenGLCanvas::OnTimer, this);

    constexpr auto FPS = 60.0;
    timer.Start(1000 / FPS);
}

void OpenGLCanvas::CompileCustomFragmentShader(
    const std::string& customFragmentShaderSource)
{
    shaderProgram.vertexShaderSource = std::string(VertexShaderSource);
    shaderProgram.fragmentShaderSource = customFragmentShaderSource;
    shaderProgram.Build();
}

OpenGLCanvas::~OpenGLCanvas() { delete openGLContext; }

bool OpenGLCanvas::InitializeOpenGLFunctions()
{
    GLenum err = glewInit();

    if (GLEW_OK != err) {
        wxLogError("OpenGL GLEW initialization failed: %s",
            reinterpret_cast<const char*>(glewGetErrorString(err)));
        return false;
    }

    wxLogDebug("Status: Using GLEW %s",
        reinterpret_cast<const char*>(glewGetString(GLEW_VERSION)));

    return true;
}

bool OpenGLCanvas::InitializeOpenGL()
{
    if (!openGLContext) {
        return false;
    }

    SetCurrent(*openGLContext);

    if (!InitializeOpenGLFunctions()) {
        wxMessageBox("Error: Could not initialize OpenGL function pointers.",
            "OpenGL initialization error", wxOK | wxICON_INFORMATION,
            this);
        return false;
    }

    wxLogDebug("OpenGL version: %s",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    wxLogDebug("OpenGL vendor: %s",
        reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

    GLfloat quadVertices[] = {
        -1.0f, -1.0f, 0.0f, // Bottom-left vertex
        1.0f, -1.0f, 0.0f, // Bottom-right vertex
        -1.0f, 1.0f, 0.0f, // Top-left vertex
        1.0f, 1.0f, 0.0f // Top-right vertex
    };

    GLuint quadVBO, quadVAO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
        GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
        (void*)0);
    glEnableVertexAttribArray(0);

    isOpenGLInitialized = true;
    openGLInitializationTime = std::chrono::high_resolution_clock::now();

    wxCommandEvent evt(wxEVT_OPENGL_INITIALIZED);
    evt.SetEventObject(this);
    ProcessWindowEvent(evt);

    return true;
}

void OpenGLCanvas::updateChannels()
{
    // Update the channels with the current data
    for (int i = 0; i < 4; ++i) {
        updateChannel(i);
    }
}

void OpenGLCanvas::updateChannel(int channelIndex)
{
    Channel& chan = channel[channelIndex];

    printf("Updating channel %d: source=%d, type=%d, filePath=%s\n",
        channelIndex, static_cast<int>(chan.source),
        static_cast<int>(chan.type), chan.filePath.c_str());

    if (chan.source == ChannelSource::None) {
        return; // No channel to update
    }

    if (chan.source == ChannelSource::File && chan.type == ChannelType::Image2D && chan.isReady == false && !chan.filePath.empty()) {
        // Load the texture from the file
        printf("Loading texture from file: %s\n", chan.filePath.c_str());
        ::wxInitAllImageHandlers();
        chan.texture = wxImage(wxString::Format("%s", chan.filePath));
        if (chan.texture.IsOk()) {
            // if (!chan.texture.HasAlpha()) {
            //     chan.texture.InitAlpha(); // Ensure the image has an alpha channel
            // }
            if (chan.textureId == 0) {
                glGenTextures(1, &chan.textureId);
            }
            glBindTexture(GL_TEXTURE_2D, chan.textureId);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, chan.texture.GetWidth(),
                chan.texture.GetHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE,
                chan.texture.GetData());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
            chan.isReady = true; // Mark the channel as ready
            wxLogDebug("Texture loaded from file: %s", chan.filePath);
            // Update the shader program with the new texture
        } else {
            wxLogError("Failed to load texture from file: %s",
                chan.filePath);
        }
    } else if (chan.source == ChannelSource::Microphone) {
        static std::vector<float> micBuffer(44100); // 1 second buffer at 44.1kHz
        static bool micInitialized = false;
#if defined(__APPLE__)
        // Core Audio microphone capture for macOS
        // This is a minimal example that reads a frame of audio data using Core Audio APIs

        static AudioComponentInstance audioUnit = nullptr;

        if (!micInitialized) {
            AudioComponentDescription desc = { 0 };
            desc.componentType = kAudioUnitType_Output;
            desc.componentSubType = kAudioUnitSubType_HALOutput;
            desc.componentManufacturer = kAudioUnitManufacturer_Apple;

            AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
            if (!comp) {
                wxLogError("Core Audio: Could not find HALOutput AudioComponent");
                return;
            }
            if (AudioComponentInstanceNew(comp, &audioUnit) != noErr) {
                wxLogError("Core Audio: Could not create AudioUnit instance");
                return;
            }

            UInt32 enableIO = 1;
            AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Input, 1, &enableIO, sizeof(enableIO));
            enableIO = 0;
            AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Output, 0, &enableIO, sizeof(enableIO));

            AudioStreamBasicDescription asbd = { 0 };
            asbd.mSampleRate = 44100.0;
            asbd.mFormatID = kAudioFormatLinearPCM;
            asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            asbd.mFramesPerPacket = 1;
            asbd.mChannelsPerFrame = 1;
            asbd.mBitsPerChannel = 32;
            asbd.mBytesPerPacket = 4;
            asbd.mBytesPerFrame = 4;

            AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output, 1, &asbd, sizeof(asbd));

            if (AudioUnitInitialize(audioUnit) != noErr) {
                wxLogError("Core Audio: Could not initialize AudioUnit");
                AudioComponentInstanceDispose(audioUnit);
                audioUnit = nullptr;
                return;
            }
            micInitialized = true;
        }

        // Calculate number of samples to read based on framerate
        double sampleRate = 44100.0;
        double frameDuration = deltaTime; // Assuming 60 FPS; you can use your actual FPS
        UInt32 numSamples = static_cast<UInt32>(sampleRate * frameDuration);
        if (numSamples > micBuffer.size())
            numSamples = micBuffer.size();

        AudioBufferList bufferList;
        float tempBuffer[44100] = { 0 };
        bufferList.mNumberBuffers = 1;
        bufferList.mBuffers[0].mNumberChannels = 1;
        bufferList.mBuffers[0].mDataByteSize = numSamples * sizeof(float);
        bufferList.mBuffers[0].mData = tempBuffer;

        UInt32 frames = numSamples;
        AudioTimeStamp timeStamp = { 0 };
        AudioUnitRenderActionFlags flags = 0;
        OSStatus status = AudioUnitRender(audioUnit, &flags, &timeStamp, 1, frames, &bufferList);
        if (status == noErr) {
            std::memcpy(micBuffer.data(), tempBuffer, numSamples * sizeof(float));
        } else {
            wxLogError("Core Audio: AudioUnitRender failed (%d)", (int)status);
            std::fill(micBuffer.begin(), micBuffer.begin() + numSamples, 0.0f);
        }

#endif
#if defined(_WIN32)
        // Windows microphone capture using WASAPI or similar APIs
        // This is a minimal example that reads a frame of audio data using WASAPI APIs
        // You would need to implement the WASAPI initialization and reading logic here
        // This is a placeholder for actual implementation
        wxLogError("Microphone capture on Windows is not implemented yet.");
#endif
#if defined(__linux__)
        // Example using PortAudio for microphone capture (pseudo-code)
        // You must link with PortAudio and handle initialization elsewhere

        if (!micInitialized) {
            // Initialize PortAudio and open stream (not shown)
            // Store stream handle in channel or static/global variable
            micInitialized = true;
        }

        // Read audio samples into micBuffer (pseudo-code)
        // pa_stream->read(micBuffer.data(), micBuffer.size());
#endif
        // Upload as a 1D texture
        if (chan.textureId == 0) {
            glGenTextures(1, &chan.textureId);
        }
        glActiveTexture(GL_TEXTURE0 + channelIndex);
        glBindTexture(GL_TEXTURE_1D, chan.textureId);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, numSamples, 0, GL_RED, GL_FLOAT, micBuffer.data());
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_1D, 0);
        chan.isReady = true; // Mark the channel as ready

    } else if (chan.source == ChannelSource::Camera) {
#if defined(__APPLE__)
        // macOS camera capture using AVFoundation
        wxLogError("Camera capture on macOS is not implemented yet.");
#endif
#if defined(_WIN32)
        // Windows camera capture using DirectShow or Media Foundation (pseudo-code)
        // You would need to implement the DirectShow/Media Foundation initialization and reading logic here
        wxLogError("Camera capture on Windows is not implemented yet.");
#endif
#if defined(__linux__)
        // Linux camera capture using V4L2 or GStreamer (pseudo-code)
        // You would need to implement the V4L2/GStreamer initialization and reading logic here
        wxLogError("Camera capture on Linux is not implemented yet.");
#endif
    }
}

void OpenGLCanvas::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    wxPoint mousePos = ScreenToClient(wxGetMousePosition());
    float mouseX = static_cast<float>(mousePos.x);
    float mouseY = static_cast<float>(mousePos.y);
    bool mouseDown = wxGetMouseState().LeftIsDown();

    // Populate date/time for iDate uniform
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    float year = static_cast<float>(1900 + localTime->tm_year);
    float month = static_cast<float>(1 + localTime->tm_mon);
    float day = static_cast<float>(localTime->tm_mday);
    float secondsToday = static_cast<float>(
        localTime->tm_hour * 3600 + localTime->tm_min * 60 + localTime->tm_sec);

    if (!isOpenGLInitialized) {
        return;
    }

    SetCurrent(*openGLContext);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    GLint depthFuncValue;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFuncValue);
    glClearDepth(depthFuncValue == GL_LESS ? 1.0f : 0.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (shaderProgram.shaderProgram.has_value()) {
        glUseProgram(shaderProgram.shaderProgram.value());

        auto viewPortSize = GetSize() * GetContentScaleFactor();

        glUniform3f(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                        "iResolution"),
            static_cast<float>(viewPortSize.x),
            static_cast<float>(viewPortSize.y), 0.0f);

        glUniform1f(
            glGetUniformLocation(shaderProgram.shaderProgram.value(), "iTime"),
            elapsedSeconds);

        // X uniform float     iTimeDelta;            // render time (in seconds)
        // X  uniform float     iFrameRate;            // shader frame rate
        // X  uniform int       iFrame;                // shader playback frame
        // X uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
        // X  uniform vec4      iDate;
        //  uniform float     iChannelTime[4];       // channel playback time (in seconds)
        //  uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
        //  uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube

        glUniform4f(
            glGetUniformLocation(shaderProgram.shaderProgram.value(), "iDate"),
            year, month, day, secondsToday);

        glUniform1i(
            glGetUniformLocation(shaderProgram.shaderProgram.value(), "iFrame"),
            frameCount);

        glUniform1f(
            glGetUniformLocation(shaderProgram.shaderProgram.value(), "iFrameRate"),
            1.0f / deltaTime); // Assuming 60 FPS for simplicity

        glUniform1f(
            glGetUniformLocation(shaderProgram.shaderProgram.value(), "iTimeDelta"),
            deltaTime);

        glUniform4f(
            glGetUniformLocation(shaderProgram.shaderProgram.value(), "iMouse"),
            mouseX, mouseY, mouseDown ? mouseX : 0.0f, mouseDown ? mouseY : 0.0f);

        for (int channelIndex = 0; channelIndex < 4; ++channelIndex) {
            Channel& chan = channel[channelIndex];
            if (chan.isReady != true) {
                continue;
            }

            if (chan.type == ChannelType::Image2D) {
                glUniform1i(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                                ("iChannel" + std::to_string(channelIndex)).c_str()),
                    channelIndex);
                glActiveTexture(GL_TEXTURE0 + channelIndex);
                glBindTexture(GL_TEXTURE_2D, chan.textureId);
                glUniform1f(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                                ("iChannelTime[" + std::to_string(channelIndex) + "]").c_str()),
                    chan.speed * elapsedSeconds);
                glUniform3f(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                                ("iChannelResolution[" + std::to_string(channelIndex) + "]").c_str()),
                    static_cast<float>(chan.texture.GetWidth()),
                    static_cast<float>(chan.texture.GetHeight()), 1.0f);
            } else if (chan.type == ChannelType::ImageCube) {
            } else if (chan.type == ChannelType::Audio) {
                glUniform1i(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                                ("iChannel" + std::to_string(channelIndex)).c_str()),
                    channelIndex);
                glActiveTexture(GL_TEXTURE0 + channelIndex);
                glBindTexture(GL_TEXTURE_1D, chan.textureId);
                glUniform1f(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                                ("iChannelTime[" + std::to_string(channelIndex) + "]").c_str()),
                    chan.speed * elapsedSeconds);
                glUniform3f(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                                ("iChannelResolution[" + std::to_string(channelIndex) + "]").c_str()),
                    static_cast<float>(chan.width), static_cast<float>(chan.height), 1.0f);
            }
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    SwapBuffers();
    frameCount++;
}

void OpenGLCanvas::OnSize(wxSizeEvent& event)
{
    bool firstApperance = IsShownOnScreen() && !isOpenGLInitialized;

    if (firstApperance) {
        InitializeOpenGL();
    }

    if (isOpenGLInitialized) {
        SetCurrent(*openGLContext);

        auto viewPortSize = event.GetSize() * GetContentScaleFactor();
        glViewport(0, 0, viewPortSize.x, viewPortSize.y);
    }

    event.Skip();
}

void OpenGLCanvas::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    if (isOpenGLInitialized) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - openGLInitializationTime);
        deltaTime = elapsedSeconds - (duration.count() / 1000.0f);
        elapsedSeconds = duration.count() / 1000.0f;
        Refresh(false);
    }
}