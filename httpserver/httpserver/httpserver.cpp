// httpserver.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include "socketio.h"
#include <process.h>
#include <locale.h>
#include "wmi.h"
#include "gpuload.h"
#include <TlHelp32.h>

long long _previousTotalTicks = 0;
long long _previousIdleTicks = 0;

// Посчитать загрузку процессора
float CalculateCPULoad(unsigned long long idleTicks, unsigned long long totalTicks)
{
   long long totalTicksSinceLastTime = totalTicks-_previousTotalTicks;
   long long idleTicksSinceLastTime  = idleTicks-_previousIdleTicks;

   float ret = 1.0f-((totalTicksSinceLastTime > 0) ? ((float)idleTicksSinceLastTime)/totalTicksSinceLastTime : 0);

   _previousTotalTicks = totalTicks;
   _previousIdleTicks  = idleTicks;
   return ret;
}

static unsigned long long FileTimeToInt64(const FILETIME & ft) 
{
	return (((unsigned long long)(ft.dwHighDateTime))<<32)|
		((unsigned long long)ft.dwLowDateTime);
}

// ПОсчитать загрузку графического процессора
// Returns 1.0f for "CPU fully pinned", 0.0f for "CPU idle", or somewhere in between
// You'll need to call this at regular intervals, since it measures the load between
// the previous call and the current one.  Returns -1.0 on error.
float GetCPULoad()
{
   FILETIME idleTime, kernelTime, userTime;
   return GetSystemTimes(&idleTime, &kernelTime, &userTime) ? 
	   CalculateCPULoad(FileTimeToInt64(idleTime), FileTimeToInt64(kernelTime) + FileTimeToInt64(userTime)) : -1.0f;
}

// Получить количество процессов
int getProcessCount ()
{
	HANDLE h = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

	PROCESSENTRY32 p;
	memset( &p, 0, sizeof(p) );
	p.dwSize = sizeof(p);

	BOOL r = Process32First( h, &p );
	int i;

	// Перебираем все процессы
	for( i = 0; r; i++ )
	{
		r = Process32Next( h, &p );
	}

	CloseHandle( h );

	return i;
}

// Рабочая цепочка
void SocketThread( void *data )
{
	// Соединяемся
	TSocketIO sockio( (SOCKET) data, true );
	THttpHeader req, resp;

	// Читаем параметры
	req.ReadFromSocket( sockio );
	req.parseReq();

	// Выводим для отладки
	printf( "Method %s url %s (%s)\n", req.method, req.url, req.httpver );

	char content[10000];

	// Какая-то ошибка
	if ( (req.method == NULL) ) // || (strcmp( req.method, "GET" ) != 0) )
	{
		sprintf( content, "<html><body>unsupported</body></html>" );

		resp.addResponce( 500, "Unsupported method", content );
		resp.WriteToSocket( sockio );
		sockio.writestr( content );
	
		return;
	}

	// Страница по умолчанию
	if (strcmp( req.url, "/" ) == 0)
		req.url = "/index.html";

	if (strcmp( req.url, "/index.html" ) == 0)
	{
		char *s = &content[0];

		// s += sprintf( s, "<!DOCTYPE html>\n" );
		s += sprintf( s, "<html>\n<head>\n" );
		s += sprintf( s, "<title>Computer status</title>\n" );
		s += sprintf( s, "<link type=\"text/css\" rel=\"stylesheet\" href=\"table.css\" />\n" );
		s += sprintf( s, "</head>\n" );
		s += sprintf( s, "<body>\n<h1>computer status</h1>\n" );

		MEMORYSTATUSEX mem;
		memset( &mem, 0, sizeof(mem) );
		mem.dwLength = sizeof(mem);

		GlobalMemoryStatusEx( &mem );

		const int mb = 1024*1024;

		s += sprintf( s, "<table class=\"blueTable\">\n"  );
		s += sprintf( s, "  <tr><td>Parameter</td><td>Value</td></tr>\n" );
		s += sprintf( s, "  <tr><td>Memory use</td><td>%d %%</td></tr>\n", mem.dwMemoryLoad );
		s += sprintf( s, "  <tr><td>Physical memory</td><td>free %d mb/total %d mb</td></tr>\n", 
			(int) (mem.ullAvailPhys / mb), (int) (mem.ullTotalPhys / mb) );
		s += sprintf( s, "  <tr><td>Pagefile</td><td>free %d mb/total %d mb</td></tr>\n", 
			(int) (mem.ullAvailPageFile / mb), (int) (mem.ullTotalPageFile / mb) );
		s += sprintf( s, "  <tr><td>Virtual memory</td><td>free %d mb/total %d mb</td></tr>\n", 
			(int) (mem.ullAvailVirtual / mb), (int) (mem.ullTotalVirtual / mb) );
		s += sprintf( s, "</table><br/>\n"  );

		s += sprintf( s, "CPU load: %.0f %%<br/>\n", GetCPULoad() * 100 );

		wchar_t wbuf[100];
		char buf[100];

		TWMIObjectEnumerator *enm1;
		if (!queryWmi( L"SELECT * FROM Win32_PerfRawData_Tcpip_NetworkInterface", &enm1 ))
		{
			s += sprintf( s, "Error querying WMI for network.<br/>\n" );
		}
		else
		{
			s += sprintf( s, "<table class=\"blueTable\">\n" );
			s += sprintf( s, "  <tr><td>Interface</td><td>Bytes</td><td>%%</td></tr>\n" );
			while (enm1->getNext())
			{				
				wcstombs( buf, enm1->getBstrProp( L"Name" ), 100 );

				int bytes = enm1->getIntProp( L"BytesTotalPerSec" );
				int bw = 100e6;

				s += sprintf( s, "  <tr><td>%s</td><td>%d</td><td>%d %%</td></tr>\n", buf,
					bytes, bytes * 100 / bw );
			}

			s += sprintf( s, "</table>\n" );
		}

		s += sprintf( s, "GPU load: %d<br\>\n", getGpuLoad() );
		s += sprintf( s, "Process count: %d<br/>\n", getProcessCount() );

		s += sprintf( s, "</body>\n</html>\n" );

		FILE *f = fopen( "www\\index.html", "wt" );
		fprintf( f, "%s", content );
		fclose(f);

		resp.addResponce( 200, "OK", strlen(content) );
		resp.WriteToSocket( sockio );
		sockio.writestr( content );

		return;
	}

	char fname[100];
	sprintf( fname, "www\\%s", &req.url[1] );

	// Остальные страницы (не специальные) - это просто файл
	FILE *f = fopen( fname, "rb" );

	// А есть ли такой файл?
	if (f == NULL)
	{
		sprintf( content, "<html><body>File %s not found</html></body>", fname );

		resp.addResponce( 404, "not found", content );
		resp.WriteToSocket( sockio );
		sockio.writestr( content );

		return;
	}

	// Что за файл (картинка или хтмл)
	if (strstr( fname, ".htm" ) != NULL)
		resp.contentType = "text/html";
	else if (strstr( fname, ".jp" ) != NULL)
		resp.contentType = "image/jpeg";

	fseek( f, 0, SEEK_END );
	resp.addResponce( 200, "OK", ftell(f) );
	resp.WriteToSocket( &sockio );

	fseek( f, 0, SEEK_SET );

	unsigned char buf[16384];
	int r;

	// Блоками перекиыдваем файл
	while (!feof(f))
	{
		r = fread( &buf[0], 1, sizeof(buf), f );
		sockio.sendData( (char*) buf, r, 0 );

		if (r != sizeof(buf))
			break;
	}

	fclose(f);

	// Все
}


int _tmain(int argc, _TCHAR* argv[])
{
	setlocale ( LC_ALL, "Russian" );

	if (!initWmi())
	{
		printf( "Failed to init WMI\n" );
		return 1;
	}

	GetCPULoad();

    printf( "Запускаем сервер\n" );

    SOCKET server;
    sockaddr_in local;

    WSADATA wsaData;
	int wsaret=WSAStartup(0x101,&wsaData);
	if(wsaret!=0) return 1;

    local.sin_family=AF_INET; 
    local.sin_addr.s_addr=INADDR_ANY; 
    local.sin_port=htons((u_short)1080); // HTTPS port

	// СОздаем сокет
    if((server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) ==INVALID_SOCKET) return 1;			

	// Привязываем к порту
    if(bind(server,(sockaddr*)&local,sizeof(local))!=0) return 1;

    if(listen(server,10)!=0) return 1;

    sockaddr_in from;
    int fromlen=sizeof(from);

	printf( "Запустили, слушаем\n" );

	SOCKET sock;

    while(true)
    { // Пришло новое соединение
		sock = accept(server, (struct sockaddr*)&from,&fromlen);
        printf( "Соединение от %s\n", inet_ntoa(from.sin_addr) );

		// Запуск цепочки
		_beginthread( SocketThread, 0, (void*) sock );		
    }

    //closesocket() closes the socket and releases the socket descriptor
    closesocket(server);

    //originally this function probably had some use
    //currently this is just for backward compatibility
    //but it is safer to call it as I still believe some
    //implementations use this to terminate use of WS2_32.DLL 
    WSACleanup();

	return 0;
}

