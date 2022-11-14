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
typedef vector<uint> SizesVector;

FileBlockVector FileBlocks;
StringVector Names, Types;
SizesVector  TypesLen, NamesLen;
StructureVector Structures;

SDNA Sdna;

// Must be called after filling Names, Types
void showBHead(BHead8* h)
{
    if (!h)
        return;

    const char* SDNAType = h->SDNAnr >= Structures.size() ? " ? " : Types[Structures[h->SDNAnr].typeIdx].c_str();

    printf("%.4s, %d(0x%x)b, %s", (char*)&(h->code), h->len, h->len, SDNAType);
    if (h->nr > 1)
        printf("[%d]", h->nr);

    printf(" (0x%llx)\n", h->old);

}

// Calc size of N-dim array in elements
int DNA_elem_array_size(const char* str)
{
    int result = 1;
    int current = 0;
    while (true) {
        char c = *str++;
        switch (c) {
        case '\0':
            return result;
        case '[':
            current = 0;
            break;
        case ']':
            result *= current;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            current = current * 10 + (c - '0');
            break;
        default:
            break;
        }
    }
}

void calcNamesLen()
{
    for(StringVector::iterator it = Names.begin(); it != Names.end(); ++it)
    {
        NamesLen.push_back(DNA_elem_array_size(it->c_str()));
        printf("'%s' size = %d items\n", it->c_str(), NamesLen.back());
    }
}

int readTag(int fd, const char* tag, const uint sz)
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
        strings.emplace_back(*name);
        ++cnt;
    }
    _lseek(fd, 4 - _tell(fd) % 4, SEEK_CUR);
    return cnt;
}

int readTypesLen(int fd, uint n)
{
    if (readTag(fd, "TLEN", 4) != 4)
        return -1;
    uint16_t  s;

    for (uint i = 0; i < n; ++i)
    {
        if (_read(fd, &s, 2) != 2)
            return -1;
        TypesLen.emplace_back(s);
        printf("  %s: %u\n", Types[i].c_str(), (int)s);
    }
    if (Types.size() & 1)
        _lseek(fd, 2, SEEK_CUR);

    return 0;
}

int readStructures(int fd)
{
    uint32_t sNumber;
    uint16_t s1, s2, fn;
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

        for (int fi = 0; fi < s2; ++fi)
        {
            if (_read(fd, &s1, 2) != 2) // type idx
                return -2;
            if (_read(fd, &fn, 2) != 2) // name idx
                return -2;

            f = new Field(fn, s1);
            s->fields.emplace_back(*f);
            if (s1 >= Types.size())
            {
                printf("EE Index %d in types >= types len %u (pos %d.%d)\n", (int)s1, Types.size(), i, fi);
                return -1;
            }
            if (fn >= Names.size())
            {
                printf("EE Index %d in names >= types len %u (pos %d.%d)\n", (int)fn, Names.size(), i, fi);
                return -2;
            }
            printf("  %s %s\n", Types[s1].c_str(), Names[fn].c_str());
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

uint readBlocks(int fd)
{
    BHead8 hd;
    const uint HDSZ = sizeof(BHead8);
    uint cnt = 0;

    while (1)
    {
        if (_read(fd, &hd, HDSZ) != HDSZ)
        {
            break;
        }

        if (hd.code == ENDB)
            break;
        if (hd.code == DNA1)
        {
            readSDNA(fd, &hd);
            calcNamesLen();
         }
        else
            _lseek(fd, hd.len, SEEK_CUR);

        FileBlocks.push_back(hd);
        ++cnt;
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


    uint bcnt = readBlocks(fd);
    for(FileBlockVector::iterator it = FileBlocks.begin(); it != FileBlocks.end(); ++it)
        showBHead(&*it);;

    printf("%u blocks\n", bcnt);

    printf("-- NAMES: %u, TYPES %u\n", Names.size(), Types.size());

    _close(fd);
}
