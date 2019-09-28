//===-- producer.c --------------------------------------------------------===//
//
// Authors: Ghassan Shobaki, Michael Dorst
//
//===----------------------------------------------------------------------===//
// CSC 139
// Fall 2019
// Section 2
// Tested on: CentOS 6.10 (athena)
//===----------------------------------------------------------------------===//

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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
void Producer(int, int, int);
void InitShm(int, int);
void SetBufSize(int);
void SetItemCnt(int);
void SetOccupancy(int);
void SetHeaderVal(int, int);
int GetBufSize();
int GetItemCnt();
int GetOccupancy();
int GetHeaderVal(int);
void WriteAtBufIndex(int, int);
int ReadAtBufIndex(int);
int GetRand(int, int);

int stoi(const char *);
int randInRange(int, int, int);

int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    fprintf(stderr, "Invalid number of command-line arguments\n");
    exit(1);
  }

  // Bounded buffer size
  int bufSize = stoi(argv[1]);
  // Number of items to be produced
  int itemCnt = stoi(argv[2]);
  // Seed for the random number generator
  int randSeed = stoi(argv[3]);

  // Validate command line arguments

  if (bufSize < 1 || bufSize > 1000)
  {
    fprintf(stderr, "Buffer size must be between 1 and 1000\n");
    exit(1);
  }

  if (itemCnt < 1)
  {
    fprintf(stderr, "Item count must be greater than 0\n");
    exit(1);
  }

  // Function that creates a shared memory segment and initializes its header
  InitShm(bufSize, itemCnt);

  // Fork a child process
  pid_t pid = fork();

  if (pid < 0)
  {
    fprintf(stderr, "Fork Failed\n");
    exit(1);
  }
  else if (pid == 0)
  {
    // Child process
    printf("Launching Consumer \n");
    execlp("./consumer", "consumer", NULL);
  }
  else
  {
    // Parent process
    // Parent will wait for the child to complete
    printf("Starting Producer\n");

    // The function that actually implements the production
    Producer(bufSize, itemCnt, randSeed);

    printf("Producer done and waiting for consumer\n");
    wait(NULL);
    printf("Consumer Completed\n");
  }

  return 0;
}

void InitShm(int bufSize, int itemCnt)
{
  // Name of shared memory object to be passed to shm_open
  const char *name = "/OS_HW1_MichaelDorst";

  // Create a shared memory segment
  int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);

  if (shm_fd < 0)
  {
    fprintf(stderr, "Producer: Unable to create shared memory segment\n");
    exit(1);
  }

  // Set the size of the segment
  if (ftruncate(shm_fd, SHM_SIZE) == -1)
  {
    fprintf(stderr, "Producer: Unable to truncate the memory segment\n");
    exit(1);
  }

  // Map the segment to memory
  gShmPtr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

  if (gShmPtr == (void *)-1)
  {
    fprintf(stderr, "Producer: Unable to map the shared memory segment\n");
    exit(1);
  }

  // Set the values of the four integers in the header
  SetBufSize(bufSize);
  SetItemCnt(itemCnt);
  SetHeaderVal(PROD_LOCK, false);
  SetHeaderVal(CONS_LOCK, false);
  SetOccupancy(0);
}

void Producer(int bufSize, int itemCnt, int randSeed)
{
  srand(randSeed);

  int index = 0;
  int i;
  for (i = 0; i < itemCnt; i++)
  {
    // If buffer is full, wait for the consumer to read
    while (GetOccupancy() == bufSize) {}

    int val = GetRand(0, 3000);

    printf("Producing Item %4d with value %4d at Index %4d\n", i, val, index);
    WriteAtBufIndex(index, val);
    index = (index + 1) % bufSize;
    SetOccupancy(GetOccupancy() + 1);
  }

  printf("Producer Completed\n");
}

/**
 * Serves the same function as atoi(), but performs a number of checks.
 *
 * Checks that num is a numeric value, and also that it is in the range of `int`
 */
int stoi(const char *num)
{
  char *end;
  errno = 0;
  // Use strtol because atoi does not do any error checking
  // 10 is for base-10
  long val = strtol(num, &end, 10);
  // If there was an error, (if the string is not numeric)
  if (end == num || *end != '\0' || errno == ERANGE)
  {
    printf("%s is not a number.\n", num);
    exit(1);
  }
  // Check if val is outside of int range
  if (val < INT_MIN || val > INT_MAX)
  {
    printf("%s has too many digits.\n", num);
    exit(1);
  }
  // Casting long to int is now safe
  return (int)val;
}

// This is a blocking call. It will wait until it can aquire a safe lock.
// This lock will be used to guard `occupancy`, to prevent corrupting the value.
void lockResource()
{
  // This function must block until the producer is locked and the consumer is
  // unlocked at the exact same time. Therefore the producer must be locked
  // first, before the consumer is confirmed not to be locked.
  while (true)
  {
    // Attempt to lock producer
    SetHeaderVal(PROD_LOCK, true);
    if (GetHeaderVal(CONS_LOCK))
    {
      // Attempt failed. Consumer already has a lock.
      // Avoid deadlock by unlocking producer
      SetHeaderVal(PROD_LOCK, false);
      // Wait for consumer to unlock.
      while (GetHeaderVal(CONS_LOCK)) {}
      // At this point, consumer is unlocked. However we cannot simply lock the
      // producer and be done, because by the time we do that, the consumer may
      // be locked again. Therefore we must repeat this process, locking the
      // producer, then checking if the consumer is still unlocked. Most of the
      // time it will be on the second pass, but we have to be sure.
    }
    else
    {
      // We did it! The producer is locked and the consumer is unlocked.
      break;
    }
  }
}

void unlockResource() { SetHeaderVal(PROD_LOCK, false); }

// Set the value of shared variable "bufSize"
void SetBufSize(int val) { SetHeaderVal(BUF_SIZE, val); }

// Set the value of shared variable "itemCnt"
void SetItemCnt(int val) { SetHeaderVal(ITEM_CNT, val); }

// Set the value of shared variable "occupancy"
// This is a blocking call, as it will attempt to acquire a lock before writing.
void SetOccupancy(int val)
{
  lockResource();
  SetHeaderVal(OCCUPANCY, val);
  unlockResource();
}

// Set the value of the ith value in the header
void SetHeaderVal(int i, int val)
{
  void *ptr = gShmPtr + i * sizeof(int);
  memcpy(ptr, &val, sizeof(int));
}

// Get the value of shared variable "bufSize"
int GetBufSize() { return GetHeaderVal(BUF_SIZE); }

// Get the value of shared variable "itemCnt"
int GetItemCnt() { return GetHeaderVal(ITEM_CNT); }

// Get the value of shared variable "occupancy"
// This is a blocking call, as it will attempt to acquire a lock before reading.
int GetOccupancy()
{
  lockResource();
  int occupancy = GetHeaderVal(OCCUPANCY);
  unlockResource();
  return occupancy;
}

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
