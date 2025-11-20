#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "../shared/shared.h"


void atualizarPanelData(PanelData* panel, Server* server) {

    // Copiar jogadores
    WaitForSingleObject(server->hMutexServer, INFINITE);
    panel->numJogadores = server->numJogadores;
    for (DWORD i = 0; i < server->numJogadores; i++) {
        panel->users[i] = server->users[i];
    }
    ReleaseMutex(server->hMutexServer);

}

void lerOuCriarRegistry(int* maxLetras, int* ritmo) {

    HKEY hKey;
    LSTATUS res;
    DWORD tipo, valor, tam_valor = 0, estado;

    //criar chave 
    res = RegCreateKeyEx(HKEY_CURRENT_USER, REG_PATH, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &estado);

    if (res == ERROR_SUCCESS) {  // se a chave foi criada ou aberta com sucesso
        if (estado == REG_CREATED_NEW_KEY) {
            _tprintf_s(_T("Chave do registry criada: %s\n"), REG_PATH);
        }
        else if (estado == REG_OPENED_EXISTING_KEY) {
            _tprintf_s(_T("Chave do registry já existente: %s\n"), REG_PATH);
        }
    }
    else {
        _tprintf_s(_T("Erro ao criar/abrir registry. Código do erro: %ld\n"), res);
        *maxLetras = DEFAULT_MAXLETRAS;
        *ritmo = DEFAULT_RITMO;
        return;
    }

    tam_valor = sizeof(DWORD);
    res = RegQueryValueEx(hKey, _T("MAXLETRAS"), NULL, &tipo, (LPBYTE)&valor, &tam_valor);
    if (res == ERROR_SUCCESS) {
        *maxLetras = (valor > MAX_LIMIT_LETRAS) ? MAX_LIMIT_LETRAS : valor;
    }
    else {
        valor = DEFAULT_MAXLETRAS;
        RegSetValueEx(hKey, _T("MAXLETRAS"), 0, REG_DWORD, (LPBYTE)&valor, sizeof(DWORD));
        *maxLetras = valor;
        _tprintf_s(_T("MAXLETRAS não encontrado, foi definido um valor por default %d"), DEFAULT_MAXLETRAS);
    }

    res = RegQueryValueEx(hKey, _T("RITMO"), NULL, &tipo, (LPBYTE)&valor, &tam_valor);
    if (res == ERROR_SUCCESS) {
        *ritmo = valor;
    }
    else {
        valor = DEFAULT_RITMO;
        RegSetValueEx(hKey, _T("RITMO"), 0, REG_DWORD, (LPBYTE)&valor, sizeof(DWORD));
        *ritmo = valor;
        _tprintf_s(_T("RITMO não encontrado, foi definido um valor por default %d"), DEFAULT_RITMO);
    }
    RegCloseKey(hKey);

    if (*maxLetras <= 0) {
        *maxLetras = DEFAULT_MAXLETRAS;
    }
    if (*ritmo <= 0) {
        *ritmo = DEFAULT_RITMO;
    }
}

BOOL CreateSharedMemmory(SDATA** ppShm, HANDLE* pHMap, int maxLetras) {

    int i;
    //criar memoria partilhada
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SDATA), SHM_NAME);
    if (hMap == NULL) { //&& GetLastError() == ERROR_ALREADY_EXISTS) {
        _tprintf_s(_T("CreateFileMapping falhou (%d).\n"), GetLastError());
        return FALSE;
    }

    //mapear a memoria
    SDATA* pShm = (SDATA*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SDATA));
    if (pShm == NULL) {
        _tprintf_s(_T("MapViewOfFile falhou (%d).\n"), GetLastError());
        CloseHandle(hMap);
        return FALSE;
    }

    //inicializar os dados que irão ser mostrados na memoria partilhada
    for (i = 0; i < maxLetras; i++) {
        pShm->letters[i] = _T('_');
    }

    pShm->lastWord[0] = _T('\0');
    //associar aos ponteiros de saida as variaveis locais
    *pHMap = hMap;
    *ppShm = pShm;

    return TRUE;
}

BOOL validarPalavra(const TCHAR* palavra, const TCHAR* letrasVisiveis, TCHAR dicionario[][MAX_TAM_PALAVRA], int numPalavras) {
    // Verifica se a palavra existe no dicionário
    BOOL encontrada = FALSE;
    for (int i = 0; i < numPalavras; i++) {
        if (_tcsicmp(palavra, dicionario[i]) == 0) {
            encontrada = TRUE;
            break;
        }
    }
    if (!encontrada) return FALSE;

    // Verifica se a palavra pode ser formada com as letras visíveis
    TCHAR letrasTemp[DEFAULT_MAXLETRAS + 1];////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    _tcscpy_s(letrasTemp, DEFAULT_MAXLETRAS + 1, letrasVisiveis);

    for (int i = 0; palavra[i] != '\0'; i++) {
        TCHAR* pos = _tcschr(letrasTemp, palavra[i]);
        if (pos == NULL) {
            return FALSE; // Letra não encontrada
        }
        else {
            *pos = '*'; // Marca a letra como usada (nas letras visíveis)
        }
    }
    return TRUE;
}

void removerLetrasUsadas(const TCHAR* palavra, TCHAR* letrasVisiveis) {
    size_t lenPalavra = _tcslen(palavra);
    size_t lenLetras = _tcslen(letrasVisiveis);

    for (size_t i = 0; i < lenPalavra; ++i) {
        TCHAR letraAtual = palavra[i];

        // Procurar a primeira ocorrência da letra visível (não usada ainda)
        for (size_t j = 0; j < lenLetras; ++j) {
            if (letrasVisiveis[j] == letraAtual) {
                letrasVisiveis[j] = _T('_');  // marcar como usada
                break;  // passa para apróxima letra da palavra
            }
        }
    }
}

void broadcast(Server* pServer, Message* msg) {
    for (DWORD i = 0; i < pServer->numJogadores;i++) {
        if (!WriteMessage(pServer->users[i].hPipe, msg)) {
            _tprintf(_T("[ERRO] Falha ao enviar mensagem para %s\n"), pServer->users[i].username);
        }
    }
}

void carregarDicionario(const TCHAR* ficheiro, TCHAR dicionario[][MAX_TAM_PALAVRA], int* numPalavras) {
    FILE* f;
    _tfopen_s(&f, ficheiro, _T("r"));
    if (!f) {
        _tprintf(_T("Erro ao abrir o dicionário!\n"));
        exit(1);
    }

    *numPalavras = 0;
    while (_fgetts(dicionario[*numPalavras], MAX_TAM_PALAVRA, f) && *numPalavras < MAX_PALAVRAS_DICIONARIO) {
        size_t len = _tcslen(dicionario[*numPalavras]);
        if (len > 0 && dicionario[*numPalavras][len - 1] == '\n') {
            dicionario[*numPalavras][len - 1] = '\0'; // remover newline
            (*numPalavras)++;
        }
    }
    fclose(f);
}

void verificaEstadoJogo(Server* pServer) {
    WaitForSingleObject(pServer->hMutexServer, INFINITE);

    if (pServer->numJogadores >= 2 && !pServer->game_active) {
        pServer->game_active = TRUE;
        SetEvent(pServer->hEventoIniciarJogo);
        _tprintf(_T("[INFO] Jogo iniciado automaticamente. Jogadores: %d\n"), pServer->numJogadores);
    }
    else if (pServer->numJogadores <= 1 && pServer->game_active) {
        pServer->game_active = FALSE;
        ResetEvent(pServer->hEventoIniciarJogo);
        _tprintf(_T("[INFO] Jogo pausado. Jogadores insuficientes (%d).\n"), pServer->numJogadores);
    }

    ReleaseMutex(pServer->hMutexServer);
}


DWORD WINAPI atendeCliente(LPVOID data) {
    ThreadArgs* args = (ThreadArgs*)data;
    // Obter os argumentos
    HANDLE hPipe = args->hPipe;
    Server* pServer = args->pServer;
    SDATA* dadosPartilhados = args->pDadosPartilhados;
    DWORD maxLetras = args->maxLetras;
    BOOL continuaThread = TRUE;
    TCHAR dicionario[MAX_PALAVRAS_DICIONARIO][MAX_TAM_PALAVRA];
    for (DWORD i = 0; i < args->numPalavrasDicionario; i++) {
        _tcscpy_s(dicionario[i], MAX_TAM_PALAVRA, args->dicionario[i]);
    }
    DWORD numPalavrasDicionario = args->numPalavrasDicionario;
    PanelData* pPanel = args->pPanel;

    // Sleep(1000000);

    free(args);
    //Server sv;
    Message msg;
    BOOL ret;

    do {
        if (!pServer->continua) break;

        ret = ReadMessage(hPipe, &msg);
        if (!ret) {
            //experimentar fechar o handle hthread caso o jogador saia de forma bruta
            _tprintf_s(_T("[ERRO] Falha ao ler do cliente ou o cliente disconectado (READFILE)\n"));
            break;
        }

        Message resposta;
        switch (msg.type) {

            //      -------------------------------------------------- MSG_ENTRAR_JOGO ---------------------------------------------------
        case MSG_ENTRAR_JOGO: {
            BOOL repetido = FALSE;
            broadcast(pServer, &resposta);
            WaitForSingleObject(pServer->hMutexServer, INFINITE); // -------- (mutex)
            resposta.type = MSG_JOGADOR_ENTROU;
            _tcscpy_s(resposta.username, MAX_USERNAME, msg.username);
            for (DWORD i = 0; i < pServer->numJogadores; i++) {

                if (_tcscmp(pServer->users[i].username, msg.username) == 0) {
                    repetido = TRUE;
                    break;
                }
            }
            if (repetido) {
                resposta.type = MSG_USERNAME_REPETIDO;
            }
            else { // se nao for repetido, adicionamos o jogador

                if (pServer->numJogadores < MAX_USERS) {
                    _tcscpy_s(pServer->users[pServer->numJogadores].username, MAX_USERNAME, msg.username);
                    _tcscpy_s(resposta.username, MAX_USERNAME, msg.username);
                    pServer->users[pServer->numJogadores].hPipe = hPipe;
                    pServer->users[pServer->numJogadores].score = 0.0;
                    pServer->numJogadores++;
                    resposta.type = MSG_CONFIRMA_ENTRADA_JOGO;
                    ReleaseMutex(pServer->hMutexServer);// --------- (release mutex)
                    verificaEstadoJogo(pServer); // Verificar se o jogo deve comecar
                    atualizarPanelData(pPanel, pServer);

                }

                else { // se o limite de jogadores for atingido
                    resposta.type = MSG_MAX_USERS_ATINGIDO;
                    ReleaseMutex(pServer->hMutexServer);// --------- (release mutex)

                }
            }

            if (!WriteMessage(hPipe, &resposta)) {
                _tprintf_s(_T("[ERRO] Escrever no pipe! (WriteFile)\n"));
            }
            //_tprintf_s(_T("[ARBITRO] Enviei ... (WriteFile)\n"));
            break;
        }

                            //      -------------------------------------------------- MSG_SAIR_JOGO ---------------------------------------------------

        case MSG_SAIR_JOGO: {
            resposta.type = MSG_JOGO_ENCERRADO;

            WaitForSingleObject(pServer->hMutexServer, INFINITE); // -------- (mutex)
            int indice = -1;
            for (DWORD i = 0; i < pServer->numJogadores;i++) {
                if (_wcsicmp(pServer->users[i].username, msg.username) == 0) {
                    indice = (int)i;
                    break;
                }
            }

            if (indice != -1) {
                //remover jogador
                for (DWORD j = (DWORD)indice; j < pServer->numJogadores;j++) {
                    // _tcscpy_s(pServer->users[j].username, MAX_USERNAME, pServer->users[j + 1].username);
                    pServer->users[j] = pServer->users[j + 1];
                    pServer->hThreads[j] = pServer->hThreads[j + 1];
                }
                pServer->numJogadores--;
                _tprintf(_T("O jogador %s saiu do jogo!\n"), msg.username);
                atualizarPanelData(pPanel, pServer);

            }

            ReleaseMutex(pServer->hMutexServer); // ----------------------------- (release mutex)

            verificaEstadoJogo(pServer);

            if (!WriteMessage(hPipe, &resposta)) {
                _tprintf_s(_T("[ERRO] Escrever no pipe! (WriteFile)\n"));
            }
            _tprintf_s(_T("[ARBITRO] Enviei ... (WriteFile)\n"));
            Message notificaJogadores;
            notificaJogadores.type = MSG_JOGADOR_SAIU;
            _tcscpy_s(notificaJogadores.username, MAX_USERNAME, msg.username);
            broadcast(pServer, &notificaJogadores);
            break;
        }

                          //      -------------------------------------------------- MSG_INDICAR_PALAVRA ---------------------------------------------------
        case MSG_INDICAR_PALAVRA: {

            BOOL valida = FALSE;
            TCHAR palavra[MAX_MSG];

            _tcscpy_s(palavra, MAX_MSG, msg.word);

            WaitForSingleObject(pServer->hMutexServer, INFINITE);// ------------------------------------------- (mutex)

            valida = validarPalavra(palavra, dadosPartilhados->letters, dicionario, numPalavrasDicionario); // Implementar esta função
            if (valida) {
                float pontos = (float)_tcslen(palavra);

                // Atualizar score do jogador
                for (DWORD i = 0; i < pServer->numJogadores; i++) {
                    if (_tcscmp(pServer->users[i].username, msg.username) == 0) {
                        pServer->users[i].score += pontos;
                        break;
                    }
                }

                // Atualizar memória partilhada
                _tcscpy_s(dadosPartilhados->lastWord, DEFAULT_MAXLETRAS, palavra);
                removerLetrasUsadas(palavra, dadosPartilhados->letters);  // Atualiza letras visíveis ---implementa esta funcao

                resposta.type = MSG_PALAVRA_ACEITE;
                atualizarPanelData(pPanel, pServer);

            }
            else {
                float penalizacao = (float)_tcslen(palavra) * 0.5f;

                for (DWORD i = 0; i < pServer->numJogadores; i++) {
                    if (_tcscmp(pServer->users[i].username, msg.username) == 0) {
                        pServer->users[i].score -= penalizacao;
                        break;
                    }
                }

                resposta.type = MSG_PALAVRA_INVALIDA;
                atualizarPanelData(pPanel, pServer);
                if (!WriteMessage(hPipe, &resposta)) {
                    _tprintf_s(_T("[ERRO] Escrever no pipe! (WriteMessage)\n"));
                }
                ReleaseMutex(pServer->hMutexServer);
                break;
            }

            ReleaseMutex(pServer->hMutexServer); // ----------------------------------------------------------- (mutex)

            // Preencher e enviar resposta
            _tcscpy_s(resposta.word, MAX_MSG, msg.word);
            _tcscpy_s(resposta.username, MAX_USERNAME, msg.username);
            broadcast(pServer, &resposta);

            static int indiceLiderAtual = -1;

            DWORD novoLiderIndice = 0;
            float maiorPontuacaoAtual = pServer->users[0].score;

            for (DWORD i = 0; i < pServer->numJogadores; i++) {
                if (pServer->users[i].score > maiorPontuacaoAtual) {
                    novoLiderIndice = i;
                    maiorPontuacaoAtual = pServer->users[i].score;
                }
            }

            if (novoLiderIndice != indiceLiderAtual) {
                indiceLiderAtual = (int)novoLiderIndice;

                Message notificaJogadores;
                notificaJogadores.type = MSG_JOGADOR_PASSOU_FRENTE;

                _tcscpy_s(notificaJogadores.username, MAX_USERNAME, pServer->users[indiceLiderAtual].username);
                _stprintf_s(notificaJogadores.word, MAX_MSG, _T("%.2f"), pServer->users[indiceLiderAtual].score);
                broadcast(pServer, &notificaJogadores);
            }

            break;
        }

                                //------------------------------------------------MSG_PEDIR_PONTUACAO-------------------------------------------------- -
        case MSG_PEDIR_PONTUACAO: {

            int indice = -1;
            WaitForSingleObject(pServer->hMutexServer, INFINITE);
            for (int i = 0; i < (int)pServer->numJogadores;i++) {
                if (_tcscmp(pServer->users[i].username, msg.username) == 0) {
                    indice = i;
                    break;
                }
            }
            ReleaseMutex(pServer->hMutexServer);

            _tcscpy_s(resposta.username, MAX_USERNAME, msg.username);

            if (indice != -1) {
                resposta.type = MSG_PONTUACAO;
                _stprintf_s(resposta.word, MAX_MSG, _T("%.2f"), pServer->users[indice].score);
            }
            else {
                resposta.type = MSG_JOGADOR_NAO_ENCONTRADO;
                _tcscpy_s(resposta.word, MAX_MSG, _T("Jogador não encontrado!\n"));
            }


            if (!WriteMessage(hPipe, &resposta)) {
                _tprintf_s(_T("[ERRO] Não foi possível enviar a pontuação...(WRITEFILE)\n"));
            }
            else {
                _tprintf_s(_T("[ARBITRO] Pontuação enviada ao jogador '%s\n'"), msg.username);
            }
            break;
        }

                                //------------------------------------------------MSG_PEDIR_JOGADORES-------------------------------------------------- -
        case MSG_PEDIR_JOGADORES: {
            resposta.type = MSG_LISTA_JOGADORES;
            resposta.word[0] = _T('\0');

            WaitForSingleObject(pServer->hMutexServer, INFINITE);
            for (DWORD i = 0; i < pServer->numJogadores;i++) {
                _tcscat_s(resposta.word, MAX_MSG, pServer->users[i].username);
                if (i < pServer->numJogadores - 1) {
                    _tcscat_s(resposta.word, MAX_MSG, _T(", "));
                }
            }
            ReleaseMutex(pServer->hMutexServer);
            _tcscpy_s(resposta.username, MAX_USERNAME, msg.username);

            if (!WriteMessage(hPipe, &resposta)) {
                _tprintf_s(_T("[ERRO] Falha ao enviar a lista de jogadores...(WRITEFILE)\n"));
            }
            else {
                _tprintf_s(_T("[ARBITRO] Lista de jogadores enviada ao jogador '%s'"), msg.username);
            }
            break;
        }
                                //------------------------------------------------MSG_FIM_TREAD-------------------------------------------------- -
        case MSG_FIM_THREAD: {
            continuaThread = FALSE;
            break;
        }
        }
    } while (continuaThread);

    _tprintf(_T("[ATENDECLIENTE] Thread terminada.\n"));
    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    ExitThread(0);
}

DWORD WINAPI aceitaLigacoes(LPVOID data) {
    ThreadArgs* baseArgs = (ThreadArgs*)data;

    do {
        WaitForSingleObject(baseArgs->pServer->hMutexServer, INFINITE);
        if (baseArgs->pServer->numJogadores >= MAX_USERS) {
            ReleaseMutex(baseArgs->pServer->hMutexServer);
            break;
        }
        ReleaseMutex(baseArgs->pServer->hMutexServer);

        HANDLE hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, MAX_USERS,
            sizeof(Message), sizeof(Message), 3000, NULL);

        if (hPipe == INVALID_HANDLE_VALUE) {
            _tprintf_s(_T("[ERRO] Criar Named Pipe! (CreateNamedPipe) Código: %lu\n"), GetLastError());
            continue;
        }

        _tprintf_s(_T("[ÁRBITRO] À espera de ligação de um jogador...\n"));

        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (!ov.hEvent) {
            _tprintf_s(_T("[ERRO] Criar evento para overlapped! Código: %lu\n"), GetLastError());
            CloseHandle(hPipe);
            continue;
        }

        HANDLE eventos[2] = { ov.hEvent, baseArgs->pServer->hEventoCancelar };

        BOOL connected = ConnectNamedPipe(hPipe, &ov);
        if (!connected) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                // Esperar até o cliente conectar
                DWORD waitRes = WaitForMultipleObjects(2, eventos, FALSE, INFINITE);
                if (waitRes == WAIT_OBJECT_0 + 1) {
                    //ExitThread(0);
                    break;
                }
                if (waitRes != WAIT_OBJECT_0) {
                    _tprintf_s(_T("[ERRO] Timeout ou erro na espera da conexão do pipe!\n"));
                    CloseHandle(ov.hEvent);
                    CloseHandle(hPipe);
                    continue;
                }

            }
            else if (err == ERROR_PIPE_CONNECTED) {
                // O cliente já está conectado, OK
                SetEvent(ov.hEvent);
            }
            else {
                _tprintf_s(_T("[ERRO] Ligação ao leitor! (ConnectNamedPipe) Código: %lu\n"), err);
                CloseHandle(ov.hEvent);
                CloseHandle(hPipe);
                continue;
            }
        }

        CloseHandle(ov.hEvent);

        _tprintf_s(_T("[ÁRBITRO] Jogador ligado com sucesso!\n"));

        // Criar argumentos da thread
        ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        if (!args) {
            _tprintf_s(_T("[ERRO] Falha ao alocar memória para argumentos da thread.\n"));
            CloseHandle(hPipe);
            continue;
        }

        args->hPipe = hPipe;
        args->pServer = baseArgs->pServer;
        args->pDadosPartilhados = baseArgs->pDadosPartilhados;
        args->maxLetras = baseArgs->maxLetras;
        args->numPalavrasDicionario = baseArgs->numPalavrasDicionario;
        args->pPanel = baseArgs->pPanel;

        for (DWORD i = 0; i < baseArgs->numPalavrasDicionario; i++) {
            _tcscpy_s(args->dicionario[i], MAX_TAM_PALAVRA, baseArgs->dicionario[i]);
        }

        HANDLE hThread = CreateThread(NULL, 0, atendeCliente, args, 0, NULL);
        if (!hThread) {
            _tprintf_s(_T("[ERRO] Criar thread de atendimento. Código: %lu\n"), GetLastError());
            CloseHandle(hPipe);
            free(args);
            continue;
        }

        // Guardar handle da thread e atualizar número de jogadores (com mutex)
        WaitForSingleObject(baseArgs->pServer->hMutexServer, INFINITE);

        DWORD indice = baseArgs->pServer->numJogadores;
        baseArgs->pServer->hThreads[indice] = hThread;

        ReleaseMutex(baseArgs->pServer->hMutexServer);

    } while (baseArgs->pServer->continua);

    _tprintf_s(_T("[AceitaLigacoes] Thread terminada.\n"));

    ExitThread(0);
}

DWORD WINAPI ThreadGerarLetras(LPVOID param) {
    ThreadLetraArgs* args = (ThreadLetraArgs*)param;
    SDATA* pShm = args->pShm;
    HANDLE hMutexShm = args->hMutexShm;
    int maxLetras = args->maxLetras;
    int* ritmo = args->ritmo;
    BOOL* continua = &(args->pServer->continua);
    HANDLE hEventoJogo = args->pServer->hEventoIniciarJogo;

    static int posLetraAntiga = 0;

    // Criar ou abrir evento para sinalizar letras novas
    HANDLE hEventoLetras = OpenEvent(EVENT_MODIFY_STATE, FALSE, EVENT_LETRAS_NAME);
    if (hEventoLetras == NULL) {
        hEventoLetras = CreateEvent(NULL, TRUE, FALSE, EVENT_LETRAS_NAME); // manual reset
        if (hEventoLetras == NULL) {
            _tprintf_s(_T("[ERRO] Falha ao criar evento de letras (%d)\n"), GetLastError());
            free(args);
            ExitThread(1);
        }
    }

    // Alfabeto com peso para aumentar probabilidade de vogais
    const TCHAR alfabetoComPeso[] = _T("AAAAEEEIIOOUUBCDFGHJKLMNPQRSTVWXYZ");
    int alfabetoLen = (int)_tcslen(alfabetoComPeso);
    srand((unsigned int)time(NULL));

    //esperar que a variavel game_active fique true, ou esperar um evento, etc

    while (*continua) {
        _tprintf_s(_T("[LETRAS] À espera do início do jogo...\n"));
        WaitForSingleObject(hEventoJogo, INFINITE);

        if (!(*continua)) break;

        _tprintf_s(_T("[LETRAS] Jogo iniciado. A gerar letras...\n"));

        while (*continua && WaitForSingleObject(hEventoJogo, 0) == WAIT_OBJECT_0) {
            // Gerar nova letra com mais probabilidade de ser vogal
            TCHAR novaLetra = alfabetoComPeso[rand() % alfabetoLen];

            WaitForSingleObject(hMutexShm, INFINITE);

            TCHAR letrasAntes[MAX_LIMIT_LETRAS + 1];
            _tcsncpy_s(letrasAntes, _countof(letrasAntes), pShm->letters, _TRUNCATE);

            BOOL inserida = FALSE;

            // Tentar inserir nova letra na primeira posição livre
            for (int i = 0; i < maxLetras; i++) {
                if (pShm->letters[i] == _T('_')) {
                    pShm->letters[i] = novaLetra;
                    inserida = TRUE;
                    break;
                }
            }

            // Se não havia espaço, fazer scroll das letras e adicionar nova no fim
            if (!inserida) {
                pShm->letters[posLetraAntiga] = novaLetra;
                posLetraAntiga = (posLetraAntiga + 1) % maxLetras;  // para quando posLetraAntiga atingir maxLetras (por exemplo 6), voltar inicio
                // exemplo ((0 +1) % 6 = 1 ;...;6 % 6 = 0; 7 % 6 = 1;...)

// pShm->letters[maxLetras - 1] = novaLetra;
            }

            // Se as letras mudaram, sinalizar aos clientes
            if (_tcscmp(letrasAntes, pShm->letters) != 0) {
                SetEvent(hEventoLetras);
            }

            ReleaseMutex(hMutexShm);
            Sleep((*ritmo) * 1000);
        }

        if (*continua) {
            _tprintf_s(_T("[LETRAS] Jogo pausado. A aguardar novo início...\n"));
        }
    }

    _tprintf_s(_T("[LETRAS] Thread terminada.\n"));

    CloseHandle(hEventoLetras);
    free(args);
    ExitThread(0);
}


int _tmain(int argc, TCHAR* argv[]) {
    int maxLetras = 0, ritmo = 0;
    SDATA* pShm = NULL;
    HANDLE hMap = NULL;
    TCHAR dicionario[MAX_PALAVRAS_DICIONARIO][MAX_TAM_PALAVRA];
    DWORD numPalavras = 0;
    Server status;
    srand((unsigned)time(NULL));

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    // 1. Carregar configs
    lerOuCriarRegistry(&maxLetras, &ritmo);
    _tprintf_s(_T("MAXLETRAS = %d | RITMO = %d\n"), maxLetras, ritmo);

    // 2. Carregar dicionário
    carregarDicionario(_T("dicionario.txt"), dicionario, &numPalavras);

    // 3. Criar memória partilhada
    if (!CreateSharedMemmory(&pShm, &hMap, maxLetras)) {
        _tprintf_s(_T("Erro ao criar memória partilhada.\n"));
        return 1;
    }


    //criar memoria partilhada para o painel
 ///////////////////////////////////////////////////////////////////////////////
    PanelData* pPanelData = NULL;
    HANDLE hPanelMem = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(PanelData),
        SHM_NAME_PANEL
    );

    if (hPanelMem == NULL) {
        _tprintf_s(_T("[ERRO] Falha ao criar memória partilhada do painel. Código: %lu\n"), GetLastError());
        return 1;
    }

    pPanelData = (PanelData*)MapViewOfFile(
        hPanelMem,
        FILE_MAP_WRITE,
        0,
        0,
        sizeof(PanelData)
    );

    if (pPanelData == NULL) {
        _tprintf_s(_T("[ERRO] Falha ao mapear memória do painel. Código: %lu\n"), GetLastError());
        CloseHandle(hPanelMem);
        return 1;
    }

    ZeroMemory(pPanelData, sizeof(PanelData));

    /////////////////////////////////


    // 4. Inicializar estado global
    // 
    // Inicializar status (Server)
    status.continua = TRUE;
    status.game_active = FALSE;
    status.numJogadores = 0;


    // 2. Criar eventos e mutexes
    status.hEventoCancelar = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (status.hEventoCancelar == NULL) {
        _tprintf_s(_T("[ERRO] Falha ao criar evento de cancelamento. Código: %lu\n"), GetLastError());
        return 1;
    }

    status.hEventoIniciarJogo = CreateEvent(NULL, TRUE, FALSE, EVENT_NOME_JOGO);
    if (status.hEventoIniciarJogo == NULL) {
        _tprintf_s(_T("[ERRO] Falha ao criar evento de início do jogo. Código: %lu\n"), GetLastError());
        return 1;
    }

    status.hMutexServer = CreateMutex(NULL, FALSE, MUTEX_SRV_NAME);
    if (status.hMutexServer == NULL) {
        _tprintf_s(_T("[ERRO] Falha ao criar mutex do servidor. Código: %lu\n"), GetLastError());
        return 1;
    }

    status.hMutexShm = CreateMutex(NULL, FALSE, MUTEX_SHM_NAME);
    if (status.hMutexShm == NULL) {
        _tprintf_s(_T("[ERRO] Falha ao criar mutex da memória partilhada. Código: %lu\n"), GetLastError());
        return 1;
    }

    // Preencher argumentos da thread aceitaLigacoes
    ThreadArgs baseArgs;
    baseArgs.pServer = &status;
    baseArgs.pDadosPartilhados = pShm;
    baseArgs.maxLetras = maxLetras;
    baseArgs.numPalavrasDicionario = numPalavras;
    baseArgs.pPanel = pPanelData;
    for (DWORD i = 0; i < numPalavras; i++) {
        _tcscpy_s(baseArgs.dicionario[i], MAX_TAM_PALAVRA, dicionario[i]);
    }

    // Criar thread aceitaLigacoes
    HANDLE hThreadAceita = CreateThread(NULL, 0, aceitaLigacoes, &baseArgs, 0, NULL);
    if (!hThreadAceita) {
        _tprintf_s(_T("[ERRO] Falha ao criar thread de ligações. Código: %lu\n"), GetLastError());
        return 1;
    }

    // Preencher argumentos da thread gerarLetras 
    ThreadLetraArgs* letraArgs = (ThreadLetraArgs*)malloc(sizeof(ThreadLetraArgs));
    if (!letraArgs) {
        _tprintf_s(_T("[ERRO] Falha ao alocar memória para argumentos de letras.\n"));
        return 1;
    }
    letraArgs->pShm = pShm;
    letraArgs->hMutexShm = status.hMutexShm;
    letraArgs->maxLetras = maxLetras;
    letraArgs->ritmo = &ritmo;
    letraArgs->game_active = &status.game_active;
    letraArgs->pServer = &status;
    // Criar thread gerar letras (com malloc!)

    HANDLE hThreadLetras = CreateThread(NULL, 0, ThreadGerarLetras, letraArgs, 0, NULL);
    if (!hThreadLetras) {
        _tprintf_s(_T("[ERRO] Falha ao criar thread de geração de letras. Código: %lu\n"), GetLastError());
        free(letraArgs);
        return 1;
    }


    _tprintf_s(_T("Servidor a correr...\n"));

    TCHAR input[MAX_MSG];
    DWORD ritmoAtual = ritmo;

    do {
        //input do teclado
        _tprintf_s(_T("Introduza um comando (encerrar para terminar): "));
        _fgetts(input, sizeof(input) / sizeof(TCHAR), stdin); // Lê o comando ou palavra do jogador

        // Remover \n do fim
        size_t len = _tcslen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        //separar o comando e argumento
        TCHAR comando[MAX_COMMAND];
        TCHAR argumento[MAX_USERNAME];
        argumento[0] = '\0';

        _stscanf_s(input, _T("%s %s"), comando, (unsigned)_countof(comando), argumento, (unsigned)_countof(argumento));

        if (_tcscmp(comando, _T("listar")) == 0) {
            WaitForSingleObject(status.hMutexServer, INFINITE);
            _tprintf_s(_T("\n[Jogadores em jogo: %d]\n"), status.numJogadores);
            for (DWORD i = 0; i < status.numJogadores; i++) {
                _tprintf_s(_T(" - %s (%.2f pontos)\n"), status.users[i].username, status.users[i].score);
            }
            ReleaseMutex(status.hMutexServer);
        }

        else if (_tcscmp(comando, _T("excluir")) == 0) {
            if (argumento[0] == _T('\0')) {
                _tprintf_s(_T("Uso correto: excluir <username>\n"));
                continue;
            }
            WaitForSingleObject(status.hMutexServer, INFINITE);
            int indice = -1;
            for (DWORD i = 0; i < status.numJogadores; i++) {
                if (_tcscmp(status.users[i].username, argumento) == 0) {
                    indice = (int)i;
                    break;
                }
            }

            if (indice != -1) {
                Message aviso;
                aviso.type = MSG_JOGADOR_EXCLUIDO;
                _tcscpy_s(aviso.username, MAX_USERNAME, status.users[indice].username);
                WriteMessage(status.users[indice].hPipe, &aviso);

                // NÃO fazer FlushFileBuffers, DisconnectNamedPipe, CloseHandle ainda!

                _tprintf_s(_T("Esperando que o jogador '%s' encerre...\n"), argumento);

                // Opcional: esperar pela thread
                WaitForSingleObject(status.hThreads[indice], INFINITE);
                CloseHandle(status.hThreads[indice]);

                Message notificaJogadores;
                notificaJogadores.type = MSG_INFORMA_EXPULSAO;
                _tcscpy_s(notificaJogadores.username, MAX_USERNAME, argumento);
                _stprintf_s(notificaJogadores.word, MAX_MSG, _T("\nJogador '%s' saiu do jogo."), argumento);

                //remover o jogador do array
                for (DWORD j = indice; j < status.numJogadores - 1; j++) {
                    status.users[j] = status.users[j + 1];
                    status.hThreads[j] = status.hThreads[j + 1];
                }
                status.numJogadores--;

                //fazer broadcast para os restantes jogadores
                broadcast(&status, &notificaJogadores);
                atualizarPanelData(pPanelData, &status);
                _tprintf_s(_T("Jogador '%s' excluído com sucesso.\n"), argumento);
            }
            else {
                _tprintf_s(_T("Jogador '%s' não encontrado.\n"), argumento);
            }

            ReleaseMutex(status.hMutexServer);

        }
        else if (_tcscmp(comando, _T("iniciarbot")) == 0) {
            if (argumento[0] == _T('\0')) {
                _tprintf_s(_T("Uso: iniciarbot <username>\n"));
                continue;
            }

            // tempo de reação aleatório entre 5 e 30 segundos
            int tempoReacao = (rand() % 26) + 5; // 5 a 30 inclusive

            // construir linha de comando para o processo bot
            TCHAR cmdLine[MAX_PATH];
            _stprintf_s(cmdLine, MAX_PATH, _T("bot.exe %s %d"), argumento, tempoReacao);

            STARTUPINFO si = { sizeof(si) };
            PROCESS_INFORMATION pi;

            // criar o processo do bot
            if (CreateProcess(
                NULL,         // Nome do executável
                cmdLine,      // Linha de comando completa
                NULL, NULL,   // Segurança
                FALSE,        // Herança de handles
                0,            // Flags
                NULL, NULL,   // Ambiente e diretório
                &si, &pi)) {
                _tprintf_s(_T("Bot '%s' iniciado com tempo de reação %d segundos (PID: %lu)\n"),
                    argumento, tempoReacao, pi.dwProcessId);

                // Fechar handles que não vamos usar
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            else {
                _tprintf_s(_T("Erro ao iniciar o bot (%lu).\n"), GetLastError());
            }
        }
        else if (_tcscmp(comando, _T("acelerar")) == 0) {
            ritmoAtual++;
            _tprintf_s(_T("Novo ritmo: %d segundos.\n"), ritmoAtual);

        }
        else if (_tcscmp(comando, _T("travar")) == 0) {
            if (ritmoAtual > 1) ritmoAtual--;
            _tprintf_s(_T("Novo ritmo: %d segundos.\n"), ritmoAtual);
        }
        else if (_tcscmp(comando, _T("encerrar")) == 0) {

            WaitForSingleObject(status.hMutexServer, INFINITE);
            _tprintf_s(_T("Encerrando o jogo...\n"));

            Message fim;
            fim.type = MSG_JOGO_ENCERRADO;

            broadcast(&status, &fim);

            status.continua = FALSE;

            SetEvent(status.hEventoCancelar);  // <- aqui!

            ReleaseMutex(status.hMutexServer);
        }

    } while (status.continua);

    //esperar que as threadsAtendeCliente terminem (PRECISAMOS DE EVENTO PARA SINALIZAR?)
    WaitForMultipleObjects(status.numJogadores, status.hThreads, TRUE, INFINITE); //verificar
    for (DWORD i = 0; i < status.numJogadores; i++) {
        FlushFileBuffers(status.users[i].hPipe);
        DisconnectNamedPipe(status.users[i].hPipe);
        //CloseHandle(status.users[i].hPipe); // JA FOI FECHADO NA THREAD ATENDECLIENTE (ACHO)
        CloseHandle(status.hThreads[i]);
    }

    status.numJogadores = 0;


    //esperar que a threadAceitalientes termine
    WaitForSingleObject(hThreadAceita, INFINITE);
    CloseHandle(hThreadAceita);

    //esperar que a threadGerarLetras termine
    status.game_active = FALSE;
    SetEvent(status.hEventoCancelar);
    SetEvent(status.hEventoIniciarJogo); // desbloquear ThreadGerarLetras se estiver à espera

    WaitForSingleObject(hThreadLetras, INFINITE);
    CloseHandle(hThreadLetras);


    // 7. Libertar recursos
    UnmapViewOfFile(pShm);
    CloseHandle(hMap);
    CloseHandle(status.hMutexServer);
    CloseHandle(status.hMutexShm);
    CloseHandle(status.hEventoCancelar);
    CloseHandle(status.hEventoIniciarJogo);
    _tprintf_s(_T("[MAIN] Thread terminada.\n"));

    return 0;
}
