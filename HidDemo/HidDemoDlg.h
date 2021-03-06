
// HidDemoDlg.h: 头文件
//

#pragma once
#include "afxwin.h"
#include "afxcmn.h"


// CHidDemoDlg 对话框
class CHidDemoDlg : public CDialog
{
// 构造
public:
	CHidDemoDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_HIDDEMO_DIALOG };
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
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CEdit m_ce_send;
	CRichEditCtrl m_re_result;
	CEdit m_ce_vendorID;
	CEdit m_ce_productID;
	CEdit m_ce_versionNO;
	afx_msg void OnBnClickedButtonOpen();

	//Show the message
	void ShowMessageString(CString Message, int color);
	
	//CString convert to BYTE
	void CstringToByte(CString sInput, BYTE bOutput[]);

	//Ascii convert to Hex
	BOOL AsciiToHex(BYTE pAsciiArray[], BYTE pHexArray[], int Len);

	//从控件获取设备属性
	BOOL GetMyDeviceAtrributes();

	BOOL SendData(BYTE pdataBuf[], USHORT pdataBufLen);


	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);

	BYTE *CHidDemoDlg::GetReadReportBuf();

	afx_msg void OnBnClickedButtonClean();
	afx_msg void OnBnClickedButtonClose();
};
