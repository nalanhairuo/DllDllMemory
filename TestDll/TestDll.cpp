
#include <stdio.h>
#include <windows.h>
typedef bool(*testFunction)();

testFunction dllFunction;

int main(void)
{
    HINSTANCE his = LoadLibraryA("LoadDll.dll");   //���ڼ���dll

    if (his == NULL)
    {
        printf("load dll failed!");
    }


    FreeLibrary(his);                              //�ͷ�dll
    system("pause");

    return 0;
}