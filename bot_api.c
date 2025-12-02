#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <windows.h>
#include <threads.h>

WCHAR* GetBotDLL() {
    static WCHAR dllPath[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = L"Gen_in_C Bot Files (*.dll)\000*.dll\000";
    ofn.lpstrFile = dllPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = L"选择Bot文件";
    if (GetOpenFileNameW(&ofn)) {
        return dllPath;
    }
    else {
        dllPath[0] = '\0'; // 未选择文件时返回空字符串
        return dllPath;
    }
}