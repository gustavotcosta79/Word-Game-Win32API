#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "../shared/shared.h"
#include "resource.h"

#define ID_TIMER 1
#define MAX_BUFFER 4096
#define DEFAULT_MAX_TO_SHOW 10

TCHAR szProgName[] = TEXT("PainelSO2Tp");


PanelData * pPanel = NULL;
SDATA* pSDATA = NULL;
TCHAR displayBuffer[MAX_BUFFER];
int maxToShow = DEFAULT_MAX_TO_SHOW;

// Prototypes
LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);
void AtualizaPainel(HWND hwnd);
void MostraInfo(HWND hwnd);

// Função principal
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc;
    wc.hInstance = hInst;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpszClassName = szProgName;
    wc.lpfnWndProc = TrataEventos;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_INFORMATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(PanelData);
    wc.hbrBackground = CreateSolidBrush(RGB(255,255,255));

   
	if (!RegisterClassEx(&wc))
		return(0);

    HWND hwnd = CreateWindow(szProgName,
                            _T("Painel do Jogo"),
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            (HWND)HWND_DESKTOP,
                            (HMENU)NULL,
                            (HINSTANCE)hInst, 0);
    if (!hwnd) {
        MessageBox(NULL, _T("Erro ao criar a janela principal."), _T("Erro"), MB_ICONERROR);
        return -1;
    }

    PanelData* ptd = (PanelData*)GetWindowLongPtr(hwnd,0);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Abrir memória partilhada SDATA
    HANDLE hMapSDATA = OpenFileMapping(FILE_MAP_READ, FALSE, SHM_NAME);
    if (!hMapSDATA) {
        MessageBox(hwnd, _T("Erro ao abrir memória SDATA."), _T("Erro"), MB_ICONERROR);
        return -1;
    }
    pSDATA = (SDATA*)MapViewOfFile(hMapSDATA, FILE_MAP_READ, 0, 0, 0);

    // Abrir memória partilhada PanelData
    HANDLE hMapPanel = OpenFileMapping(FILE_MAP_READ, FALSE, SHM_NAME_PANEL);
    if (!hMapPanel) {
        MessageBox(hwnd, _T("Erro ao abrir memória PanelData."), _T("Erro"), MB_ICONERROR);
        return -1;
    }
    pPanel = (PanelData*)MapViewOfFile(hMapPanel, FILE_MAP_READ, 0, 0, 0);

    SetTimer(hwnd, ID_TIMER, 1000, NULL);  // Atualização a cada 1s

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnmapViewOfFile(pSDATA);
    UnmapViewOfFile(pPanel);
    CloseHandle(hMapSDATA);
    CloseHandle(hMapPanel);

    return (int)msg.wParam;
}



LRESULT CALLBACK TrataEventos(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DWORD res;
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_RED:
            AtualizaPainel(hwnd);
            break;
        case ID_INFO:
            MostraInfo(hwnd);
            break;
        case ID_SAIR:
           res =  MessageBox(hwnd, _T("Pretende mesmo sair?"), _T("Confirmação"), MB_YESNO | MB_ICONQUESTION);
           if (res == IDYES){
            PostQuitMessage(0);
           }
            break;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            res = MessageBox(hwnd, _T("Pretende mesmo sair?"), _T("Confirmação"), MB_YESNO | MB_ICONQUESTION);
            if (res == IDYES) {
                DestroyWindow(hwnd);
            }            
        }
        break;
    case WM_TIMER:
        OutputDebugString(_T("Timer ativado\n"));
        AtualizaPainel(hwnd);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Escrever texto com múltiplas linhas
        RECT rect;
        GetClientRect(hwnd, &rect);
        DrawText(hdc, displayBuffer, -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void AtualizaPainel(HWND hwnd) {
    if (!pSDATA || !pPanel) return;

    _stprintf_s(displayBuffer, MAX_BUFFER,
        _T("Última Palavra: %s\n\nLetras Visíveis: %s\n\nJogadores (máx. %d):\n"),
        pSDATA->lastWord, pSDATA->letters, maxToShow);

    DWORD mostrar = min(maxToShow, pPanel->numJogadores);
    for (DWORD i = 0; i < mostrar; i++) {
        _stprintf_s(displayBuffer + _tcslen(displayBuffer), MAX_BUFFER - _tcslen(displayBuffer),
            _T("- %s: %.1f pontos\n"),
            pPanel->users[i].username,
            pPanel->users[i].score);
    }

    InvalidateRect(hwnd, NULL, TRUE);  // Redesenhar janela
}



void MostraInfo(HWND hwnd) {
    MessageBox(hwnd,_T("Duarte Santos - 2022149622\nGustavo Costa - 2023145800"), _T("AUTORES:"), MB_OK | MB_ICONINFORMATION);
   
}