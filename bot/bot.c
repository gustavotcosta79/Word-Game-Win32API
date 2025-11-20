#include <stdlib.h>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>
#include "../shared/shared.h" 

typedef struct {
    HANDLE hPipe;
    TCHAR username[MAX_USERNAME];
    BOOL* ativo;
} BotThreadParams;


DWORD WINAPI ListenerThread(LPVOID lpParam) {
    BotThreadParams* params = (BotThreadParams*)lpParam;
    Message resposta;

    while (*(params->ativo)) {
        if (ReadMessage(params->hPipe, &resposta)) {

            switch (resposta.type) {
            case MSG_PALAVRA_ACEITE:
                //_tprintf(_T("[BOT] Palavra aceite e pontuada: %s\n"), palavra);
                break;
            case MSG_PALAVRA_INVALIDA:
                // _tprintf(_T("[BOT] Palavra inválida: %s\n"), palavra);
                break;
            case MSG_JOGO_ENCERRADO:
                //_tprintf(_T("[BOT] O jogo foi encerrado.\n"));
                resposta.type = MSG_FIM_THREAD;
                WriteMessage(params->hPipe, &resposta);
                *params->ativo = FALSE;
                break;
            case MSG_JOGADOR_EXCLUIDO:
                //_tprintf(_T("[BOT]Foi excluído do jogo.\n"));
                resposta.type = MSG_FIM_THREAD;
                WriteMessage(params->hPipe, &resposta);
                *params->ativo = FALSE;
                //WriteMessage(hPipe, &resposta);   
                break;
            default:
                break;
            }
        }
        else {
            break; // erro de leitura ou pipe fechado
        }
    }
    free(lpParam);

    ExitThread(0);
}


void bot_loop(HANDLE hPipe, TCHAR* username, int tempoReacao, TCHAR dicionario[][MAX_TAM_PALAVRA], int numPalavras) {
    BOOL ativo = TRUE;

    // Criar thread para escutar mensagens do árbitro
    BotThreadParams* params = (BotThreadParams*)malloc(sizeof(BotThreadParams));
    params->hPipe = hPipe;
    _tcscpy_s(params->username, MAX_USERNAME, username);
    params->ativo = &ativo;

    HANDLE hThread = CreateThread(NULL, 0, ListenerThread, params, 0, NULL);

    while (ativo) {
        int slept = 0;
        int totalSleep = tempoReacao * 1000;
        while (slept < totalSleep && ativo) {
            Sleep(100);
            slept += 100;
        }

        if (!ativo) break;

        // Palavra aleatória
        int i = rand() % numPalavras;
        Message tentativa = { 0 };
        tentativa.type = MSG_INDICAR_PALAVRA;
        _tcscpy_s(tentativa.username, MAX_USERNAME, username);
        _tcscpy_s(tentativa.word, MAX_MSG, dicionario[i]);

        //WriteMessage(hPipe, &tentativa);
        if (WriteMessage(hPipe, &tentativa) == FALSE) {
            //_tprintf(_T("[BOT] Erro ao enviar palavra: %s\n"), dicionario[i]);
            ativo = FALSE;
        }
        else {
            //_tprintf(_T("[BOT] Palavra enviada: %s\n"), dicionario[i]);
        }
    }

    // Espera pela thread listener terminar (opcional)
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
}

int _tmain(int argc, TCHAR* argv[]) {
    if (argc != 3) {
        //_tprintf(_T("Uso: bot.exe <username> <tempoReacao>\n"));
        return 1;
    }

    // Copiar o username para um buffer com tamanho conhecido
    TCHAR username[MAX_USERNAME];
    _tcscpy_s(username, MAX_USERNAME, argv[1]);

    int tempoReacao = _tstoi(argv[2]);

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    // Abrir pipe
    HANDLE hPipe = CreateFile(
        PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        //_tprintf(_T("Erro ao ligar ao árbitro via pipe. Código: %d\n"), GetLastError());
        return 1;
    }

    // Enviar pedido de entrada no jogo
    Message msg = { 0 };
    msg.type = MSG_ENTRAR_JOGO;
    _tcscpy_s(msg.username, MAX_USERNAME, username);
    WriteMessage(hPipe, &msg);

    // Esperar confirmação
    Message resposta;
    if (!ReadMessage(hPipe, &resposta) || resposta.type != MSG_CONFIRMA_ENTRADA_JOGO) {
        //_tprintf(_T("Erro: Entrada no jogo não foi aceite (%d)\n"), resposta.type);
        CloseHandle(hPipe);
        return 1;
    }

    //_tprintf(_T("[BOT] Entrou no jogo com username '%s'\n"), username);

    TCHAR dicionario[10][MAX_TAM_PALAVRA] = {
        _T("casa"), _T("pato"), _T("gato"),
        _T("roda"), _T("pipa"), _T("bola"),
        _T("barco"), _T("rato"), _T("lago"), _T("nuvem")
    };

    srand((unsigned int)time(NULL));

    bot_loop(hPipe, username, tempoReacao, dicionario, 10);

    //_tprintf(_T("[BOT] Saiu do jogo\n"));
    CloseHandle(hPipe);
    return 0;
}