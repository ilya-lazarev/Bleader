// Bleader.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <string>
#include <vector>
#include "inc/bl_defs.h"
#include "inc/DNA_sdna_types.h"
#include "inc/BLI_endian_defines.h"
#include "inc/DNA_ID_enums.h"

using namespace std;

struct FHeader
{
    char magic[7];
    uchar bits, endian;
    char version[3];
    uint16_t v_major, v_minor;
};

#define SIZEOFBLENDERHEADER 12

uint32_t flags = 0;

SDNA Sdna;
struct Field
{
    int nameIdx, typeIdx;
    Field(int name, int type) : nameIdx(name), typeIdx(type) {}
};

typedef vector<Field> FieldVector;

struct Structure
{
    int typeIdx;
    FieldVector fields;
    Structure(int t) : typeIdx(t) {}
    size_t fieldsNumber() const 
    { 
        return fields.size();  
    }
};

typedef vector<Structure> StructureVector;
typedef vector<BHead8> FileBlockVector;
typedef vector<string> StringVector;
typedef vector<size_t> SizesVector;

FileBlockVector FileBlocks;
StringVector Names, Types;
SizesVector  TypesLen;
StructureVector Structures;

void showBHead(size_t n, BHead8* h)
{
    if (!h)
        return;
    printf("%8d: %.4s, %d(%x) bytes, SDNA=%d, %d items (old %llx)\n", n, (char*)&(h->code), h->len, h->len, h->SDNAnr, h->nr, h->old);
}

int readTag(int fd, const char* tag, const size_t sz)
{
    char hdr[8];

    if (_read(fd, hdr, sz) != sz)
    {
        printf("Could not read header %*s block header\n", sz, tag);
        return -1;
    }

    if (strncmp(hdr, tag, sz))
    {
        printf("%*s header not found\n", sz, tag);
        return -2;
    }
    return static_cast<int>(sz);
}

string *readSz(int fd)
{
    string *s = new string;
    char c;
    int r;

    do
    {
        r = _read(fd, &c, 1);
        if (r != 1)
        {
            delete s;
            return nullptr;
        }
        if(c)
            s->append(1, c);
    } while (c != 0);
    return s;
}

int readStrings(int fd, const char *tag, StringVector &strings)
{
    int cnt = 0;
    uint32_t  stringsNumber;

    if (readTag(fd, tag, 4) != 4)
        return -1;

    if (_read(fd, &stringsNumber, 4) != 4)
        return -2;

    string *name = 0;
    while (stringsNumber--)
    {
        if (!(name = readSz(fd)))
        {
            printf("Error reading %u name\n", cnt);
            return -1;
        }
        printf("  %s: %u '%s'\n", tag, cnt, name->c_str());
        strings.emplace_back(*name);
        ++cnt;
    }
    _lseek(fd, 4 - _tell(fd) % 4, SEEK_CUR);
    return cnt;
}

int readTypesLen(int fd, size_t n)
{
    if (readTag(fd, "TLEN", 4) != 4)
        return -1;
    uint16_t  s;
    int r;
    while (n--)
    {
        r = _read(fd, &s, 2);
        if (r != 2)
            return -1;
        TypesLen.emplace_back(s);
    }
    _lseek(fd, 4 - _tell(fd) % 4, SEEK_CUR);
    return 0;
}

int readStructures(int fd)
{
    uint32_t sNumber;
    uint16_t s1, s2;
    Field *f = nullptr;
    Structure *s = 0;

    if (readTag(fd, "STRC", 4) != 4)
        return -1;

    if (_read(fd, &sNumber, 4) != 4)
        return -2;

    for (uint32_t i = 0; i < sNumber; ++i)
    {
        if (_read(fd, &s1, 2) != 2) // typeIdx
            return -2;
        if (_read(fd, &s2, 2) != 2) // num of fields
            return -2;

        s = new Structure(s1);
        printf("STRC %s\n", Types[s1].c_str());

        for (uint16_t fi = 0; fi < s2; ++fi)
        {
            if (_read(fd, &s1, 2) != 2) // type idx
                return -2;
            if (_read(fd, &s2, 2) != 2) // name idx
                return -2;

            f = new Field(s2, s1);
            s->fields.emplace_back(*f);
            printf("  %s %s\n", Types[s1].c_str(), Names[s2].c_str());
        }

        Structures.emplace_back(*s);
    }
    return (int)sNumber;
}


int readSDNA(int fd, BHead8 *hd)
{
    int cnt = 0;
    uint8_t* data = 0;

    if (readTag(fd, "SDNA", 4) != 4)
        return -1;
    readStrings(fd, "NAME", Names);
    readStrings(fd, "TYPE", Types);
    readTypesLen(fd, Types.size());
    readStructures(fd);
    return cnt;
}

size_t readBlocks(int fd)
{
    BHead8 hd;
    const size_t HDSZ = sizeof(BHead8);
    size_t cnt = 0;

    while (1)
    {
        if (_read(fd, &hd, HDSZ) != HDSZ)
        {
            break;
        }
        showBHead(cnt, &hd);
        ++cnt;
        if (hd.code == DNA1)
            readSDNA(fd, &hd);
        else
            _lseek(fd, hd.len, SEEK_CUR);

        FileBlocks.push_back(hd);
    }
    return cnt;
}

int main(int ac, char** av)
{
    FHeader hdr;

    if (ac < 2)
    {
        printf("Usage: %s <Blender file>\n", av[0]);
        return -1;
    }

    int fd = _open(av[1], O_RDONLY | O_BINARY);

    if (fd == -1)
    {
        printf("Cannot open %s\n", av[1]);
        return -2;
    }

    if (_read(fd, &hdr, SIZEOFBLENDERHEADER) != SIZEOFBLENDERHEADER)
    {
        printf("Cannot read %s\n", av[1]);
        _close(fd);
        return -3;
    }

    if (strncmp(hdr.magic, "BLENDER", 7) || (hdr.bits != '_' && hdr.bits != '-') || (hdr.endian != 'v' && hdr.endian != 'V') ||
        !isdigit(hdr.version[0]) || !isdigit(hdr.version[1]) || !isdigit(hdr.version[2]))
    {
        printf("%s is not Blender file\n", av[1]);
        _close(fd);
        return -3;
    }

    hdr.v_major = hdr.version[0] - '0';
    hdr.v_minor = (hdr.version[1] - '0') * 10 + hdr.version[2] - '0';

    flags |= hdr.endian == 'v' ? L_ENDIAN : B_ENDIAN;
    flags |= hdr.bits == '_' ? FD_FLAGS_FILE_POINTSIZE_IS_4 : 0;
    Sdna.pointer_size = hdr.bits == '_' ? 4 : 8;

    printf("File is %s endian, %s bits, version %d.%d\n", flags & L_ENDIAN ? "Little" : "Big", flags & FD_FLAGS_FILE_POINTSIZE_IS_4 ? "32" : "64",
        hdr.v_major, hdr.v_minor);


    size_t bcnt = readBlocks(fd);

    printf("%u blocks\n", bcnt);

    printf("-- NAMES: %u, TYPES %u\n", Names.size(), Types.size());

    _close(fd);
}
