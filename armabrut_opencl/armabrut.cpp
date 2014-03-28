#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include "armabrut.h"

#ifdef _WIN32
#include <conio.h>
#elif linux
#ifndef STDIN_FILENO
#define STDIN_FILENO 1
#endif
void _putch(char c)
{
    putchar(c);
};
char _getch()
{
    return getchar();
};
int _kbhit()
{
    struct timeval tv;
    fd_set rdfs;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&rdfs);
    FD_SET(STDIN_FILENO, &rdfs);

    select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &rdfs);

}
#include <strings.h>
int _stricmp(const char *s1, const char *s2)
{
    return strcasecmp(s1, s2);
}
#endif

#ifndef BUILD_DLL

#include "md5.h"

#define max_bufsize 65535

bool verify_crypt_cert=false;
unsigned char* data;
unsigned int _magic1=0, _magic2=0, _md5_ecdsa=0, data_size=0;


unsigned int GenerateNumber_core(int push_value, int* in_value) //NextRandomRange
{
    int val1=*in_value/0x2710;
    int val2=*in_value%0x2710;
    int res1=val2*0x16BD;
    int res2=val1*0x16BD;
    int res3=val2*0xC45;
    unsigned int new1=res2+res3;
    unsigned int new2=new1%0x2710;
    unsigned int new3=new2*0x2710;
    unsigned int new4=new3+res1;
    unsigned int div1=new4%0x5F5E100+1;
    unsigned int div2=div1%0x5F5E100;
    *in_value=div2;
    unsigned int div3=div2/0x2710;
    unsigned int div4=div3*push_value;
    unsigned int ret1=div4/0x2710;
    return ret1;
}

unsigned int GenerateNumberDword(int* in_value) //NextRandomNumber
{
    unsigned char val1=(unsigned char)GenerateNumber_core(0x100, in_value);
    unsigned char val2=(unsigned char)GenerateNumber_core(0x100, in_value);
    unsigned char val3=(unsigned char)GenerateNumber_core(0x100, in_value);
    unsigned char val4=(unsigned char)GenerateNumber_core(0x100, in_value);
    return (((val1<<24)|(val2<<16))|(val3<<8))|val4;
}

void TEA_Decrypt_Nrounds(unsigned int *k, unsigned int *data, unsigned int rounds)
{
    unsigned int v0, v1, sum, i;
    unsigned int delta=0x9e3779b9;
    unsigned int k0=k[0], k1=k[1], k2=k[2], k3=k[3];

    v0=data[0];
    v1=data[1];
    while(rounds--)
    {
        sum=0xC6EF3720;
        for(i=0; i<32; i++)
        {
            v1-=((v0<<4)+k2)^(v0+sum)^((v0>>5)+k3);
            v0-=((v1<<4)+k0)^(v1+sum)^((v1>>5)+k1);
            sum-=delta;
        }
    }
    data[0]=v0;
}

int brute(unsigned int _sym) //Verify the symmetric
{
    unsigned int* buffer_400;

    unsigned int magic1=_magic1;
    unsigned int magic2=_magic2;
    unsigned int sym=_sym;
    unsigned int md5_ecdsa=_md5_ecdsa;

    unsigned int sym_xor_magic1=magic1^sym;
    unsigned int sym_xor_magic1_mul_10_plus_1=(magic1^sym)*10+1;
    unsigned int sym_xor_md5_ecdsa=sym^md5_ecdsa;
    unsigned int sym_not_xor_magic1=magic1^~sym;

    buffer_400=(unsigned int*)malloc(0x400);
    for(unsigned int i=0; i<0x100; i++) //loop1
    {
        unsigned int val1=GenerateNumberDword((int*)&sym_xor_magic1);
        unsigned int val2=GenerateNumberDword((int*)&sym_xor_md5_ecdsa);
        unsigned int val3=GenerateNumberDword((int*)&sym_xor_magic1_mul_10_plus_1);
        unsigned int val4=val1^val2^val3^sym;
        buffer_400[i]=val4;
    }
    unsigned int new_value=GenerateNumber_core(0x10, (int*)&sym_xor_magic1)+GenerateNumber_core(0x10, (int*)&sym_xor_md5_ecdsa);
    unsigned char shr=(unsigned char)new_value;
    for(unsigned int i=0,j=0; i<magic2; i++,j++) //loop2
    {
        if(j==0x100)
            j=0;
        unsigned int new_value_and3=(buffer_400[j]>>shr)&3;
        if(!new_value_and3)
        {
            buffer_400[j]|=GenerateNumberDword((int*)&sym_xor_magic1);
            GenerateNumberDword((int*)&sym_xor_md5_ecdsa);
        }
        else
        {
            if(new_value_and3==1)
            {
                buffer_400[j]&=GenerateNumberDword((int*)&sym_xor_md5_ecdsa);
                GenerateNumberDword((int*)&sym_xor_magic1);
            }
            else
                buffer_400[j]^=(GenerateNumberDword((int*)&sym_xor_magic1)^GenerateNumberDword((int*)&sym_xor_md5_ecdsa));
        }
    }
    unsigned int hash[4]= {0};
    md5((long unsigned int*)hash, (void*)buffer_400, 0x400);
    free(buffer_400);
    unsigned int rounds=GenerateNumber_core(0x190, (int*)&sym_not_xor_magic1)+0x321;
    unsigned int buf_size=0, mini_crc=0, keyA=0;
    unsigned char key_mini_crc=hash[0]&7;
    unsigned char* p_block=0;
    const unsigned char *p_certs=data;
    const unsigned char *p_certs_array_end=data+data_size;

    while(1)
    {
        if(p_certs<p_certs_array_end)
        {
            buf_size=*(unsigned int*)p_certs;
            p_certs+=4;
            if(buf_size)
            {
                mini_crc=*p_certs;
                p_certs++;
                if(key_mini_crc==mini_crc)
                {
                    p_block=(unsigned char*)malloc(8);
                    memcpy(p_block, p_certs, 8);
                    p_certs = p_certs + buf_size;
                    TEA_Decrypt_Nrounds((unsigned int*)hash, (unsigned int*)p_block, rounds + 2);
                    keyA = *(unsigned int *)(p_block);
                    free(p_block);
                    if(keyA == 0xFFFFFFFF || ((keyA & 0xFFFFFFF0) == 0))
                        return 1;
                }
                else
                    p_certs=p_certs+buf_size;
            }
            else
                break;
        }
        else
            break;
    }
    return 0;
}



static FILE  *outf;  /* output stream           */
static int    found; /* number of found keys    */
static time_t start_time; /* start time (in seconds) */

void print_found2(unsigned long hash, unsigned long key)
{
    if(!verify_crypt_cert) //Verify symmetric key...
    {
        if(outf==NULL)
        {
            int len=printf("\r%0.8X = %0.8X", hash, key);
            while(len++<79)
                _putch(' ');
            _putch('\n');
        }
        else
        {
            fprintf(outf, "%0.8X = %0.8X\n", hash, key);
            fflush(outf);
        }
        found++;
    }
    else
    {
        if(brute(key))
        {
            if(outf==NULL)
            {
                int len=printf("\r%0.8X = %0.8X", hash, key);
                while(len++ < 79)
                    _putch(' ');
                _putch('\n');
            }
            else
            {
                fprintf(outf, "%0.8X = %0.8X\n", hash, key);
                fflush(outf);
            }
            found++;
            exit(0);
        }
    }
}

/*void print_found2(unsigned long hash, unsigned long key)
{
    if(outf == NULL)
    {
        int len = printf("\r%0.8X = %0.8X", hash, key);
        while(len++ < 79) _putch(' ');
        _putch('\n');
    }
    else
    {
        fprintf(outf, "%0.8X = %0.8X\n", hash, key);
        fflush(outf);
    }
    found++;
}*/

void print_progress2(double checked, double all, time_t* start)
{
    // display perfomance counters
    double pdone = (checked / all) * 100.0;
    double elaps = (double)(time(NULL) - *start);
    double estim = ((100.0 - pdone) / (pdone / elaps)) / 60.0;
    double speed = checked / elaps;

    int len = printf("\rdone: %-.3f%%, %d key(s) found, estimated %-.2f min, speed %-.0f hash/sec",
                     pdone, found, estim, speed);

    while(len++ < 80) _putch(' ');

    if(_kbhit() != 0 && _getch() == ' ')
    {
        time_t pause = time(NULL);
        while(_getch() != ' ');
        *start += (time(NULL) - pause);
    }
}

void print_error2(const char* error)
{
    puts(error);
}

static char *getarg(char *id, int argc, char *argv[])
{
    for(int i = 1; i < argc - 1; i++)
    {
        if(_stricmp(argv[i], id) == 0) return argv[i + 1];
    }
    return NULL;
}

unsigned short MakeDate(unsigned int year, unsigned int month, unsigned int day)
{
    const unsigned long secondsperday=(24*60*60);
    const int dateoffset=10592;

    struct tm tm= {0};
    tm.tm_year=year-1900;
    tm.tm_mon=month-1;
    tm.tm_mday=day+1;
    tm.tm_hour=0;
    tm.tm_min=0;
    tm.tm_sec=0;
    unsigned long seconds=mktime(&tm);
    if(seconds==(unsigned long)(-1))
        return (unsigned short)(-1);

    long days=(seconds/secondsperday);
    if(days<dateoffset)
        return (unsigned short)(-1);

    return (unsigned short)(days-dateoffset);
}

int main(int argc, char *argv[])
{
    char *p_alg=getarg("-a", argc, argv);
    char *p_hash=getarg("-h", argc, argv);
    char *pparam=getarg("-p", argc, argv);
    int alg;

    if(
        (p_alg==NULL) ||
        (p_hash==NULL) ||
        ((alg=atoi(p_alg))==1&&pparam==NULL))
    {
        printf("armabrut -a [alg] -h [hash] -p [param] -o [out file] -f [from key] -t [to key]\n");
        return 0;
    }
    unsigned long a_param[5];
    a_param[4]=7;
    memset(a_param, 0, sizeof(a_param));
    if(alg==3||alg==4)
    {
        FILE* pFile;
        size_t result;
        verify_crypt_cert=true; //we must verify all
        alg-=3; //give original algo number
        char* certpath=getarg("-certs", argc, argv);
        char* val1=getarg("-magic1", argc, argv);
        char* val2=getarg("-magic2", argc, argv);
        char* val3=getarg("-md5", argc, argv);
        if(!certpath||!val1||!val2)
        {
            puts("You missed some arguments, please try again...\n-certs [file] -magic1 [magic1] -magic2 [magic2] -md5 [md5 public (optional)]");
            return 0;
        }

        sscanf(val1, "%X", &_magic1);
        sscanf(val2, "%X", &_magic2);
        if(val3)
            sscanf(val3, "%X", &_md5_ecdsa);

        /*replace with linux compatible stuff
        HANDLE hFile=CreateFileA(certpath, GENERIC_ALL, 0, 0, OPEN_EXISTING, 0, 0);
        if(hFile==INVALID_HANDLE_VALUE)
        {
        	puts("Could not open certs file!");
        	return 0;
        }
        data_size=GetFileSize(hFile, 0);
        data=(unsigned char*)malloc(data_size);
        DWORD read=0;
        ReadFile(hFile, data, data_size, &read, 0);
        CloseHandle(hFile);
        */

        pFile=fopen(certpath, "rb");
        if(pFile==NULL)
        {
            puts("File error");
            return 0;
        }

        fseek(pFile, 0, SEEK_END);
        data_size=ftell(pFile);
        rewind(pFile);

        data=(unsigned char*)malloc(data_size);
        if(!data)
        {
            puts("Memory error");
            return 0;
        }

        result=fread(data, 1, data_size, pFile);
        if(result!=data_size)
        {
            puts("Reading error");
            return 0;
        }
        fclose(pFile);
    }
    else if(alg==7||alg==8)
    {
        char* pdata=getarg("-d", argc, argv);
        if(!pdata)
        {
            puts("You missed some arguments, please try again...\n-d [data]");
            return 0;
        }
        sscanf(pdata, "%X", &a_param[1]);
    }

    unsigned long param=pparam!=NULL?strtoul(pparam, NULL, 16):0;
    a_param[0]=param;
    char *p_out=getarg("-o", argc, argv);
    char *pfrom=getarg("-f", argc, argv);
    char *p_to=getarg("-t", argc, argv);
    unsigned long from=pfrom!=NULL?strtoul(pfrom, NULL, 16):0;
    unsigned long to=p_to!=NULL?strtoul(p_to, NULL, 16):0xFFFFFFFF;

    if(from>to)
    {
        printf("Invalid brute range\n");
        return 0;
    }
    if((p_out!=NULL)&&((outf=fopen(p_out, "a"))==NULL))
    {
        printf("Can not open output file\n");
        return 0;
    }
    hash_list list;

    for(list.count=0; list.count<MAX_HASHES; p_hash++)
    {
        list.hash[list.count++]=strtoul(p_hash, NULL, 16);
        if((p_hash=strchr(p_hash, ','))==NULL)
            break;
    }
    std::sort(&list.hash[0], &list.hash[list.count]);
    start_time=time(NULL);
    if(alg==6||alg==7||alg==8)
        to=0x5F5E101;
    int stop=0;
	DWORD ticks=GetTickCount();
    arma_do_brute(alg, &list, from, to, a_param, print_found2, print_progress2, print_error2, &start_time, &stop);
	ticks=GetTickCount()-ticks;
	printf_s("\n%ums -> %fs\n", ticks, (double)ticks/1000);
    return 0;
}

#endif
