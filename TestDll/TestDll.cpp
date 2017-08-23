
#include <stdio.h>
#include <windows.h>
typedef bool(*testFunction)();

testFunction dllFunction;

int main(void)
{
    HINSTANCE his = LoadLibraryA("LoadDll.dll");   //用于加载dll

    if (his == NULL)
    {
        printf("load dll failed!");
    }


    FreeLibrary(his);                              //释放dll
    system("pause");

    return 0;
}