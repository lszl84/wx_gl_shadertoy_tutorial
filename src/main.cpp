#include <wx/settings.h>
#include <wx/splitter.h>
#include <wx/stc/stc.h>
#include <wx/wx.h>

#include "channel.h"
#include "openglcanvas.h"

constexpr size_t IndentWidth = 4;

class MyApp : public wxApp {
public:
    MyApp() { }
    bool OnInit() wxOVERRIDE;
};

class MyFrame : public wxFrame {
public:
    MyFrame(const wxString& title);

    void channelChanged(uint channelIndex);

private:
    OpenGLCanvas* openGLCanvas { nullptr };
    wxStyledTextCtrl* textCtrl { nullptr };
    wxTextCtrl* logTextCtrl { nullptr };

    void OnTextChange(wxStyledTextEvent& event);
    void OnCharAdded(wxStyledTextEvent& event);
    void OnOpenGLInitialized(wxCommandEvent& event);

    void BuildShaderProgram();
    void StylizeTextCtrl();

    void OnSize(wxSizeEvent& event);
};

class ChannelFrame : public wxFrame {
public:
    ChannelFrame(wxWindow* parent, Channel& channel, uint channelIndex)
        : wxFrame(parent, wxID_ANY, "Channel Setup", wxDefaultPosition, wxSize(400, 300))
        , m_channel(channel)
        , m_channelIndex(channelIndex)
    {
        m_parent = dynamic_cast<MyFrame*>(parent);
        auto* panel = new wxPanel(this);
        auto* sizer = new wxBoxSizer(wxVERTICAL);

        printf("Editing channel %d: source=%d, type=%d, filePath=%s\n",
            channelIndex, static_cast<int>(channel.source),
            static_cast<int>(channel.type), channel.filePath.c_str());
        // Channel type
        auto* typeLabel = new wxStaticText(panel, wxID_ANY, "Channel Type:");
        wxArrayString types;
        types.Add("None");
        types.Add("Texture2D");
        types.Add("Cubemap");
        types.Add("Audio");
        m_typeChoice = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, types);
        int typeIndex = static_cast<int>(channel.type);
        if (typeIndex < 0 || typeIndex >= static_cast<int>(types.GetCount())) {
            typeIndex = 0;
        }
        m_typeChoice->SetSelection(typeIndex);
        sizer->Add(typeLabel, 0, wxLEFT | wxTOP, FromDIP(8));
        sizer->Add(m_typeChoice, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));

        // Input source selection
        auto* inputSourceLabel = new wxStaticText(panel, wxID_ANY, "Input Source:");

        // We'll fill this based on channel type below
        m_inputSourceChoice = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);

        // Helper lambda to update input sources based on channel type
        auto updateInputSources = [this, &channel]() {
            wxArrayString inputSources;
            int typeSel = m_typeChoice->GetSelection();
            if (typeSel == 3) { // Audio
                inputSources.Add("File");
                inputSources.Add("Microphone");
            } else if (typeSel == 1) { // Texture2D
                inputSources.Add("File");
                inputSources.Add("Camera");
            } else if (typeSel == 2) { // Cubemap
                inputSources.Add("File");
            } else {
                inputSources.Add("None");
            }
            m_inputSourceChoice->Set(inputSources);

            // Try to preserve previous selection if possible, else select first
            int prevSel = 0;
            if (typeSel == static_cast<int>(channel.type)) {
                // Map channel.source to the correct index in inputSources
                if (typeSel == 3) { // Audio
                    if (channel.source == ChannelSource::File)
                        prevSel = 0;
                    else if (channel.source == ChannelSource::Microphone)
                        prevSel = 1;
                    else
                        prevSel = 0;
                } else if (typeSel == 1) { // Texture2D
                    if (channel.source == ChannelSource::File)
                        prevSel = 0;
                    else if (channel.source == ChannelSource::Camera)
                        prevSel = 1;
                    else
                        prevSel = 0;
                } else if (typeSel == 2) { // Cubemap
                    prevSel = 0; // Only "File"
                } else {
                    prevSel = 0;
                }
            }
            m_inputSourceChoice->SetSelection(prevSel);
        };

        sizer->Add(inputSourceLabel, 0, wxLEFT, FromDIP(8));
        sizer->Add(m_inputSourceChoice, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));

        // Texture path and speed fields (shown only if "File" is selected)
        m_filePanel = new wxPanel(panel);
        auto* filePanelSizer = new wxBoxSizer(wxVERTICAL);

        // Texture path
        auto* texLabel = new wxStaticText(m_filePanel, wxID_ANY, "Texture Path:");
        auto* texPathSizer = new wxBoxSizer(wxHORIZONTAL);
        m_texCtrl = new wxTextCtrl(m_filePanel, wxID_ANY, wxString::FromUTF8(channel.filePath));
        auto* fileBtn = new wxButton(m_filePanel, wxID_ANY, "...", wxDefaultPosition, wxSize(FromDIP(30), -1));
        texPathSizer->Add(m_texCtrl, 1, wxEXPAND);
        texPathSizer->Add(fileBtn, 0, wxLEFT, FromDIP(5));
        fileBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            wxFileDialog dlg(this, "Select Texture File", "", "", "All Files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (dlg.ShowModal() == wxID_OK) {
                m_texCtrl->SetValue(dlg.GetPath());
            }
        });
        filePanelSizer->Add(texLabel, 0, wxLEFT | wxTOP, FromDIP(8));
        filePanelSizer->Add(texPathSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));

        // Speed input
        auto* speedLabel = new wxStaticText(m_filePanel, wxID_ANY, "Speed:");
        m_speedCtrl = new wxTextCtrl(m_filePanel, wxID_ANY, wxString::Format("%.2f", channel.speed));
        filePanelSizer->Add(speedLabel, 0, wxLEFT, FromDIP(8));
        filePanelSizer->Add(m_speedCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));

        m_filePanel->SetSizer(filePanelSizer);
        sizer->Add(m_filePanel, 0, wxEXPAND);

        // OK/Cancel buttons
        auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        auto* okBtn = new wxButton(panel, wxID_OK, "OK");
        auto* cancelBtn = new wxButton(panel, wxID_CANCEL, "Cancel");
        btnSizer->AddStretchSpacer();
        btnSizer->Add(okBtn, 0, wxRIGHT, FromDIP(5));
        btnSizer->Add(cancelBtn, 0);

        sizer->Add(btnSizer, 0, wxEXPAND | wxALL, FromDIP(8));

        panel->SetSizer(sizer);

        okBtn->Bind(wxEVT_BUTTON, &ChannelFrame::OnOK, this);
        cancelBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });

        updateInputSources();
        UpdateFilePanelVisibility();

        // Bind input source change event
        m_inputSourceChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            UpdateFilePanelVisibility();
        });

        // Bind channel type change event to update input sources
        m_typeChoice->Bind(wxEVT_CHOICE, [this, updateInputSources](wxCommandEvent&) {
            updateInputSources();
            UpdateFilePanelVisibility();
        });

        Centre();
    }

private:
    Channel& m_channel;
    uint m_channelIndex;
    wxTextCtrl* m_texCtrl = nullptr;
    wxChoice* m_typeChoice = nullptr;
    wxTextCtrl* m_speedCtrl = nullptr;
    wxChoice* m_inputSourceChoice = nullptr;
    wxPanel* m_filePanel = nullptr;
    MyFrame* m_parent = nullptr;

    void UpdateFilePanelVisibility()
    {
        bool show = (m_inputSourceChoice->GetSelection() == 0) && (m_typeChoice->GetSelection() != 0); // "File"
        m_filePanel->Show(show);
        Layout();
        Fit();
    }

    void OnOK(wxCommandEvent&)
    {
        m_channel.type = static_cast<ChannelType>(m_typeChoice->GetSelection());
        int sourceSel = m_inputSourceChoice->GetSelection();
        if (m_channel.type == ChannelType::None) {
            m_channel.source = ChannelSource::None;
        } else if (sourceSel == 0) { // "File"
            m_channel.source = ChannelSource::File;
        } else if (sourceSel == 1) { // "Microphone" or "Camera"
            m_channel.source = (m_channel.type == ChannelType::Audio) ? ChannelSource::Microphone : ChannelSource::Camera;
        } else {
            m_channel.source = ChannelSource::None; // Default case
        }
        if (m_inputSourceChoice->GetSelection() == 0) { // "File"
            m_channel.filePath = m_texCtrl->GetValue().ToStdString();
            m_channel.speed = std::stof(m_speedCtrl->GetValue().ToStdString());
            printf("Channel %d updated: source=%d, type=%d, filePath=%s, speed=%.2f\n",
                m_channelIndex, static_cast<int>(m_channel.source),
                static_cast<int>(m_channel.type), m_channel.filePath.c_str(),
                m_channel.speed);
        }
        Close();
        m_parent->channelChanged(m_channelIndex);
    }
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    MyFrame* frame = new MyFrame("Hello OpenGL");
    frame->Show(true);

    return true;
}

constexpr auto ShaderHeader = R"(#version 330 core
// Based on
// https://www.shadertoy.com/view/33cGDj
// modified to correct uninitialised variables
// and other minor issues.
uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform float     iTime;                 // shader playback time (in seconds)
uniform float     iTimeDelta;            // render time (in seconds)
uniform float     iFrameRate;            // shader frame rate
uniform int       iFrame;                // shader playback frame
uniform float     iChannelTime[4];       // channel playback time (in seconds)
uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
uniform vec4      iDate;                 // (year, month, day, time in seconds)

out vec4 FragColor;

void mainImage( out vec4 fragColor, in vec2 fragCoord );

void main()
{
    mainImage(FragColor, gl_FragCoord.xy);
}
)";

constexpr auto InitialShaderText = R"(
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	float i = 0;
	float d = 0;
	float z = fract(dot(fragCoord,sin(fragCoord)))-0.5;
	vec4 o = vec4(0,0,0,1);
	vec4 p = vec4(0,0,0,1);
	for(vec2 r = iResolution.xy; ++i < 77.; z += .6*d) {
		p = vec4(z*normalize(vec3(fragCoord-.5*r,r.y)),.1*iTime);
		p.z += iTime;
		fragColor = p;
		p.xy *= mat2(cos(2.+fragColor.z+vec4(0,11,33,0)));
		p.xy *= mat2(cos(fragColor+vec4(0,11,33,0)));
		fragColor = (1.+sin(.5*fragColor.z+length(p-fragColor)+vec4(0,4,3,6))) / (.5+2.*dot(fragColor.xy,fragColor.xy));
		p = abs(fract(p)-.5);
		d = abs(min(length(p.xy)-.125,min(p.x,p.y)+0.001))+0.001;
		o += (fragColor.w/d)*fragColor;
	}
	fragColor = tanh(o/2000);
}

)";

MyFrame::MyFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title)
{
    wxGLAttributes vAttrs;
    vAttrs.PlatformDefaults().Defaults().EndList();

    if (wxGLCanvas::IsDisplaySupported(vAttrs)) {
        wxSplitterWindow* mainSplitter = new wxSplitterWindow(
            this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
        openGLCanvas = new OpenGLCanvas(mainSplitter, vAttrs);

        // Left panel with vertical layout: textCtrl, channel buttons, logPanel
        wxPanel* leftPanel = new wxPanel(mainSplitter);
        auto leftSizer = new wxBoxSizer(wxVERTICAL);

        textCtrl = new wxStyledTextCtrl(leftPanel);
        StylizeTextCtrl();
        leftSizer->Add(textCtrl, 1, wxEXPAND);

        // Channel buttons in a horizontal row
        auto channelButtons = new wxPanel(leftPanel);
        auto channelSizer = new wxBoxSizer(wxHORIZONTAL);
        auto channel0Button = new wxButton(channelButtons, wxID_ANY, "Channel 0");
        auto channel1Button = new wxButton(channelButtons, wxID_ANY, "Channel 1");
        auto channel2Button = new wxButton(channelButtons, wxID_ANY, "Channel 2");
        auto channel3Button = new wxButton(channelButtons, wxID_ANY, "Channel 3");

        // Channel data storage
        // static Channel channels[4];

        // Lambda to show the ChannelFrame for a given channel index
        auto showChannelSetup = [this](int channelIdx) {
            ChannelFrame* dlg = new ChannelFrame(this, openGLCanvas->channel[channelIdx], channelIdx);
            dlg->Show();
        };

        channel0Button->Bind(wxEVT_BUTTON, [showChannelSetup](wxCommandEvent&) { showChannelSetup(0); });
        channel1Button->Bind(wxEVT_BUTTON, [showChannelSetup](wxCommandEvent&) { showChannelSetup(1); });
        channel2Button->Bind(wxEVT_BUTTON, [showChannelSetup](wxCommandEvent&) { showChannelSetup(2); });
        channel3Button->Bind(wxEVT_BUTTON, [showChannelSetup](wxCommandEvent&) { showChannelSetup(3); });
        channelSizer->Add(channel0Button, 1, wxEXPAND | wxALL, FromDIP(5));
        channelSizer->Add(channel1Button, 1, wxEXPAND | wxALL, FromDIP(5));
        channelSizer->Add(channel2Button, 1, wxEXPAND | wxALL, FromDIP(5));
        channelSizer->Add(channel3Button, 1, wxEXPAND | wxALL, FromDIP(5));
        channelButtons->SetSizer(channelSizer);
        channelButtons->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        leftSizer->Add(channelButtons, 0, wxEXPAND);

        // Log panel
        auto logPanel = new wxPanel(leftPanel);
        auto logLabel = new wxStaticText(logPanel, wxID_ANY, "Compilation Errors:");
        logTextCtrl = new wxTextCtrl(logPanel, wxID_ANY, wxEmptyString, wxDefaultPosition,
            wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

        auto logSizer = new wxBoxSizer(wxVERTICAL);
        logSizer->Add(logLabel, 0, wxEXPAND | wxALL, FromDIP(5));
        logSizer->Add(logTextCtrl, 1, wxEXPAND);

        logPanel->SetSizer(logSizer);
        leftSizer->Add(logPanel, 0, wxEXPAND | wxTOP, FromDIP(5));

        leftPanel->SetSizer(leftSizer);

        this->Bind(wxEVT_OPENGL_INITIALIZED, &MyFrame::OnOpenGLInitialized,
            this);

        mainSplitter->SetSashGravity(0.5);
        mainSplitter->SetMinimumPaneSize(FromDIP(200));
        mainSplitter->SplitVertically(leftPanel, openGLCanvas);

        this->SetSize(FromDIP(wxSize(1200, 600)));
        this->SetMinSize(FromDIP(wxSize(800, 400)));

        this->Bind(wxEVT_STC_CHANGE, &MyFrame::OnTextChange, this);
        this->Bind(wxEVT_STC_CHARADDED, &MyFrame::OnCharAdded, this);
        this->Bind(wxEVT_SIZE, &MyFrame::OnSize, this);
    }
}

void MyFrame::OnCharAdded(wxStyledTextEvent& event)
{
    auto newLine = (textCtrl->GetEOLMode() == wxSTC_EOL_CR) ? 13 : 10;

    // copy the leading whitespace from the previous line to preserve
    // indentation
    if (event.GetKey() == newLine) {
        auto currentLine = textCtrl->LineFromPosition(textCtrl->GetCurrentPos());

        if (currentLine > 0) {
            auto previousLine = textCtrl->GetLine(currentLine - 1);
            size_t characterCountToCopy { 0 };
            for (const auto& character : previousLine) {
                if (character == ' ' || character == '\t') {
                    ++characterCountToCopy;
                } else {
                    break;
                }
            }

            textCtrl->AddText(previousLine.Left(characterCountToCopy));
        }
    }

    // when adding a single closing brace, reduce indentation by one level
    auto nonWhitespaceCharsInLine = textCtrl->GetLine(textCtrl->GetCurrentLine())
                                        .Trim(false)
                                        .Trim(true)
                                        .length();

    if (event.GetKey() == '}' && nonWhitespaceCharsInLine == 1) {
        auto currentIndent = textCtrl->GetLineIndentation(textCtrl->GetCurrentLine());
        textCtrl->SetLineIndentation(textCtrl->GetCurrentLine(),
            currentIndent - IndentWidth);
    }
}

void MyFrame::OnOpenGLInitialized(wxCommandEvent& event)
{
    BuildShaderProgram();
}

void MyFrame::OnTextChange(wxStyledTextEvent& event)
{
    BuildShaderProgram();
}

void MyFrame::BuildShaderProgram()
{
    wxString channelUniforms = wxEmptyString;

    for (int i = 0; i < 4; ++i) {
        const auto& channel = openGLCanvas->channel[i];
        if (channel.type == ChannelType::Image2D) {
            channelUniforms += wxString::Format("uniform sampler2D iChannel%d;\n", i);
        } else if (channel.type == ChannelType::Audio) {
            channelUniforms += wxString::Format("uniform sampler1D iChannel%d;\n", i);
        } else if (channel.type == ChannelType::ImageCube) {
            channelUniforms += wxString::Format("uniform samplerCube iChannel%d;\n", i);
        }
    }
    auto fullshader = ShaderHeader + channelUniforms.ToStdString() + textCtrl->GetText().ToStdString();
    openGLCanvas->CompileCustomFragmentShader(fullshader);
    logTextCtrl->SetValue(fullshader + openGLCanvas->GetShaderBuildLog());
}

wxFont GetMonospacedFont(wxFontInfo&& fontInfo)
{
    const wxString preferredFonts[] = { "Menlo", "Consolas", "Monaco",
        "DejaVu Sans Mono", "Courier New" };

    for (const wxString& fontName : preferredFonts) {
        fontInfo.FaceName(fontName);
        wxFont font(fontInfo);

        if (font.IsOk() && font.IsFixedWidth()) {
            return font;
        }
    }

    fontInfo.Family(wxFONTFAMILY_TELETYPE);
    return wxFont(fontInfo);
}

void MyFrame::StylizeTextCtrl()
{
    textCtrl->StyleClearAll();

    textCtrl->SetTabWidth(IndentWidth);

    textCtrl->SetMarginWidth(0, FromDIP(50));
    textCtrl->SetMarginType(0, wxSTC_MARGIN_NUMBER);

    textCtrl->SetWrapMode(wxSTC_WRAP_WORD);

    textCtrl->SetText(InitialShaderText);

    wxFont fixedFont = GetMonospacedFont(wxFontInfo(14));

    for (size_t n = 0; n < wxSTC_STYLE_MAX; n++) {
        textCtrl->StyleSetFont(n, fixedFont);
    }

    textCtrl->SetLexer(wxSTC_LEX_CPP);

    // Set GLSL keywords
    wxString glslKeywords = wxT("attribute const uniform varying break continue "
                                "do for while if else in out inout true false");
    textCtrl->SetKeyWords(0, glslKeywords);

    textCtrl->StyleSetBold(wxSTC_C_WORD, true);

    if (wxSystemSettings::GetAppearance().IsDark()) {
        textCtrl->StyleSetForeground(
            wxSTC_C_PREPROCESSOR,
            wxColour(168, 70, 20)); // Preprocessor directive color
        textCtrl->StyleSetForeground(wxSTC_C_STRING,
            wxColour(163, 61, 61)); // String color
        textCtrl->StyleSetForeground(wxSTC_C_CHARACTER,
            wxColour(163, 21, 21)); // Char color
        textCtrl->StyleSetForeground(wxSTC_C_COMMENT,
            wxColour(150, 150, 150)); // Comment color
        textCtrl->StyleSetForeground(
            wxSTC_C_COMMENTLINE, wxColour(150, 150, 150)); // Comment line color
        textCtrl->StyleSetForeground(wxSTC_C_WORD,
            wxColour(255, 102, 0)); // Keyword color
        textCtrl->StyleSetForeground(
            wxSTC_C_IDENTIFIER, wxColour(220, 220, 220)); // Identifier color
        textCtrl->StyleSetForeground(wxSTC_C_NUMBER,
            wxColour(183, 101, 81)); // Number color
        textCtrl->StyleSetForeground(wxSTC_C_OPERATOR,
            wxColour(200, 200, 200)); // Operator color
        textCtrl->StyleSetForeground(
            wxSTC_C_DEFAULT,
            wxColour(220, 220, 220)); // Default color for other tokens
    } else {
        textCtrl->StyleSetForeground(
            wxSTC_C_PREPROCESSOR,
            wxColour(168, 70, 20)); // Preprocessor directive color
        textCtrl->StyleSetForeground(wxSTC_C_STRING,
            wxColour(163, 21, 21)); // String color
        textCtrl->StyleSetForeground(wxSTC_C_CHARACTER,
            wxColour(163, 21, 21)); // Char color
        textCtrl->StyleSetForeground(wxSTC_C_COMMENT,
            wxColour(100, 100, 100)); // Comment color
        textCtrl->StyleSetForeground(
            wxSTC_C_COMMENTLINE, wxColour(100, 100, 100)); // Comment line color
        textCtrl->StyleSetForeground(wxSTC_C_WORD,
            wxColour(255, 102, 0)); // Keyword color
        textCtrl->StyleSetForeground(wxSTC_C_IDENTIFIER,
            wxColour(0, 0, 0)); // Identifier color
        textCtrl->StyleSetForeground(wxSTC_C_NUMBER,
            wxColour(163, 21, 21)); // Number color
        textCtrl->StyleSetForeground(wxSTC_C_OPERATOR,
            wxColour(0, 0, 0)); // Operator color
        textCtrl->StyleSetForeground(
            wxSTC_C_DEFAULT,
            wxColour(0, 0, 0)); // Default color for other tokens
    }
}

void MyFrame::OnSize(wxSizeEvent& event)
{
    // a workaround for the OpenGLCanvas not getting the initial size event
    // if contained in wxSplitterWindow
    if (!openGLCanvas->IsOpenGLInitialized() && openGLCanvas->IsShownOnScreen()) {
        openGLCanvas->InitializeOpenGL();

        // we just need one shot for this workaround, so unbind
        this->Unbind(wxEVT_SIZE, &MyFrame::OnSize, this);
    }
    event.Skip();
}

void MyFrame::channelChanged(uint channelIndex)
{
    openGLCanvas->updateChannel(channelIndex);
    BuildShaderProgram();
}