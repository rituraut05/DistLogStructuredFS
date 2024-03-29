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
Inode_t* fetchInode(int iNum){
    if(iNum<0 || iNum>MAXINODE)
    {
        printError(__LINE__);
        return NULL;
    }
    int imapAddr = cr->iMap[iNum/MAXIMAPSIZE];
    if(imapAddr==-1) {
        printError(__LINE__);
        return NULL;        
    }
    int imapIndex = iNum % MAXIMAPSIZE;
    Imap_t imap;
    lseek(disk,imapAddr,SEEK_SET);
    read(disk,&imap,sizeof(Imap_t));
    
    int inodeAddr = imap.iLoc[imapIndex];
    if(inodeAddr==-1) {
        printError(__LINE__);
        return NULL;       
    }
    Inode_t* inode = (Inode_t*)malloc(sizeof(Inode_t));
    lseek(disk,inodeAddr,SEEK_SET);
    read(disk, inode,sizeof(Inode_t));
    return inode;

}

Imap_t* fetchImap(int iNum){
    if(iNum<0 || iNum>MAXINODE)
    {
        printError(__LINE__);
        return NULL;
    }
    int imapAddr = cr->iMap[iNum/MAXIMAPSIZE];

    if(imapAddr==-1) {
        printError(__LINE__);
        return NULL;        
    }

    Imap_t *imap = (Imap_t*)malloc(sizeof(Imap_t));
    lseek(disk, imapAddr,SEEK_SET);
    read(disk, imap,sizeof(Imap_t));

    return imap;
}

int fsFindInodeAddr(int iParent)
{
    int imapAddr = cr->iMap[iParent/MAXIMAPSIZE];
    if(imapAddr==-1)
    {
        printError(__LINE__);
        return -1;        
    }
    int imapIndex = iParent % MAXIMAPSIZE;
    Imap_t imap;
    lseek(disk,imapAddr,SEEK_SET);
    read(disk,&imap,sizeof(Imap_t));

    int inodeAddr = imap.iLoc[imapIndex];
    return inodeAddr;
}

int fsLookup(int iParent, char *name)
{
    //Sanitycheck for iParent
    if(iParent<0 || iParent>MAXINODE)
    {
        printError(__LINE__);
        return -1;
    }
    Inode_t* inode = fetchInode(iParent);
    if(inode == NULL)
    return -1;
    
    if(inode->type !=dir)
    {
        printError(__LINE__);
        return -1;
    }
    //Search Through every Direct pointer
    for(int i=0;i<MAXDP;i++)
    {
        int dataBlockAddr = inode->dp[i];
        if(dataBlockAddr==-1)
            continue;
        Dir_t block;
        lseek(disk,dataBlockAddr,SEEK_SET);
        read(disk, &block,sizeof(Dir_t));
        for(int j=0;j<MAXDIRSIZE;j++)
        {
            if(strcmp(block.dTable[j].name,name)==0)
                return block.dTable[j].iNum;
        }

    }
    printError(__LINE__);
    return -1;



}

void* fsRead(int inum, char *buffer, int block) {
    // get the inode
    Inode_t* inode = fetchInode(inum);
    if(inode == NULL || inode->dp[block] == -1){
        printError(__LINE__);
        return NULL;
    }
        
    lseek(disk, inode->dp[block], SEEK_SET);

    if(inode->type == regular) {
        read(disk, buffer, MFS_BLOCK_SIZE);
        return 0;
    }
    Dir_t* dir;
    read(disk, dir, MFS_BLOCK_SIZE);
    return dir;
}

int dumpFileInodeDataImap(Inode_t* inode, int inum, char* data, int block){
    if(inode->type == regular){
        lseek(disk, cr->endofLog, SEEK_SET);
        write(disk, data, MFS_BLOCK_SIZE);
        inode->dp[block] = cr->endofLog;
        cr->endofLog += MFS_BLOCK_SIZE;

        lseek(disk, cr->endofLog, SEEK_SET); //we don't need this right?
        write(disk, inode, sizeof(Inode_t));
        
        Imap_t* imap = fetchImap(inum);
        int imapIndex = inum % MAXIMAPSIZE;
        imap->iLoc[imapIndex] = cr->endofLog;
        cr->endofLog += sizeof(Inode_t);

        lseek(disk, cr->endofLog, SEEK_SET); //we don't need this right?
        write(disk, imap, sizeof(Imap_t));
        cr->iMap[inum/MAXIMAPSIZE] = cr->endofLog;
        cr->endofLog += sizeof(Imap_t);

        lseek(disk,0,SEEK_SET);
        write(disk,cr,sizeof(CR_t));

        fsync(disk);
        return 0;
    }else{
        return -1;
    }
}

int dumpDirInodeDataImap(Inode_t* inode, int inum, Dir_t* dirBlock, int block){
    if(inode->type == dir){
        lseek(disk, cr->endofLog, SEEK_SET);
        write(disk, dirBlock, sizeof(Dir_t));
        inode->dp[block] = cr->endofLog;
        cr->endofLog += sizeof(Dir_t);

        lseek(disk, cr->endofLog, SEEK_SET); //we don't need this right?
        write(disk, inode, sizeof(Inode_t));
        
        Imap_t* imap = fetchImap(inum);
        int imapIndex = inum % MAXIMAPSIZE;
        imap->iLoc[imapIndex] = cr->endofLog;
        cr->endofLog += sizeof(Inode_t);

        lseek(disk, cr->endofLog, SEEK_SET); //we don't need this right?
        write(disk, imap, sizeof(Imap_t));
        cr->iMap[inum/MAXIMAPSIZE] = cr->endofLog;
        cr->endofLog += sizeof(Imap_t);

        lseek(disk,0,SEEK_SET);
        write(disk,cr,sizeof(CR_t));

        fsync(disk);
        return 0;
    }else{
        return -1;
    }
}

int fsWrite(int inum, char *buffer, int block) {
    // get the inode
    Inode_t* inode = fetchInode(inum);
    if(inode == NULL)
        return -1;

    return dumpFileInodeDataImap(inode, inum, buffer, block);
 
}

int isUnlinkValid(int inum){
    Inode_t* inode = fetchInode(inum);
    if(inode->type == dir && inode->size!=0)
        return 0;
    return 1;
}

int fsUnlink(int iParent, char *name) {

    //get inode of the directory by iParent
    Inode_t* inode = fetchInode(iParent);
    if(inode == NULL) // failure. Failure modes: pinum does not exist, directory is NOT empty. Note that the name not existing is NOT a failure by our definition
        return -1;

    if(inode->type != dir)
        return -1;
    
    Dir_t* dirBlock = (Dir_t*)malloc(sizeof(Dir_t));
    int delInum = -1, delBlock = -1;
    // go through all the directoryBlocks pointed by the dir Inode.
    for(int i=0; i < MAXDP; i++) {
        int dirBlockAddr = inode->dp[i];
        if(dirBlockAddr == -1)
            continue;
               
        lseek(disk, dirBlockAddr, SEEK_SET);
        read(disk, dirBlock, sizeof(Dir_t));
        
        // find for given name in the directory entries. If found, set iNum to -1
        for(int j=0; j < MAXDIRSIZE; j++){
            if(strcmp(dirBlock->dTable[j].name, name) == 0) {
                delBlock = i;
                delInum = dirBlock->dTable[j].iNum;
                if(!isUnlinkValid(delInum)){
                    printError(__LINE__);
                    return -1;
                }
                    
                dirBlock->dTable[j].name[0] = '\0';
                dirBlock->dTable[j].iNum = -1;
                break;
            }
        }
        if(delInum != -1)
            break;
    }

    if(delInum == -1)
        return 0;
    
    // dump these blocks on disc
    dumpDirInodeDataImap(inode, iParent, dirBlock, delBlock);

    // update the unlinked file's imap[delInodeAddr] = -1
    Imap_t* delInodeImap = fetchImap(delInum);
    delInodeImap->iLoc[delInum % MAXIMAPSIZE] = -1;
    cr->iMap[delInum/MAXIMAPSIZE] = cr->endofLog;
    lseek(disk, cr->endofLog, SEEK_SET);
    write(disk, delInodeImap, sizeof(Imap_t));
    cr->endofLog += sizeof(Imap_t);

    lseek(disk,0,SEEK_SET);
    write(disk,cr,sizeof(CR_t));

    fsync(disk);

    return 0;
}

int updateDirUnlink(Inode_t* inode, char* name, int* delInodeAddr) {
    int isFound = 0;
    int newBlockAddr = -1;
    int newInodeAddr = -1;
    for(int i=0;i<MAXDP;i++)
    {
        int dataBlockAddr = inode->dp[i];
        if(dataBlockAddr==-1)
            continue;
        Dir_t block;
        lseek(disk,dataBlockAddr,SEEK_SET);
        read(disk, &block,sizeof(Dir_t));
        for(int j=0;j<MAXDIRSIZE;j++)
        {
            if(strcmp(block.dTable[j].name,name)==0) {
                *delInodeAddr = block.dTable[j].iNum;
                block.dTable[j].iNum = -1;
                isFound=1;
                newBlockAddr = cr->endofLog;
                lseek(disk,newBlockAddr,SEEK_SET);
                write(disk,&block,sizeof(Dir_t));
                cr->endofLog+=sizeof(Dir_t);
                break;
            }
        }
        if(isFound==1)
        {
            inode->dp[i]=newBlockAddr;
            newInodeAddr = cr->endofLog;
            lseek(disk, newInodeAddr, SEEK_SET);
            write(disk, inode, sizeof(Inode_t)); 
            cr->endofLog+=sizeof(Inode_t); 
            break;      
        }

    }
    return  newInodeAddr;
}

int updateDir(Inode_t* inode,int newiNum,char* name)
{
    int isFound = 0;
    int newBlockAddr = -1;
    int newInodeAddr=-1;
    for(int i=0;i<MAXDP;i++)
    {
        int dataBlockAddr = inode->dp[i];
        if(dataBlockAddr==-1)
            continue;
        Dir_t block;
        lseek(disk,dataBlockAddr,SEEK_SET);
        read(disk, &block,sizeof(Dir_t));
        for(int j=0;j<MAXDIRSIZE;j++)
        {
            if(block.dTable[j].iNum==-1)
            {
                block.dTable[j].iNum=newiNum;
                int len = strlen(name);
                memcpy(block.dTable[j].name,name,len);
                isFound=1;
                newBlockAddr = cr->endofLog;
                lseek(disk,newBlockAddr,SEEK_SET);
                write(disk,&block,sizeof(Dir_t));
                cr->endofLog+=sizeof(Dir_t);
                break;
            }
        }
        if(isFound==1)
        {
            inode->dp[i]=newBlockAddr;
            newInodeAddr = cr->endofLog;
            lseek(disk,newInodeAddr,SEEK_SET);
            write(disk,inode,sizeof(Inode_t)); 
            cr->endofLog+=sizeof(Inode_t); 
            break;      
        }

    }
    return  newInodeAddr;
}

int updateCRMap(int iNum, int iAddr)
{
    int imapAddr = cr->iMap[iNum/MAXIMAPSIZE];
    if(imapAddr==-1)
    {
        Imap_t* temp = getImap();
        int newimapAddr = cr->endofLog;
        // why aren't we updating the temp.iLoc[iNum % MAXIMAPSIZE] = iAddr;
        lseek(disk,newimapAddr,SEEK_SET);
        write(disk,temp,sizeof(Imap_t));  
        cr->endofLog+=sizeof(Imap_t);
        cr->iMap[iNum/MAXIMAPSIZE] = newimapAddr;
        return newimapAddr;           
    }
    int imapIndex = iNum % MAXIMAPSIZE;
    Imap_t imap;
    lseek(disk,imapAddr,SEEK_SET);
    read(disk,&imap,sizeof(Imap_t));

    //writing new Imap
    imap.iLoc[imapIndex] = iAddr;
    int newimapAddr = cr->endofLog;
    lseek(disk,newimapAddr,SEEK_SET);
    write(disk,&imap,sizeof(Imap_t));
    cr->endofLog+=sizeof(Imap_t);
    cr->iMap[iNum/MAXIMAPSIZE] = newimapAddr;
    return newimapAddr;

}

int fsCreate(int iParent, enum TYPE type, char *name)
{
    //Check if name is already in use?
    int isValid = fsLookup(iParent, name);
    if(isValid!=-1)
    {
        printError(__LINE__);
        return -1;        
    }

    //We will check for next available inode Value: Todo
    int inodeAddr = fsFindInodeAddr(iParent);

    if(inodeAddr==-1)
    {
        printError(__LINE__);
        return -1;       
    }
    Inode_t inode;
    lseek(disk,inodeAddr,SEEK_SET);
    read(disk,&inode,sizeof(Inode_t));
    if(inode.type!=dir)
    {
        printError(__LINE__);
        return -1;
    }
    inode.size++;
    //Updating Parent Indo
    int newiNum = cr->iCount;
    cr->iCount++;
    int newiParentAddr = updateDir(&inode,newiNum,name);
    updateCRMap(iParent,newiParentAddr);

    //For new File
    int dataBlockAddr=cr->endofLog;
    Inode_t* newInode = getInode();
    lseek(disk,dataBlockAddr,SEEK_SET);
    if(type==dir)
    {
        Dir_t* temp = getDir();
        temp->dTable[1].iNum=iParent;
        write(disk,temp,sizeof(Dir_t));
        newInode->type=dir;      
    }
    else
    {
        char* temp = (char*)malloc(sizeof(BLOCKSIZE));
        write(disk,temp,sizeof(BLOCKSIZE));
        newInode->type=regular;       
    }
    cr->endofLog+=dataBlockAddr;
    newInode->dp[0]=dataBlockAddr;
    int newInodeAddr = cr->endofLog;
    lseek(disk,newInodeAddr,SEEK_SET);
    write(disk,newInode,sizeof(Inode_t));
    cr->endofLog+=sizeof(Inode_t);
    updateCRMap(newiNum,newInodeAddr);
    
    lseek(disk,0,SEEK_SET);
    write(disk,cr,sizeof(CR_t));

    fsync(disk);

    return 0;


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
        cr = (CR_t*)malloc(sizeof(CR_t));
        printf("Disk Already Initilised\n");
        printf("%lld\n",diskStat.st_size);

        lseek(disk,0,SEEK_SET);
        read(disk, cr, sizeof(CR_t));
        return 1;

    }
    cr = (CR_t*)malloc(sizeof(CR_t));

    cr->iCount=0;
    cr->endofLog=sizeof(CR_t);
    for(int i=0;i<MAXIMAPS;i++){
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
    cr->endofLog+=sizeof(Dir_t);

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
    // fsCreate(0, regular, "writeTest");

    // printf("lookup inum = %d\n", fsLookup(0, "writeTest"));
    // int inum = fsLookup(0, "writeTest");
    // char block[BLOCKSIZE] = "Hello\0";
    // // memset(block, 0, sizeof(block));
    // fsWrite(inum, block, 0);

    // char readBlock[BLOCKSIZE];
    // fsRead(inum, readBlock, 0);
    // printf("read value = %s\n", readBlock);
    
    // fsUnlink(0, "writeTest");
    // fsRead(inum, readBlock, 0);
    // printf("read value = %s\n", readBlock);

    fsCreate(0, dir, "rootFolder");
    int inum = fsLookup(0, "rootFolder");
    fsCreate(inum, regular, "file1");
    fsUnlink(0, "rootFolder");


    
    
    return 0;
}


