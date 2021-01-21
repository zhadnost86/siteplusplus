#include <Windows.h>;
#include <wbemidl.h>

// Работа с интерфейсом WMI
class TWMIObjectEnumerator
{
protected:
	VARIANT prop;
public:
	IEnumWbemClassObject *enm;
	IWbemClassObject *obj;

	// Получить текущую запись
	bool getNext();

	// Получить для текущей записи значение
	wchar_t *getBstrProp( wchar_t *name )
	{
		obj->Get( name, 0, &prop, 0, 0 );

		return prop.bstrVal;
	}

	int getIntProp( wchar_t *name )
	{
		obj->Get( name, 0, &prop, 0, 0 );

		return prop.intVal;
	}
};

bool initWmi();
bool queryWmi( wchar_t *query, TWMIObjectEnumerator **enumerator );

HRESULT lasthr();