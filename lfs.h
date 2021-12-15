#include "mfs.h"

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



int disk;
CR_t* cr;

struct Inode_t* getInode();
struct Imap_t* getImap();
struct Dir_t* getDir();
Inode_t* fetchInode(int iNum);
Imap_t* fetchImap(int iNum);
int fsFindInodeAddr(int iParent);
int fsLookup(int iParent, char *name);
void* fsRead(int inum, char *buffer, int block);
int dumpFileInodeDataImap(Inode_t* inode, int inum, char* data, int block);
int dumpDirInodeDataImap(Inode_t* inode, int inum, Dir_t* dirBlock, int block);
int fsWrite(int inum, char *buffer, int block) ;
int fsUnlink(int iParent, char *name);
int updateDirUnlink(Inode_t* inode, char* name, int* delInodeAddr);
int updateDir(Inode_t* inode,int newiNum,char* name);
int updateCRMap(int iNum, int iAddr);
int fsCreate(int iParent, enum TYPE type, char *name);
int fsInit(char* fsImage);
int fsShutDown();
int fsStat(int iNum, MFS_Stat_t* stat);


