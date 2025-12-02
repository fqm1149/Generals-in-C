#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <windows.h>
#include <threads.h>

char* GetBotDLL() {
    static char dllPath[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Gen_in_C Bot Files (*.gbt)\0*.gbt\0";
    ofn.lpstrFile = dllPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "选择Bot文件";

    if (GetOpenFileNameA(&ofn)) {
        return dllPath;
    }
    else {
        dllPath[0] = '\0'; // 未选择文件时返回空字符串
        return dllPath;
    }
}