#include "stdafx.h"
#include <WinSock2.h>
#include <vector>

using namespace std;

struct TSocketIO
{
public:
	SOCKET sock;

	TSocketIO( SOCKET socket, bool useSsl )
	{
		sock = socket;
	}

	~TSocketIO()
	{
		closesocket( sock );
	}

	// Функция ждем окончания строки
	void readstr( char *buf );

	// Пишем в сокет строку и перенос
	void writestr( char *buf );

	// Функции чтения и записи
	int recvData( char *buf, int size, int flags = 8 );
	void sendData( char *buf, int size, int flags = 0 );
};

// Пропустить начальные пробелы
char *skipLeadingSpaces( char *s );

char* alloccpy( char *buf );


// Ключ-значение (для заголовков и куков)
struct TKeyVal
{
public:
	char* key;
	char* val;

	TKeyVal()
	{
		key = val = NULL;
	}

	// Построить из строки с разделителем (key=value)
	TKeyVal( char *str, char *delim )
	{
		char *t = strstr( str, delim );

		if (t != NULL)
		{
			*t = 0;
			t++;
		}

		key = skipLeadingSpaces( str );
		val = skipLeadingSpaces(t);
	}
};

// HTTP-заголовок
struct THttpHeader
{
public:
	// Все строчки заголовка
	vector<char*> strs;

	// Поля заголовка
	vector<TKeyVal*> fields;

	// Куки
	vector<TKeyVal*> cookies;

	// MIME-тип содержимого (для ответа)
	char *contentType;

	THttpHeader()
	{
		method = NULL;
		url = NULL;
		httpver = NULL;
		contentType = NULL;
	}

	~THttpHeader()
	{
	}

	// Прочитать заголовки из потока
	void ReadFromSocket( TSocketIO *sock )
	{
		char buf[1000];

		while (true)
		{
			sock->readstr( buf );

			// Признак окончания - пустая строка
			if (buf[0] == 0)
				break;

			strs.push_back( alloccpy( buf ) );	
		}
	}

	void ReadFromSocket( TSocketIO &sock )
	{
		ReadFromSocket	( &sock );
	}

	// Записать заголовки в поток
	void WriteToSocket( TSocketIO *sock )
	{
		for( int i = 0; i < strs.size(); i++ )
			sock->writestr( strs[i] );

		// Признак окончания
		sock->writestr( "" );
	}

	void WriteToSocket( TSocketIO &sock )
	{
		WriteToSocket( &sock );
	}

	// Добавить новую строчку
	void addLine( char *fmt, ... )
	{
		char buf[1000];

	    va_list argptr;
		va_start(argptr, fmt);
	    vsprintf(buf, fmt, argptr);
		va_end(argptr);

		strs.push_back( alloccpy( buf ) );
	}

	// Добавить заголовки ответа
	void addResponce( int code, char *descr, int contentSize )
	{
		addLine( "HTTP/1.1 %d %s", code, descr );
		if (contentType == NULL)
			addLine( "Content-Type: text/html" );
		else
			addLine( "Content-Type: %s", contentType );
		addLine( "Connection: Closed" );
		addLine( "Content-Length: %d", contentSize );
	}
	
	void addResponce( int code, char *descr, char *content )
	{
		addResponce( code, descr, strlen(content) );
	}

	// Метод, адрес, версия протокола
	char *method, *url, *httpver;

	// Разобрать запрос
	void parseReq()
	{
		if (strs.size() == 0) return;

		char *t;

		// Первая строчка - особая
		method = strtok_s( strs[0], " ", &t );
		url = strtok_s( NULL, " ", &t );
		httpver = strtok_s( NULL, " ", &t );

		// Остальные - просто параметры
		for( int i = 1; i < strs.size(); i++ )
		{
			TKeyVal *v = new TKeyVal( strs[i], ":" );
			fields.push_back( v );
		}
	}

	// Найти параметр по имени
	char *getField( char *name )
	{
		for( int i = 0; i < fields.size(); i++ )
			if (strcmp(fields[i]->key, name) == 0)
				return fields[i]->val;

		return NULL;
	}
};
