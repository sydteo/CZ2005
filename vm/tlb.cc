#include "copyright.h"
#include "tlb.h"
#include "syscall.h"
#include "machine.h"
#include "thread.h"
#include "system.h"
#include "utility.h"

//----------------------------------------------------------------------
// UpdateTLB
//      Called when exception is raised and a page isn't in the TLB.
// Figures out what to do (get from IPT, or pageoutpagein) and does it.
//----------------------------------------------------------------------

void UpdateTLB(int possible_badVAddr)
{
  int badVAddr;
  unsigned int vpn;
  int phyPage;

  if(possible_badVAddr) // get the bad address from the correct location
    badVAddr=possible_badVAddr; // fault in kernel
  else
    badVAddr=machine->registers[BadVAddrReg]; // fault in userprog
  
  vpn=(unsigned)badVAddr/PageSize;
  
  if((phyPage=VpnToPhyPage(vpn))!=-1) {
    InsertToTLB(vpn, phyPage);
  } else {
    if(vpn>=currentThread->space->numPages && !GetMmap(vpn))
      machine->RaiseException(AddressErrorException, badVAddr);
    else
      InsertToTLB(vpn, PageOutPageIn(vpn));
  }
}

//----------------------------------------------------------------------
// VpnToPhyPage
//      Gets a phyPage for a vpn, if exists in ipt.
//----------------------------------------------------------------------

int VpnToPhyPage(int vpn)
{
  //your code here to get a physical frame for page vpn
  //you can refer to PageOutPageIn(int vpn) to see how an entry was created in ipt
  int i;

  // Manually added
  printf("====================\n\n");
  printf("VpnToPhyPage triggered, we are now printing the corresponding output.\n");
  printf("Current page id         = %d\nCurrent page number     = %d\nPage Size               = %d\n\n", currentThread->pid, vpn, PageSize);
  for (i=0; i<NumPhysPages; i++) {
	printf("Printing memoryTable[%d]:\n",i);
	printf("memoryTable[%d].pid      = %d,\n",i, memoryTable[i].pid);
	printf("memoryTable[%d].vPage    = %d,\n", i, memoryTable[i].vPage);
	printf("memoryTable[%d].lastUsed = %d\n", i, memoryTable[i].lastUsed);
	printf("memoryTable[%d].valid    = %d,\n", i, memoryTable[i].valid);
	printf("memoryTable[%d].dirty    = %d,\n", i, memoryTable[i].dirty);
	printf("memoryTable[%d].TLBentry = %d,\n\n", i, memoryTable[i].TLBentry);
  }

  for (i=0; i<NumPhysPages; i++) {
	if (memoryTable[i].valid && memoryTable[i].pid==currentThread->pid && memoryTable[i].vPage==vpn) {
	printf("Virtual Page Number found in the IPT table!\n\n");
	printf("Found at physical frame number: %d\n\n", i);
	return i;
	}
  }
  printf("Virtual Page Number NOT found in the IPT table!\n\n");
  printf("====================\n\n");
  return -1;
}

//----------------------------------------------------------------------
// InsertToTLB
//      Put a vpn/phyPage combination into the TLB. If TLB is full, use FIFO 
//    replacement
//----------------------------------------------------------------------

void InsertToTLB(int vpn, int phyPage)
{
  int i = 0; //entry in the TLB
  
  //your code to find an empty in TLB or to replace the oldest entry if TLB is full
  static int FIFOPointer = 0;
  
  // Manually added
  printf("====================\n\n");
  printf("InsertToTLB triggered, we are now printing the corresponding output.\n");
  printf("Current page id              = %d\nCurrent page number          = %d\nPage Size                    = %d\nFIFOPointer                  = %d\n\n", currentThread->pid, vpn, PageSize, FIFOPointer);

  for (i=0; i<TLBSize; i++) {
	printf("Printing TLB[%d]:\n", i);
	printf("machine->tlb[%d].virtualPage  = %d,\n", i, machine->tlb[i].virtualPage);
	printf("machine->tlb[%d].physicalPage = %d,\n", i, machine->tlb[i].physicalPage);
	printf("machine->tlb[%d].valid        = %d,\n", i, machine->tlb[i].valid);
	printf("machine->tlb[%d].readOnly     = %d,\n", i, machine->tlb[i].readOnly);
	printf("machine->tlb[%d].use          = %d,\n", i, machine->tlb[i].use);
	printf("machine->tlb[%d].dirty        = %d\n\n", i, memoryTable[i].dirty);
  }

  //your code to find an empty in TLB or to replace the oldest entry if TLB is full
  for (i=0; i<TLBSize; i++) {
	if (!machine->tlb[i].valid) break;
  }
  if (i==TLBSize) {
	i = FIFOPointer;
  	// FIFOPointer = (i+1)%TLBSize; // Putting the FIFOPOinter here or below doesn't make a difference to the output
  }
  FIFOPointer = (i+1)%TLBSize;  // Putting the FIFOPOinter here or above doesn't make a difference to the output
  printf("Insert to TLB position at    = %d\n\n", i);
  printf("====================\n");
  // copy dirty data to memoryTable
  if(machine->tlb[i].valid){
    memoryTable[machine->tlb[i].physicalPage].dirty=machine->tlb[i].dirty;
    memoryTable[machine->tlb[i].physicalPage].TLBentry=-1;
  }

  //update the TLB entry
  machine->tlb[i].virtualPage  = vpn;
  machine->tlb[i].physicalPage = phyPage;
  machine->tlb[i].valid        = TRUE;
  machine->tlb[i].readOnly     = FALSE;
  machine->tlb[i].use          = FALSE;
  machine->tlb[i].dirty        = memoryTable[phyPage].dirty;

  //update the corresponding memoryTable
  memoryTable[phyPage].TLBentry=i;
  DEBUG('p', "The corresponding TLBentry for Page %i in TLB is %i ", vpn, i);
  //reset lastUsed to current ticks since it is being used at this moment.
  //for the implementation of LRU algorithm.
  memoryTable[phyPage].lastUsed = stats->totalTicks; 
  
  //increase the number of tlb misses
  stats->numTlbMisses++;
  
}

//----------------------------------------------------------------------
// PageOutPageIn
//      Calls DoPageOut and DoPageIn and handles memoryTable
// bookkeeping. Use lru algorithm to find the replacement page.
//----------------------------------------------------------------------

int PageOutPageIn(int vpn)
{
  int phyPage; 
  
  //increase the number of page faults
  stats->numPageFaults++;
  //call the LRU algorithm, which returns the freed physical frame
  phyPage=lruAlgorithm();
  
  //Page out the victim page to free the physical frame
  DoPageOut(phyPage);
  //Page in the new page to the freed physical frame
  DoPageIn(vpn, phyPage);
  
  //update memoryTable for this frame
  memoryTable[phyPage].valid=TRUE;
  memoryTable[phyPage].pid=currentThread->pid;
  memoryTable[phyPage].vPage=vpn;
  memoryTable[phyPage].dirty=FALSE;
  memoryTable[phyPage].TLBentry=-1;
  memoryTable[phyPage].lastUsed=0;
  memoryTable[phyPage].swapPtr=currentThread->space->swapPtr;
  
  return phyPage;
}

//----------------------------------------------------------------------
// DoPageOut
//      Actually pages out a phyPage to it's swapfile.
//----------------------------------------------------------------------

void DoPageOut(int phyPage)
{
  MmapEntry *mmapPtr;
  int numBytesWritten;
  int mmapBytesToWrite;

  if(memoryTable[phyPage].valid){           // check if pageOut possible
    if(memoryTable[phyPage].TLBentry!=-1){
      memoryTable[phyPage].dirty=
        machine->tlb[memoryTable[phyPage].TLBentry].dirty;
      machine->tlb[memoryTable[phyPage].TLBentry].valid=FALSE;
    }
    if(memoryTable[phyPage].dirty){        // pageOut is necessary
      if((mmapPtr=GetMmap(memoryTable[phyPage].vPage))){ // it's mmaped
        DEBUG('p', "mmap paging out: pid %i, phyPage %i, vpn %i\n",
          memoryTable[phyPage].pid, phyPage, memoryTable[phyPage].vPage);
        if(memoryTable[phyPage].vPage==mmapPtr->endPage)
          mmapBytesToWrite=mmapPtr->lastPageLength;
        else
          mmapBytesToWrite=PageSize;
        numBytesWritten=mmapPtr->openFile->
          WriteAt(machine->mainMemory+phyPage*PageSize, mmapBytesToWrite,
            (memoryTable[phyPage].vPage-mmapPtr->beginPage)*PageSize);
        ASSERT(mmapBytesToWrite==numBytesWritten);
      } else { // it's not mmaped
        DEBUG('p', "paging out: pid %i, phyPage %i, vpn %i\n",
          memoryTable[phyPage].pid, phyPage, memoryTable[phyPage].vPage);
        numBytesWritten=memoryTable[phyPage].swapPtr->
          WriteAt(machine->mainMemory+phyPage*PageSize, PageSize,
            memoryTable[phyPage].vPage*PageSize);
        ASSERT(PageSize==numBytesWritten);
      }
      
      //increase the number of page faults
      stats->numPageOuts++;
    }
    
    memoryTable[phyPage].valid=FALSE;
  }
}

//----------------------------------------------------------------------
// DoPageIn
//      Actually pages in a phyPage/vpn combo from the swapfile.
//----------------------------------------------------------------------

void DoPageIn(int vpn, int phyPage)
{
  MmapEntry *mmapPtr;
  int numBytesRead;
  int mmapBytesToRead;

  if((mmapPtr=GetMmap(vpn))){ // mmaped file
    DEBUG('p', "mmap paging in: pid %i, phyPage %i, vpn %i\n",
      currentThread->pid, phyPage, vpn);
    if(vpn==mmapPtr->endPage)
      mmapBytesToRead=mmapPtr->lastPageLength;
    else
      mmapBytesToRead=PageSize;
    numBytesRead=
      mmapPtr->openFile->ReadAt(machine->mainMemory+phyPage*PageSize,
                mmapBytesToRead,
                (vpn-mmapPtr->beginPage)*PageSize);
    ASSERT(numBytesRead==mmapBytesToRead);
  } else { // not mmaped
    DEBUG('p', "paging in: pid %i, phyPage %i, vpn %i\n", currentThread->pid,
      phyPage, vpn);
    numBytesRead=currentThread->space->swapPtr->ReadAt(machine->mainMemory+
                   phyPage*PageSize,
                   PageSize,
                   vpn*PageSize);
    ASSERT(PageSize==numBytesRead);
  }
}

//----------------------------------------------------------------------
// lruAlgorithm
//      Determine where a vpn should go in phymem, and therefore what
// should be paged out. This lru algorithm is the one discussed in the 
// lectures.
//----------------------------------------------------------------------

int lruAlgorithm(void)
{
  //your code here to find the physical frame that should be freed 
  //according to the LRU algorithm. 
  int phyPage;
  int smallest_tick = memoryTable[0].lastUsed;
  int i;

  // Manually added
  printf("lruAlgorithm triggered, we are now printing the corresponding output.\n\n");
  
  for (i=0; i<NumPhysPages; i++) {
	if (!memoryTable[i].valid) {
		printf("Insert to physical frame     = %d\n\n", i);
		printf("====================\n");
		return i;
	}
	if (memoryTable[i].lastUsed<smallest_tick) {
		phyPage = i;
		smallest_tick = memoryTable[i].lastUsed;
	}
  }
  printf("Insert to physical frame     = %d\n\n", phyPage);
  printf("====================\n");
  return phyPage;
}

//----------------------------------------------------------------------
// GetMmap
//      Return an MmapEntry structure corresponding to the vpn.  Returns
// 0 if does not exist.
//----------------------------------------------------------------------

MmapEntry *GetMmap(int vpn)
{
  MmapEntry *mmapPtr;

  mmapPtr=currentThread->space->mmapEntries;
  while(mmapPtr->next){
    mmapPtr=mmapPtr->next;
    if(vpn>=mmapPtr->beginPage && vpn<=mmapPtr->endPage)
      return mmapPtr;
  }
  return 0;
}

//----------------------------------------------------------------------
// PageOutMmapSpace
//      Pages out stuff being mmaped (or just between beginPage and
// endPage.
//----------------------------------------------------------------------

void PageOutMmapSpace(int beginPage, int endPage)
{
  int vpn;
  int phyPage;

  for(vpn=beginPage; vpn<=endPage; vpn++){
    if((phyPage=VpnToPhyPage(vpn))==-1)
      continue;
    DoPageOut(phyPage);
  }
}
