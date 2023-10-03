#include <wx/wx.h>

#include <wx/settings.h>
#include <wx/stc/stc.h>

#include <wx/splitter.h>

#include "openglcanvas.h"

constexpr size_t IndentWidth = 4;

class MyApp : public wxApp
{
public:
    MyApp() {}
    bool OnInit() wxOVERRIDE;
};

class MyFrame : public wxFrame
{
public:
    MyFrame(const wxString &title);

private:
    OpenGLCanvas *openGLCanvas{nullptr};
    wxStyledTextCtrl *textCtrl{nullptr};
    wxTextCtrl *logTextCtrl{nullptr};

    void OnTextChange(wxStyledTextEvent &event);
    void OnCharAdded(wxStyledTextEvent &event);
    void OnOpenGLInitialized(wxCommandEvent &event);

    void BuildShaderProgram();
    void StylizeTextCtrl();

    void OnSize(wxSizeEvent &event);
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    MyFrame *frame = new MyFrame("Hello OpenGL");
    frame->Show(true);

    return true;
}

constexpr auto InitialShaderText = R"(#version 330 core

uniform vec2 iResolution;
uniform float iTime;

out vec4 FragColor;

void mainImage( out vec4 fragColor, in vec2 fragCoord );

void main()
{
    mainImage(FragColor, gl_FragCoord.xy);      
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
}
)";

MyFrame::MyFrame(const wxString &title)
    : wxFrame(nullptr, wxID_ANY, title)
{
    wxGLAttributes vAttrs;
    vAttrs.PlatformDefaults().Defaults().EndList();

    if (wxGLCanvas::IsDisplaySupported(vAttrs))
    {
        wxSplitterWindow *mainSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
        wxSplitterWindow *textSplitter = new wxSplitterWindow(mainSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);

        textCtrl = new wxStyledTextCtrl(textSplitter);
        StylizeTextCtrl();

        auto logPanel = new wxPanel(textSplitter);
        auto logLabel = new wxStaticText(logPanel, wxID_ANY, "Compilation Errors:");
        logTextCtrl = new wxTextCtrl(logPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

        auto logSizer = new wxBoxSizer(wxVERTICAL);
        logSizer->Add(logLabel, 0, wxEXPAND | wxALL, FromDIP(5));
        logSizer->Add(logTextCtrl, 1, wxEXPAND);

        logPanel->SetSizer(logSizer);

        textSplitter->SetSashGravity(1.0);
        textSplitter->SetMinimumPaneSize(FromDIP(50));
        textSplitter->SplitHorizontally(textCtrl, logPanel, -FromDIP(100));

        openGLCanvas = new OpenGLCanvas(mainSplitter, vAttrs);

        this->Bind(wxEVT_OPENGL_INITIALIZED, &MyFrame::OnOpenGLInitialized, this);

        mainSplitter->SetSashGravity(0.5);
        mainSplitter->SetMinimumPaneSize(FromDIP(200));
        mainSplitter->SplitVertically(textSplitter, openGLCanvas);

        this->SetSize(FromDIP(wxSize(1200, 600)));
        this->SetMinSize(FromDIP(wxSize(800, 400)));

        this->Bind(wxEVT_STC_CHANGE, &MyFrame::OnTextChange, this);
        this->Bind(wxEVT_STC_CHARADDED, &MyFrame::OnCharAdded, this);
        this->Bind(wxEVT_SIZE, &MyFrame::OnSize, this);
    }
}

void MyFrame::OnCharAdded(wxStyledTextEvent &event)
{
    auto newLine = (textCtrl->GetEOLMode() == wxSTC_EOL_CR) ? 13 : 10;

    // copy the leading whitespace from the previous line to preserve indentation
    if (event.GetKey() == newLine)
    {
        auto currentLine = textCtrl->LineFromPosition(textCtrl->GetCurrentPos());

        if (currentLine > 0)
        {
            auto previousLine = textCtrl->GetLine(currentLine - 1);
            size_t characterCountToCopy{0};
            for (const auto &character : previousLine)
            {
                if (character == ' ' || character == '\t')
                {
                    ++characterCountToCopy;
                }
                else
                {
                    break;
                }
            }

            textCtrl->AddText(previousLine.Left(characterCountToCopy));
        }
    }

    // when adding a single closing brace, reduce indentation by one level
    auto nonWhitespaceCharsInLine = textCtrl->GetLine(textCtrl->GetCurrentLine()).Trim(false).Trim(true).length();

    if (event.GetKey() == '}' && nonWhitespaceCharsInLine == 1)
    {
        auto currentIndent = textCtrl->GetLineIndentation(textCtrl->GetCurrentLine());
        textCtrl->SetLineIndentation(textCtrl->GetCurrentLine(), currentIndent - IndentWidth);
    }
}

void MyFrame::OnTextChange(wxStyledTextEvent &event)
{

    BuildShaderProgram();
}

void MyFrame::OnOpenGLInitialized(wxCommandEvent &event)
{
    BuildShaderProgram();
}

void MyFrame::BuildShaderProgram()
{
    openGLCanvas->CompileCustomFragmentShader(textCtrl->GetText().ToStdString());
    logTextCtrl->SetValue(openGLCanvas->GetShaderBuildLog());
}

wxFont GetMonospacedFont(wxFontInfo &&fontInfo)
{
    const wxString preferredFonts[] = {"Menlo", "Consolas", "Monaco", "DejaVu Sans Mono", "Courier New"};

    for (const wxString &fontName : preferredFonts)
    {
        fontInfo.FaceName(fontName);
        wxFont font(fontInfo);

        if (font.IsOk() && font.IsFixedWidth())
        {
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

    wxFont fixedFont = GetMonospacedFont(wxFontInfo(13));

    for (size_t n = 0; n < wxSTC_STYLE_MAX; n++)
    {
        textCtrl->StyleSetFont(n, fixedFont);
    }

    textCtrl->SetLexer(wxSTC_LEX_CPP);

    // Set GLSL keywords
    wxString glslKeywords = wxT("attribute const uniform varying break continue do for while if else in out inout true false");
    textCtrl->SetKeyWords(0, glslKeywords);

    textCtrl->StyleSetBold(wxSTC_C_WORD, true);

    if (wxSystemSettings::GetAppearance().IsDark())
    {
        textCtrl->StyleSetForeground(wxSTC_C_PREPROCESSOR, wxColour(168, 70, 20));  // Preprocessor directive color
        textCtrl->StyleSetForeground(wxSTC_C_STRING, wxColour(163, 61, 61));        // String color
        textCtrl->StyleSetForeground(wxSTC_C_CHARACTER, wxColour(163, 21, 21));     // Char color
        textCtrl->StyleSetForeground(wxSTC_C_COMMENT, wxColour(150, 150, 150));     // Comment color
        textCtrl->StyleSetForeground(wxSTC_C_COMMENTLINE, wxColour(150, 150, 150)); // Comment line color
        textCtrl->StyleSetForeground(wxSTC_C_WORD, wxColour(255, 102, 0));          // Keyword color
        textCtrl->StyleSetForeground(wxSTC_C_IDENTIFIER, wxColour(220, 220, 220));  // Identifier color
        textCtrl->StyleSetForeground(wxSTC_C_NUMBER, wxColour(183, 101, 81));       // Number color
        textCtrl->StyleSetForeground(wxSTC_C_OPERATOR, wxColour(200, 200, 200));    // Operator color
        textCtrl->StyleSetForeground(wxSTC_C_DEFAULT, wxColour(220, 220, 220));     // Default color for other tokens
    }
    else
    {
        textCtrl->StyleSetForeground(wxSTC_C_PREPROCESSOR, wxColour(168, 70, 20));  // Preprocessor directive color
        textCtrl->StyleSetForeground(wxSTC_C_STRING, wxColour(163, 21, 21));        // String color
        textCtrl->StyleSetForeground(wxSTC_C_CHARACTER, wxColour(163, 21, 21));     // Char color
        textCtrl->StyleSetForeground(wxSTC_C_COMMENT, wxColour(100, 100, 100));     // Comment color
        textCtrl->StyleSetForeground(wxSTC_C_COMMENTLINE, wxColour(100, 100, 100)); // Comment line color
        textCtrl->StyleSetForeground(wxSTC_C_WORD, wxColour(255, 102, 0));          // Keyword color
        textCtrl->StyleSetForeground(wxSTC_C_IDENTIFIER, wxColour(0, 0, 0));        // Identifier color
        textCtrl->StyleSetForeground(wxSTC_C_NUMBER, wxColour(163, 21, 21));        // Number color
        textCtrl->StyleSetForeground(wxSTC_C_OPERATOR, wxColour(0, 0, 0));          // Operator color
        textCtrl->StyleSetForeground(wxSTC_C_DEFAULT, wxColour(0, 0, 0));           // Default color for other tokens
    }
}

void MyFrame::OnSize(wxSizeEvent &event)
{
    // a workaround for the OpenGLCanvas not getting the initial size event
    // if contained in wxSplitterWindow
    if (!openGLCanvas->IsOpenGLInitialized() && openGLCanvas->IsShownOnScreen())
    {
        openGLCanvas->InitializeOpenGL();

        // we just need one shot for this workaround, so unbind
        this->Unbind(wxEVT_SIZE, &MyFrame::OnSize, this);
    }
    event.Skip();
}