#pragma once
#include "CommonLib.h"
#include "../Bitmap.h"

namespace GraphEngine {
    namespace Display {

        class CReadJPG
        {
        public:
            CReadJPG(const byte_t* ptr = 0, int length = 0);

            BitmapPtr  Read();
            void Read(BitmapPtr pBitmap);

        private:

            void  readJPG(BitmapPtr pBitmap);
            int m_length;
            const byte_t *m_data;

        };
    }
}