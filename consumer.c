//===-- consumer.c --------------------------------------------------------===//
//
// Authors: Ghassan Shobaki, Michael Dorst
//
//===----------------------------------------------------------------------===//
// CSC 139
// Fall 2019
// Section 2
// Tested on: CentOS 6.10 (athena)
//===----------------------------------------------------------------------===//

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>

// Size of shared memory block
// Pass this to ftruncate and mmap
#define SHM_SIZE 4096
// The index of bufferSize in the shared memory header
#define BUF_SIZE 0
// The index of itemCnt in the shared memory header
#define ITEM_CNT 1
// The index of occupancy in the shared memory header
// Occupancy is used to track how many items have yet to be consumed.
#define OCCUPANCY 2
// Because occupancy will be written to by both processes, it requires a lock
// to ensure that both do not try to write its value at the same time.
// Each process must have its own lock, or else the lock would also be written
// by both processes, which would be just as bad.
// The index of the producer lock in the shared memory header
#define PROD_LOCK 3
// The index of the consumer lock in the shared memory header
#define CONS_LOCK 4
// The size of the header
#define HEADER_SIZE 5

// Global pointer to the shared memory block
// This should receive the return value of mmap
// Don't change this pointer in any function
void *gShmPtr;

// You won't necessarily need all the functions below
void SetOccupancy(int);
void SetHeaderVal(int, int);
int GetBufSize();
int GetItemCnt();
int GetOccupancy();
int GetHeaderVal(int);
void WriteAtBufIndex(int, int);
int ReadAtBufIndex(int);

int main()
{
  // Name of shared memory block to be passed to shm_open
  const char *name = "/OS_HW1_MichaelDorst";

  // Open the shared memory segment created by the producer
  int shm_fd = shm_open(name, O_RDWR, 0666);
  // Check for errors
  if (shm_fd < 0)
  {
    fprintf(stderr, "Consumer: Unable to open shared memory segment\n");
    exit(1);
  }
  // Map the segment to memory
  gShmPtr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  // Check for errors
  if (gShmPtr == (void *)-1)
  {
    fprintf(stderr, "Consumer: Unable to map the shared memory segment\n");
    exit(1);
  }

  int bufSize = GetBufSize();
  int itemCnt = GetItemCnt();

  printf("Consumer reading: bufSize = %d\n", bufSize);
  printf("Consumer reading: itemCnt = %d\n", itemCnt);
  printf("Consumer reading: occupancy = %d\n", GetOccupancy());

  int index = 0;
  int i;
  for (i = 0; i < itemCnt; i++)
  {
    // If buffer is empty, wait for the producer to write
    while (GetOccupancy() == 0) {}

    int val = ReadAtBufIndex(index);
    printf("Consuming Item %4d with value %4d at Index %4d\n", i, val, index);
    index = (index + 1) % bufSize;
    SetOccupancy(GetOccupancy() - 1);
  }

  // remove the shared memory segment
  if (shm_unlink(name) == -1)
  {
    printf("Error removing %s\n", name);
    exit(-1);
  }

  return 0;
}

// This is a blocking call. It will wait until it can aquire a safe lock.
// This lock will be used to guard `occupancy`, to prevent corrupting the value.
void lockResource()
{
  // For rationale on this code, see `lockResource()` in `producer.c`
  while (true)
  {
    SetHeaderVal(CONS_LOCK, true);
    if (GetHeaderVal(PROD_LOCK))
    {
      SetHeaderVal(CONS_LOCK, false);
      while (GetHeaderVal(PROD_LOCK)) {}
    }
    else
    {
      break;
    }
  }
}

void unlockResource() { SetHeaderVal(CONS_LOCK, false); }

// Set the value of shared variable "occupancy"
void SetOccupancy(int val) { SetHeaderVal(2, val); }

// Set the value of the ith value in the header
void SetHeaderVal(int i, int val)
{
  void *ptr = gShmPtr + i * sizeof(int);
  memcpy(ptr, &val, sizeof(int));
}

// Get the value of shared variable "bufSize"
int GetBufSize() { return GetHeaderVal(0); }

// Get the value of shared variable "itemCnt"
int GetItemCnt() { return GetHeaderVal(1); }

// Get the value of shared variable "occupancy"
int GetOccupancy() { return GetHeaderVal(2); }

// Get the ith value in the header
int GetHeaderVal(int i)
{
  int val;
  void *ptr = gShmPtr + i * sizeof(int);
  memcpy(&val, ptr, sizeof(int));
  return val;
}

// Write the given val at the given index in the bounded buffer
void WriteAtBufIndex(int indx, int val)
{
  // Skip the four-integer header and go to the given index
  void *ptr = gShmPtr + HEADER_SIZE * sizeof(int) + indx * sizeof(int);
  memcpy(ptr, &val, sizeof(int));
}

// Read the val at the given index in the bounded buffer
int ReadAtBufIndex(int indx)
{
  int val;

  // Skip the four-integer header and go to the given index
  void *ptr = gShmPtr + HEADER_SIZE * sizeof(int) + indx * sizeof(int);
  memcpy(&val, ptr, sizeof(int));
  return val;
}

// Get a random number in the range [x, y]
int GetRand(int x, int y)
{
  int r = rand();
  r = x + r % (y - x + 1);
  return r;
}
