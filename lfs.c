#include<stdio.h>
#include<stdlib.h>
#include "lfs.h"
#include<string.h>


Inode_t* getInode()
{
    Inode_t* temp = (Inode_t*)malloc(sizeof(Inode_t));
    temp->size=0;
    temp->type = dir;
    for(int i=0;i<MAXDP;i++)
        temp->dp[i]=-1;
    return temp;
}
Imap_t* getImap()
{
    Imap_t* temp = (Imap_t*)malloc(sizeof(Imap_t));
    for(int i=0;i<MAXIMAPSIZE;i++)
        temp->iLoc[i]=-1;
    return temp;
}

Dir_t* getDir(){
    Dir_t* dir = (Dir_t*)malloc(sizeof(Dir_t));
    dir->dTable[0].name = ".";
    dir->dTable[1].name = "..";
    for(int i=0; i<MAXDIRSIZE; i++){
        dir->dTable[i].iNum = -1;
    }
    return dir;
}


