#pragma once
#include "CommonLib.h"
#include "../Bitmap.h"

namespace GraphEngine {
    namespace Display {


        struct argb
        {
            byte    blue;
            byte    green;
            byte    red;
            byte    alfa;
        };

        struct dib
        {
            int row_byte_length() const
            {
                int nBytesPerLine = m_bmp_size.cx *( bytes_per_pixel());
                nBytesPerLine = ((nBytesPerLine + 31) & (~31)) / 8;
                return nBytesPerLine;
            }
            byte_t* row_ptr(int r)
            {
                  return (bitmap_bits() + (m_bmp_size.cy - r - 1) * row_byte_length());
            }

            SIZE size() const {return m_bmp_size;}
            int         bitmap_width() const  { return m_bmp_size.cx; }
            int         bitmap_height() const { return m_bmp_size.cy; }
            byte_t *      bitmap_bits() {return m_bits;}

            dib(): m_bpp(0), m_bits(0), m_buf_size(0)
            {
                m_bmp_size.cx = 0; m_bmp_size.cy = 0;
            }

            void resizeDib(SIZE sz, int bpp)
            {
                m_bmp_size = sz;
                m_bpp = bpp;
                m_buf_size  = 4 * ((bitmap_width() * m_bpp + 31) / 32) * bitmap_height();/*bitmap_width() * bitmap_height()*/;

                if(m_buf_size == 0)
                    throw CommonLib::CExcBase("Wrong bitmap size");

                m_bits = new unsigned char [m_buf_size];
            }

            int bytes_per_pixel() const {return m_bpp;}
            void alpha_inv()
            {
                argb* pc = (argb*)bitmap_bits();
                argb* pcend = pc + (size().cx * size().cy);
                double K = 0;
                while(pc < pcend)
                {
                    K = pc->alfa / 255.0;
                    pc->blue = (byte)(pc->blue * K);
                    pc->red = (byte)(pc->red * K);
                    pc->green = (byte)(pc->green * K);
                    //pc->alfa = 255 - pc->alfa;
                    ++pc;
                }
            }


            unsigned char* m_bits;
            SIZE m_bmp_size;
            int m_bpp;
            int m_buf_size;

        };

        class ReadPng
        {
        public:
            ReadPng(const byte_t* ptr = 0, int length = 0);

            BitmapPtr Read();
            void Read(BitmapPtr ptrBitmap);

            const byte_t * start;
            const byte_t * end;
            const byte_t * pos;
        private:

            void  readPNG();
            dib m_dib;
            int m_length;
            const byte_t * m_data;

        };


    }
}