#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <shared.h>


void LibertarRecursos(LPVOID data) {
    ThreadArgsJogador* args = (ThreadArgsJogador*)data;
    CloseHandle(args->hPipe);
    CloseHandle(args->hThread);
	CloseHandle(args->monitorArgs->hEventoLetras);
    free(args);
    free(args->monitorArgs);
}

// funcao para se conectar ao servidor
HANDLE ConnectToServer() {
    HANDLE hPipe = CreateFile(PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Erro ao conectar ao servidor: %d\n"), GetLastError());
        return NULL;
    }
    _tprintf(_T("Conectado ao servidor com sucesso.\n"));
    return hPipe;
}


DWORD WINAPI ThreadReceberMensagens(LPVOID data) {
    ThreadArgsJogador* args = (ThreadArgsJogador*)data;
    //HANDLE hPipe = (HANDLE)param;
    Message message;


    while (1) {
        if (ReadMessage(args->hPipe, &message)) {
            switch (message.type) {
            case MSG_PONTUACAO:
                _tprintf(_T("Sua pontuação: %s\n"), message.word);
                break;
            case MSG_LISTA_JOGADORES:
                _tprintf(_T("Lista de jogadores:\n%s\n"), message.word);
                break;
            case MSG_PALAVRA_ACEITE:
                _tprintf(_T("Palavra aceite: %s -> Jogador: %s\n"), message.word, message.username);
                break;
            case MSG_PALAVRA_INVALIDA:
                _tprintf(_T("Palavra inválida!\n"));
                break;
            case MSG_JOGADOR_SAIU:
                _tprintf(_T("Jogador saiu: %s\n"), message.username);
                break;
            case MSG_JOGO_ENCERRADO:
                _tprintf(_T("O jogo foi encerrado.\n"));
                message.type = MSG_FIM_THREAD;
                WriteMessage(args->hPipe, &message);  
                //FUNCAO DE LIBERTAR RECURSOS
                LibertarRecursos(args);
                ExitProcess(0);
                break;

            case MSG_JOGADOR_NAO_ENCONTRADO:
                _tprintf(_T("Jogador não encontrado.\n"));
                break;
            case MSG_INFORMA_EXPULSAO:
                _tprintf(_T("O jogador %s foi expulso.\n"),message.username);
                break;
            case MSG_JOGADOR_EXCLUIDO:
                _tprintf(_T("Foi excluído do jogo.\n"));
                message.type = MSG_FIM_THREAD;
                WriteMessage(args->hPipe, &message);
                //FUNCAO DE LIBERTAR RECURSOS
                LibertarRecursos(args);
                ExitProcess(0);
                break;
            
            case MSG_JOGADOR_ENTROU:
                _tprintf(_T("O jogador %s entrou no jogo.\n"),message.username);
                break;
            
            case MSG_JOGADOR_PASSOU_FRENTE: 
                _tprintf(_T("O jogador %s é o novo lider com %s pontos.\n"),message.username,message.word);
                break;

            default:
                _tprintf(_T("Resposta do servidor desconhecida.\n"));
                break;
            }
        }
        else {
            _tprintf(_T("Erro ao ler resposta do servidor.\n"));
        }
    }
    ExitThread(0);
}



DWORD WINAPI ThreadMonitorLetras(LPVOID data) {
    ThreadArgsMonitor* args = (ThreadArgsMonitor*)data;
    SDATA* pShm = args->pShm;


    HANDLE hEvento = OpenEvent(SYNCHRONIZE, FALSE, EVENT_LETRAS_NAME);
    if (hEvento == NULL) {
        _tprintf(_T("[ERRO] Cliente não conseguiu abrir o evento de letras. Código: %lu\n"), GetLastError());
        return 1;
    }

    TCHAR ultimasLetras[MAX_LIMIT_LETRAS + 1] = _T("");

    while (1) {
        DWORD res = WaitForSingleObject(hEvento, INFINITE);
        if (res == WAIT_OBJECT_0) {
            if (_tcscmp(ultimasLetras, pShm->letters) != 0) {
                _tcscpy_s(ultimasLetras, _countof(ultimasLetras), pShm->letters);
                \
                // Construir string formatada
                TCHAR output[3 * MAX_LIMIT_LETRAS + 20] = _T("[PAINEL] Letras visíveis: [ ");
                for (int i = 0; i < MAX_LIMIT_LETRAS && pShm->letters[i] != '\0'; i++) {
                    if (pShm->letters[i] != _T('_')) {
                        _tcsncat_s(output, _countof(output), &pShm->letters[i], 1);
                    } else {
                        _tcscat_s(output, _countof(output), _T("_"));
                    }

                    if (i < MAX_LIMIT_LETRAS - 1) {
                        _tcscat_s(output, _countof(output), _T(" | "));
                    }
                }
                _tcscat_s(output, _countof(output), _T(" ]"));

                _tprintf(_T("%s\n"), output);
            }
        } else {
            _tprintf(_T("[ERRO] Falha ao esperar pelo evento de letras. Código: %lu\n"), GetLastError());
            break;
        }
    }

    CloseHandle(hEvento);
    return 0;
}





int _tmain(int argc, TCHAR* argv[]) 
{
    TCHAR command[MAX_COMMAND];
    TCHAR username[MAX_USERNAME];
    Message message;
   
    #ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
    #endif

    ThreadArgsJogador* pArgs = (ThreadArgsJogador*)malloc(sizeof(ThreadArgsJogador));
    if (pArgs == NULL) {
    _tprintf(_T("Erro ao alocar memória para os argumentos da thread.\n"));
    return 1;
    }


    if (argc < 2) {
        _tprintf(_T("Erro: Nome de usuário não fornecido.\nUso: %s <username>\n"), argv[0]);
        return 1;
    }

    _tcscpy_s(username, MAX_USERNAME, argv[1]);

    pArgs->hPipe = ConnectToServer();
    if (pArgs->hPipe == NULL) {
        return 1;
    }

    message.type = MSG_ENTRAR_JOGO;
    _tcscpy_s(message.username, MAX_USERNAME, username);
    if (!WriteMessage(pArgs->hPipe, &message)) {
        _tprintf(_T("Erro ao enviar mensagem de entrada.\n"));
        CloseHandle(pArgs->hPipe);
        return 1;
    }


    if (ReadMessage(pArgs->hPipe, &message)) {
        switch (message.type) {
        case MSG_CONFIRMA_ENTRADA_JOGO:
            _tprintf(_T("Bem-vindo ao jogo, %s!\n"), message.username);
            break;
        case MSG_MAX_USERS_ATINGIDO:
            _tprintf(_T("Limite de jogadores atingido. Programa encerrado!\n"));
            CloseHandle(pArgs->hPipe);
            return 1;
		case MSG_USERNAME_REPETIDO:
            _tprintf(_T("Nome do utilizador já está em uso. Programa encerrado!\n"));
            CloseHandle(pArgs->hPipe);
            return 1;
        default:
            _tprintf(_T("Erro ao entrar no jogo.\n"));
            CloseHandle(pArgs->hPipe);
            return 1;
        }

    } else {
        _tprintf(_T("Erro ao ler resposta do servidor.\n"));
        CloseHandle(pArgs->hPipe);
        return 1;
    }

    HANDLE hEvento = OpenEvent(SYNCHRONIZE, FALSE, EVENT_LETRAS_NAME);
    if (hEvento == NULL) {
        _tprintf(_T("[ERRO] Cliente não conseguiu abrir o evento de letras. Código: %lu\n"), GetLastError());
        return 1;
    }

	// Cria a thread para monitorar as letras
    HANDLE hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    if (hMap == NULL) {
        _tprintf(_T("Erro ao abrir memória partilhada. Código: %lu\n"), GetLastError());
        return 1;
    }
    SDATA* pShm = (SDATA*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SDATA));
    if (pShm == NULL) {
        _tprintf(_T("Erro ao mapear memória partilhada. Código: %lu\n"), GetLastError());
        return 1;
    }
    ThreadArgsMonitor* monitorArgs = (ThreadArgsMonitor*)malloc(sizeof(ThreadArgsMonitor));
	if (monitorArgs == NULL) {
		_tprintf(_T("Erro ao alocar memória para os argumentos do monitor.\n"));
		CloseHandle(pArgs->hPipe);
		return 1;
	}
    monitorArgs->hEventoLetras = hEvento;
	monitorArgs->pShm = pShm;

	pArgs->monitorArgs = monitorArgs;

    HANDLE hThreadLetras = CreateThread(NULL, 0, ThreadMonitorLetras, (LPVOID)monitorArgs, 0, NULL);




    // Cria a thread para escutar mensagens do servidor
    pArgs->hThread = CreateThread(NULL, 0, ThreadReceberMensagens, (LPVOID)pArgs, 0, NULL);
    if (pArgs->hThread == NULL) {
        _tprintf(_T("Erro ao criar thread de recepção.\n"));
        CloseHandle(pArgs->hPipe);
        return 1;
    }  

    while (1) {
        _tprintf(_T("Digite uma palavra ou comando (ex: :pont, :jogs, :sair): "));
        _fgetts(command, sizeof(command) / sizeof(TCHAR), stdin);
        //waitForMultipleObjets ( teclado ou evento da outra thread)
        size_t len = _tcslen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = _T('\0');
        }

        _tcscpy_s(message.word, MAX_MSG, command);

        if (command[0] == _T(':')) {
            if (_tcscmp(command, _T(":sair")) == 0) {
                message.type = MSG_SAIR_JOGO;
                WriteMessage(pArgs->hPipe, &message);
                break;
            } else if (_tcscmp(command, _T(":pont")) == 0) {
                message.type = MSG_PEDIR_PONTUACAO;
                WriteMessage(pArgs->hPipe, &message);
            } else if (_tcscmp(command, _T(":jogs")) == 0) {
                message.type = MSG_PEDIR_JOGADORES;
                WriteMessage(pArgs->hPipe, &message);
            } else {
                _tprintf(_T("Comando desconhecido.\n"));
                continue;
            }
        } else {
            message.type = MSG_INDICAR_PALAVRA;
            WriteMessage(pArgs->hPipe, &message);
        }

    }
	// Espera a thread de recepção terminar
	WaitForSingleObject(pArgs->hThread, INFINITE);	
	LibertarRecursos(&pArgs);
    return 0;
}

