// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>
#include "benchmarks.h"

// Benchmarks to run
const char* benchmarks[] = {
    "loadLoop"
  , "storeLoop"
  , "modifyLoop"
  , "copyLoop"
  , "cacheLoop"
  , "scratchpadLoop"
  , "messageLoop"
  , NULL
};

int main()
{
  HostLink hostLink;

  printf("Starting\n");
  for (int i = 0; benchmarks[i] != NULL; i++) {
    // Boot benchmark
    char codeFile[256], dataFile[256];
    snprintf(codeFile, sizeof(codeFile), "%s-code.v", benchmarks[i]);
    snprintf(dataFile, sizeof(dataFile), "%s-data.v", benchmarks[i]);
    hostLink.boot(codeFile, dataFile);

    // Trigger execution
    hostLink.go();

    // Wait for response
    const int numThreads = 1 << LogThreadsUsed;
    uint32_t msg[1 << TinseLogWordsPerMsg];
    uint32_t total = 0;
    for (int j = 0; j < numThreads; j++) {
      hostLink.recv(msg);
      total += msg[0];
    }
    printf("%s: %d cycles on average\n", benchmarks[i], total/numThreads);
  }

  return 0;
}
