
// HidDemoDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "HidDemo.h"
#include "HidDemoDlg.h"
#include "afxdialogex.h"

#include "Dbt.h"

extern "C" {
#include "hidsdi.h"
#include "SETUPAPI.H"
}


#define COLOR_RED		0xFF0000
#define COLOR_GREEN		0x00FF00
#define COLOR_BLUE		0x0000FF
#define COLOR_BLACK		0x000000
#define COLOR_WHITE		0xFFFFFF


#ifdef _DEBUG
#define new DEBUG_NEW
#endif





/*****************************************************************/
//HID所用到的全局变量

//保存设备路径
CString g_MyDevPathName = _T("");

//标识设备是否找到
BOOL g_MyDevFound = FALSE;

//保存设备属性, DWORD->CString 这样做可以省略其中的转换
USHORT g_VendorID, g_ProductID, g_VersionNO;

//保存读数据的设备句柄
HANDLE	g_ReadHandle = INVALID_HANDLE_VALUE;

//保存写数据的设备句柄
HANDLE g_WriteHandle = INVALID_HANDLE_VALUE;

//标识是否正在发送数据
BOOL g_DataSending = FALSE;

//发送报告的缓冲区, 1字节报告ID+64字节数据 (64: 是开发板的描述符中规定的, 每个HID设备一般不一样)
BYTE g_WriteReportBuf[2*1024] = { 0 };

//接收报告的缓冲区, 1字节报告ID+64字节数据 (64: 是开发板的描述符中规定的, 每个HID设备一般不一样)
BYTE g_ReadReportBuf[2 * 1024] = { 0 };

//发送报告用的OVERLAPPEND
OVERLAPPED g_WriteOverLapped;

//接收报告用的OVERLAPPEND
OVERLAPPED g_ReadOverLapped;

//指向写报告线程的指针
CWinThread *g_pWriteReportThread;

//指向读报告线程的指针
CWinThread *g_pReadReportThread;

//用于注册设备通知事件用的广播接口   Note: 定义该结构体时, 需要添加 #include "Dbt.h"
DEV_BROADCAST_DEVICEINTERFACE g_DevBroadcastDeviceInterface;


//保存设备输入/输出报告的长度
USHORT   g_InputReportByteLength;
USHORT   g_OutputReportByteLength;






/*****************************************************************/


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


// CHidDemoDlg 对话框



CHidDemoDlg::CHidDemoDlg(CWnd* pParent /*=NULL*/)
	: CDialog(IDD_HIDDEMO_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CHidDemoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_SEND, m_ce_send);
	DDX_Control(pDX, IDC_RICHEDIT_RESULT, m_re_result);
	DDX_Control(pDX, IDC_EDIT_VENDOR_ID, m_ce_vendorID);
	DDX_Control(pDX, IDC_EDIT_PRODUCT_ID, m_ce_productID);
	DDX_Control(pDX, IDC_EDIT_VERSION_NO, m_ce_versionNO);
}

BEGIN_MESSAGE_MAP(CHidDemoDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_OPEN, &CHidDemoDlg::OnBnClickedButtonOpen)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CHidDemoDlg::OnBnClickedButtonSend)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_BUTTON_CLEAN, &CHidDemoDlg::OnBnClickedButtonClean)
	ON_BN_CLICKED(IDC_BUTTON_CLOSE, &CHidDemoDlg::OnBnClickedButtonClose)
END_MESSAGE_MAP()


// CHidDemoDlg 消息处理程序

BOOL CHidDemoDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
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

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CHidDemoDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CHidDemoDlg::OnPaint()
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
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CHidDemoDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}





void CHidDemoDlg::ShowMessageString(CString Message, int color)
{
	CHARFORMAT	cf;
	int r, g, b;
	int len;

	r = ((color >> 16) & 0xFF);
	g = ((color >> 8) & 0xFF);
	b = ((color >> 0) & 0xFF);

	memset(&cf, 0, sizeof(cf));

	m_re_result.GetDefaultCharFormat(cf);


	cf.dwMask = CFM_COLOR;
	cf.dwEffects &= ~CFE_AUTOCOLOR;

	cf.crTextColor = RGB(r, g, b);

	m_re_result.SetSel(-1, -1);
	m_re_result.SetSelectionCharFormat(cf);

	len = m_re_result.GetWindowTextLength();

	if ((len + Message.GetLength()) > (m_re_result.GetLimitText()))
	{
		m_re_result.SetWindowText(_T("Clear Screen ...\r\n"));
	}

	Message += "\r\n";


	m_re_result.SetSel(-1, -1);
	m_re_result.ReplaceSel(Message);

	m_re_result.PostMessage(WM_VSCROLL, SB_BOTTOM, 0);

}



void CHidDemoDlg::CstringToByte(CString sInput, BYTE bOutput[])
{
	BYTE srcBuf[1024] = { 0 };	// The Bigger the better     by Huihh 2016.9.12
	BYTE desBuf[1024] = { 0 };
	int srcLen = 0;


	strcpy((char *)srcBuf, sInput);
	srcLen = strlen(sInput);


	AsciiToHex(srcBuf, desBuf, srcLen);   //这里输入长度为字符的个数   by Huihh 2016.11.11

	for (int i = 0; i<(srcLen / 2); i++)
	{
		bOutput[i] = desBuf[i];
	}

}






BOOL CHidDemoDlg::AsciiToHex(BYTE pAsciiArray[], BYTE pHexArray[], int Len)
{
	BYTE tempBuf[2] = { 0 };

	if (Len % 2 != 0)
	{
		AfxMessageBox(_T("Ascii Convert Hex Failed, Please input Convert Length in even numbers, Try Again Later"));
		return FALSE;
	}

	int HexLen = Len / 2;   // 2 Character Convert 1 Hex  by Huihh 2016.9.8

	for (int i = 0; i<HexLen; i++)
	{
		tempBuf[0] = *pAsciiArray++;
		tempBuf[1] = *pAsciiArray++;

		for (int j = 0; j<2; j++)
		{
			if (tempBuf[j] <= 'F' && tempBuf[j] >= 'A')
			{
				tempBuf[j] = tempBuf[j] - 'A' + 10;
			}
			else if (tempBuf[j] <= 'f' && tempBuf[j] >= 'a')
			{
				tempBuf[j] = tempBuf[j] - 'a' + 10;
			}
			else if (tempBuf[j] >= '0' && tempBuf[j] <= '9')
			{
				tempBuf[j] = tempBuf[j] - '0';
			}
			else
			{
				AfxMessageBox(_T("pAsciiArray Contain illegality Character, Please Try Again after Check "));
				return FALSE;
			}
		}

		pHexArray[i] = tempBuf[0] << 4;
		pHexArray[i] |= tempBuf[1];
	}

	return TRUE;
}

BOOL CHidDemoDlg::GetMyDeviceAtrributes()
{
	CString strVendorID, strProductID, strVersionNO;
	BYTE tmpBuf[2] = { 0 };

	GetDlgItemText(IDC_EDIT_VENDOR_ID, strVendorID);
	GetDlgItemText(IDC_EDIT_PRODUCT_ID, strProductID);
	GetDlgItemText(IDC_EDIT_VERSION_NO, strVersionNO);

	//将从控件获取到的设备属性字符串转化为 USHORT 类型, 用于后面比对是否为要打开的设备
	CstringToByte(strVendorID, tmpBuf);
	g_VendorID = tmpBuf[0] << 8 | tmpBuf[1];

	CstringToByte(strProductID, tmpBuf);
	g_ProductID = tmpBuf[0] << 8 | tmpBuf[1];

	CstringToByte(strVersionNO, tmpBuf);
	g_VersionNO = tmpBuf[0] << 8 | tmpBuf[1];

	return TRUE;
}

void CHidDemoDlg::OnBnClickedButtonOpen()
{
	// TODO: 在此添加控件通知处理程序代码

	//定义 GUID 的结构体, 用于保存HID设备的接口类GUID
	GUID HidGuid;

	//定义 DEVINFO 句柄, 用于保存获取到的设备信息集合
	HDEVINFO DevInfo;

	//保存当前搜索到的第几个设备, 0表示第一个设备
	DWORD DevIndex;

	//定义 SP_DEVICE_INTERFACE_DATA 结构体, 用于保存设备的驱动接口信息
	SP_DEVICE_INTERFACE_DATA DevInterfaceData;

	//BOOL变量, 用于保存函数调用是否成功
	BOOL RetCode;

	//定义接收时需要保存详细信息的缓冲区长度
	DWORD RequiredSize;

	//定义 PSP_DEVICE_INTERFACE_DETAIL_DATA 指针, 用于指向设备详细信息结构体的指针
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDevDetailData;

	//定义 HANDLE 句柄, 用于保存打开设备的句柄
	HANDLE DevHandle;

	//定义 HIDD_ATTRIBUTES 结构体变量, 保存设备的属性
	HIDD_ATTRIBUTES DevAttributes;



	//定义 PHIDP_PREPARSED_DATA 结构体, 调用 API 解析数据 
	PHIDP_PREPARSED_DATA	PreparsedData;
	
	//定义 HIDP_CAPS 结构体, 用于保存设备 IO 能力
	HIDP_CAPS	Capabilities;





	//初始化设备位找到
	g_MyDevFound = FALSE;

	//获取控件中的设备属性(VendorID, ProductID, VersionNO)
	GetMyDeviceAtrributes();


	//初始化读,写句柄为无效句柄
	g_ReadHandle = INVALID_HANDLE_VALUE;
	g_WriteHandle = INVALID_HANDLE_VALUE;

	//对 DevInterfaceData 结构体的 cbSize 初始化为结构体大小
	DevInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	//对 DevAttributes 结构体的 Size 初始化为结构体大小
	DevAttributes.Size = sizeof(HIDD_ATTRIBUTES);

	//调用 HidD_GetHidGuid()函数获取 HID 设备的 GUID, 并保存在 HidGuid 中
	HidD_GetHidGuid(&HidGuid);

	CString str;
	str.Format(_T("GUID: { %08X-%04x-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X }"),
		HidGuid.Data1,
		HidGuid.Data2,
		HidGuid.Data3,
		HidGuid.Data4[0], HidGuid.Data4[1], HidGuid.Data4[2], HidGuid.Data4[3], HidGuid.Data4[4], HidGuid.Data4[5], HidGuid.Data4[6], HidGuid.Data4[7]);

	ShowMessageString(str, COLOR_BLACK);



	//根据 HidGuid 获取设备信息集合, 其中 Flags 参数设置为 
	//DIGCF_DEVICEINTERFACE|DIGCF_PRESENT, 前者表示使用 GUID 为
	//接口类GUID, 后者表示只列举正在使用的设备, 因为我们只查找已连接上的设备, 
	//返回的句柄保存在 DevInfo 中
	//Note: 设备信息集合使用完毕后, 要使用函数 SetupDiDestroyDeviceInfoList()销毁, 否则会造成内存泄漏
	DevInfo = SetupDiGetClassDevsW(&HidGuid,
								    NULL,
								    NULL,
									DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	ShowMessageString(_T("Start search for specific device"), COLOR_BLACK);

	//对设备集合中每一个设备进行列举, 检查是否是我们要找的设备,
	//当找到我们指定的设备后, 或者设备已列举完毕, 则退出查找,
	//首先指向第一个设备, 即 DevIndex = 0
	DevIndex = 0;

	while (1)
	{
		//调用 SetupDiEnumDeviceInterfaces() 函数在设备信息集合中获取编号为 DevIndex 的设备信息
		RetCode = SetupDiEnumDeviceInterfaces(DevInfo,
												NULL,
												&HidGuid,
												DevIndex,
												&DevInterfaceData);

		//获取信息失败, 则说明设备已查找完毕, 退出循环
		if (RetCode == FALSE)
			break;
		
		//将指向下一个设备
		DevIndex++;


		//获取信息成功, 则继续获取该设备的详细信息, 在获取设备的详细信息时, 需要先知道保存详细
		//信息需要多大的缓存区, 通过第一次调用 SetupDiGetDeviceInterfaceDetail() 函数来获取, 这时,
		//提供的缓冲区和长度都为 NULL, 并提供一个用来保存需要多大缓冲区的变量 RequiredSize
		RetCode = SetupDiGetDeviceInterfaceDetail(DevInfo,
													&DevInterfaceData,
													NULL,
													NULL,
													&RequiredSize,
													NULL);

		//分配大小为 RequiredSize 的缓冲区, 用于保存设备详细信息
		pDevDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(RequiredSize);

		if (pDevDetailData == NULL)
		{
			AfxMessageBox(_T("Run out of Memory, Save Detail Information failed"));
			SetupDiDestroyDeviceInfoList(DevInfo);
			return;
		}

		//设置结构体成员变量 cbSize 的值为 结构体大小
		//Note: 只是结构体大小, 不包括后面的缓冲区大小
		pDevDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);


		//第二次调用 SetupDiGetDeviceInterfaceDetail() 函数, 获取设备的详细信息,
		//这次调用设置使用的缓冲区以及缓冲区大小
		RetCode = SetupDiGetDeviceInterfaceDetail(DevInfo,
													&DevInterfaceData,
													pDevDetailData,
													RequiredSize,
													NULL,
													NULL);

		//将设备路径保存, 然后销毁刚申请的内存空间
		g_MyDevPathName = pDevDetailData->DevicePath;
		free(pDevDetailData);


		//如果调用失败, 则查找下一个设备
		if (RetCode == FALSE)
			continue;


		//如果调用成功, 则使用不带读写访问的 CreateFile() 函数获取设备的属性, 
		//对于一些系统独占设备(键盘,鼠标), 使用读写访问方式是无法打开的,
		//而使用不带读写方式才可以打开, 从而获得设备的属性

		DevHandle = CreateFile(g_MyDevPathName,
								NULL,
								FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								NULL);

		//打开成功, 则获取设备属性
		if (DevHandle != INVALID_HANDLE_VALUE)
		{
			//获取设备的属性并保存在 DevAttributes 结构体中
			RetCode = HidD_GetAttributes(DevHandle, &DevAttributes);

			//关闭刚打开的设备
			CloseHandle(DevHandle);

			//获取失败, 则查找下一个
			if (RetCode == FALSE)
				continue;

			//获取成功, 则将属性中的 VendorID, ProductID, VersionNumber 与
			//我们需要的进行比对, 一致的话, 则说明她就是我们要找的设备
			if (DevAttributes.VendorID == g_VendorID) 
			{
				ShowMessageString(_T("Vendor ID matching"), COLOR_BLACK);

				if (DevAttributes.ProductID == g_ProductID)
				{
					ShowMessageString(_T("Product ID matching"), COLOR_BLACK);

					//因在HID设备枚举中, 看不到版本号, 则此处不再比对版本号   
					//if (DevAttributes.VersionNumber == g_VersionNO)   
					{
						ShowMessageString(_T("Version N0. matching"), COLOR_BLACK);

						//设置设备已找到标识位
						g_MyDevFound = TRUE;
						ShowMessageString(_T("The device has been found"), COLOR_BLACK);

						//读方式打开设备
						g_ReadHandle = CreateFile(g_MyDevPathName,
							GENERIC_READ,
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
							NULL);

						if (g_ReadHandle != INVALID_HANDLE_VALUE)
							ShowMessageString(_T("Open the device with R-access has been successfully"), COLOR_BLACK);
						else
							ShowMessageString(_T("Open the device with R-access has been failed"), COLOR_RED);

						//写方式打开设备
						g_WriteHandle = CreateFile(g_MyDevPathName,
							GENERIC_WRITE,
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
							NULL);

						if (g_WriteHandle != INVALID_HANDLE_VALUE)
							ShowMessageString(_T("Open the device with W-access has been successfully"), COLOR_BLACK);
						else
							ShowMessageString(_T("Open the device with W-access has been failed"), COLOR_RED);


						//此处获取设备的额外信息, 用于发送和接收数据时确定发送和接收的数据长度
						RetCode = HidD_GetPreparsedData(g_ReadHandle, &PreparsedData);
						if (RetCode != TRUE)
							ShowMessageString(_T("Hid Get Preparsed Data"), COLOR_RED);

						RetCode = HidP_GetCaps(PreparsedData, &Capabilities);
						if (RetCode != HIDP_STATUS_SUCCESS)
							ShowMessageString(_T("HidP_GetCaps Failed"), COLOR_RED);

						//保存输入输出的数据长度   读 = 输入   写 = 输出
						g_InputReportByteLength = Capabilities.InputReportByteLength;
						g_OutputReportByteLength = Capabilities.InputReportByteLength;


						//标识当前可以发送数据
						g_DataSending = FALSE;

						//手动触发事件, 让读报告线程恢复运行, 因为在这之前并没有调用读数据的函数, 也
						//就不会引起事件的产生, 所以需要先手动触发一次事件, 让读报告线程恢复运行
						SetEvent(g_ReadOverLapped.hEvent);

						break;
					}
				}
			}
		}
		//打开失败, 则查找下一个设备
		else
			continue;
	}	

	//调用 SetupDiDestroyDeviceInfoList() 函数销毁设备信息集合
	SetupDiDestroyDeviceInfoList(DevInfo);

	//如果设备已经找到, 那么应该使能各操作按钮,并同时禁止打开设备按钮
	if (g_MyDevFound)
	{
		//禁止打开设备按键, 使能关闭设备
		GetDlgItem(IDC_BUTTON_OPEN)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_CLOSE)->EnableWindow(TRUE);
	}
	else
		ShowMessageString(_T("The specified device was not found"), COLOR_RED);
}





void CHidDemoDlg::OnBnClickedButtonSend()
{
	// TODO: 在此添加控件通知处理程序代码
	

	CString strSendData, strDisp, strTmp;
	SHORT sendDataLen = 0, currOffset = 0;


	//获取要发送的数据, 并设置到全局的发送缓冲区中
	GetDlgItemText(IDC_EDIT_SEND, strSendData);

	if (strSendData.GetLength() == 0)
	{
		AfxMessageBox(_T("Please Input send Data, Try again"));
		return;
	}

	if ( (strSendData.GetLength() % 2) != 0 )
	{
		AfxMessageBox(_T("Input send Data must be divisible by 2, Try again"));
		return;
	}

	CstringToByte(strSendData, g_WriteReportBuf);

	sendDataLen = strSendData.GetLength() / 2;


	BOOL RetCode = FALSE;




	while (sendDataLen >= 0)
	{
		RetCode = SendData(&g_WriteReportBuf[currOffset], g_OutputReportByteLength);

		if (RetCode == FALSE)
		{
			return;
		}

		currOffset += (g_OutputReportByteLength - 1);
		sendDataLen -= (g_OutputReportByteLength - 1);
	}

}


BOOL CHidDemoDlg::SendData(BYTE pdataBuf[], USHORT pdataBufLen)
{

	//如果设备未找到, 则返回
	if (g_MyDevFound == FALSE)
	{
		ShowMessageString(_T("The specified device was not found"), COLOR_RED);
		return FALSE;
	}

	//如果句柄无效, 则表示以写方式打开设备时失败
	if (g_WriteHandle == INVALID_HANDLE_VALUE)
	{
		ShowMessageString(_T("An invalid write handle, May be failed on open the device with W-access"), COLOR_RED);
		return FALSE;
	}

	//检查是否有数据正在发送
	if (g_DataSending == TRUE)
	{
		ShowMessageString(_T("Transmitting data, Just a monment"), COLOR_RED);
		return FALSE;
	}

	BYTE sendBuf[512] = { 0 };


	CString strSendData, strDisp, strTmp;

	memcpy(&sendBuf[1], pdataBuf, (pdataBufLen-1));

	//Report ID: 该字节好像任何值都可以, 暂时不知道为什么   by Huihh 2018.07.20
	sendBuf[0] = 0x00;

	strDisp.Format(_T("Send Report %02d Octets: "), pdataBufLen);
	ShowMessageString(strDisp, COLOR_GREEN);


	strDisp = "";
	for (int i = 0; i < pdataBufLen; i++)
	{
		strTmp.Format(_T("%02x "), sendBuf[i]);
		strDisp += strTmp;
	}
	ShowMessageString(strDisp, COLOR_GREEN);


	//设置正在发送标识位
	g_DataSending = TRUE;


	//调用WriteFile函数发送数据
	BOOL RetCode = WriteFile(g_WriteHandle,
		sendBuf,
		pdataBufLen,
		NULL,
		&g_WriteOverLapped);


	//如果函数返回 FALSE, 可能是真的失败, 也可能是IO挂起了
	if (RetCode == FALSE)
	{
		//获取最后的错误代码
		DWORD lastError = GetLastError();

		//查看是否 IO 挂起
		if ((lastError == ERROR_IO_PENDING) || (lastError == ERROR_SUCCESS))
		{
			//调试通过后, 不再需要显示该信息
			ShowMessageString(_T("IO Pending"), COLOR_BLUE);
			while (g_DataSending == TRUE);
			return TRUE;
		}
		//函数调用时发生错误, 显示错误代码
		else
		{
			g_DataSending = FALSE;
			strDisp.Format(_T("Send Data failed, Last Error: %d"), lastError);
			ShowMessageString(strDisp, COLOR_RED);

			if (lastError == 1)
				ShowMessageString(_T("Device not support WriteFile Function"), COLOR_RED);

			return FALSE;
		}
	}
	//函数返回成功
	else
	{
		g_DataSending = FALSE;
		return TRUE;
	}
}


















//Note: 线程声明必须在 OnShowWindow()之前, 或者将线程的实现放在该函数之前, 否则编译错误, 提示线程没有声明
UINT WriteReportThread(LPVOID pParam);
UINT ReadReportThread(LPVOID pParam);


void CHidDemoDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	// TODO: 在此处添加消息处理程序代码

	//初始化要查找的设备属性值 VendorID, ProductID, VersionNO
	SetDlgItemText(IDC_EDIT_VENDOR_ID, _T("5548"));
	SetDlgItemText(IDC_EDIT_PRODUCT_ID, _T("6666"));
	SetDlgItemText(IDC_EDIT_VERSION_NO, _T("0300"));



	//初始化写报告时用的 OVERLAPPED 结构体
	//偏移量设置为0
	g_WriteOverLapped.Offset = 0;
	g_WriteOverLapped.OffsetHigh = 0;
	
	//创建一个事件, 提供给 WriteFile 使用, 当 WriteFile 完成时, 
	//会设置该事件为触发状态
	g_WriteOverLapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);


	//创建写报告的线程(处于挂起状态)
	g_pWriteReportThread = AfxBeginThread(WriteReportThread,
											this,
											THREAD_PRIORITY_NORMAL,
											0,
											CREATE_SUSPENDED,
											NULL);

	//如果创建成功, 则恢复该线程的运行
	if (g_pWriteReportThread != NULL)
	{
		g_pWriteReportThread->ResumeThread();
	}



	//初始化读报告时用的 OVERLAPPED 结构体
	//偏移量设置为0
	g_ReadOverLapped.Offset = 0;
	g_ReadOverLapped.OffsetHigh = 0;

	//创建一个事件, 提供给 ReadFile 使用, 当 ReadFile 完成时, 
	//会设置该事件为触发状态
	g_ReadOverLapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);


	//创建读报告的线程(处于挂起状态)
	g_pReadReportThread = AfxBeginThread(ReadReportThread,
											this,
											THREAD_PRIORITY_NORMAL,
											0,
											CREATE_SUSPENDED,
											NULL);

	//如果创建成功, 则恢复该线程的运行
	if (g_pReadReportThread != NULL)
	{
		g_pReadReportThread->ResumeThread();
	}

	GUID HidGuid;

	//获取HID设备的接口类GUID
	HidD_GetHidGuid(&HidGuid);

	//设置 DEV_BROADCAST_DEVICEINTERFACE 结构体, 用来注册设备改变时的通知
	g_DevBroadcastDeviceInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	g_DevBroadcastDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	g_DevBroadcastDeviceInterface.dbcc_classguid = HidGuid;

	//注册设备改变时收到通知
	RegisterDeviceNotification(m_hWnd,
								&g_DevBroadcastDeviceInterface,
								DEVICE_NOTIFY_WINDOW_HANDLE);

}



//写报告线程, 该线程比较简单, 只是简单的等待事件被触发,
//然后清除数据正在发送的标识位
UINT WriteReportThread(LPVOID pParam)
{
	while (1)
	{
		ResetEvent(g_WriteOverLapped.hEvent);

		//等待事件触发
		WaitForSingleObject(g_WriteOverLapped.hEvent, INFINITE);

		//清除数据正在发送标志
		g_DataSending = FALSE;
	}

	return 0;
}





//仅用于 ReadReportThread 线程中, 用于获取主窗口类中的全局变量
//因该全局变量不是类的成员变量, 所以只能通过类的成员函数获取
BYTE *CHidDemoDlg::GetReadReportBuf()
{
	return g_ReadReportBuf;
}




//读报告的线程, 由于使用的是异步调用, 因而在调用 ReadFile() 函数时提供一个
// OVERLAPPED 的结构, 该结构中包含有一个事件的句柄, 平时该事件是处于无信号状态的,
//因而等待该事件的函数就会被挂起, 从而该线程被阻塞, 当数据正确返回后, 事件被触发, 
//线程恢复运行. 并检查返回的数据量以及报告ID是否正确, 从而设置界面上各开关的状态. 由于
//该函数并不是该工程中主窗口类中的成员函数, 所以无法直接调用类中的成员函数. 在创建该线程时, 通过
// pParam 参数传递了一个 this 指针, 将参数 pParam 强制转化为 主窗口类的指针, 即可访问其类中成员函数
UINT ReadReportThread(LPVOID pParam)
{
	CHidDemoDlg *pAppDlg;

	DWORD readLen;


	//将参数 pParam 强制转化为 主窗口类, 以供下面程序调用其成员函数
	pAppDlg = (CHidDemoDlg *)pParam;

	//该线程是个死循环, 直到程序退出时, 它才退出
	while (1)
	{
		//设置事件为无效状态
		ResetEvent(g_ReadOverLapped.hEvent);

		//如果设备已经找到
		if (g_MyDevFound == TRUE)
		{
			//读句柄有效
			if (g_ReadHandle != INVALID_HANDLE_VALUE)
			{
				ReadFile(g_ReadHandle,
							g_ReadReportBuf,
							g_InputReportByteLength,
							NULL,
							&g_ReadOverLapped);
			}
			//读句柄无效
			else
				pAppDlg->ShowMessageString(_T("An invalid read handle, May be failed on open the device with R-access"), COLOR_RED); 
			
	
			//等待事件触发
			WaitForSingleObject(g_ReadOverLapped.hEvent, INFINITE);



			//如果等待过程中设备被拔出, 也会导致事件触发, 但此时 g_MyDevFound
			//被设置为假, 因此在这里判断 g_MyDevFound 为假的话就进入下一轮循环
			if (g_MyDevFound == FALSE) 
				continue;


			//如果设备没有被拔下, 则是 ReadFile() 函数正常操作完成.
			//通过 GetOverlappedResult() 函数来获取实际读取到的字节数

			GetOverlappedResult(g_ReadHandle, &g_ReadOverLapped, &readLen, TRUE);

			//如果字节数不为0, 则将读到的数据显示在界面上
			if (readLen != 0)
			{
				CString strDisp, strTmp;
				strDisp.Format(_T("Read Report %d Octets" ), readLen);
				pAppDlg->ShowMessageString(strDisp, COLOR_BLUE);

				strDisp = "";
				for (int i = 0; i < readLen; i++)
				{	 
					// *((pAppDlg->GetReadReportBuf())+i) 用于获取全局 g_ReadReportBuf[] 中每个元素的值, 再将其转化为CString类型
					strTmp.Format(_T("%02x "), *((pAppDlg->GetReadReportBuf())+i) );
					strDisp += strTmp;
				}

				pAppDlg->ShowMessageString(strDisp, COLOR_BLUE);
				
			}
		}
		//设备未找到, 阻塞线程, 直到下次事件被触发
		else
			WaitForSingleObject(g_ReadOverLapped.hEvent, INFINITE);
	}

	return 0;
}


void CHidDemoDlg::OnBnClickedButtonClean()
{
	// TODO: 在此添加控件通知处理程序代码
	SetDlgItemText(IDC_RICHEDIT_RESULT, _T(""));

}


void CHidDemoDlg::OnBnClickedButtonClose()
{
	// TODO: 在此添加控件通知处理程序代码

	//如果读数据的句柄不是无效句柄, 则关闭之
	if (g_ReadHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(g_ReadHandle);
		g_ReadHandle = INVALID_HANDLE_VALUE;
	}

	//设置设备状态为未找到
	g_MyDevFound = FALSE;

	//修改按键使能情况
	GetDlgItem(IDC_BUTTON_OPEN)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_CLOSE)->EnableWindow(FALSE);

}
