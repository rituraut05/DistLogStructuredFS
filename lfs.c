#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "lfs.h"
#include "mfs.h"


int disk;
CR_t* cr;

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
    char curr[] = ".";
    char parent[] = "..";
    memcpy(dir->dTable[0].name,curr,4);
    memcpy(dir->dTable[1].name,parent,4);
    for(int i=0; i<MAXDIRSIZE; i++){
        dir->dTable[i].iNum = -1;
    }
    return dir;
}

void printError(int line)
{
    printf("Error at line %d \n",((line)));
}
int fsInit(int portNum, char* fsImage)
{
    disk  = open(fsImage, O_RDWR|O_CREAT, S_IRWXU);
    struct stat diskStat;

    if(disk<0)
    {
        printError(__LINE__);
    }

    fstat(disk,&diskStat);
    if(diskStat.st_size > sizeof(CR_t))
    {
        printf("Disk Already Initilised\n");
        printf("%d\n",diskStat.st_size);

        lseek(disk,0,SEEK_SET);
        //read(disk,cr,sizeof(CR_t));
        return 1;

    }
    
    Dir_t* root = getDir();
    printf("%s\n",root->dTable[0].name);
    printf("%s\n",root->dTable[1].name);

    cr = (CR_t*)malloc(sizeof(CR_t));

    cr->iCount=0;
    cr->endofLog=sizeof(CR_t);
    for(int i=0;i<MAXIMAPS;i++)
    {
        cr->iMap[i]=-1;
    }

    //Updating CR in the disk
    //lseek(disk,0,SEEK_SET);
    //write(disk,cr,sizeof(CR_t));

    //Root dir

    Dir_t* root = getDir();
    root->dTable[0].iNum=0;
    root->dTable[1].iNum=0;

    int rootAddr = cr->endofLog;
    lseek(disk,cr->endofLog,SEEK_SET);
    write(disk,root, sizeof(Dir_t));
    cr->endofLog+=BLOCK_SIZE;

    //inode for rootDir

    Inode_t* iroot = getInode();
    iroot->dp[0]=rootAddr;
    int irootAddr = cr->endofLog;
    lseek(disk,cr->endofLog,SEEK_SET);
    write(disk, iroot, sizeof(Inode_t));
    cr->endofLog+=sizeof(Inode_t);

    //filling Imap
    Imap_t* imap = getImap();
    imap->iLoc[0]=irootAddr;
    lseek(disk,cr->endofLog,SEEK_SET);
    write(disk,imap,sizeof(Imap_t));
    cr->iMap[0] = cr->endofLog;
    cr->endofLog+=sizeof(Imap_t);
    cr->iCount=1;

    //Updating CR in the disk
    lseek(disk,0,SEEK_SET);
    write(disk,cr,sizeof(CR_t));

    fsync(disk);

    

    return 2;
}
int main()
{
    fsInit(0,"hello");
    
    return 0;
}


