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

// Global pointer to the shared memory block
// This should receive the return value of mmap
// Don't change this pointer in any function
void *gShmPtr;

// You won't necessarily need all the functions below
void Producer(int, int, int);
void InitShm(int, int);
void SetBufSize(int);
void SetItemCnt(int);
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
  int in = 0;
  int out = 0;
  // Name of shared memory object to be passed to shm_open
  const char *name = "OS_HW1_MichaelDorst";

  gShmPtr = mmap(NULL, (bufSize + 4) * sizeof(int), PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, NULL, 0);

  // Set the values of the four integers in the header
  SetBufSize(bufSize);
  SetItemCnt(itemCnt);
  SetIn(in);
  SetOut(out);
}

void Producer(int bufSize, int itemCnt, int randSeed)
{
  int in = 0;
  int out = 0;

  srand(randSeed);

  // Write code here to produce itemCnt integer values in the range [0-3000]
  // Use the functions provided below to get/set the values of shared variables
  // "in" and "out"
  // Use the provided function WriteAtBufIndex() to write into the bounded
  // buffer
  // Use the provided function GetRand() to generate a random number in the
  // specified range
  // **Extremely Important: Remember to set the value of any shared variable you
  // change locally
  // Use the following print statement to report the production of an item:
  // printf("Producing Item %d with value %d at Index %d\n", i, val, in);
  // where i is the item number, val is the item value, in is its index in the
  // bounded buffer

  for (int i = 0; i < itemCnt; i++)
  {
    int random = GetRand(0, 3000);
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

// Set the value of shared variable "bufSize"
void SetBufSize(int val) { SetHeaderVal(0, val); }

// Set the value of shared variable "itemCnt"
void SetItemCnt(int val) { SetHeaderVal(1, val); }

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
