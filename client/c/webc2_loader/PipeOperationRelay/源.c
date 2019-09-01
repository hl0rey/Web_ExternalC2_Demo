#include <Windows.h>
#include <stdio.h>

#define PAYLOAD_MAX_SIZE 512 * 1024
#define BUFFER_MAX_SIZE 1024 * 1024

//桥，字面意思。方便把自定义的管道和beacon管道桥接的结构体
struct BRIDGE
{
	HANDLE client;
	HANDLE server;
};

//从beacon读取数据
DWORD read_frame(HANDLE my_handle, char* buffer, DWORD max) {

	DWORD size = 0, temp = 0, total = 0;
	/* read the 4-byte length */
	ReadFile(my_handle, (char*)& size, 4, &temp, NULL);
	printf("read_frame length: %d\n", size);
	/* read the whole thing in */
	while (total < size) {
		ReadFile(my_handle, buffer + total, size - total, &temp,
			NULL);
		total += temp;
	}
	return size;
}

//向beacon写入数据
void write_frame(HANDLE my_handle, char* buffer, DWORD length) {
	printf("write_frame length: %d\n", length);
	DWORD wrote = 0;
	WriteFile(my_handle, (void*)& length, 4, &wrote, NULL);
	printf("write %d bytes.\n", wrote);
	WriteFile(my_handle, buffer, length, &wrote, NULL);
	printf("write %d bytes.\n", wrote);
}

//从控制器读取数据
DWORD read_client(HANDLE my_handle, char* buffer) {
	DWORD size = 0;
	DWORD readed = 0;
	ReadFile(my_handle, &size, 4, NULL, NULL);
	printf("read_client length: %d\n", size);
	ReadFile(my_handle, buffer, size, &readed, NULL);
	printf("final data from client: %d\n", readed);
	return readed;
}

//向控制器写入数据
void write_client(HANDLE my_handle, char* buffer, DWORD length) {
	DWORD wrote = 0;
	WriteFile(my_handle, buffer, length, &wrote, NULL);
	printf("write client total %d data %d\n", wrote, length);
}

//客户端读管道、服务端写管道逻辑
DWORD WINAPI ReadOnlyPipeProcess(LPVOID lpvParam) {
	//把两条管道的句柄取出来
	struct BRIDGE* bridge = (struct BRIDGE*)lpvParam;
	HANDLE hpipe = bridge->client;
	HANDLE beacon = bridge->server;
	
	DWORD length = 0;
	char* buffer = VirtualAlloc(0, BUFFER_MAX_SIZE, MEM_COMMIT, PAGE_READWRITE);
	if (buffer == NULL)
	{
		exit(-1);
	}
	//再次校验管道
	if ((hpipe == INVALID_HANDLE_VALUE) || (beacon == INVALID_HANDLE_VALUE))
	{
		return FALSE;
	}
	while (TRUE)
	{
		if (ConnectNamedPipe(hpipe, NULL))
		{
			printf("client want read.\n");
			length = read_frame(beacon, buffer, BUFFER_MAX_SIZE);
			printf("read from beacon: %d\n", length);
			//分两次传送，发一次长度，再发数据。
			write_client(hpipe,(char *) &length, 4);
			FlushFileBuffers(hpipe);
			write_client(hpipe, buffer, length);
			FlushFileBuffers(hpipe);
			DisconnectNamedPipe(hpipe);
			//清空缓存区
			ZeroMemory(buffer, BUFFER_MAX_SIZE);
			length = 0;
		}

	}

	return 1;
}

//客户端写管道、服务端读管道逻辑
DWORD WINAPI WriteOnlyPipeProcess(LPVOID lpvParam) {
	//取出两条管道
	struct BRIDGE* bridge = (struct BRIDGE*)lpvParam;
	HANDLE hpipe = bridge->client;
	HANDLE beacon = bridge->server;
	
	DWORD length = 0;
	char* buffer = VirtualAlloc(0, BUFFER_MAX_SIZE, MEM_COMMIT, PAGE_READWRITE);
	if (buffer == NULL)
	{
		exit(-1);
	}
	if ((hpipe == INVALID_HANDLE_VALUE) || (beacon == INVALID_HANDLE_VALUE))
	{
		return FALSE;
	}
	while (TRUE)
	{
		if (ConnectNamedPipe(hpipe, NULL))
		{
			//一次性读，一次性写
			printf("client want write.\n");
			length = read_client(hpipe, buffer);
			printf("read from client: %d\n", length);
			write_frame(beacon, buffer, length);
			DisconnectNamedPipe(hpipe);
			//清空缓存区
			ZeroMemory(buffer, BUFFER_MAX_SIZE);
			length = 0;
		}

	}

	return 2;
}

int main(int argc, char* argv[]) {

	//创建客户端读管道
	HANDLE hPipeRead = CreateNamedPipe("\\\\.\\pipe\\hlread", PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, BUFFER_MAX_SIZE, BUFFER_MAX_SIZE, 0, NULL);
	//创建客户端写管道
	HANDLE hPipeWrite = CreateNamedPipe("\\\\.\\pipe\\hlwrite", PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, BUFFER_MAX_SIZE, BUFFER_MAX_SIZE, 0, NULL);
	//与beacon建立连接
	HANDLE hfileServer = CreateFileA("\\\\.\\pipe\\hltest", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, NULL);


	//检测管道和连接是否建立成功
	if ((hPipeRead == INVALID_HANDLE_VALUE) || (hPipeWrite == INVALID_HANDLE_VALUE) || (hfileServer == INVALID_HANDLE_VALUE))
	{
		if (hPipeRead == INVALID_HANDLE_VALUE)
		{
			printf("error during create readpipe.");
		}
		if (hPipeWrite == INVALID_HANDLE_VALUE)
		{
			printf("error during create writepipe.");
		}
		if (hfileServer == INVALID_HANDLE_VALUE)
		{
			printf("error during connect to beacon.");
		}
		exit(-1);
	}
	else
	{	
		//一切正常
		printf("all pipes are ok.\n");
	}

	
	//放入客户端读管道和beacon连接
	struct BRIDGE readbridge;
	readbridge.client = hPipeRead;
	readbridge.server = hfileServer;
	//启动客户端读管道逻辑
	HANDLE hTPipeRead = CreateThread(NULL, 0, ReadOnlyPipeProcess, (LPVOID)& readbridge, 0, NULL);
	
	//放入客户端写管道和beacon连接
	struct BRIDGE writebridge;
	writebridge.client = hPipeWrite;
	writebridge.server = hfileServer;
	//启动客户端写管道逻辑
	HANDLE hTPipeWrite = CreateThread(NULL, 0, WriteOnlyPipeProcess, (LPVOID)& writebridge, 0, NULL);

	//代码没有什么意义，直接写个死循环也行
	HANDLE waitHandles[] = { hPipeRead,hPipeWrite };
	while (TRUE)
	{
		WaitForMultipleObjects(2, waitHandles, TRUE, INFINITE);
	}

	return 0;

}