#include "openglcanvas.h"

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

void OpenGLCanvas::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

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

        glUniform2f(glGetUniformLocation(shaderProgram.shaderProgram.value(),
                        "iResolution"),
            static_cast<float>(viewPortSize.x),
            static_cast<float>(viewPortSize.y));

        glUniform1f(
            glGetUniformLocation(shaderProgram.shaderProgram.value(), "iTime"),
            elapsedSeconds);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    SwapBuffers();
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
        elapsedSeconds = duration.count() / 1000.0f;
        Refresh(false);
    }
}