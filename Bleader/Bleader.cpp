// Bleader.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
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

typedef vector<BHead8> FileBlockVector;

FileBlockVector FileBlocks;

void showBHead(size_t n, BHead8* h)
{
    if (!h)
        return;
    printf("%8d: %.4s, %d(%x) bytes, SDNA=%d, %d items (old %llx)\n", n, (char*)&(h->code), h->len, h->len, h->SDNAnr, h->nr, h->old);
}



int readSDNA(int fd, BHead8 *hd)
{
    int cnt = 0;
    uint8_t* data = 0;

    const size_t SDNAHDRSIZE = 4;
    char sdnahdr[SDNAHDRSIZE];

    if (_read(fd, sdnahdr, SDNAHDRSIZE) != SDNAHDRSIZE)
    {
        printf("Could not read DNA1 block header\n");
        return -1;
    }

    if (strncmp(sdnahdr, "SDNA", SDNAHDRSIZE))
    {
        printf("SDNA header not found\n");
        return -1;
    }
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
    _close(fd);
}
