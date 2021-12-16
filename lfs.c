#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "lfs.h"

#define DEBUG 0





int fsInit(char * fsImage){
  //this binds the server to port 100000
  //sd = UDP_Open(10000);
  // assert(sd > -1);

  int sentinel, i = 0;

  //first open disk
  disk = open(fsImage,O_RDWR);
    
  //if disk doesn't exist, create it
  if((int)disk < 0){
    disk = open(fsImage, O_RDWR| O_CREAT| O_TRUNC, S_IRWXU);
 
    
    //create checkpoint region and write it
    //CR_t cr;
      
    //this ideally needs to be done with lseek at the end, but this is the easiest way to do it
    cr.endLog = sizeof(cr)+ sizeof(Imap_t) +sizeof(Inode_t)+sizeof(Dir_t);
    for(i = 0; i < 256; i++ ){
      //may need this calculation later
      cr.iMap[i] = -1;
    }
      
    /* Disk Image implemented as:                                                      
       | CR | Imap p1 | Inode1| Data1 | Inode2 | Data2 | ...| Imap p2 | Inode1 | Data1 |... |<-endLog
    */
      
    //point the first piece of the imap to the end of the log
    cr.iMap[0] = sizeof(cr);  
    
    lseek(disk, 0, 0);
    write(disk, &cr, sizeof(cr));
       
    //create and write root data, inode, imap
    Imap_t imapP1;
    for(i = 0; i < 16; i++)
      imapP1.iLoc[i] = -1; 
       
       
    imapP1.iLoc[0] = sizeof(cr)+sizeof(imapP1); 
        
    //check all good before write
    if(DEBUG){
      printf("imap1.iLoc[0]: %d\n", imapP1.iLoc[0]);
      printf("imap1.iLoc[1]: %d\n", imapP1.iLoc[1]);
    }
       
    write(disk, &imapP1, sizeof(imapP1));
       
    Inode_t root_inode;
    root_inode.stats.type = 0;
    for(i = 0; i < 14; i++)
      root_inode.dp[i] = -1;
       
    //point this to the first datablock, right next to the inode
    root_inode.dp[0] = sizeof(cr)+sizeof(imapP1)+sizeof(root_inode) ; 
    //root_inode.stats.size = MFS_BLOCK_SIZE; 
    root_inode.stats.size = 2*MFS_BLOCK_SIZE; 
       
       
    write(disk, &root_inode, sizeof(root_inode));
       
    // This is the first Directory Block
    Dir_t rootDirBlock;      
   
    //prepare rootDirBlock
    sentinel = sizeof(rootDirBlock)/sizeof(rootDirBlock.dTable[0]);
    for(i = 0; i < sentinel; i ++){
      rootDirBlock.dTable[i].inum= -1;
      memcpy(rootDirBlock.dTable[i].name, "\0", sizeof("\0"));
    }
    
    //these are the first two directory entries into the rootDirBlock
   
    memcpy(rootDirBlock.dTable[0].name, ".\0", sizeof(".\0"));
    rootDirBlock.dTable[0].inum= 0;
    
    memcpy(rootDirBlock.dTable[1].name, "..\0", sizeof("..\0"));
    rootDirBlock.dTable[1].inum=0;

    
    //write rootDirBlock
    write(disk, &rootDirBlock, sizeof(rootDirBlock));   
  }
  else{
    //to check all set up okay
    if(DEBUG)
      printDisk();

    //disk image exists
    //load checkpoint region into memory
    (void) read(disk,&cr, sizeof(cr));
   
    //check if this is right
    if(DEBUG){
      printf("printing cr\n");
      printf("printing  cr.endLog: %d \n", cr.endLog);
      for(i = 0; i < 5; i++){
	printf("printing cr.iMap[%d]: %d \n",i, cr.iMap[i]);
      }
    }
  
  }
  //load inodeMap into memory
  for(i = 0; i < 4096; i++){
    inodeMap.inodes[i] = -1;
  }
  i =0;
  sentinel = 0;
  int j = 0; 
  Imap_t imapPiece;
  while(cr.iMap[i] >= 0){
    lseek(disk,cr.iMap[i], 0);
    read(disk,&imapPiece, sizeof(imapPiece));
    while(imapPiece.iLoc[sentinel] >= 0){
      inodeMap.inodes[j] = imapPiece.iLoc[sentinel];
      j++;sentinel++;
    }
    i++;
  }

  //check inodeMap, etc  
  if(DEBUG){
    printf("disk: |%d|", (int)disk);     
    for(i = 0; i < 10; i++){
      printf("inodeMap.inodes[%d]: %d\n",i, inodeMap.inodes[i]);
    }  
  }
  memLoad();
  //make sure you flush
  /************************************************************************/
  //fsync(disk); 
  /************************************************************************/
  return 0;
}


int fsLookup(int pinum, char * name){
  //check if pinum is valid
  if(pinum >4095  || pinum < 0)
    return -1;
  if(inodeMap.inodes[pinum] == -1)
    return -1;
  
 
  //check if name is valid
  if(strlen(name) < 1 || strlen(name) > 28)
    return -1;
  
  int bytesRead;
  int i,dirBlockLocation, j;
 
 int pinumLocation = inodeMap.inodes[pinum];
  //lseek to this location
  lseek(disk, pinumLocation, 0);
   
  //we have just lseeked to the inode, we need to read the inode
  Inode_t pInode;
  bytesRead = read(disk,&pInode, sizeof(pInode));

  
  //check if this is a directory or not
  if(pInode.stats.type != MFS_DIRECTORY){
    // close(disk);
    return -1;
  }

  //NEED TO LOOP THROUGH THE ENTIRE 14 blocks
  for(j = 0; j < 14; j++){
    //set up so that the directory block is right next to the inode
    dirBlockLocation = pInode.dp[j];
    //lseek too the directory block
    lseek(disk, dirBlockLocation, 0);
    //read the directory block
    Dir_t dirBlock;
    bytesRead += read(disk,&dirBlock, sizeof(dirBlock));
    //loop through the directory block to find name
    for(i = 0; i < 128; i++){
      //string compare the names
      if(strncmp(dirBlock.dTable[i].name, name, 28) == 0){
	return dirBlock.dTable[i].inum;
      }
    }
  }
  return -1;
}

int fsCreate(int pinum, int type, char* name){
  //take care of error cases
  if((type != MFS_DIRECTORY && type != MFS_REGULAR_FILE) || pinum > 4095 || pinum < 0)
    return -1;
 
  //if name is too long return -1
  if(strlen(name) < 1 || strlen(name) > 28)
    return -1;
  
  //if pinum dne return -1
  if(inodeMap.inodes[pinum] == -1)
    return -1;

  //if file already exists, return success  
  if(fsLookup(pinum, name) >= 0)
    return 0;

  //get pinum address
  int pinumLocation = inodeMap.inodes[pinum];

  //lseek to this location
  lseek(disk, pinumLocation, 0);
  
  //we have just lseeked to the inode, we need to read the inode
  Inode_t pInode;
  int bytesRead = read(disk,&pInode, sizeof(pInode));
  
  //check if this is a directory or not
  if(pInode.stats.type != MFS_DIRECTORY){
    return -1;
  }
  
  if(pInode.stats.size >= (MFS_BLOCK_SIZE * 14*128)){
     return -1;
  }
  
  int newInum = createInode(pinum,type);
  
  //get the directoryBlockIndex in inode
  int inodeDirBlockIndex = (int) pInode.stats.size/(MFS_BLOCK_SIZE*128);
  
 
  //potential error case
  if(inodeDirBlockIndex < 0 ||inodeDirBlockIndex > 14){
    return -1;
  }
 
  //this is the last full index
  int dirBlockIndex = (int) (pInode.stats.size/(MFS_BLOCK_SIZE) %128);
  pInode.stats.size += MFS_BLOCK_SIZE;
 
 
  if(dirBlockIndex < 0){
    return -1;
  }

  //take care of case where directory index is full
  //need new directory block
  if(dirBlockIndex == 0){
    pInode.dp[inodeDirBlockIndex] = cr.endLog;
    Dir_t freshDirBlock;
    int k;
    for(k = 0; k < 128; k++){
      memcpy(freshDirBlock.dTable[k].name, "\0", sizeof("\0"));
      freshDirBlock.dTable[k].inum = -1;
    }
    lseek(disk, cr.endLog, 0);
    write(disk, &freshDirBlock, sizeof(freshDirBlock));
    
    //update endlog and write out the checkpoint
    cr.endLog += MFS_BLOCK_SIZE;
    lseek(disk, 0, 0);
    write(disk, &cr, sizeof(cr));   
  }
  
  //IMPORTANT
  //write out the inode here. Either way, inode needs to be written out since size is updated
  //do it here because if we need to update the block ptrs, that happens just before this
  lseek(disk, pinumLocation, 0);
  write(disk, &pInode, sizeof(pInode));
 
  memLoad();

  lseek(disk, pInode.dp[inodeDirBlockIndex], 0);
  Dir_t dirBlock;
  bytesRead = read(disk,&dirBlock, sizeof(dirBlock));
  
  //set the name etc
  int newIndex = dirBlockIndex;
  memcpy(dirBlock.dTable[newIndex].name, "\0", sizeof("\0"));
  memcpy(dirBlock.dTable[newIndex].name, name, 28);
  dirBlock.dTable[newIndex].inum = newInum;
 
  //write out updated directory block to disk
  lseek(disk, pInode.dp[inodeDirBlockIndex], 0);
  write(disk, &dirBlock, sizeof(dirBlock));
  
  /************************************************************************/
  //fsync(disk); 
  /************************************************************************/
  memLoad();
  return 0;
}


//return the index of the new imap Piece
int createImapPiece(){
  
  memLoad();
  int i, newImapPieceIndex;
  for(i = 0; i < 256; i++){
    if(cr.iMap[i] == -1){
      newImapPieceIndex = i;
      i = 5000;
    }
  }

  cr.iMap[newImapPieceIndex] = cr.endLog;
  //write to disk
  Imap_t newPiece;
  for(i = 0; i < 16; i++){
    newPiece.iLoc[i] = -1;
  } 
  cr.endLog += sizeof(newPiece);
  lseek(disk,0, 0);
  write(disk, &cr, sizeof(cr));
  
  lseek(disk,cr.iMap[newImapPieceIndex], 0);
  write(disk, &newPiece, sizeof(newPiece));
  memLoad();
  return newImapPieceIndex;
}


//updates imapPiece, deletes imapPiece if need be
int deleteInode(int inum){
  printf("inum is: %d\n", inum);
  int imapPieceIndex =inum/16;
  int imapInodeIndex = inum%16;
  int i;
  if(imapInodeIndex < 0){
    return -1;
  }

  Imap_t imapPiece;
  lseek(disk, cr.iMap[imapPieceIndex], 0);
  read(disk, &imapPiece, sizeof(imapPiece));

  imapPiece.iLoc[imapInodeIndex] = -1;
  i = 0;
  while(imapPiece.iLoc[i] > 0 && i < 16)
    i++;
  printf("i is: %d\n", i);

  if(i == 0){
    int  test =deleteImapPiece(imapPieceIndex);
  }
  else{
    //write out to disk
     lseek(disk, cr.iMap[imapPieceIndex], 0);
     write (disk, &imapPiece, sizeof(imapPiece));
  }
  memLoad();
  return 0;

}


int deleteImapPiece(int imapPieceIndex){
  memLoad();
  cr.iMap[imapPieceIndex] = -1;
  //write to disk
  lseek(disk,0, 0);
  write(disk, &cr, sizeof(cr));
  memLoad();
  return 0;
}

//int createInode(int pinum, int type, int imapPieceIndex){
int createInode(int pinum, int type){
  memLoad();
  int i, newInodeNum, newImapInodeIndex, test;
  for(i = 0; i < 4096; i++ ){
    if(inodeMap.inodes[i] == -1){
      newInodeNum = i;
      i = 5000;
    }
  }
 
  int imapPieceIndex = newInodeNum/16;
  newImapInodeIndex = newInodeNum%16;
 
  if(newImapInodeIndex < 0){
    return -1;
  }
  if(newImapInodeIndex == 0){
    test =createImapPiece();
    newImapInodeIndex = 0;
  }
  
  Imap_t imapPiece;
  lseek(disk, cr.iMap[imapPieceIndex], 0);
  read(disk, &imapPiece, sizeof(imapPiece));
  
  //update the inode address in the imapPiece
  imapPiece.iLoc[newImapInodeIndex] = cr.endLog;
  lseek(disk, cr.iMap[imapPieceIndex], 0);
  write(disk, &imapPiece, sizeof(imapPiece));
  
  Inode_t newInode;
  newInode.stats.type = type;
  for(i = 0; i < 14; i++)
    newInode.dp[i] = -1;
       
  //point this to the first datablock, right next to the inode
  newInode.dp[0] = cr.endLog+sizeof(newInode); 
  if(type == MFS_DIRECTORY)
    newInode.stats.size = 2*MFS_BLOCK_SIZE; 
  else
    newInode.stats.size = 0; 
    
  lseek(disk, cr.endLog, 0);
  write(disk, &newInode, sizeof(newInode));
  
  //update cr.endLog
  cr.endLog += sizeof(newInode);
  
  if(type == MFS_DIRECTORY){
   // This is the first Directory Block
    Dir_t dirBlock;      
   
    //prepare rootDirBlock
    int sentinel = sizeof(dirBlock)/sizeof(dirBlock.dTable[0]);
    for(i = 0; i < sentinel; i ++){
      dirBlock.dTable[i].inum= -1;
      memcpy(dirBlock.dTable[i].name, "\0", sizeof("\0"));
    }

    //these are the first two directory entries into the rootDirBlock
    memcpy(dirBlock.dTable[0].name, ".\0", sizeof(".\0"));
    dirBlock.dTable[0].inum= newInodeNum;
    
    memcpy(dirBlock.dTable[1].name, "..\0", sizeof("..\0"));
    dirBlock.dTable[1].inum=pinum;
  
    write(disk, &dirBlock, sizeof(dirBlock));
    //update cr.endLog
    cr.endLog += sizeof(dirBlock);
  }
  else{
    char * newblock = malloc(MFS_BLOCK_SIZE);
    write(disk, newblock, MFS_BLOCK_SIZE);
    free(newblock);
    //update cr.endLog
    cr.endLog += MFS_BLOCK_SIZE;
  }

  //need to update endLog, write out to disk and then reload stuff back into memory
  lseek(disk, 0, 0);
  write(disk, &cr, sizeof(cr));

  //load inodeMap, cr into mem
  memLoad();
  
  //return the new inode num
  return newInodeNum;
}

int memLoad(){
  //need to first load cr into memory
  lseek(disk, 0, 0);
  read(disk, &cr, sizeof(cr));
  
  //load inodeMap into memory
  int i =0,sentinel = 0,  j = 0;
    
  for(i = 0; i < 4096; i++){
    inodeMap.inodes[i] = -1;
  }
  
  i=0;sentinel = 0;
  Imap_t imapPiece; 
  for(i = 0; i < 256; i++){
    if(cr.iMap[i] >= 0){
      lseek(disk, cr.iMap[i], 0);
      read(disk,&imapPiece, sizeof(imapPiece));
      for(j = 0 ; j < 16 ; j++){
	if(imapPiece.iLoc[j] >= 0){
	  inodeMap.inodes[sentinel] = imapPiece.iLoc[j];
	  sentinel++;
	}
	else{
	  //j = 5000;
	}
      }
    }
    else{
      // i = 5000;
    }
  }

  return 0;
}

int fsRead(const int inum, char *buffer, int block){
  if(inum > 4095 || inum < 0)
    return -1;

  if(block > 13 || block < 0 )
    return -1;

  memLoad();
  
  if(inodeMap.inodes[inum] == -1){
    return -1;
  }

  //else read inode from disk
  Inode_t inode;
  lseek(disk, inodeMap.inodes[inum], 0);
  read(disk, &inode, sizeof(inode));

  //check if block is allocated or not
  if(inode.dp[block] == -1){
    return -1;
  }
  
  //else lseek to this location and write out 4096 bytes
  lseek(disk,inode.dp[block], 0);
  read(disk, buffer, MFS_BLOCK_SIZE);
  return 0;
}

int fsWrite(int inum, char *buffer, int block){
  if(inum > 4095 || inum < 0)
    return -1;
  
  if(block > 13 || block < 0 )
    return -1;

  memLoad();

  if(inodeMap.inodes[inum] == -1){
    return -1;  
  }
  
  //else read inode from disk
  Inode_t inode;
  lseek(disk, inodeMap.inodes[inum], 0);
  read(disk, &inode, sizeof(inode));
  
  //check inode type, if not regular file, return -1
  if(inode.stats.type != MFS_REGULAR_FILE){
    return -1;
  }
  
  //check if block is allocated or not
  if(inode.dp[block] == -1){
    int oldEndLog = cr.endLog;
    
    Dir_t freshDirBlock;
    int k;
    for(k = 0; k < 128; k++){
      memcpy(freshDirBlock.dTable[k].name, "\0", sizeof("\0"));
      freshDirBlock.dTable[k].inum = -1;
    }
    lseek(disk, cr.endLog, 0);
    write(disk, &freshDirBlock, sizeof(freshDirBlock));
    
    cr.endLog += MFS_BLOCK_SIZE;
    lseek(disk, 0, 0);
    write(disk, &cr, sizeof(cr));
    
    inode.dp[block] = oldEndLog;
    inode.stats.size = (block+1)*(MFS_BLOCK_SIZE);
    lseek(disk, inodeMap.inodes[inum], 0);
    write(disk, &inode, sizeof(inode));
    
    lseek(disk, oldEndLog, 0);
  }
  else{
  
    //else lseek to this location and write out 4096 bytes
    lseek(disk,inode.dp[block], 0);
    inode.stats.size = (block + 1)*(MFS_BLOCK_SIZE);
  }

  write(disk, buffer, MFS_BLOCK_SIZE);
 
  lseek(disk, inodeMap.inodes[inum], 0);
  write(disk, &inode, sizeof(inode));
  /********************************************************************/
  //fsync(disk);
  /********************************************************************/
  memLoad();
  return 0;
}

//You are overwriting the value of the imap Piece.

int fsUnlink(int pinum, char * name){
  int i, k;
  memLoad();
  //take care of error cases
  if(pinum < 0 || pinum > 4095)
    return -1;
  
  if(strlen(name) > 28 || strlen(name) < 0)
    return -1;


  if(inodeMap.inodes[pinum] == -1 )
    return -1;  
  
  if(strcmp(name, "..\0") == 0 || strcmp(name, ".\0") == 0 )
    return -1;

  
  //get pinum address
  int pinumLocation = inodeMap.inodes[pinum];

  //lseek to this location
  lseek(disk, pinumLocation, 0);
  
  //we have just lseeked to the inode, we need to read the inode
  Inode_t pInode;
  int bytesRead =  read(disk,&pInode, sizeof(pInode));
  if(DEBUG){
    printf("bytesRead: |%d|\n", bytesRead);
    
  }

  /*************************************************************************/
  /*
  close(disk);printDisk();exit(0);
  if(pInode.stats.type == MFS_REGULAR_FILE){
    printInode(&pInode);
    exit(0);
  }
  */
  /*************************************************************************/
  //check if this is a directory or not
   if(pInode.stats.type != MFS_DIRECTORY)
     return -1;
  
  int found = -1, deleteDirBlockLoc, deleteIndex, deleteInodeLocation, deleteINum;
  Dir_t dirBlock;
  for(i = 0; i < 14; i++){
    if(pInode.dp[i] >= 0 )
    {
      lseek(disk,pInode.dp[i],0);
      read(disk, &dirBlock, sizeof(dirBlock));
      for(k = 0; k < 128; k++)
      {
	      if((dirBlock.dTable[k].inum>0) && strcmp(dirBlock.dTable[k].name, name) ==0)
        {
	      //printf("dirBlock.dTable[k].name, name: %s, %s\n",dirBlock.dTable[k].name, name );
	      //printf(" dirBlock.dTable[k].inum:%d\n", dirBlock.dTable[k].inum);
	      //exit(0);
	        found = 1;
	        deleteInodeLocation = inodeMap.inodes[dirBlock.dTable[k].inum%MFS_BLOCK_SIZE];
          deleteINum = dirBlock.dTable[k].inum;
	        deleteIndex = k;
	        deleteDirBlockLoc = pInode.dp[i];
	        i = 5000;
          break;
	 
	      }
      }
    }
    if(found==1)
        break;

  } 
 
  if(found < 0)
    return 0;
  
  /**************************************ERROR***********************************/
  //there is something wrong here with the inodeLocation that you are passing to delete
  /**************************************ERROR***********************************/
  Inode_t inodeToDelete;
  lseek(disk, deleteInodeLocation,0);
  read(disk, &inodeToDelete, sizeof(inodeToDelete));
  //printf("SIZE: %d\n", inodeToDelete.stats.size);exit(0);  

 

  if(inodeToDelete.stats.type == MFS_DIRECTORY){  
    if(inodeToDelete.stats.size > 2*(MFS_BLOCK_SIZE)){
      return -1;
    }


   
    /* else{
      dirBlock.dTable[deleteIndex].inum = -1;
      memcpy(dirBlock.dTable[deleteIndex].name, "\0", sizeof("\0"));
      //write out the directory block
      lseek(disk, pInode.dp[i],0);
      write(disk, &dirBlock, sizeof(dirBlock));
      
       }
      */
   }
  
  deleteInode(deleteINum);

  
  dirBlock.dTable[deleteIndex].inum = -1;
  memcpy(dirBlock.dTable[deleteIndex].name, "\0", sizeof("\0"));
  //write out the directory block
  lseek(disk, deleteDirBlockLoc,0);
  write(disk, &dirBlock, sizeof(dirBlock));
      
  //update pInode
  int sizeIndex = 0;
  for(i =0; i < 14; i++)
    if(pInode.dp[i] != -1)
      sizeIndex =i;
	
  //if(sizeIndex > 3)
  pInode.stats.size = (sizeIndex+1)*MFS_BLOCK_SIZE;
  //pInode.stats.size -=MFS_BLOCK_SIZE;
  //write out the pInode
  lseek(disk, pinumLocation,0);
  write(disk, &pInode, sizeof(pInode));
  memLoad();
  //delete the directory file, update pInode
  //close(disk);
  //printDisk();
  //disk = open(file, O_RDWR);
  return 0;


  //TODO
  //get to the directory block, search for name
  //if the index is 3 AND no other files exist, delete(update Imap, ckpt, etc) pInode from the whole disk
  //if last piece in Imappiece, then remove the Imap, update ckpt
 
  // return 0;
}

int fsStat(const int inum, MFS_Stat_t * m){
  if (inum < 0|| inum > 4095)
    return -1;
 
  if(disk < 0)
    return -1;

  //load into memory (maybe unnecessary, but always assume the worst 
  //eg - someone forgot to call it somewhere else)
  memLoad();  

  if(inodeMap.inodes[inum] == -1){
    return -1;  
  }
  
  //else read inode from disk
  Inode_t inode;
  lseek(disk, inodeMap.inodes[inum], 0);
  read(disk, &inode, sizeof(inode));
  
  //deep copy struct and return
  m->type = inode.stats.type;
  m->size = inode.stats.size;
  //close(disk);
  return 0;
}

int fsShutdown(){
  //may need to check disk here
  close(disk);
  exit(0);
  return 0;
}