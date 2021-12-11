#define MAXINODE 4096
#define MAXIMAPSIZE 16
#define MAXIMAPS 256
#define MAXDIRSIZE 128
#define MAXDP 14

enum TYPE {regular,dir};

int disk;

typedef struct CR_t
{
    int endofLog;
    int iMap[MAXIMAPS];
    int iCount;
};

typedef struct Inode_t {
    int size;
    enum TYPE type;
    int dp[MAXDP];
};

typedef struct DirEntry_t {
    char name[28];
    int iNum;
};

typedef struct Dir_t {
    DirEntry_t dTable[128];
};

typedef struct Imap_t
{
    int iLoc[MAXIMAPSIZE];
};

Inode_t* getInode();
Imap_t* getImap();
Dir_t* getDir();


