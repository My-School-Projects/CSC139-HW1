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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>

// Size of shared memory block
// Pass this to ftruncate and mmap
#define SHM_SIZE 4096

// Global pointer to the shared memory block
// This should receive the return value of mmap
// Don't change this pointer in any function
void *gShmPtr;

// You won't necessarily need all the functions below
void SetIn(int);
void SetOut(int);
void SetHeaderVal(int, int);
int GetBufSize();
int GetItemCnt();
int GetIn();
int GetOut();
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
  if (gShmPtr == (void *) -1)
  {
    fprintf(stderr, "Consumer: Unable to map the shared memory segment\n");
    exit(1);
  }

  int bufSize = GetBufSize();
  int itemCnt = GetItemCnt();
  int out = GetOut();

  printf("Consumer reading: bufSize = %d\n", bufSize);
  printf("Consumer reading: itemCnt = %d\n", itemCnt);
  printf("Consumer reading: in = %d\n", GetIn());
  printf("Consumer reading: out = %d\n", out);

  int i;
  for (i = 0; i < itemCnt; i++)
  {
    // If buffer is empty, wait for the producer to write
    while (GetIn() == out) {}

    int val = ReadAtBufIndex(out);
    printf("Consuming Item %4d with value %4d at Index %4d\n", i, val, out);
    SetOut(out = (out + 1) % bufSize);
  }

  // remove the shared memory segment
  if (shm_unlink(name) == -1)
  {
    printf("Error removing %s\n", name);
    exit(-1);
  }

  return 0;
}

// Set the value of shared variable "in"
void SetIn(int val) { SetHeaderVal(2, val); }

// Set the value of shared variable "out"
void SetOut(int val) { SetHeaderVal(3, val); }

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

// Get the value of shared variable "in"
int GetIn() { return GetHeaderVal(2); }

// Get the value of shared variable "out"
int GetOut() { return GetHeaderVal(3); }

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
  void *ptr = gShmPtr + 4 * sizeof(int) + indx * sizeof(int);
  memcpy(ptr, &val, sizeof(int));
}

// Read the val at the given index in the bounded buffer
int ReadAtBufIndex(int indx)
{
  int val;

  // Skip the four-integer header and go to the given index
  void *ptr = gShmPtr + 4 * sizeof(int) + indx * sizeof(int);
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
