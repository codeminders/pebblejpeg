//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "pebble.h"
#include "picojpeg.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>


//------------------------------------------------------------------------------
#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
//------------------------------------------------------------------------------
typedef unsigned char uint8;
typedef unsigned int uint;

//------------------------------------------------------------------------------
void set_pixel_from_grayscale (int nOffset, uint8 btValue);
void jpeg_data_to_bitmap(int nImgHeight, int nImgWidth);



//------------------------------------------------------------------------------

GBitmap g_Bmp = {0};

// 144*168 - display size
// 144 - rows
// 168 - columns
// 4 byte - block size for row

//Video buffer - max Pebble display size
//unsigned char g_BmpData[168][20] = {{0},{0}};
unsigned char g_BmpData[168][20];

//Temporary buffer for decoded JPEG image - max Pebble display size
unsigned char g_BmpTmpGrey[(144/8)*1*168];

//Trigger value for decision to convert 8-bit grey color to black or white Pebble pixel
unsigned char g_BlackTriggerValue = 128;

//Error code of JPEG image decoding. 
//0 - everything OK
unsigned char g_btJpgErrorCode;

#define 	JPEG_EC_OK							0
#define 	JPEG_EC_ERROR_BASE					10
#define 	JPEG_EC_INITIAL_DECODING_ERROR		(JPEG_EC_ERROR_BASE + 0)
#define 	JPEG_EC_NO_MORE_BLOCKS				(JPEG_EC_ERROR_BASE + 1)
#define 	JPEG_EC_REDUCTION_NOT_SUPPORTED		(JPEG_EC_ERROR_BASE + 2)
#define 	JPEG_EC_NO_GRAYSCALE				(JPEG_EC_ERROR_BASE + 3)
#define 	JPEG_EC_MCU_Y_ERROR					(JPEG_EC_ERROR_BASE + 4)


void init_bitmap()
{

	g_Bmp.addr = &g_BmpData;
	g_Bmp.row_size_bytes = 20;
	GRect rect = {{0,0},{144,168}};
	g_Bmp.bounds = rect;

	memset(g_BmpData, 0x0, sizeof (g_BmpData));
}

//------------------------------------------------------------------------------
static uint g_nInFileSize;
static uint g_nInFileOfs;
//------------------------------------------------------------------------------
static uint g_nBytesReadFromImgData = 0;
const uint8 *g_pImageData = 0;

unsigned char pjpeg_need_bytes_callback(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data)
{
   uint n;
   
   n = min(g_nInFileSize - g_nInFileOfs, buf_size);
	if (n) {
		memcpy(pBuf, &g_pImageData[g_nBytesReadFromImgData], n);
		g_nBytesReadFromImgData += n;
	}

   *pBytes_actually_read = (unsigned char)(n);
   g_nInFileOfs += n;
   return 0;
}

//------------------------------------------------------------------------------
// Loads JPEG image from specified file. Returns NULL on failure.
// On success, the malloc()'d image's width/height is written to *x and *y, and
// the number of components (1 or 3) is written to *comps.
// pScan_type can be NULL, if not it'll be set to the image's pjpeg_scan_type_t.
// Not thread safe.
// If reduce is non-zero, the image will be more quickly decoded at approximately
// 1/8 resolution (the actual returned resolution will depend on the JPEG 
// subsampling factor).
uint8 *pjpeg_load_from_data(const unsigned char *pImgData, int nImgDataSize, int *x, int *y, int *comps, pjpeg_scan_type_t *pScan_type, int reduce)
{
   pjpeg_image_info_t image_info;
   int mcu_x = 0;
   int mcu_y = 0;
   uint row_pitch;
   uint8 *pImage;
   uint8 status;
   uint decoded_width, decoded_height;
   uint row_blocks_per_mcu, col_blocks_per_mcu;

   *x = 0;
   *y = 0;
   *comps = 0;
   if (pScan_type) *pScan_type = PJPG_GRAYSCALE;

   g_nInFileOfs = 0;

	g_nInFileSize = nImgDataSize;

	g_pImageData = pImgData;
	g_nBytesReadFromImgData = 0;
  
  // Fill the initial values by Black
  memset(g_BmpTmpGrey,0,sizeof(g_BmpTmpGrey));

   status = pjpeg_decode_init(&image_info, pjpeg_need_bytes_callback, NULL, (unsigned char)reduce);
         
   if (status)
   {
		g_btJpgErrorCode = JPEG_EC_INITIAL_DECODING_ERROR;
		return NULL;
   }
   
   if (pScan_type)
      *pScan_type = image_info.m_scanType;

   // In reduce mode output 1 pixel per 8x8 block.
   decoded_width = reduce ? (image_info.m_MCUSPerRow * image_info.m_MCUWidth) / 8 : image_info.m_width;
   decoded_height = reduce ? (image_info.m_MCUSPerCol * image_info.m_MCUHeight) / 8 : image_info.m_height;

	int nPrevComps = image_info.m_comps;
	image_info.m_comps = 1;

   row_pitch = decoded_width * image_info.m_comps;
   pImage = 0;

	//No reduction
	//row_blocks_per_mcu = image_info.m_MCUWidth >> 3;
	//col_blocks_per_mcu = image_info.m_MCUHeight >> 3;
   
   for ( ; ; )
   {
      int y, x;
      uint8 *pDst_row;

      status = pjpeg_decode_mcu();
      
      if (status)
      {
         if (status != PJPG_NO_MORE_BLOCKS)
         {
         	g_btJpgErrorCode = JPEG_EC_NO_MORE_BLOCKS;
            return NULL;
         }

         break;
      }

      if (mcu_y >= image_info.m_MCUSPerCol)
      {
		  g_btJpgErrorCode = JPEG_EC_MCU_Y_ERROR;
	      return NULL;
      }

      if (reduce)
      {
		  g_btJpgErrorCode = JPEG_EC_REDUCTION_NOT_SUPPORTED;
		  return NULL;
	  }
      else
      {
         // Copy MCU's pixel blocks into the destination bitmap.
         pDst_row = pImage + (mcu_y * image_info.m_MCUHeight) * row_pitch + (mcu_x * image_info.m_MCUWidth * image_info.m_comps);

         for (y = 0; y < image_info.m_MCUHeight; y += 8)
         {
            const int by_limit = min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

            for (x = 0; x < image_info.m_MCUWidth; x += 8)
            {
               uint8 *pDst_block = pDst_row + x * image_info.m_comps;

               // Compute source byte offset of the block in the decoder's MCU buffer.
               uint src_ofs = (x * 8U) + (y * 16U);
               const uint8 *pSrcR = image_info.m_pMCUBufR + src_ofs;
               const uint8 *pSrcG = image_info.m_pMCUBufG + src_ofs;
               const uint8 *pSrcB = image_info.m_pMCUBufB + src_ofs;

               const int bx_limit = min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

               if (image_info.m_scanType == PJPG_GRAYSCALE)
               {
                  int bx, by;
                  for (by = 0; by < by_limit; by++)
                  {
                     uint8 *pDst = pDst_block;

                     for (bx = 0; bx < bx_limit; bx++)
                     {
                       set_pixel_from_grayscale (pDst - pImage, *pSrcR);
                       pDst++;
                       pSrcR++;
                     }

                     pSrcR += (8 - bx_limit);

                     pDst_block += row_pitch;
                  }
               }
               else
               {
   				   g_btJpgErrorCode = JPEG_EC_NO_GRAYSCALE;
   				   return NULL;
               }
            }

            pDst_row += (row_pitch * 8);
         }
      }

      mcu_x++;
      if (mcu_x == image_info.m_MCUSPerRow)
      {
         mcu_x = 0;
         mcu_y++;
      }
   }

   *x = decoded_width;
   *y = decoded_height;
   *comps = image_info.m_comps;

   jpeg_data_to_bitmap(image_info.m_height, image_info.m_width);

   pImage = (uint8*)g_BmpData;
   return pImage;
}


void set_pixel_from_grayscale (int nOffset, uint8 btValue)
{
	int nByteArrayIndex;
	int nInByteIndex;
	uint8 btTmp;

	nByteArrayIndex = nOffset / (sizeof(uint8) * 8);
	nInByteIndex = nOffset % (sizeof(uint8) * 8);

	if (btValue > g_BlackTriggerValue)  {
		btTmp = g_BmpTmpGrey[nByteArrayIndex];
		btTmp |= (0x80 >> nInByteIndex);
		g_BmpTmpGrey[nByteArrayIndex] = btTmp;
	}
// else - nothing to do, because of the 'g_BmpTmpGrey' array was inittially filled by appropriate 'Black' values
}


uint8 invert_bits_order(uint8 btData)
{
	uint8 btTmp;
	int i;

	btTmp = 0;
	for (i=0; i<8; i++)  {
		btTmp <<= 1;
		if(btData & 1) 
			btTmp |= 1;
		btData >>= 1;
	}

	return btTmp;
}


void jpeg_data_to_bitmap(int nImgHeight, int nImgWidth)
{
	uint8 *pSrc, *pDst;
	int i;

	pSrc = g_BmpTmpGrey;

	for (i=0; i < nImgHeight; i++) {
		for (int j=0; j < nImgWidth/8; j++) {
			g_BmpData[i][j] = invert_bits_order(*pSrc++);
		}
	}
}



