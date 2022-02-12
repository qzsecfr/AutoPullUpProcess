#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <TlHelp32.h>
#include <vector>
#include <algorithm>
#include <codecvt>
#include <cwchar>

using namespace std;

struct ProcessInfo
{
	DWORD PID;
	string PName;

	ProcessInfo(DWORD pid, string pname) : PID(pid), PName(pname) {}
	// 用来根据PID从小到大排序进程信息
	bool operator< (const ProcessInfo& rhs) const
	{
		return PID < rhs.PID;
	}
};

string WCHAR2String(LPCWSTR pwszSrc)
{
	int nLen = WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, NULL, 0, NULL, NULL);
	if (nLen <= 0)
	{
		return string("");
	}
	char *pszDst = new char[nLen];
	if (pszDst == nullptr)
	{
		return string("");
	}

	WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, pszDst, nLen, NULL, NULL);
	pszDst[nLen - 1] = '\0';

	string strTmp(pszDst);
	delete[] pszDst;
	return strTmp;
}

wstring str2wstr(const string& str)
{
	using convert_typeX = codecvt_utf8<wchar_t>;
	wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.from_bytes(str);
}

vector<ProcessInfo> GetProcessInfo()
{
	vector<ProcessInfo> PInfo;

	STARTUPINFO st;
	PROCESS_INFORMATION pi;
	PROCESSENTRY32 ps;

	ZeroMemory(&st, sizeof(STARTUPINFO));
	st.cb = sizeof(STARTUPINFO);
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&ps, sizeof(PROCESSENTRY32));
	ps.dwFlags = sizeof(PROCESSENTRY32);

	// 拍摄进程快照
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE ||
		!Process32First(hSnapshot, &ps))
	{
		return PInfo;
	}

	do
	{
		PInfo.emplace_back(ps.th32ParentProcessID, WCHAR2String(ps.szExeFile));
	} while (Process32Next(hSnapshot, &ps));

	CloseHandle(hSnapshot);
	sort(PInfo.begin(), PInfo.end());
	return PInfo;
}

bool CheckProcessExistence(const vector<ProcessInfo>& PInfo, const string& PName)
{
	for (auto pInfo : PInfo)
	{
		if (strcmp(PName.c_str(), pInfo.PName.c_str()) == 0)
		{
			return true;
		}
	}
	return false;
}

bool PullUpProcessExe(const string& exeFullPath)
{
	STARTUPINFO st;
	ZeroMemory(&st, sizeof(STARTUPINFO));
	st.cb = sizeof(STARTUPINFO);
	st.dwFlags = STARTF_USESHOWWINDOW;
	st.wShowWindow = SW_SHOW;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

	// 为防止路径中存在空格，此处使用双引号将程序执行全路径引用起来
	string safeExeFullPath = string("\"") + exeFullPath + string("\"");
	wstring wExeFullPath = str2wstr(safeExeFullPath);
	wchar_t wBuf[1024] = { '\0' };
	wmemcpy_s(wBuf, 1024, wExeFullPath.c_str(), wExeFullPath.size());

	bool ret = CreateProcess(NULL, wBuf, NULL, NULL, false, 0, NULL, NULL, &st, &pi);
	if (ret)
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

struct ExeInfo
{
	string exeName;
	string exeFullPath;

	ExeInfo(string name, string path) : exeName(name), exeFullPath(path) {}
};

vector<ExeInfo> InitConcernedExeInfo(const string& cfgFile)
{
	vector<ExeInfo> exeInfo;

	FILE* file;
	file = fopen(cfgFile.c_str(), "r");
	if (!file)
	{
		return exeInfo;
	}
	while (!feof(file))
	{
		char name[100] = { '\0' }, path[1024] = { '\0' };
		int num = fscanf(file, "%s,%s\n", &name, &path);
		if (num <= 0 || name[0] == '%')
		{
			continue;
		}
		exeInfo.emplace_back(name, path);
	}
}