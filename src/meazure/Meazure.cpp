/*
 * Copyright 2001, 2004, 2011 C Thing Software
 *
 * This file is part of Meazure.
 * 
 * Meazure is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Meazure is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Meazure.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include <htmlhelp.h>
#include "Meazure.h"
#include "MainFrm.h"
#include "Colors.h"
#include "VersionInfo.h"
#include "PositionLogMgr.h"
#include "ProfileMgr.h"
#include "ToolMgr.h"
#include "ScreenMgr.h"
#include "CommandLineInfo.h"
#include "Hooks/Hooks.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// Shared global variables. Used by the Mouse Hook
// DLL to route pointer movement messages back to this app.

#pragma data_seg("MeaMainShared")
volatile HWND   g_meaMainWnd = NULL;
#pragma data_seg()

#pragma comment(linker, "/section:MeaMainShared,rws")


// The application class object.

CMeazureApp theApp;


BEGIN_MESSAGE_MAP(CMeazureApp, CWinApp)
    //{{AFX_MSG_MAP(CMeazureApp)
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
        // NOTE - the ClassWizard will add and remove mapping macros here.
        //    DO NOT EDIT what you see in these blocks of generated code!
    //}}AFX_MSG_MAP
    ON_THREAD_MESSAGE(MEA_MOUSE_MSG, OnMouseHook)
END_MESSAGE_MAP()


CMeazureApp::CMeazureApp() : CWinApp()
{
}


BOOL CMeazureApp::InitInstance()
{
    // Parse the command line to see if we are being started with
    // a profile.
    //
    MeaCommandLineInfo cmdLineInfo;
    ParseCommandLine(cmdLineInfo);

    // Ensure that only one instance of the app is running.
    //
    if (InterlockedExchange(reinterpret_cast<LPLONG>(const_cast<HWND*>(&g_meaMainWnd)), reinterpret_cast<LONG>(g_meaMainWnd)) != NULL) {

        // Bring the window forward if obscured.

        ::SetForegroundWindow(g_meaMainWnd);

        // Make the window visible if minimized.

        if (IsIconic(g_meaMainWnd)) {
            ShowWindow(g_meaMainWnd, SW_RESTORE);
        }

        // Load the command line file, if any

        if (!cmdLineInfo.m_strFileName.IsEmpty()) {
            COPYDATASTRUCT cds;

            cds.cbData = ID_MEA_COPYDATA;
            cds.dwData = static_cast<DWORD>(cmdLineInfo.m_strFileName.GetLength() + 1) * sizeof(TCHAR);
            cds.lpData = reinterpret_cast<LPVOID>(const_cast<LPTSTR>(static_cast<LPCTSTR>(cmdLineInfo.m_strFileName)));
            ::SendMessage(g_meaMainWnd, WM_COPYDATA, NULL, reinterpret_cast<LPARAM>(&cds));
        }

        return FALSE;
    }

    // Tell the installer that the program is running so that it cannot
    // be uninstalled out from under it.
    //
    CreateMutex(NULL, FALSE, "MeazureInstallerMutex");

    // Initialize colors
    //
    MeaColors::Initialize();

    // App appearance
    //
    InitCommonControls();       // To support XP visual styles, if available
    CWinApp::InitInstance();
    
    // Set the base key for saving/restoring state in the registry
    //
    SetRegistryKey(_T("C Thing Software"));

    // To create the main window, this code creates a new frame window
    // object and then sets it as the application's main window object.
    //
    CMainFrame* pFrame = new CMainFrame;
    m_pMainWnd = pFrame;

    // Tell frame about command line file, if any
    //
    pFrame->SetCommandLineFile(cmdLineInfo.m_strFileName);

    // Create and load the frame with its resources
    //
    pFrame->LoadFrame(IDR_MAINFRAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        NULL, NULL);

    InterlockedExchange(reinterpret_cast<LPLONG>(const_cast<HWND*>(&g_meaMainWnd)), reinterpret_cast<LONG>(pFrame->m_hWnd));

    if (pFrame->IsNewInstall()) {
        OnAppAbout();
    }

    // Set the help file pathname by replacing the old WinHelp
    // .hlp with the new HTML Help .chm.
    TCHAR *cptr = const_cast<TCHAR*>(_tcsstr(m_pszHelpFilePath, _T(".hlp")));
    if (cptr == NULL) {
        cptr = const_cast<TCHAR*>(_tcsstr(m_pszHelpFilePath, _T(".HLP")));
    }
    if (cptr != NULL) {
        _tcscpy_s(cptr, 5, ".chm");
    }

    return TRUE;
}


int CMeazureApp::ExitInstance() 
{
    MeaPositionLogMgr::DestroyInstance();
    MeaProfileMgr::DestroyInstance();
    MeaToolMgr::DestroyInstance();
    MeaUnitsMgr::DestroyInstance();
    MeaScreenMgr::DestroyInstance();
    
    return CWinApp::ExitInstance();
}


void CMeazureApp::OnMouseHook(WPARAM wParam, LPARAM lParam)
{
    static_cast<CMainFrame*>(m_pMainWnd)->GetView()->OnMouseHook(wParam, lParam);
}


BOOL CMeazureApp::SaveAllModified()
{
    return MeaPositionLogMgr::Instance().SaveIfModified();
}


void CMeazureApp::WinHelp(DWORD dwData, UINT nCmd)
{

    switch (nCmd) {
    case HELP_CONTEXT:
        TRACE("Context: %s, 0x%X\n", m_pszHelpFilePath, dwData);
        ::HtmlHelp(*AfxGetMainWnd(), m_pszHelpFilePath, HH_HELP_CONTEXT, dwData);
        break;
    case HELP_INDEX:
        TRACE("Index: %s, 0x%X\n", m_pszHelpFilePath, dwData);
        ::HtmlHelp(*AfxGetMainWnd(), m_pszHelpFilePath, HH_DISPLAY_INDEX, dwData);
        break;
    case HELP_COMMAND:      // Using this for Search
        {
            HH_FTS_QUERY q;     // A blank query is required

            q.cbStruct          = sizeof(q);    // Sizeof structure in bytes.
            q.fUniCodeStrings   = FALSE;        // TRUE if all strings are unicode.
            q.pszSearchQuery    = NULL;         // String containing the search query.
            q.iProximity        = HH_FTS_DEFAULT_PROXIMITY; // Word proximity - only one option
            q.fStemmedSearch    = FALSE;        // TRUE for StemmedSearch only.
            q.fTitleOnly        = FALSE;        // TRUE for Title search only.
            q.fExecute          = FALSE;        // TRUE to initiate the search.
            q.pszWindow         = NULL;         // Window to display in
            TRACE("Search: %s, 0x%X\n", m_pszHelpFilePath, dwData);
            ::HtmlHelp(*AfxGetMainWnd(), m_pszHelpFilePath, HH_DISPLAY_SEARCH, reinterpret_cast<DWORD>(&q));
        }
        break;
    case HELP_FINDER:
    default:
        TRACE("Contents: %s, 0x%X\n", m_pszHelpFilePath, dwData);
        ::HtmlHelp(*AfxGetMainWnd(), m_pszHelpFilePath, HH_DISPLAY_TOC, dwData);
        break;
    }
}


//*************************************************************************
// AboutDlg
//*************************************************************************


/// Application About dialog.
///
class CAboutDlg : public CDialog
{
public:
    /// Constructs the About dialog. To display the dialog, call the
    /// DoModal method.
    ///
    CAboutDlg();


    /// Called when the dialog is about to be displayed.
    ///
    /// @return TRUE to continue the display of the dialog.
    ///
    virtual BOOL OnInitDialog();

    //{{AFX_DATA(CAboutDlg)
    enum { IDD = IDD_ABOUTBOX };
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CAboutDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    //}}AFX_VIRTUAL

    /// @fn DoDataExchange(CDataExchange* pDX)
    /// Called to perform Dynamic Data Exchange (DDX) for the dialog.
    /// @param pDX  [in] DDX operation object.

protected:
    //{{AFX_MSG(CAboutDlg)
    afx_msg void OnHomeUrl();
    afx_msg void OnFeedbackEmail();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    //}}AFX_MSG

    /// @fn OnHomeUrl()
    /// Called when the home page URL text link is clicked. Brings up
    /// a browser pointed at the home page.

    /// @fn OnFeedbackEmail()
    /// Called when the feedback email text link is clicked. Brings up
    /// the email client.

    /// @fn OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
    /// Called when a control on the dialog needs to have its color set.
    /// This method sets the appropriate colors for the links.
    /// @param pDC          [in] DC for the control.
    /// @param pWnd         [in] Window for the control.
    /// @param nCtlColor    [in] Control type.
    /// @return Brush with which to draw the control.

    /// @fn OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
    /// Called to set the cursor on the dialog.
    /// @param pWnd     [in] Window on which to set the cursor.
    /// @param nHitTest [in] What are we testing.
    /// @param message  [in] Message for cursor.
    /// @return 0 to continue processing.

    DECLARE_MESSAGE_MAP()

private:
    static bool     m_visited;          ///< Indicates if the home page has been visited.
    static bool     m_emailed;          ///< Indicates if the email link has been visited.
    static COLORREF m_unvisitedColor;   ///< Link color before it has been visited.
    static COLORREF m_visitedColor;     ///< Link color after it has been visited.

    CFont           m_titleFont;        ///< Font used for the dialog title.
    CFont           m_linkFont;         ///< Font used for the link text.
    HCURSOR         m_linkCursor;       ///< Hand cursor to indicate a link.
};


bool CAboutDlg::m_visited = false;
bool CAboutDlg::m_emailed = false;
COLORREF CAboutDlg::m_unvisitedColor    = RGB(  0, 0, 255); // Blue
COLORREF CAboutDlg::m_visitedColor      = RGB(128, 0, 128); // Purple


BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
    //{{AFX_MSG_MAP(CAboutDlg)
    ON_BN_CLICKED(IDC_HOME_URL, OnHomeUrl)
    ON_BN_CLICKED(IDC_FEEDBACK_EMAIL, OnFeedbackEmail)
    ON_WM_CTLCOLOR()
    ON_WM_SETCURSOR()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD), m_linkCursor(NULL)
{
    //{{AFX_DATA_INIT(CAboutDlg)
    //}}AFX_DATA_INIT
}


BOOL CAboutDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    //
    // Set title font and append the version number
    //
    LOGFONT lf;
    CString str;

    CStatic *titleStr   = static_cast<CStatic*>(GetDlgItem(IDC_PROGRAM_TITLE));
    CStatic *versionStr = static_cast<CStatic*>(GetDlgItem(IDC_PROGRAM_VERSION));
    CStatic *buildStr   = static_cast<CStatic*>(GetDlgItem(IDC_PROGRAM_BUILD));

    versionStr->GetWindowText(str);
    str += g_versionInfo.GetProductVersion();
    versionStr->SetWindowText(str);

    CString bstr;
    buildStr->GetWindowText(str);
    bstr.Format(_T("%d"), g_versionInfo.GetProductBuild());
    str += bstr;
    buildStr->SetWindowText(str);

    CFont *font = titleStr->GetFont();
    font->GetLogFont(&lf);
    lf.lfWeight = FW_BOLD;
    m_titleFont.CreateFontIndirect(&lf);
    titleStr->SetFont(&m_titleFont);

    //
    // Set the links font
    //
    CStatic *url = static_cast<CStatic*>(GetDlgItem(IDC_HOME_URL));
    CStatic *email = static_cast<CStatic*>(GetDlgItem(IDC_FEEDBACK_EMAIL));
    font = url->GetFont();
    font->GetLogFont(&lf);
    lf.lfUnderline = TRUE;
    m_linkFont.CreateFontIndirect(&lf);
    url->SetFont(&m_linkFont);
    email->SetFont(&m_linkFont);

    m_linkCursor = AfxGetApp()->LoadCursor(IDC_HAND_CUR);

    return TRUE;
}


void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAboutDlg)
    //}}AFX_DATA_MAP
}


HBRUSH CAboutDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    if (pWnd->GetDlgCtrlID() == IDC_HOME_URL) {
        pDC->SetTextColor(m_visited ? m_visitedColor : m_unvisitedColor);
        pDC->SetBkMode(TRANSPARENT);
    }

    if (pWnd->GetDlgCtrlID() == IDC_FEEDBACK_EMAIL) {
        pDC->SetTextColor(m_emailed ? m_visitedColor : m_unvisitedColor);
        pDC->SetBkMode(TRANSPARENT);
    }

    return hbr;
}


BOOL CAboutDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
    CPoint point;
    CRect  rect1, rect2;

    //
    // Test if cursor is over control
    //
    if (nHitTest == HTCLIENT) {
        GetCursorPos(&point);
        GetDlgItem(IDC_HOME_URL)->GetWindowRect(&rect1);
        GetDlgItem(IDC_FEEDBACK_EMAIL)->GetWindowRect(&rect2);
        if (rect1.PtInRect(point) || rect2.PtInRect(point)) {
            ::SetCursor(m_linkCursor);
            return(TRUE);
        }
    }
    
    return CDialog::OnSetCursor(pWnd, nHitTest, message);
}


void CAboutDlg::OnHomeUrl() 
{
    CString url;

    static_cast<CStatic*>(GetDlgItem(IDC_HOME_URL))->GetWindowText(url);
    url = _T("http://") + url;
    HINSTANCE h = ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);

    if (reinterpret_cast<UINT>(h) > 32) {
        m_visited = true;

        Invalidate();
        UpdateWindow();
    } else {
        CString msg;
        msg.Format(IDS_MEA_NOEXEC, static_cast<LPCTSTR>(url));
        MessageBox(msg, NULL, MB_OK | MB_ICONERROR);
    }
}


void CAboutDlg::OnFeedbackEmail() 
{
    CString email;

    static_cast<CStatic*>(GetDlgItem(IDC_FEEDBACK_EMAIL))->GetWindowText(email);
    email = _T("mailto:") + email;
    HINSTANCE h = ShellExecute(NULL, _T("open"), email, NULL, NULL, SW_SHOWNORMAL);

    if (reinterpret_cast<UINT>(h) > 32) {
        m_emailed = true;

        Invalidate();
        UpdateWindow();
    } else {
        CString msg;
        msg.Format(IDS_MEA_NOEXEC, static_cast<LPCTSTR>(email));
        MessageBox(msg, NULL, MB_OK | MB_ICONERROR);
    }
}


void CMeazureApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}
