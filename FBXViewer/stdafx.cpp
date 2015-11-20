// stdafx.cpp : ֻ������׼�����ļ���Դ�ļ�
// FBXViewer.pch ����ΪԤ����ͷ
// stdafx.obj ������Ԥ����������Ϣ

#include "stdafx.h"

// TODO: �� STDAFX.H ��
// �����κ�����ĸ���ͷ�ļ����������ڴ��ļ�������


#pragma region debug functions
void _DebugAssert(bool expression, const char *Text, ...)
{
    if(expression)  //�����ʽ�Ƿ�Ϊ�棬��Ϊ��������DebugUtility::Assert
    {
        return;
    }
    char CaptionText[12];
    char ErrorText[2048];
    va_list valist;

    strcpy_s(CaptionText, 11, "Error");

    // Build variable text buffer
    va_start(valist, Text);
    vsprintf_s(ErrorText, 1900, Text, valist);
    va_end(valist);

    // Display the message box
    MessageBoxA(NULL, ErrorText, CaptionText, MB_OK | MB_ICONEXCLAMATION);
    OutputDebugStringA(ErrorText);
}

void _DebugAssert(bool expression, const wchar_t *Text, ...)
{
    if(expression)  //�����ʽ�Ƿ�Ϊ�棬��Ϊ��������DebugUtility::Assert
    {
        return;
    }
    wchar_t CaptionText[12];
    wchar_t ErrorText[2048];
    va_list valist;

    wcscpy_s(CaptionText, 11, L"Error");

    // Build variable text buffer
    va_start(valist, Text);
    vswprintf_s(ErrorText, 2000, Text, valist);
    va_end(valist);

    // Display the message box
    MessageBoxW(NULL, ErrorText, CaptionText, MB_OK | MB_ICONEXCLAMATION);
    OutputDebugStringW(ErrorText);

	DebugBreak();
}

void DebugPrintf(const char *Text, ...)
{
    char _Text[2048];
    va_list valist;

    // Build variable text buffer
    va_start(valist, Text);
    vsprintf_s(_Text, 2000, Text, valist);
    va_end(valist);

    // write to debug console
    OutputDebugStringA(_Text);
}

void DebugPrintf(const wchar_t *Text, ...)
{
    wchar_t _Text[2048];
    va_list valist;

    // Build variable text buffer
    va_start(valist, Text);
    vswprintf_s(_Text, 2000, Text, valist);
    va_end(valist);

    // write to debug console
    OutputDebugStringW(_Text);
}

#define TEST_FILE_PATH "FBX/inputFile.txt"
const void GetTestFileName(char* buffer)
{
    FILE* fp = fopen(TEST_FILE_PATH, "r");
    DebugAssert(NULL != fp, "open file %s failed\n", TEST_FILE_PATH);
    fgets(buffer, MAX_PATH, fp);
    fclose(fp);
}

const void GetTestFileNameW(wchar_t* buffer)
{
    FILE* fp = fopen(TEST_FILE_PATH, "r");
    DebugAssert(NULL != fp, "open file %s failed\n", TEST_FILE_PATH);
    fgetws(buffer, MAX_PATH, fp);
    fclose(fp);
}
#pragma endregion

#pragma region �ַ���ת��
//ת�����ֽ��ַ���ʽ�ַ���strΪ���ַ���ʽ�ַ���
//������
//str ��ת���Ķ��ֽ��ַ���ʽ�ַ���
//����ֵ��ת����Ŀ��ַ���ʽ�ַ���
std::wstring CStr2WStr(std::string str)
{
    int len =  MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, NULL);
    wchar_t* buffer = new wchar_t[len+1];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, len);
    buffer[len] = L'\0';
    std::wstring return_value = std::wstring(buffer);
    delete[] buffer;
    return return_value;
}

//ת�����ַ���ʽ�ַ���strΪ���ֽ��ַ���ʽ�ַ���
std::string WStr2CStr(const std::wstring &str)
{
    //��ȡ�������Ĵ�С��������ռ䣬��������С�ǰ��ֽڼ����
    int len=WideCharToMultiByte(CP_ACP,0,str.c_str(),str.size(),NULL,0,NULL,NULL);
    char *buffer=new char[len+1];
    WideCharToMultiByte(CP_ACP,0,str.c_str(),str.size(),buffer,len,NULL,NULL);
    buffer[len]='\0';
    //ɾ��������������ֵ
    std::string return_value(buffer);
    delete[] buffer;
    return return_value;
}
#pragma endregion

#pragma region �������Ƚ�
bool floatEqual(float a, float b, float epsilon)
{
    return fabs(a - b) <= ( (fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

bool floatGreaterThan(float a, float b, float epsilon)
{
    return (a - b) > ( (fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

bool floatLessThan(float a, float b, float epsilon)
{
    return (b - a) > ( (fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}
#pragma endregion