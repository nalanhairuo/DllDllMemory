
#include <stdio.h>
#include <windows.h>
typedef int(*testFunction)();
testFunction dllFunction;

typedef HMODULE(*LoadInjectModule)();
LoadInjectModule dllFunction2;

int main(void)
{
    HINSTANCE his = LoadLibraryA("LoadDll.dll");   //���ڼ���dll

    if (his == NULL)
    {
        printf("load dll failed!");
    }

    dllFunction = (testFunction)GetProcAddress(his, "example");
    dllFunction();

    dllFunction2 = (LoadInjectModule)GetProcAddress(his, "LoadInjectModule");
    dllFunction2();

    FreeLibrary(his);                              //�ͷ�dll
    system("pause");

    return 0;
}