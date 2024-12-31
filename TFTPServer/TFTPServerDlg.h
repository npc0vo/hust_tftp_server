
// TFTPServerDlg.h: 头文件
//

#pragma once


// CTFTPServerDlg 对话框
class CTFTPServerDlg : public CDialogEx
{
// 构造
public:
	CTFTPServerDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TFTPSERVER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CEdit m_edit1;
	CString logstr;
	CWinThread* WorkThread;
	int done;
	afx_msg void OnBnClickedButton1();
	void AddLog(CString s);
	CMFCEditBrowseCtrl FilepathEdit;
	CString rootpath;
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedCancel();
	afx_msg LRESULT UpdateLog(WPARAM waram,LPARAM lparam);
};
