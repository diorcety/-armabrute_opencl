#ifdef BUILD_DLL

#include <time.h>
#include <algorithm>
#include "dll.h"

PRINT_FOUND print_found_dll;
PRINT_PROGRESS print_progress_dll;
PRINT_ERROR print_error_dll;
int stop=0;

void DLL_EXPORT BruteSettings(HWND parent)
{
    MessageBoxA(parent, "No settings to tweak :)", "armabrut", MB_ICONINFORMATION);
}

void DLL_EXPORT BruteStop()
{
    stop=1;
}

void DLL_EXPORT SetCallbacks(PRINT_FOUND cb1, PRINT_PROGRESS cb2, PRINT_ERROR cb3)
{
    print_found_dll=cb1;
    print_progress_dll=cb2;
    print_error_dll=cb3;
}

void DLL_EXPORT BruteStart(int alg, hash_list *list, unsigned long from, unsigned long to, unsigned long* param)
{
    time_t start=time(NULL);
    unsigned long to_=to;
    if(alg==6||alg==7||alg==8)
        to_=100000000u;
    std::sort(&list->hash[0], &list->hash[list->count]);
    arma_do_brute(alg, list, from, to, param, print_found_dll, print_progress_dll, print_error_dll, &start, &stop);
    stop=0;
}

extern "C" BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
}

#endif
