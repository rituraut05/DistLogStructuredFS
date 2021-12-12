#define MAXINODE 4096
#define MAXIMAPSIZE 16
#define MAXIMAPS 256
#define MAXDIRSIZE 128
#define MAXDP 14
#define BLOCKSIZE 4096

enum TYPE {regular,dir};



typedef struct CR_t
{
    int endofLog;
    int iMap[MAXIMAPS];
    int iCount;
}CR_t;

typedef struct Inode_t {
    int size;
    enum TYPE type;
    int dp[MAXDP];
}Inode_t;

typedef struct DirEntry_t {
    char name[28];
    int iNum;
}DirEntry_t;

typedef struct Dir_t {
    struct DirEntry_t dTable[MAXDIRSIZE];
}Dir_t;

typedef struct Imap_t
{
    int iLoc[MAXIMAPSIZE];
}Imap_t;

struct Inode_t* getInode();
struct Imap_t* getImap();
struct Dir_t* getDir();


