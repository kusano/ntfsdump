#define WIN32_LEAN_AND_MEAN
#include <cstdio>
#include <Windows.h>
#include <tchar.h>
#include <locale.h>
#include <shellapi.h>
#include <vector>
#include <functional>

#pragma comment(lib, "shell32.lib")

using namespace std;

#pragma pack(push, 1)

//  http://www.writeblocked.org/resources/ntfs_cheat_sheets.pdf

struct BootSector
{
    BYTE        jump[3];
    BYTE        oemID[8];
    WORD        bytePerSector;
    BYTE        sectorPerCluster;
    BYTE        reserved[2];
    BYTE        zero1[3];
    BYTE        unused1[2];
    BYTE        mediaDescriptor;
    BYTE        zeros2[2];
    WORD        sectorPerTrack;
    WORD        headNumber;
    DWORD       hiddenSector;
    BYTE        unused2[8];
    LONGLONG    totalSector;
    LONGLONG    MFTCluster;
    LONGLONG    MFTMirrCluster;
    signed char clusterPerRecord;
    BYTE        unused3[3];
    signed char clusterPerBlock;
    BYTE        unused4[3];
    LONGLONG    serialNumber;
    DWORD       checkSum;
    BYTE        bootCode[0x1aa];
    BYTE        endMarker[2];
};

struct RecordHeader
{
    BYTE        signature[4];
    WORD        updateOffset;
    WORD        updateNumber;
    LONGLONG    logFile;
    WORD        sequenceNumber;
    WORD        hardLinkCount;
    WORD        attributeOffset;
    WORD        flag;
    DWORD       usedSize;
    DWORD       allocatedSize;
    LONGLONG    baseRecord;
    WORD        nextAttributeID;
    BYTE        unsed[2];
    DWORD       MFTRecord;
};

struct AttributeHeaderR
{
    DWORD       typeID;
    DWORD       length;
    BYTE        formCode;
    BYTE        nameLength;
    WORD        nameOffset;
    WORD        flag;
    WORD        attributeID;
    DWORD       contentLength;
    WORD        contentOffset;
    WORD        unused;
};

struct AttributeHeaderNR
{
    DWORD       typeID;
    DWORD       length;
    BYTE        formCode;
    BYTE        nameLength;
    WORD        nameOffset;
    WORD        flag;
    WORD        attributeID;
    LONGLONG    startVCN;
    LONGLONG    endVCN;
    WORD        runListOffset;
    WORD        compressSize;
    DWORD       zero;
    LONGLONG    contentDiskSize;
    LONGLONG    contentSize;
    LONGLONG    initialContentSize;
};

struct FileName
{
    LONGLONG    parentDirectory;
    LONGLONG    dateCreated;
    LONGLONG    dateModified;
    LONGLONG    dateMFTModified;
    LONGLONG    dateAccessed;
    LONGLONG    logicalSize;
    LONGLONG    diskSize;
    DWORD       flag;
    DWORD       reparseValue;
    BYTE        nameLength;
    BYTE        nameType;
    BYTE        name[1];
};

struct AttributeRecord
{
    DWORD       typeID;
    WORD        recordLength;
    BYTE        nameLength;
    BYTE        nameOffset;
    LONGLONG    lowestVCN;
    LONGLONG    recordNumber;
    WORD        sequenceNumber;
    WORD        reserved;
};

#pragma pack(pop)

struct Run
{
    LONGLONG    offset;
    LONGLONG    length;
    Run(LONGLONG offset, LONGLONG length): offset(offset), length(length) {}
};

void seek(HANDLE h, ULONGLONG position)
{
    LARGE_INTEGER pos;
    pos.QuadPart = (LONGLONG)position;

    LARGE_INTEGER result;
    if (!SetFilePointerEx(h, pos, &result, SEEK_SET) ||
        pos.QuadPart != result.QuadPart)
        throw "Failed to seek";
}

LPBYTE findAttribute(RecordHeader *record, DWORD recordSize, DWORD typeID,
    function<bool(LPBYTE)> condition = [&](LPBYTE){return true;})
{
    LPBYTE p = LPBYTE(record) + record->attributeOffset;
    while (true)
    {
        if (p + sizeof (AttributeHeaderR) > LPBYTE(record) + recordSize)
            break;

        AttributeHeaderR *header = (AttributeHeaderR *)p;
        if (header->typeID == 0xffffffff)
            break;

        if (header->typeID == typeID &&
            p + header->length <= LPBYTE(record) + recordSize &&
            condition(p))
            return p;

        p += header->length;
    }
    return NULL;
}

vector<Run> parseRunList(BYTE *runList, DWORD runListSize, LONGLONG totalCluster)
{
    vector<Run> result;

    LONGLONG offset = 0LL;

    LPBYTE p = runList;
    while (*p != 0x00)
    {
        if (p + 1 > runList + runListSize)
            throw _T("Invalid data run");

        int lenLength = *p&0xf;
        int lenOffset = *p>>4;
        p++;

        if (p + lenLength + lenOffset > runList + runListSize ||
            lenLength >= 8  ||
            lenOffset >= 8)
            throw _T("Invalid data run");

        if (lenOffset == 0)
            throw _T("Sparse file is not supported");

        ULONGLONG length = 0;
        for (int i=0; i<lenLength; i++)
            length |= *p++ << (i*8);

        LONGLONG offsetDiff = 0;
        for (int i=0; i<lenOffset; i++)
            offsetDiff |= *p++ << (i*8);
        if (offsetDiff >= (1LL<<((lenOffset*8)-1)))
            offsetDiff -= 1LL<<(lenOffset*8);

        offset += offsetDiff;

        if (offset<0 || totalCluster<=offset)
            throw _T("Invalid data run");

        result.push_back(Run(offset, length));
    }

    return result;
}

void readRecord(HANDLE h, LONGLONG recordIndex, vector<Run> &MFTRunList,
    DWORD recordSize, DWORD clusterSize, DWORD sectorSize, BYTE *buffer)
{
    LONGLONG sectorOffset = recordIndex * recordSize / sectorSize;
    DWORD sectorNumber = recordSize / sectorSize;

    for (DWORD sector=0; sector<sectorNumber; sector++)
    {
        LONGLONG cluster = (sectorOffset + sector) / (clusterSize / sectorSize);
        LONGLONG vcn = 0LL;
        LONGLONG offset = -1LL;

        for (const Run &run: MFTRunList)
        {
            if (cluster < vcn + run.length)
            {
                offset = (run.offset + cluster - vcn) * clusterSize
                    + (sectorOffset + sector) * sectorSize % clusterSize;
                break;
            }
            vcn += run.length;
        }
        if (offset == -1LL)
            throw _T("Failed to read file record");

        seek(h, offset);
        DWORD read;
        if (!ReadFile(h, buffer+sector*sectorSize, sectorSize, &read, NULL) ||
            read != sectorSize)
            throw _T("Failed to read file record");
    }

    //  Fixup
    RecordHeader *header = (RecordHeader *)buffer;
    LPWORD update = LPWORD(buffer + header->updateOffset);

    if (LPBYTE(update + header->updateNumber) > buffer + recordSize)
        throw _T("Update sequence number is invalid");

    for (int i=1; i<header->updateNumber; i++)
        *LPWORD(buffer + i*sectorSize - 2) = update[i];
}

int main()
{
    LPWSTR *argv = NULL;
    HANDLE h = INVALID_HANDLE_VALUE;
    HANDLE output = INVALID_HANDLE_VALUE;
    int result = -1;

    try
    {
        _tsetlocale(LC_ALL, _T(""));

        //  Argument
        int argc;
        argv = CommandLineToArgvW(GetCommandLine(), &argc);

        TCHAR drive[] = _T("\\\\?\\_:");
        LONGLONG targetRecord = -1;
        LPWSTR outputFile = NULL;

        switch (argc)
        {
        case 2:
            drive[4] = argv[1][0];
            break;
        case 4:
            drive[4] = argv[1][0];
            targetRecord = _tstoi64(argv[2]);
            outputFile = argv[3];
            break;
        default:
            _tprintf(_T("Usage:\n"));
            _tprintf(_T("  ntfsdump DriveLetter\n"));
            _tprintf(_T("  ntfsdump DriveLetter RecordIndex OutputFileName\n"));
            throw 0;
        }

        //  Open
        h = CreateFile(
            drive,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if (h == INVALID_HANDLE_VALUE)
            throw _T("Failed to open drive");

        //  Boot Sector
        BootSector bootSector;
        DWORD read;
        if (!ReadFile(h, &bootSector, sizeof bootSector, &read, NULL) ||
            read != sizeof bootSector)
            throw _T("Failed to read boot sector");

        printf("OEM ID: \"%s\"\n", bootSector.oemID);
        if (memcmp(bootSector.oemID, "NTFS    ", 8) != 0)
            throw _T("Volume is not NTFS");

        DWORD clusterSize = bootSector.bytePerSector * bootSector.sectorPerCluster;
        DWORD recordSize = bootSector.clusterPerRecord >= 0 ?
            bootSector.clusterPerRecord * clusterSize :
            1 << -bootSector.clusterPerRecord;
        LONGLONG totalCluster = bootSector.totalSector / bootSector.sectorPerCluster;

        _tprintf(_T("Byte/Sector: %u\n"), bootSector.bytePerSector);
        _tprintf(_T("Sector/Cluster: %u\n"), bootSector.sectorPerCluster);
        _tprintf(_T("Total Sector: %llu\n"), bootSector.totalSector);
        _tprintf(_T("Cluster of MFT: %llu\n"), bootSector.MFTCluster);
        _tprintf(_T("Cluster/Record: %u\n"), bootSector.clusterPerRecord);
        _tprintf(_T("Cluster Size: %u\n"), clusterSize);
        _tprintf(_T("Record Size: %u\n"), recordSize);

        //  Read MFT size and run list
        seek(h, bootSector.MFTCluster*clusterSize);
        vector<BYTE> MFTRecord(recordSize);
        if (!ReadFile(h, &MFTRecord[0], recordSize, &read, NULL) ||
            read != recordSize)
            throw _T("Failed to read MFT record");
        RecordHeader *MFTRecordHeader = (RecordHeader *)&MFTRecord[0];

        AttributeHeaderNR *MFTData = (AttributeHeaderNR *)findAttribute(
            MFTRecordHeader, recordSize, 0x80);
        if (MFTData == NULL)
            //  $Attribute_List in $MFT is not supported
            throw _T("Failed to find $Data attribute of $MFT");

        ULONGLONG recordNumber = MFTData->contentSize / recordSize;
        _tprintf(_T("MFT size: %llu\n"), MFTData->contentSize);
        _tprintf(_T("Record number: %llu\n"), recordNumber);

        vector<Run> MFTRunList = parseRunList(
            LPBYTE(MFTData) + MFTData->runListOffset,
            MFTData->length - MFTData->runListOffset,
            totalCluster);

        _tprintf(_T("MFT run list:\n"));
        for (Run &run: MFTRunList)
            _tprintf(_T("  %16llx %16llx\n"), run.offset, run.length);

        if (argc == 2)
        {
            //  Read file list
            _tprintf(_T("File List:\n"));

            vector<BYTE> record(recordSize);
            RecordHeader *recordHeader = (RecordHeader *)&record[0];

            for (ULONGLONG recordIndex=0; recordIndex<recordNumber; recordIndex++)
            {
                _tprintf(_T("%12lld"), recordIndex);

                try
                {
                    readRecord(h, recordIndex, MFTRunList, recordSize,
                        clusterSize, bootSector.bytePerSector, &record[0]);

                    if (memcmp(recordHeader->signature, "FILE", 4) !=  0)
                    {
                        _tprintf(_T(" -\n"));
                        continue;
                    }

                    if (recordHeader->baseRecord != 0LL)
                    {
                        _tprintf(_T(" extension for %llu\n"),
                            recordHeader->baseRecord & 0xffffffffffff);
                        continue;
                    }

                    switch (recordHeader->flag)
                    {
                    case 0x0000: _tprintf(_T(" x    "));  break;
                    case 0x0001: _tprintf(_T("      "));  break;
                    case 0x0002: _tprintf(_T(" x dir"));  break;
                    case 0x0003: _tprintf(_T("   dir"));  break;
                    default:     _tprintf(_T(" ?????"));
                    }

                    AttributeHeaderR *name = (AttributeHeaderR *)findAttribute(
                        recordHeader, recordSize, 0x30,
                        [&](LPBYTE a) -> bool
                        {
                            AttributeHeaderR *name = (AttributeHeaderR *)a;
                            if (name->formCode != 0)
                                throw _T("Non-esident $File_Name is not supported");
                            FileName *content =(FileName *)(a + name->contentOffset);
                            if (LPBYTE(content) + sizeof (FileName)
                                > &record[0] + recordSize)
                                throw _T("$File_Name size is invalid");

                            //  0x02 = DOS Name
                            return content->nameType != 0x02;
                        }
                    );
                    //  $File_Name outside the record is not supported
                    if (name == NULL)
                        throw _T("Failed to find $File_Name attribute");

                    FileName *content =(FileName *)(LPBYTE(name)
                        + name->contentOffset);
                    if (content->name + content->nameLength
                        > &record[0] + recordSize)
                        throw _T("$File_Name size is invalid");

                    _tprintf(_T(" %.*s\n"), content->nameLength, content->name);
                }
                catch (LPCWSTR error)
                {
                    _tprintf(_T(" %s\n"), error);
                }
            }
        }
        if (argc == 4)
        {
            //  Read file
            _tprintf(_T("Record index: %llu\n"), targetRecord);
            _tprintf(_T("Output file name: %s\n"), outputFile);

            vector<BYTE> record(recordSize);
            RecordHeader *recordHeader = (RecordHeader *)&record[0];

            readRecord(h, targetRecord, MFTRunList, recordSize, clusterSize,
                bootSector.bytePerSector, &record[0]);

            AttributeHeaderR *attributeList = (AttributeHeaderR *)findAttribute(
                recordHeader, recordSize, 0x20);
            AttributeHeaderR *dataTemp = (AttributeHeaderR *)findAttribute(
                recordHeader, recordSize, 0x80);

            if (dataTemp != NULL)
            {
                _tprintf(_T("$Data in the record\n"));

                output = CreateFile(outputFile, GENERIC_WRITE, 0,
                    NULL, CREATE_ALWAYS, 0, NULL);
                if (output == INVALID_HANDLE_VALUE)
                    throw _T("Failed to open output file");

                if (dataTemp->formCode == 0)
                {
                    _tprintf(_T("Resident\n"));

                    AttributeHeaderR *data = dataTemp;

                    _tprintf(_T("File size: %u\n"), data->contentLength);

                    if (data->contentOffset + data->contentLength > data->length)
                        throw _T("File size is too large");

                    DWORD written;
                    if (!WriteFile(output, LPBYTE(data)+data->contentOffset,
                        data->contentLength, &written, NULL) ||
                        written != data->contentLength)
                        throw _T("Failed to write output file");
                }
                else
                {
                    _tprintf(_T("Non-resident\n"));

                    AttributeHeaderNR *data = (AttributeHeaderNR *)dataTemp;

                    _tprintf(_T("File size: %llu\n"), data->contentSize);

                    vector<Run> runList = parseRunList(
                        LPBYTE(data) + data->runListOffset,
                        data->length - data->runListOffset, totalCluster);

                    LONGLONG writeSize = 0;
                    _tprintf(_T("Run list:\n"));

                    vector<BYTE> cluster(clusterSize);

                    for (Run &run: runList)
                    {
                        _tprintf(_T("  %16llx %16llx\n"), run.offset, run.length);

                        seek(h, run.offset*clusterSize);
                        for (LONGLONG i=0; i<run.length; i++)
                        {
                            if (writeSize + run.length > data->contentSize)
                                throw _T("File size error");

                            if (!ReadFile(h, &cluster[0], clusterSize, &read, NULL) ||
                                read != clusterSize)
                                throw _T("Failed to read cluster");

                            DWORD s = DWORD(min(data->contentSize - writeSize, clusterSize));
                            DWORD written;
                            if (!WriteFile(output, &cluster[0], s, &written, NULL) ||
                                written != s)
                                throw _T("Failed to write output file");
                            writeSize += s;
                        }
                    }

                    if (writeSize != data->contentSize)
                        throw _T("File size error");
                }
            }
            else
            {
                _tprintf(_T("$Data attribute in another record\n"));

                if (attributeList == NULL)
                    throw _T("$Data nor $Attribute_List is not found");

                if (attributeList->formCode != 0)
                    throw _T("Non-resident $Attribute_List is not supported");

                output = CreateFile(outputFile, GENERIC_WRITE, FILE_SHARE_READ,
                    NULL, CREATE_ALWAYS, 0, NULL);
                if (output == INVALID_HANDLE_VALUE)
                    throw _T("Failed to open output file");

                LONGLONG fileSize = -1LL;
                LONGLONG writeSize = 0LL;

                vector<BYTE> cluster(clusterSize);

                LPBYTE content = LPBYTE(attributeList) + attributeList->contentOffset;
                DWORD p = 0;
                while (p + sizeof (AttributeRecord) <= attributeList->contentLength)
                {
                    AttributeRecord *attr = (AttributeRecord *)(content + p);

                    if (attr->typeID == 0x80)
                    {
                        _tprintf(_T("MFT extension: %lld\n"), attr->recordNumber & 0xffffffffffff);

                        vector<BYTE> extRecord(recordSize);
                        RecordHeader *extRecordHeader = (RecordHeader *)&extRecord[0];

                        readRecord(h, attr->recordNumber & 0xffffffffffff,
                            MFTRunList, recordSize, clusterSize,
                            bootSector.bytePerSector, &extRecord[0]);

                        if (memcmp(extRecordHeader->signature, "FILE", 4) != 0)
                            throw _T("Extenion record is invalid");

                        AttributeHeaderNR *data = (AttributeHeaderNR *)findAttribute(
                            extRecordHeader, recordSize, 0x80);
                        if (data == NULL)
                            throw _T("Could not find $Data in the extension record");

                        if (fileSize == -1LL)
                        {
                            fileSize = data->contentSize;
                            _tprintf(_T("File size: %llu\n"), fileSize);
                        }

                        vector<Run> runList = parseRunList(
                            LPBYTE(data) + data->runListOffset,
                            data->length - data->runListOffset, totalCluster);

                        _tprintf(_T("Run list:\n"));
                        for (Run &run: runList)
                        {
                            _tprintf(_T("  %16llx %16llx\n"), run.offset, run.length);

                            seek(h, run.offset*clusterSize);
                            for (LONGLONG i=0; i<run.length; i++)
                            {
                                if (writeSize + run.length > fileSize)
                                    throw _T("File size error");

                                if (!ReadFile(h, &cluster[0], clusterSize, &read, NULL) ||
                                    read != clusterSize)
                                    throw _T("Failed to read cluster");

                                DWORD s = (DWORD)min(fileSize - writeSize, clusterSize);
                                DWORD written;
                                if (!WriteFile(output, &cluster[0], s, &written, NULL) ||
                                    written != s)
                                    throw _T("Failed to write output file");
                                writeSize += s;
                            }
                        }
                    }

                    p += attr->recordLength;
                }

                if (writeSize != fileSize)
                {
                    _tprintf(_T("Expected size: %llu\n"), fileSize);
                    _tprintf(_T("Actual size: %llu\n"), writeSize);
                    throw _T("File size is strange");
                }
            }

            _tprintf(_T("Success\n"));
        }

        result = 0;
    }
    catch (LPWSTR error)
    {
        _tprintf(_T("Error: %s\n"), error);
    }
    catch (...)
    {
    }

    if (argv != NULL)
        GlobalFree(argv);
    if (h != NULL)
        CloseHandle(h);
    if (output != NULL)
        CloseHandle(output);

    return result;
}
