# ntfsdump

Extract files from NTFS Volume.
Administrator privilege may be required.

```
ntfsdump>ntfsdump C
OEM ID: "NTFS    "
Byte/Sector: 512
Sector/Cluster: 8
Total Sector: 998955007
Cluster of MFT: 786432
Cluster/Record: 4294967286
Cluster Size: 4096
Record Size: 1024
MFT stage: 2
MFT size: 1245446144
Record number: 1216256
MFT run list: 8
             c0000             c820
           23d660e             c812
           13c9750             c810
           29130b0              6fe
           657e638             38a8
           4a306e8            11518
           3b18165             cd4d
           3a0fa02             2973
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
     1215192       ntfsdump.cpp
 :
 :
```
```
ntfsdump>ntfsdump C 1215192 hoge.txt
OEM ID: "NTFS    "
Byte/Sector: 512
Sector/Cluster: 8
Total Sector: 998955007
Cluster of MFT: 786432
Cluster/Record: 4294967286
Cluster Size: 4096
Record Size: 1024
MFT stage: 2
MFT size: 1245446144
Record number: 1216256
MFT run list: 8
             c0000             c820
           23d660e             c812
           13c9750             c810
           29130b0              6fe
           657e638             38a8
           4a306e8            11518
           3b18165             cd4d
           3a0fa02             2973
Record index: 1215192
Output file name: hoge.txt
Stage: 2 ($Data is non-resident)
Run list: 1
             56652                6
Success
```
```
ntfsdump>type hoge.txt
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

## License

[MIT License](https://github.com/kusano/ntfsdump/blob/master/LICENSE).

Note that this software is based on the following documents:

- http://www.writeblocked.org/resources/ntfs_cheat_sheets.pdf
- https://flatcap.org/linux-ntfs/ntfs/
- http://www.kes.talktalk.net/ntfs/
- http://blogs.technet.com/b/askcore/archive/2009/10/16/the-four-stages-of-ntfs-file-growth.aspx

As mentioned in the license file, I make no warranty that this software does not infringe owners of these documents and Microsoft Corporation's rights.
