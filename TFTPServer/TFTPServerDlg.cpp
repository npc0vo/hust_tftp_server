
// TFTPServerDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "TFTPServer.h"
#include "TFTPServerDlg.h"
#include "afxdialogex.h"
#include "util.h"
#include "afxwin.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CTFTPServerDlg 对话框



CTFTPServerDlg::CTFTPServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TFTPSERVER_DIALOG, pParent)
	, logstr(_T(""))
	, rootpath(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTFTPServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_LOG, m_edit1);
	DDX_Text(pDX, IDC_EDIT_LOG, logstr);
	DDX_Control(pDX, IDC_MFCEDITBROWSE1, FilepathEdit);
	DDX_Text(pDX, IDC_MFCEDITBROWSE1, rootpath);
}

BEGIN_MESSAGE_MAP(CTFTPServerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_QUERYDRAGICON()
	ON_MESSAGE(WM_ADDLOG,&CTFTPServerDlg::UpdateLog)
	ON_BN_CLICKED(IDC_BUTTON1, &CTFTPServerDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CTFTPServerDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDCANCEL, &CTFTPServerDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// CTFTPServerDlg 消息处理程序

BOOL CTFTPServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	SetTimer(1, 2000, nullptr);
	FILE* file = fopen(LOG_PATH, "w");
	if (file != NULL) {
		fclose(file);
	}
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CTFTPServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CTFTPServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CTFTPServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

afx_msg LRESULT CTFTPServerDlg::UpdateLog(WPARAM wParam, LPARAM lParam)
{
	CString* message = (CString*)lParam;
	AddLog(*message);

	return 0;
}
void CTFTPServerDlg::AddLog(CString s)
{
	UpdateData(TRUE);

	logstr += s;
	logstr += _T("\r\n");
	
	m_edit1.SetWindowText(logstr);
	m_edit1.LineScroll(m_edit1.GetLineCount());
	UpdateData(FALSE);

	int size = sizeof(logstr);
	if (size > 4096)
	{
		logstr.Empty();
	}

}
void CTFTPServerDlg::OnBnClickedButton1()
{
	
	// TODO: 在此添加控件通知处理程序代码
	//ZHIHUI
	
	UpdateData(TRUE);
	SERVER_PORT = GetDlgItemInt(IDC_EDIT_PORT);
	
	UpdateData(FALSE);
	int length = rootpath.GetLength() + 1;
	strcpy(TFTP_SERVER_ROOT, CT2A(rootpath));
	mylog(LOG_PATH, "\nPORT:%d,ROOTPATH:%s\n", SERVER_PORT,TFTP_SERVER_ROOT);
	if (SERVER_PORT >= 1 && SERVER_PORT <= 65535) {
		CString logMessage;
		logMessage.Format(_T("成功绑定端口: %d"), SERVER_PORT);
		//AddLog(logMessage);
	}
	else {
		MessageBox("port值非法!!!", "Warning");
		return;
	}
	if (rootpath.IsEmpty())
	{
		MessageBox("请初始化服务器根目录", "Warning");
		return;
	}
	else {
		CString logMessage;
		logMessage.Format(_T("成功初始化服务器根目录:%s"), rootpath);
		//AddLog(logMessage);
		mylog(LOG_PATH,"成功初始化");
	}
	Done = 0;
	ThreadParams params;
	params.port = SERVER_PORT;
	params.done = this->done;
	params.dlg = this;
	CWinThread* pWorkThread=AfxBeginThread(startServer,&params);
	this->WorkThread = pWorkThread;

}


void CTFTPServerDlg::OnBnClickedButton2()
{
	// TODO: 在此添加控件通知处理程序代码
	Done = 1;
	WaitForSingleObject(this->WorkThread, INFINITE);


}


void CTFTPServerDlg::OnBnClickedCancel()
{
	// TODO: 在此添加控件通知处理程序代码
	Done = 1;
	WaitForSingleObject(this->WorkThread, INFINITE);
	CDialogEx::OnCancel();
}



void CTFTPServerDlg::OnTimer(UINT_PTR nIDEvent)
{
	
	if (nIDEvent == 1)
	{
		UINT length;
		int nlength;
		// 打开日志文件
		CFile file;
		if (file.Open(LOG_PATH, CFile::modeRead))
		{
			// 获取文件长度
			length = (UINT)file.GetLength();
			if (length > 0)
			{
				// 
				TCHAR* pBuffer = new TCHAR[length + 1];
				if (pBuffer)
				{
					// 读取文件内容
					UINT nRead = file.Read(pBuffer, length * sizeof(TCHAR));
					pBuffer[nRead / sizeof(TCHAR)] = _T('\0'); // 确保字符串以null结尾

					// 将读取的内容转换为CString
					logstr = pBuffer;

					// 释放缓冲区
					delete[] pBuffer;
				}
			}

			// 关闭文件
			file.Close();
		}
		SetDlgItemText(IDC_EDIT_LOG, logstr);
		nlength = length;
		m_edit1.SetSel(nlength, nlength);
	}
}