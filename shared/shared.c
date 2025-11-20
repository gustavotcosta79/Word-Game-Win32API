
#include <windows.h>
#include "shared.h"
#include <tchar.h>   

BOOL WriteMessage(HANDLE hPipe, Message* message) {
    OVERLAPPED overlapped = { 0 };
    DWORD bytesWritten = 0;
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    overlapped.hEvent = hEvent;

    if (!WriteFile(hPipe, message, sizeof(Message), NULL, &overlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            DWORD result = WaitForSingleObject(hEvent, INFINITE);
            if (result == WAIT_OBJECT_0) {
                if (GetOverlappedResult(hPipe, &overlapped, &bytesWritten, FALSE)) {
                    CloseHandle(hEvent);
                    return TRUE;
                }
            }
        }
        CloseHandle(hEvent);
        return FALSE;
    }
    CloseHandle(hEvent);
    return TRUE;
    }

BOOL ReadMessage(HANDLE hPipe, Message* message) {
    OVERLAPPED overlapped = { 0 };
    DWORD bytesRead = 0;
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    overlapped.hEvent = hEvent;

    if (!ReadFile(hPipe, message, sizeof(Message), NULL, &overlapped)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            DWORD result = WaitForSingleObject(hEvent, INFINITE);
            if (result == WAIT_OBJECT_0) {
                if (GetOverlappedResult(hPipe, &overlapped, &bytesRead, FALSE)) {
                    CloseHandle(hEvent);
                    return TRUE;
                }
            }
        }
        CloseHandle(hEvent);
        return FALSE;
    }
    CloseHandle(hEvent);
    return TRUE;
}  







