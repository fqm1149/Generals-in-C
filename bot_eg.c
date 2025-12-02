#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <windows.h>
#include <threads.h>

__declspec(dllexport) void bot_function() {
	printf("hello,world!");
}