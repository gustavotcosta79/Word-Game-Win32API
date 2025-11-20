#pragma once

// ---------- Constantes gerais ----------
#define MAX_USERS 20
#define MAX_USERNAME 20
#define MAX_COMMAND 60
#define MAX_MSG 500

// ---------- Definições por defeito ----------
#define DEFAULT_MAXLETRAS 6
#define DEFAULT_RITMO 3
#define MAX_LIMIT_LETRAS 12

// ---------- Dicionário ----------
#define MAX_PALAVRAS_DICIONARIO 200
#define MAX_TAM_PALAVRA 20

// ---------- Named Pipe ----------
#define PIPE_NAME _T("\\\\.\\pipe\\TrabSO2")

// ---------- Shared Memory ----------
#define SHM_NAME _T("memory")
#define SHM_NAME_PANEL _T("panel_memory")

// ---------- Eventos e Mutex ----------
#define EVENT_LETRAS_NAME _T("event_letras")
#define EVENT_NOME_JOGO _T("event_jogo")
#define MUTEX_SRV_NAME _T("mutex_srv")
#define MUTEX_SHM_NAME _T("mutex_shm")

// ---------- Registo (Registry) ----------
#define REG_PATH _T("Software\\TrabSO2")

// Mensagens entre árbitro <-> jogador
typedef enum {
    // Jogador → Árbitro                                                         arbitro     jogador
    MSG_ENTRAR_JOGO,           // Jogador pede para entrar no jogo                  X       
    MSG_SAIR_JOGO,             // Jogador quer sair do jogo                         X       
    MSG_INDICAR_PALAVRA,       // Jogador envia uma palavra                         X
    MSG_PEDIR_PONTUACAO,       // Jogador pede a própria pontuação                  X
    MSG_PEDIR_JOGADORES,       // Jogador pede lista de jogadores                   X
    MSG_FIM_THREAD,             // Jogador pede para a thread terminar                           X


    // Árbitro → Jogador
    MSG_CONFIRMA_ENTRADA_JOGO, // Árbitro confirma entrada no jogo                  X
    MSG_PONTUACAO,             // Árbitro envia pontuação do jogador                X
    MSG_LISTA_JOGADORES,       // Árbitro envia lista atual de jogadores            X
    MSG_PALAVRA_ACEITE,        // Palavra indicada foi aceite e pontuada            X
    MSG_PALAVRA_INVALIDA,      // Palavra inválida (erro)                           X
    MSG_NOVA_LETRA,            // Nova letra adicionada ao vetor visível            
    MSG_JOGADOR_ENTROU,        // Notificação de novo jogador (broadcast)     
    MSG_JOGADOR_SAIU,          // Notificação de jogador que saiu (broadcast)       x
    MSG_JOGADOR_PASSOU_FRENTE, // Notificação de mudança de liderança (broadcast) 
    MSG_JOGO_ENCERRADO,        // Notificação de fim do jogo (broadcast)            X
    MSG_USERNAME_REPETIDO,     // Username já usado                                 X
    MSG_MAX_USERS_ATINGIDO,     // Limite de jogadores atingido                     X   
    MSG_JOGADOR_NAO_ENCONTRADO, // Jogador não encontrado                           X
    MSG_JOGADOR_EXCLUIDO,       // Jogador excluído pelo arbitro                    X
    MSG_INFORMA_EXPULSAO        //informa jogadores sobre expulsao                  X
} MessageType;

//typedef struct {
//    TCHAR letters [MAX_LIMIT_LETRAS];
//    TCHAR lastWord [DEFAULT_MAXLETRAS];
//} SDATA;
typedef struct {
    TCHAR username[MAX_USERNAME];
    HANDLE hPipe;
    FLOAT score;
} User;

typedef struct {
    // Para o painel
    TCHAR letters[MAX_LIMIT_LETRAS];
    TCHAR lastWord[DEFAULT_MAXLETRAS];
} SDATA;

typedef struct {
    TCHAR letters[MAX_LIMIT_LETRAS];
    TCHAR lastWord[DEFAULT_MAXLETRAS];
    User users[MAX_USERS];
    DWORD numJogadores;
} PanelData;


typedef struct {
    MessageType type; //mensagem pode ser do tipo join, exit, ou tentativa de advinhar uma palavra (word)
    TCHAR word[MAX_MSG];
    TCHAR username[MAX_USERNAME];
} Message;



typedef struct {
    User users[MAX_USERS];
    DWORD numJogadores;
    HANDLE hMutexServer;
    HANDLE hMutexShm;
    BOOL game_active;
    HANDLE hThreads[MAX_USERS];
    BOOL continua;
    HANDLE hEventoCancelar; //encerrar (usado para terminar a thread aceitaLigacoes e outras
    HANDLE hEventoIniciarJogo;
} Server;


typedef struct {
    HANDLE hPipe;
    Server* pServer;
    SDATA* pDadosPartilhados;
    DWORD maxLetras;
    TCHAR dicionario[MAX_PALAVRAS_DICIONARIO][MAX_TAM_PALAVRA];
    DWORD numPalavrasDicionario;
    PanelData* pPanel;

} ThreadArgs;


typedef struct {
    SDATA* pShm;
    Server* pServer;
    HANDLE hMutexShm;
    int maxLetras;
    int* ritmo;
    BOOL* game_active;
    HANDLE eventoLetras;
} ThreadLetraArgs;


/// Estruturas para o jogador
typedef struct {
    SDATA* pShm;
    HANDLE hEventoLetras;
}ThreadArgsMonitor;

typedef struct {
    HANDLE hPipe;
    HANDLE hThread;
    ThreadArgsMonitor* monitorArgs;
} ThreadArgsJogador;

BOOL WriteMessage(HANDLE hPipe, Message* message);
BOOL ReadMessage(HANDLE hPipe, Message* message);



//____________________________
// OPCIONAL
//Fazer um waitforMultipleObjects(evento e teclado) na main do jogador para desbloquead o fgets


// PROXIMAS COISAS
// 
//tratar do control + c do lado do cliente

//BOT: iniciar bot no server
//     indicar que o bot deve terminar


// painel