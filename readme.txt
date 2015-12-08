* Coded by 
*
* cypher <the.cypher@gmail.com>

armabrut_opencl requires ATI/Nvidia GPU or any CPU that at least supports OpenCL 1.1. 
For Radeon, all 5xxx, 6xxx, 7xxx are supported. No 4xxx !
You also need to have OpenCL Runtime for your card installed.

Also you are able pause brute by spacebar pressed in the console

This can also be used as a dll plugin for ArmadilloKeytool by mr.exodia

Note on -a param:
0: arma v3.7-v7.2
1: arma v7.4+
2: some crc32+random number (maybe arma <v3.x)
3,4: 1 and 2 with layer 2 verification of the sym, ignore this
(5: arma random dword seed) not yet supported
(6: arma random dword) not yet supported
7: stolen keys v3.7-v7.2
8: stolen keys 7.4+

Sample Usage:
armabrut_opencl.exe -a 0 -h 0765D867    -> result should be A7762CB7 in few seconds
armabrut_opencl.exe -a 0 -h 0765D867 -f A7762BB6 -t A8762CB4
armabrut_opencl.exe -a 1 -h 132391AE -p 6CA2B2C9 -o keys.txt
armabrut_opencl.exe -a 1 -h 4B3602BA,0A9EAB25 -p CD336B24 -o keys.txt
armabrut_opencl.exe -a 7 -h 9443B5D9 -d 434220C8 -> result should be 132669B8 in few seconds

Benchmark:
armabrut_opencl.exe -a 1 -h 132391AE -p 6CA2B2C9

ATI
------
HD4870:
estimated 290.91 min, speed 245820 hash/sec
HD5870:
estimated 86.27 min, speed 833863 hash/sec
HD6870:
estimated 128.22 min, speed 557192 hash/sec
HD7970:
estimated 13.5 min, speed 5276297 hash/sec
R9 285
estimated 20min, speed 3750000 hash/sec

NVIDIA
-------
GTX 550ti:
speed 494409 hash/sec  vs  458752 hash/sec with CUDA on same device
GTX 560ti:  
680k/sec  vs 800k/sec CUDA
GTX 650:
estimated 154.28 min, speed 459510 hash/sec
GTX580:
estimated 46.18 min, speed 1539609 hash/sec (CUDA estimated 69.75 min, speed 1022830 hash/sec)
GTX580(Overclock):
estimated 36.12 min, speed 1973388 hash/sec (CUDA estimated 57.14 min, speed 1247369 hash/sec)
GTX680:
estimated 29min, speed 2350654 hash/sec
GTX770:
estimated 32.35 min, speed 2167825 hash/sec (CUDA estimated 36.18 min, speed 1941114 hash/sec)
GTX 780m
estimated 50min, speed 1359757 hash/sec
GTX 970
estimated 11min, speed 6300000 hash/sec

Troubleshooting:
If it doesnt run for you, run as Debug which outputs some more info.
If your display driver crashes, you need to play with #define CYCLE_STEP_1 in brute_opencl.cpp. 
This value NEEDS to be multiple of 0xFF
// HD5870/6870 runs fine with FF0FF. Seems to be max before display driver dies for ATI
// GTX 550ti/560ti/650 runs fine with FE01, seems to be working for all? Nvidia