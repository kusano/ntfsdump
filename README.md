ntfsdump
====

Extract files from NTFS Volume.
Administrator privilege may be required.

```
>ntfsdump C
Cluster Size: 4096
Record Size: 1024
MFT size: 1409286144
Record number: 1376256
MFT run list:
             c0000             cb40
          15d0eb7e             c802
          1f348c69             c837
           c711b1f             c801
          1f03d5b5             c80b
          315a9f0d             c813
          25071a50             8c68
File List:
           0       $MFT
           1       $MFTMirr
           2       $LogFile
           3       $Volume
           4       $AttrDef
           5   dir .
           6       $Bitmap
           7       $Boot
           8       $BadClus
           9 ????? $Secure
          10       $UpCase
          11   dir $Extend
          12       Failed to find $File_Name attribute
          13       Failed to find $File_Name attribute
          14       Failed to find $File_Name attribute
          15       Failed to find $File_Name attribute
          16 -
 :
 :
     1375296       ntfsdump.cpp
 :
 :
```
```
>ntfsdump C 1375296 hoge.txt
OEM ID: "NTFS    "
Byte/Sector: 512
Sector/Cluster: 8
Total Sector: 7813771263
Cluster of MFT: 786432
Cluster/Record: 4294967286
Cluster Size: 4096
Record Size: 1024
MFT size: 1409286144
Record number: 1376256
MFT run list:
             c0000             cb40
          15d0eb7e             c802
          1f348c69             c837
           c711b1f             c801
          1f03d5b5             c80b
          315a9f0d             c813
          25071a50             8c68
Record index: 1375296
Output file name: hoge.txt
$Data in the record
Non-resident
File size: 21516
Run list:
           217c7aa                1
           abadf22                3
           abadf32                2
Success
```
```
>cat hoge.txt
#define WIN32_LEAN_AND_MEAN
#include <cstdio>
#include <Windows.h>
#include <tchar.h>
#include <locale.h>
#include <shellapi.h>
#include <vector>
#include <functional>
 :
 :
```
