/*
*
* Copyright (c) 2013
*
* cypher <the.cypher@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "time.h"
#include "armabrut.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <sstream>
#include <CL\cl.h>
#include <Windows.h>

#define MAX_OUT_KEYS (16384-1)
// HD5870/6870 runs fine with FF0FF. Seems to be max before display driver dies for ATI
// GTX 550ti/560ti/650 runs fine with FE01
// Value NEEDS to be dividable by 0xFF
#define CYCLE_STEP_1  (0x00000FE01)
//could run full range in one step but doing 0x100 cycles for better progress printing
#define CYCLE_STEP_02 (0x10000FE)

//kernel names for different algos
const char algKernels[9][10] = {"arma_alg0","arma_alg1", "arma_alg2", "arma_alg3", "arma_alg4", "arma_alg5", "arma_alg6", "arma_alg7", "arma_alg8"};

PRINT_ERROR  errorCallback;
int* stopBrute;

//general CL stuff
cl_context context = NULL;
cl_device_id* devices;
cl_command_queue commandQueue;
cl_program program;
cl_kernel  kernel;
//IN/OUT data and buffers
cl_uint* out_hashes;
cl_uint* out_keys;
cl_uint* in_hashes;
unsigned int salt;
unsigned int seed;
cl_uint num_keys_found;
cl_mem inBuf;
cl_mem outHashesBuf;
cl_mem outKeysBuf;
cl_mem numKeysFoundBuf;

//prints error and info messages depending on release/debug/dll setting
inline void checkErr(cl_int status, char* msg, ...)
{
#ifndef BUILD_DLL
    char* buf = new char[strlen(msg)+20];
    va_list argptr;
    va_start(argptr, msg);
    if (status != CL_SUCCESS) {
        strcpy(buf, "Error: ");
        strcat(buf, msg);
        strcat(buf, " failed\n");
        vfprintf(stderr, buf, argptr);
    } else {
#ifdef DEBUG
        strcpy(buf, "INFO: ");
        strcat(buf, msg);
        strcat(buf, "\n");
        vfprintf(stderr, buf, argptr);
#endif
    }
    va_end(argptr);
#else
    char* buf = new char[strlen(msg)+20];
    char* bufFormatted = new char[strlen(msg)+20];
    va_list argptr;
    va_start(argptr, msg);
    if (status != CL_SUCCESS) {
        strcpy(buf, "Error: ");
        strcat(buf, msg);
        strcat(buf, " failed\n");
        vsprintf(bufFormatted, buf, argptr);
        errorCallback(bufFormatted);
    } else {
#ifdef DEBUG
        strcpy(buf, "INFO: ");
        strcat(buf, msg);
        strcat(buf, "\n");
        vsprintf(bufFormatted, buf, argptr);
        errorCallback(bufFormatted);
#endif
    }

#endif
}

//mem alloc and variable setting etc. needs to be run once per session only
int initializeHost(unsigned long* param)
{
    salt = param[0];
    seed = param[1];
    num_keys_found = 0;
    out_hashes = NULL;
    out_keys = NULL;
    in_hashes = NULL;

    //alloc memory for host
    cl_uint sizeInBytes = MAX_OUT_KEYS * sizeof(cl_uint);
    out_hashes = (cl_uint *) malloc(sizeInBytes);
    out_keys = (cl_uint *) malloc(sizeInBytes);
    sizeInBytes = MAX_HASHES * sizeof(cl_uint);
    in_hashes = (cl_uint *) malloc(sizeInBytes);

    if(!out_hashes || !out_keys || !in_hashes) {
        checkErr(-1,"Init output variables");
        return CL_FALSE;
    }

    //initialize arrays
    for(int i=0; i<MAX_OUT_KEYS; i++) {
        out_hashes[i] = 0xDEADBEEF;
        out_keys[i] = 0xDEADBEEF;
    }

    return CL_TRUE;
}

//setting up OpenCL with our kernels etc. needs to be run once per session only
int initializeCL(int alg)
{
    cl_int status = 0;
    size_t deviceListSize = 0;
    size_t tempSize = 0;
    cl_uint numPlatforms = 0;
    cl_platform_id platform = NULL;
    cl_device_id device = NULL;
    bool gpuFound = false;

    //get suitable device: GPU
    status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if(status != CL_SUCCESS) {
        checkErr(status,"Getting Platforms");
        return CL_FALSE;
    }

    if(numPlatforms > 0) {
        cl_platform_id* platforms = new cl_platform_id[numPlatforms];
        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if(status != CL_SUCCESS) {
            checkErr(status,"Getting Platform IDs");
            return CL_FALSE;
        }
        for(unsigned int i=0; i < numPlatforms; ++i) {
            platform = platforms[i];
            status = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 1, &device, NULL);
            if(status == CL_SUCCESS) {
                gpuFound = true;
                break; //we found a GPU device
            }
        }
        delete platforms;
    }

    if(NULL == platform) {
        checkErr(-1,"NULL platform found");
        return CL_FALSE;
    }

    if(!gpuFound) {
        checkErr(-1,"No GPU device found");
        return CL_FALSE;
    }

    //create context
    /*
    cl_context_properties cps[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platform, 0 };
    context = clCreateContextFromType(cps, CL_DEVICE_TYPE_GPU, NULL,  NULL, &status);
    if(status != CL_SUCCESS) {
        checkErr(status,"Creating GPU Context");

        context = clCreateContextFromType(cps, CL_DEVICE_TYPE_CPU, NULL,  NULL, &status);
        if(status != CL_SUCCESS) {
            checkErr(status,"Creating GPU or CPU Context");
            return CL_FALSE;
        }
        checkErr(status,"Using CPU");
    } else {
        checkErr(status,"Using GPU");
    }*/

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if(status != CL_SUCCESS) {
        char buf[20];
        sprintf(buf, "Creating GPU Context %d", status);
        return CL_FALSE;
    }

    //query context for device list
    status = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL, &deviceListSize);
    if(status != CL_SUCCESS) {
        checkErr(status,"Getting Context Info (deviceListSize)");
        return CL_FALSE;
    }

    int deviceCount = (int)(deviceListSize / sizeof(cl_device_id));
    checkErr(CL_SUCCESS,"Device count: %d", deviceCount);

    devices = (cl_device_id*)malloc(deviceListSize);
    if(devices == 0) {
        checkErr(status,"No devices found");
        return CL_FALSE;
    }

    status = clGetContextInfo(context, CL_CONTEXT_DEVICES, deviceListSize, devices, NULL);
    if(status != CL_SUCCESS) {
        checkErr(status,"Getting Context Info (devices)");
        return CL_FALSE;
    }

    //getting info about available extensions on platform
    status = clGetDeviceInfo(devices[0],  CL_DEVICE_EXTENSIONS, 0, NULL, &tempSize);
    char* extensions = new char[tempSize];
    status = clGetDeviceInfo(devices[0], CL_DEVICE_EXTENSIONS, sizeof(char) * tempSize, extensions, NULL);
    checkErr(status,"Available extensions: %s",extensions);

    //creating command queue. for now on first device. TODO: optimize to use all or best device
    commandQueue = clCreateCommandQueue(context, devices[0], 0, &status);
    if(status != CL_SUCCESS) {
        checkErr(status,"Creating Command Queue");
        return CL_FALSE;
    }

    //creating kernel IN/OUT buffers
    inBuf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MAX_HASHES * sizeof(cl_uint), in_hashes, &status);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Creating input buffer");
        return CL_FALSE;
    }

    outHashesBuf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MAX_OUT_KEYS * sizeof(cl_uint), out_hashes, &status);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Creating output buffer");
        return CL_FALSE;
    }

    outKeysBuf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MAX_OUT_KEYS * sizeof(cl_uint), out_keys, &status);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Creating output buffer");
        return CL_FALSE;
    }

    numKeysFoundBuf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(cl_uint), &num_keys_found, &status);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Creating output buffer");
        return CL_FALSE;
    }

    //building kernel: load *.cl, create program object, create kernel objects
    //const char* filename  = "Z:\\__dev\\armabrut_opencl\\armabrut_opencl\\brute_opencl.cl";
    char modPath[MAX_PATH];
    char kernelPath[MAX_PATH];
    HMODULE hm = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR) &initializeCL, &hm))
    {
        checkErr(-1, "GetModuleHandle failed");
    }
    GetModuleFileNameA(hm, modPath, sizeof(modPath));
    strncpy(kernelPath, modPath, strlen(modPath)-14);
    strcat(kernelPath, "\\brute_opencl.cl");
    // the above trickery is needed if used as DLL with AKT tool as it changes working directory
    std::ifstream kernelFile(kernelPath, std::ios::in);
    if (!kernelFile.is_open())
    {
        checkErr(-1,"Opening brute_opencl.cl");
        return CL_FALSE;
    }

    std::ostringstream oss;
    oss << kernelFile.rdbuf();
    std::string  sourceStr = oss.str();
    const char* source = sourceStr.c_str();
    size_t sourceSize[] = { strlen(source) };

    program = clCreateProgramWithSource(context, 1, &source, sourceSize, &status);
    if(status != CL_SUCCESS) {
        checkErr(status,"Loading brute_opencl.cl into cl_program");
        return CL_FALSE;
    }

    // create a cl program executable for all the devices specified
    status = clBuildProgram(program, 1, devices, NULL, NULL, NULL);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Building Program");
        char buffer[2048];
        clGetProgramBuildInfo(program, *devices, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, NULL);
        checkErr(CL_SUCCESS,"--- Build log ---\n %s",buffer);
        return CL_FALSE;
    }

    // get kernel object handles for every kernel with the given name
    kernel = clCreateKernel(program, algKernels[alg], &status);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Creating Kernel '%s' from program",algKernels[alg]);
        return CL_FALSE;
    }

    return CL_TRUE;
}

//run the kernel after setting kernel args and parallelism properties. called per cycle
int runCL(int alg, unsigned int cycle_offset)
{
    cl_int status = 0;
    cl_uint maxDims;
    size_t maxWorkGroupSize;
    size_t maxWorkItemSizes[3];
    size_t kernelWorkGroupSize;
    size_t globalThreads[1];
    size_t localThreads[1];
    cl_event events[4];

    //getting max work groups size
    status = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), (void*)&maxWorkGroupSize, NULL);
    if(status != CL_SUCCESS)
    {
        checkErr(status, "Getting Device Info: maxWorkGroupSize");
        return CL_FALSE;
    }
    checkErr(CL_SUCCESS,"maxWorkGroupSize: %d", maxWorkGroupSize);

    //getting max dimensions
    status = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), (void*)&maxDims, NULL);
    if(status != CL_SUCCESS)
    {
        checkErr(status, "Getting Device Info: maxDims");
        return CL_FALSE;
    }
    checkErr(CL_SUCCESS,"maxDims: %d", maxDims);

    //getting max work item size
    status = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t)*maxDims, (void*)maxWorkItemSizes, NULL);
    if(status != CL_SUCCESS)
    {
        checkErr(status, "Getting Device Info: maxWorkItemSizes");
        return CL_FALSE;
    }
    checkErr(CL_SUCCESS,"maxWorkItemSizes: %d", maxWorkItemSizes[0]);

    //getting max kernel group size
    status = clGetKernelWorkGroupInfo(kernel, devices[0], CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &kernelWorkGroupSize, 0);
    if(status != CL_SUCCESS)
    {
        checkErr(status, "Getting Device Info: kernelWorkGroupSize");
        return CL_FALSE;
    }

    //setting kernel arguments
    // the output keys/hashes to the kernel
    status = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&outHashesBuf);
    if(status != CL_SUCCESS)
    {
        checkErr(status, " Setting kernel argument´outHashesBuf");
        return CL_FALSE;
    }

    status = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&outKeysBuf);
    if(status != CL_SUCCESS)
    {
        checkErr(status, " Setting kernel argument outKeysBuf");
        return CL_FALSE;
    }

    // the input hashes to the kernel
    status = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&inBuf);
    if(status != CL_SUCCESS)
    {
        checkErr(status, "Setting kernel argument inBuf");
        return CL_FALSE;
    }

    // key to start from
    status = clSetKernelArg(kernel, 3,sizeof(cl_uint), (void *)&cycle_offset);
    if(status != CL_SUCCESS)
    {
        checkErr(status, "Setting kernel argument cycle_offset");
        return CL_FALSE;
    }

    status = clSetKernelArg(kernel, 4,sizeof(cl_mem), (void *)&numKeysFoundBuf);
    if(status != CL_SUCCESS)
    {
        checkErr(status, "Setting kernel argument numKeysFoundBuf");
        return CL_FALSE;
    }

    if(alg==1) {
        //alg1 so we need salt
        status = clSetKernelArg(kernel, 5,sizeof(cl_uint), (void *)&salt);
        if(status != CL_SUCCESS)
        {
            checkErr(status, "Setting kernel argument salt");
            return CL_FALSE;
        }
    } else if(alg==7 || alg==8) {
        //alg7/8 so we need seed
        status = clSetKernelArg(kernel, 5,sizeof(cl_uint), (void *)&seed);
        if(status != CL_SUCCESS)
        {
            checkErr(status, "Setting kernel argument salt");
            return CL_FALSE;
        }
    }

    if(alg==8) {
        //alg8 so we need salt
        status = clSetKernelArg(kernel, 6,sizeof(cl_uint), (void *)&salt);
        if(status != CL_SUCCESS)
        {
            checkErr(status, "Setting kernel argument salt");
            return CL_FALSE;
        }
    }

    /* ### Info for tweaking ####
      - All the hashes from..to = global_work_size <= sizeof(size_t) and multiple of local_work_size), no need for inter-workitem communication.
      - Max parallel work groups = device compute units. Each group many parallel work-items (local_work_size)
      - Total number of work-items per work-group <= CL_DEVICE_MAX_WORK_GROUP_SIZE and CL_KERNEL_WORK_GROUP_SIZE
      - CL_DEVICE_MAX_WORK_GROUP_SIZE should be multiple of 8 for CPU, recommended 64-128 for GPU
      - CL_DEVICE_MAX_WORK_ITEM_SIZES: triplet of max items per dimension. And triplet multiplied <= CL_DEVICE_MAX_WORK_GROUP_SIZE and CL_KERNEL_WORK_GROUP_SIZE
      - And let every work item do 1 hash computation.
    */
    globalThreads[0] = (alg == 1 || alg==8) ? CYCLE_STEP_1 : CYCLE_STEP_02; //total number of hashes per cycle
    localThreads[0]  = 0xFF;//255 hashes per compute unit (256 would be max)

    //sanity checks, in case we go nuts on the values
    if(localThreads[0] > maxWorkGroupSize || localThreads[0] > maxWorkItemSizes[0]) {
        checkErr(-1,"Device does not support requested number of work items so this ");
        return CL_FALSE;
    }
    if((cl_uint)(localThreads[0]) > kernelWorkGroupSize) {
        checkErr(CL_SUCCESS,"local_work_size to big, adjusted to kernel max");
        localThreads[0] = kernelWorkGroupSize;
    }
    checkErr(CL_SUCCESS,"kernelWorkGroupSize: %d",kernelWorkGroupSize);

    //actually run the kernel
    status = clEnqueueNDRangeKernel(commandQueue, kernel,
                                    1, //1 dimension
                                    NULL, //must be NULL, global offset
                                    globalThreads, //total range to brute
                                    localThreads, //max hashes per compute unit
                                    0, //num of events to wait for
                                    NULL, //list of events to wait for
                                    &events[0] //event ptr for accessing this kernel run
                                   );
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Enqueueing kernel onto command queue");
        return CL_FALSE;
    }

    // wait for the kernel call to finish execution
    status = clWaitForEvents(1, &events[0]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Waiting for kernel run to finish");
        return CL_FALSE;
    }

    status = clReleaseEvent(events[0]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Release event object");
        return CL_FALSE;
    }

    //read back the results
    status = clEnqueueReadBuffer(commandQueue, outHashesBuf, CL_TRUE, 0, MAX_OUT_KEYS * sizeof(cl_uint), out_hashes, 0, NULL, &events[1]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"clEnqueueReadBuffer");
        return CL_FALSE;
    }

    status = clEnqueueReadBuffer(commandQueue, outKeysBuf, CL_TRUE, 0, MAX_OUT_KEYS * sizeof(cl_uint), out_keys, 0, NULL, &events[2]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"clEnqueueReadBuffer");
        return CL_FALSE;
    }

    status = clEnqueueReadBuffer(commandQueue, numKeysFoundBuf, CL_TRUE, 0, sizeof(cl_uint), &num_keys_found, 0, NULL, &events[3]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"clEnqueueReadBuffer");
        return CL_FALSE;
    }

    // Wait for the read buffers to finish execution
    status = clWaitForEvents(3, &events[1]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Waiting for read buffer call to finish");
        return CL_FALSE;
    }

    status = clReleaseEvent(events[1]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Release event object");
        return CL_FALSE;
    }

    status = clReleaseEvent(events[2]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Release event object");
        return CL_FALSE;
    }

    status = clReleaseEvent(events[3]);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Release event object");
        return CL_FALSE;
    }

    //seems we made it ;)
    return CL_TRUE;
}

//cleanup OpenCL resources. needs to be run once per session only
int cleanupCL(void)
{
    cl_int status;

    status = clReleaseKernel(kernel);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Releasing Kernel");
        return CL_FALSE;
    }

    status = clReleaseProgram(program);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Releasing Program");
        return CL_FALSE;
    }

    status = clReleaseMemObject(outHashesBuf);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Releasing outputBuffer");
        return CL_FALSE;
    }

    status = clReleaseMemObject(outKeysBuf);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Releasing outputBuffer");
        return CL_FALSE;
    }

    status = clReleaseMemObject(numKeysFoundBuf);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Releasing outputBuffer");
        return CL_FALSE;
    }

    status = clReleaseCommandQueue(commandQueue);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Releasing CommandQueue");
        return CL_FALSE;
    }

    status = clReleaseContext(context);
    if(status != CL_SUCCESS)
    {
        checkErr(status,"Releasing context");
        return CL_FALSE;
    }
    return CL_TRUE;
}

//cleanup host resources. needs to be run once per session only
void cleanupHost(void)
{
    if(in_hashes != NULL)
    {
        free(in_hashes);
        in_hashes = NULL;
    }
    if(out_hashes != NULL)
    {
        free(out_hashes);
        out_hashes = NULL;
    }
    if(out_keys != NULL)
    {
        free(out_keys);
        out_keys = NULL;
    }
    if(devices != NULL)
    {
        free(devices);
        devices = NULL;
    }
}

void arma_do_brute(int alg, hash_list* list, unsigned long from, unsigned long to, unsigned long* param, PRINT_FOUND print_found, PRINT_PROGRESS print_progress, PRINT_ERROR print_error, time_t* time_start, int* stop)
{
    stopBrute = stop;
    errorCallback = print_error;

    unsigned int num_done = 0;
    unsigned int cyc_step = (alg == 1 || alg==8) ? CYCLE_STEP_1 : CYCLE_STEP_02;
    unsigned int cyc_need = (to/cyc_step) - (from/cyc_step);

    if((to-from)<cyc_step) {
        checkErr(-1,"Need to brute at least %0.8X hashes. So we",cyc_step);
        return;
    }

    checkErr(CL_SUCCESS,"Cycles needed: %d",cyc_need);

    int status = 0;
    if(initializeHost(param) != CL_TRUE) {
        status--;
        checkErr(status,"Initializing Host");
        return;
    } else {
        for(int h=0; h<MAX_HASHES; h++) {
            in_hashes[h] = (cl_uint)list->hash[h];
        }
        checkErr(status,"Host init success");
    }

    if(initializeCL(alg) != CL_TRUE) {
        status--;
        checkErr(status,"Initializing OpenCL");
        return;
    } else {
        checkErr(status,"OpenCL init success");
    }

    //actually run the brute-force on GPU broken into cycles
    unsigned int numLastRound = 0;
    for(unsigned int j=from; j<=to-cyc_step; j+=cyc_step) {
        if(!*stopBrute) {
            if(runCL(alg,j) != CL_TRUE) {
                status--;
                checkErr(status,"Running OpenCL");
                return;
            } else {
                if((num_keys_found-numLastRound)>0) { //got new keys, wohoo
                    for(unsigned int i=numLastRound; i<num_keys_found; i++) {
                        print_found(out_hashes[i], out_keys[i]);
                    }
                    numLastRound = num_keys_found;
                }
            } //else
        }// if!stopBrute
        num_done++;

        if(!*stopBrute) {
            print_progress((double)num_done * cyc_step, (double)cyc_need * cyc_step, time_start);
        }
    }//for loop end

    if(cleanupCL() != CL_TRUE) {
        status--;
        checkErr(status,"Cleaning up OpenCL");
        return;
    } else {
        checkErr(status,"Cleaning up OpenCL success");
    }

    cleanupHost();
}
