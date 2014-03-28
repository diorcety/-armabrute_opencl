#ifndef _ARMABRUT_H_
#define _ARMABRUT_H_

#include <time.h>

#ifndef BUILD_DLL

void print_found2(unsigned long hash, unsigned long key);
void print_progress2(double checked, double all, unsigned int start);
void print_error2(const char* error);

#endif

//Callbacks
typedef void (*PRINT_FOUND)(unsigned long checksum, unsigned long key);
typedef void (*PRINT_PROGRESS)(double checked, double all, time_t* start);
typedef void (*PRINT_ERROR)(const char* error_msg);

#define MAX_HASHES 32

typedef struct _hash_list
{
    int count;
    unsigned long hash[MAX_HASHES];
} hash_list;

void arma_do_brute(int alg, hash_list *list, unsigned long from, unsigned long to, unsigned long* param, PRINT_FOUND print_found, PRINT_PROGRESS print_progress, PRINT_ERROR print_error, time_t* time_start, int* stop);

#endif
