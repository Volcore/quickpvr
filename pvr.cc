/*******************************************************************************
  Copyright (c) 2009, Limbic Software, Inc.
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the Limbic Software, Inc. nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY LIMBIC SOFTWARE, INC. ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL LIMBIC SOFTWARE, INC. BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
#include "pvr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>

unsigned int countBits(unsigned int x)
{
    x  = x - ((x >> 1) & 0x55555555);
    x  = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x  = x + (x >> 4);
    x &= 0xF0F0F0F;
    return (x * 0x01010101) >> 24;
}

typedef struct
{
	uint32_t PackedData[2];
}AMTC_BLOCK_STRUCT;

const unsigned int PVRTEX_CUBEMAP               = (1<<12);

extern void Decompress(AMTC_BLOCK_STRUCT *pCompressedData,
					   const int Do2bitMode,
					   const int XDim,
					   const int YDim,
					   const int AssumeImageTiles,
					   unsigned char* pResultImage);

/*******************************************************************************
  This PVR code is loosely based on Wolfgang Engel's Oolong Engine:

        http://oolongengine.com/

    Thank you, Wolfgang!
 ******************************************************************************/

const char *typeStrings[] =
{
    "<invalid>", "<invalid>", "<invalid>", "<invalid>",
    "<invalid>", "<invalid>", "<invalid>", "<invalid>",
    "<invalid>", "<invalid>", "<invalid>", "<invalid>",
    "<invalid>", "<invalid>", "<invalid>", "<invalid>",
    "RGBA4444", "RGBA5551", "RGBA8888", "RGB565",
    "RGB555", "RGB888", "I8", "AI8",
    "PVRTC2", "PVRTC4"
};

typedef struct PVRHeader
{
    uint32_t      size;
    uint32_t      height;
    uint32_t      width;
    uint32_t      mipcount;
    uint32_t      flags;
    uint32_t      texdatasize;
    uint32_t      bpp;
    uint32_t      rmask;
    uint32_t      gmask;
    uint32_t      bmask;
    uint32_t      amask;
    uint32_t      magic;
    uint32_t      numtex;
} PVRHeader;

PVRTexture::PVRTexture()
:data(NULL)
{
}

PVRTexture::~PVRTexture()
{
    if(this->data)
        free(this->data);
}

bool PVRTexture::loadApplePVRTC(uint8_t* data, int size)
{
    // additional heuristic
    if(size>sizeof(PVRHeader))
    {
        PVRHeader *header = (PVRHeader *)data;
        if( header->size == sizeof( PVRHeader )
        &&( header->magic == 0x21525650 ) )
            // this looks more like a PowerVR file.
            return false;
    }

    // default to 2bpp, 8x8
    int mode = 1;
    int res = 8;

    // this is a tough one, could be 2bpp 8x8, 4bpp 8x8
    if(size==32)
    {
        // assume 4bpp, 8x8
        mode = 0;
        res = 8;
    } else
    {
        // Detect if it's 2bpp or 4bpp
        int shift = 0;
        int test2bpp = 0x40; // 16x16
        int test4bpp = 0x80; // 16x16

        while(shift<10)
        {
            int s2 = shift<<1;

            if((test2bpp<<s2)&size)
            {
                res = 16<<shift;
                mode = 1;
                this->format = "PVRTC2";
                break;
            }

            if((test4bpp<<s2)&size)
            {
                res = 16<<shift;
                mode = 0;
                this->format = "PVRTC4";
                break;
            }


            ++shift;
        }

        if(shift==10)
            // no mode could be found.
            return false;
        printf("detected apple %ix%i %i bpp pvrtc\n", res, res, mode*2+2);
    }

    // there is no reliable way to know if it's a 2bpp or 4bpp file. Assuming
    this->width = res;
    this->height = res;
    this->bpp = (mode+1)*2;
    this->numMips = 0;
    this->data = (uint8_t*)malloc(this->width*this->height*4);

    Decompress((AMTC_BLOCK_STRUCT*)data, mode, this->width,
                    this->height, 0, this->data);

    for(int y=0; y<res/2; ++y)
    for(int x=0; x<res; ++x)
    {
        int src = (x+y*res)*4;
        int dst = (x+(res-y-1)*res)*4;

        for(int c=0; c<4; ++c)
        {
            uint8_t tmp = this->data[src+c];
            this->data[src+c] = this->data[dst+c];
            this->data[dst+c] = tmp;
        }
    }

    return true;
}

ePVRLoadResult PVRTexture::loadPVR2(uint8_t *data, int length) {
    if(length<sizeof(PVRHeader))
    {
        free(data);
        return PVR_LOAD_INVALID_FILE;
    }



    // parse the header
    uint8_t* p = data;
    PVRHeader *header = (PVRHeader *)p;
    p += sizeof( PVRHeader );

    if( header->size != sizeof( PVRHeader ) )
    {
        free( data );
        return PVR_LOAD_INVALID_FILE;
    }

    if( header->magic != 0x21525650 )
    {
        free( data );
        return PVR_LOAD_INVALID_FILE;
    }
    
    if(header->numtex<1)
    {
        header->numtex = (header->flags & PVRTEX_CUBEMAP)?6:1;
    }
    
    if( header->numtex != 1 )
    {
        free( data );
        return PVR_LOAD_MORE_THAN_ONE_SURFACE;
    }

    if(header->width*header->height*header->bpp/8 > length-sizeof(PVRHeader))
    {
        return PVR_LOAD_INVALID_FILE;
    }

    int ptype = header->flags & PVR_PIXELTYPE_MASK;
    printf("Pixeltype: 0x%02x\n", ptype);

    this->width = header->width;
    this->height = header->height;
    this->numMips = header->mipcount;
    this->bpp = header->bpp;

    printf("Width: %i\n", this->width);
    printf("Height: %i\n", this->height);

    this->data = (uint8_t*)malloc(this->width*this->height*4);

    if(ptype<PVR_MAX_TYPE)
        this->format = typeStrings[ptype];
    else
        this->format = "<unknown>";

    switch(ptype)
    {
    case PVR_TYPE_RGBA4444:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                int v1 = *in++;
                int v2 = *in++;

                uint8_t a = (v1&0x0f)<<4;
                uint8_t b = (v1&0xf0);
                uint8_t g = (v2&0x0f)<<4;
                uint8_t r = (v2&0xf0);

                *out++ = r;
                *out++ = g;
                *out++ = b;
                *out++ = a;
            }
        }
        break;
    case PVR_TYPE_RGBA5551:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                unsigned short v = *(unsigned short*)in;
                in += 2;

                uint8_t r = (v&0xf800)>>8;
                uint8_t g = (v&0x07c0)>>3;
                uint8_t b = (v&0x003e)<<2;
                uint8_t a = (v&0x0001)?255:0;

                *out++ = r;
                *out++ = g;
                *out++ = b;
                *out++ = a;
            }
        }
        break;
    case PVR_TYPE_RGBA8888:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                *out++ = *in++;
                *out++ = *in++;
                *out++ = *in++;
                *out++ = *in++;
            }
        }
        break;
    case PVR_TYPE_RGB565:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                short v = *(short*)in;
                in += 2;
                

                uint8_t b = (v&0x001f)<<3;
                uint8_t g = (v&0x07e0)>>3;
                uint8_t r = (v&0xf800)>>8;
                uint8_t a = 255;

                if(x==128&&y==128)
                {
                    printf("%04x\n", v);
                    printf("%i %i %i\n", r, g, b);
                }
                
                *out++ = r;
                *out++ = g;
                *out++ = b;
                *out++ = a;
            }
        }
        break;
    case PVR_TYPE_RGB555:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                short v = *(short*)in;
                in += 2;

                uint8_t r = (v&0x001f)<<3;
                uint8_t g = (v&0x03e0)>>2;
                uint8_t b = (v&0x7c00)>>7;
                uint8_t a = 255;

                *out++ = r;
                *out++ = g;
                *out++ = b;
                *out++ = a;
            }
        }
        break;
    case PVR_TYPE_RGB888:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                *out++ = *in++;
                *out++ = *in++;
                *out++ = *in++;
                *out++ = 255;
            }
        }
        break;
    case PVR_TYPE_I8:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                int i = *in++;

                *out++ = i;
                *out++ = i;
                *out++ = i;
                *out++ = 255;
            }
        }
        break;
    case PVR_TYPE_AI8:
        {
            uint8_t *in  = p;
            uint8_t *out = this->data;
            for(int y=0; y<this->height; ++y)
            for(int x=0; x<this->width; ++x)
            {
                int i = *in++;
                int a = *in++;

                *out++ = i;
                *out++ = i;
                *out++ = i;
                *out++ = a;
            }
        }
        break;
    case PVR_TYPE_PVRTC2:
        {
            Decompress((AMTC_BLOCK_STRUCT*)p, 1, this->width,
                    this->height, 1, this->data);
        } break;
    case PVR_TYPE_PVRTC4:
        {
            Decompress((AMTC_BLOCK_STRUCT*)p, 0, this->width,
                    this->height, 1, this->data);
        } break;
    default:
        printf("unknown PVR type %i!\n", ptype);
        free(this->data);
        this->data = NULL;
        free(data);
        return PVR_LOAD_UNKNOWN_TYPE;
    }
    free(data);
    return PVR_LOAD_OKAY;
}

ePVRLoadResult PVRTexture::load(const char *const path)
{
    uint8_t *data;
    unsigned int length;

    FILE *fp = fopen(path, "rb");
    if(fp==NULL)
        return PVR_LOAD_FILE_NOT_FOUND;

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    data = (uint8_t*)malloc(length);
    fread(data, 1, length, fp);

    fclose(fp);

    // use a heuristic to detect potential apple PVRTC formats
    if(countBits(length)==1)
    {
        // very likely to be apple PVRTC
        if(loadApplePVRTC(data, length))
            return PVR_LOAD_OKAY;
    }
    if (length < 4) {
      return PVR_LOAD_INVALID_FILE;
    }
    // Detect which PVR file it is
    uint32_t *magic = reinterpret_cast<uint32_t*>(data);
    if (magic[0] == 0x03525650) {
      // PVR3 format
      return loadPVR3(data, length);
    } else {
      return loadPVR2(data, length);
    }
}

/*******************************************************************************
  PVR 3 support
 ******************************************************************************/
#pragma pack(push, 1)
struct PVR3Header {
  uint32_t version;
  uint32_t flags;
  union {
    uint8_t format_chars[8];
    uint32_t format_split[2];
    uint64_t format;
  };
  uint32_t colorspace;
  uint32_t channeltype;
  uint32_t height;
  uint32_t width;
  uint32_t depth;
  uint32_t num_surfaces;
  uint32_t num_faces;
  uint32_t mipcount;
  uint32_t metadata_size;
};
#pragma pack(pop)

enum ePVR3Format {
  kPVR3_PVRTC_2BPP_RGB = 0,
  kPVR3_PVRTC_2BPP_RGBA = 1,
  kPVR3_PVRTC_4BPP_RGB = 2,
  kPVR3_PVRTC_4BPP_RGBA = 3,
  kPVR3_PVRTC2_2BPP = 4,
  kPVR3_PVRTC2_4BPP = 5,
  kPVR3_ETC1 = 6,
  kPVR3_DXT1 = 7,
  kPVR3_DXT2 = 8,
  kPVR3_DXT3 = 9,
  kPVR3_DXT4 = 10,
  kPVR3_DXT5 = 11,
  kPVR3_BC1 = 7,
  kPVR3_BC2 = 9,
  kPVR3_BC3 = 11,
  kPVR3_BC4 = 12,
  kPVR3_BC5 = 13,
  kPVR3_BC6 = 14,
  kPVR3_BC7 = 15,
  kPVR3_UYVY = 16,
  kPVR3_YUY2 = 17,
  kPVR3_BW_1BPP = 18,
  kPVR3_R9G9B9E5 = 19,
  kPVR3_RGBG8888 = 20,
  kPVR3_GRGB8888 = 21,
  kPVR3_ETC2_RGB = 22,
  kPVR3_ETC2_RGBA = 23,
  kPVR3_ETC2_RGB_A1 = 24,
  kPVR3_EAC_R11_U = 25,
  kPVR3_EAC_R11_S = 26,
  kPVR3_EAC_RG11_U = 27,
  kPVR3_EAC_RG11_S = 28,
};

static inline char printascii(char in) {
  if (in >= ' ' && in <= '}') {
    return in;
  }
  return ' ';
}

struct FormatInfo {
  uint32_t bpp;
};

#define PVR3TAG(a, b, c, d, e, f, g, h) (\
    (static_cast<uint64_t>(a) << 56) | \
    (static_cast<uint64_t>(b) << 48) | \
    (static_cast<uint64_t>(c) << 40) | \
    (static_cast<uint64_t>(d) << 32) | \
    (static_cast<uint64_t>(e) << 24) | \
    (static_cast<uint64_t>(f) << 16) | \
    (static_cast<uint64_t>(g) << 8) | \
    (static_cast<uint64_t>(h) << 0) )


static const std::map<uint64_t, FormatInfo> kFormats({
  {PVR3TAG(0, 0, 0, 8,   0,   0,   0, 'i'), {8}},
  {PVR3TAG(0, 0, 8, 8,   0,   0, 'i', 'a'), {16}},
  {PVR3TAG(0, 5, 6, 5,   0, 'b', 'g', 'r'), {16}},
  {PVR3TAG(4, 4, 4, 4, 'a', 'b', 'g', 'r'), {16}},
  {PVR3TAG(0, 8, 8, 8,   0, 'b', 'g', 'r'), {24}},
  {PVR3TAG(8, 8, 8, 8, 'a', 'b', 'g', 'r'), {32}},
  {PVR3TAG(0, 0, 0, 0,   0,   0,   0,   kPVR3_PVRTC_2BPP_RGB), {2}},
  {PVR3TAG(0, 0, 0, 0,   0,   0,   0,   kPVR3_PVRTC_2BPP_RGBA), {2}},
  {PVR3TAG(0, 0, 0, 0,   0,   0,   0,   kPVR3_PVRTC_4BPP_RGB), {4}},
  {PVR3TAG(0, 0, 0, 0,   0,   0,   0,   kPVR3_PVRTC_4BPP_RGBA), {4}},
});

ePVRLoadResult PVRTexture::loadPVR3(uint8_t *data, int length) {
  if (length < sizeof(PVR3Header)) {
    return PVR_LOAD_INVALID_FILE;
  }
  PVR3Header *header = reinterpret_cast<PVR3Header*>(data);
  if (header->version != 0x03525650) {
    printf("PVR3: invalid header!\n");
    return PVR_LOAD_INVALID_FILE;
  }
  // Determine the format
  auto fit = kFormats.find(header->format);
  FormatInfo format;
  if (fit != kFormats.end()) {
    format = fit->second;
  } else {
    // More complicated or unsupported format!
    char a = printascii(header->format_chars[0]);
    char b = printascii(header->format_chars[1]);
    char c = printascii(header->format_chars[2]);
    char d = printascii(header->format_chars[3]);
    int e = header->format_chars[4];
    int f = header->format_chars[5];
    int g = header->format_chars[6];
    int h = header->format_chars[7];
    printf("Unsupported PVR3 format: 0x%08x ('%c%c%c%c') 0x%08x (%i%i%i%i)\n", header->format_split[0], a, b, c, d, header->format_split[1], e, f, g, h);
    return PVR_LOAD_UNKNOWN_TYPE;
  }
  // Compute data size
  this->width = header->width;
  this->height = header->height;
  this->numMips = header->mipcount;
  this->bpp = format.bpp;
  this->should_flip = false;
  printf("Width: %i\n", this->width);
  printf("Height: %i\n", this->height);
  printf("BPP: %i\n", this->bpp);
  this->data = (uint8_t*)malloc(this->width*this->height*4);
  // Read
  uint8_t *p = data + sizeof(PVR3Header) + header->metadata_size;
  switch (header->format) {
  case PVR3TAG(0, 0, 0, 8,   0,   0,   0, 'i'): {
    uint8_t *in  = p;
    uint8_t *out = this->data;
    for(int y=0; y<this->height; ++y)
    for(int x=0; x<this->width; ++x)
    {
        int i = *in++;

        *out++ = i;
        *out++ = i;
        *out++ = i;
        *out++ = 255;
    }
    break;
  }
  case PVR3TAG(0, 0, 8, 8,   0,   0, 'i', 'a'): {
    uint8_t *in  = p;
    uint8_t *out = this->data;
    for(int y=0; y<this->height; ++y)
    for(int x=0; x<this->width; ++x)
    {
        int i = *in++;
        int a = *in++;

        *out++ = i;
        *out++ = i;
        *out++ = i;
        *out++ = a;
    }
    break;
  }
  case PVR3TAG(4, 4, 4, 4, 'a', 'b', 'g', 'r'): {
    uint8_t *in  = p;
    uint8_t *out = this->data;
    for(int y=0; y<this->height; ++y) {
      for(int x=0; x<this->width; ++x) {
        int v1 = *in++;
        int v2 = *in++;
        uint8_t a = (v1&0x0f)<<4;
        uint8_t b = (v1&0xf0);
        uint8_t g = (v2&0x0f)<<4;
        uint8_t r = (v2&0xf0);
        *out++ = r;
        *out++ = g;
        *out++ = b;
        *out++ = a;
      }
    }
    break;
  }
  case PVR3TAG(0, 8, 8, 8,  0, 'b', 'g', 'r'): {
    uint8_t *in  = p;
    uint8_t *out = this->data;
    for(int y=0; y<this->height; ++y)
    for(int x=0; x<this->width; ++x) {
      *out++ = *in++;
      *out++ = *in++;
      *out++ = *in++;
      *out++ = 255;
    }
    break;
  }
  case PVR3TAG(8, 8, 8, 8, 'a', 'b', 'g', 'r'): {
    uint8_t *in  = p;
    uint8_t *out = this->data;
    for(int y=0; y<this->height; ++y)
    for(int x=0; x<this->width; ++x) {
      *out++ = *in++;
      *out++ = *in++;
      *out++ = *in++;
      *out++ = *in++;
    }
    break;
  }
  case PVR3TAG(0, 5, 6, 5,   0, 'b', 'g', 'r'): {
    uint8_t *in  = p;
    uint8_t *out = this->data;
    for(int y=0; y<this->height; ++y) {
      for(int x=0; x<this->width; ++x) {
        short v = *(short*)in;
        in += 2;
        uint8_t b = (v&0x001f)<<3;
        uint8_t g = (v&0x07e0)>>3;
        uint8_t r = (v&0xf800)>>8;
        uint8_t a = 255;
        *out++ = r;
        *out++ = g;
        *out++ = b;
        *out++ = a;
      }
    }
    break;
  }
  case PVR3TAG(0, 0, 0, 0,   0,   0,   0, kPVR3_PVRTC_2BPP_RGB): {
    Decompress((AMTC_BLOCK_STRUCT*)p, 1, this->width, this->height, 1, this->data);
    break;
  }
  case PVR3TAG(0, 0, 0, 0,   0,   0,   0, kPVR3_PVRTC_4BPP_RGB): {
    Decompress((AMTC_BLOCK_STRUCT*)p, 0, this->width, this->height, 1, this->data);
    break;
  }
  case PVR3TAG(0, 0, 0, 0,   0,   0,   0, kPVR3_PVRTC_2BPP_RGBA): {
    Decompress((AMTC_BLOCK_STRUCT*)p, 1, this->width, this->height, 1, this->data);
    break;
  }
  case PVR3TAG(0, 0, 0, 0,   0,   0,   0, kPVR3_PVRTC_4BPP_RGBA): {
    Decompress((AMTC_BLOCK_STRUCT*)p, 0, this->width, this->height, 1, this->data);
    break;
  }
  default: {
    // More complicated or unsupported format!
    char a = printascii(header->format_chars[0]);
    char b = printascii(header->format_chars[1]);
    char c = printascii(header->format_chars[2]);
    char d = printascii(header->format_chars[3]);
    int e = header->format_chars[4];
    int f = header->format_chars[5];
    int g = header->format_chars[6];
    int h = header->format_chars[7];
    printf("Unsupported PVR3 format: 0x%08x ('%c%c%c%c') 0x%08x (%i%i%i%i)\n", header->format_split[0], a, b, c, d, header->format_split[1], e, f, g, h);
    return PVR_LOAD_UNKNOWN_TYPE;
  }
  }
  return PVR_LOAD_OKAY;
}

