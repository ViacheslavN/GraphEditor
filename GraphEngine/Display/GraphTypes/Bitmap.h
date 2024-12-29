#pragma once
#include "GraphTypes.h"
#include "Color.h"
#include "../../CommonLib/Serialize/SerializeObj.h"
#include "../../CommonLib/stream/stream.h"

namespace GraphEngine
{
	namespace Display
	{
		typedef std::shared_ptr<class CBitmap> BitmapPtr;

		class CBitmap
		{
			CBitmap(CommonLib::IAllocPtr pAlloc = CommonLib::IAllocPtr());
			CBitmap(CommonLib::IReadStream *pStream, CommonLib::IAllocPtr pAlloc = CommonLib::IAllocPtr());
			CBitmap(size_t width, size_t height, eBitmapFormatType type, CommonLib::IAllocPtr pAlloc = CommonLib::IAllocPtr());
			CBitmap(unsigned char* bits, size_t width, size_t height,
				eBitmapFormatType type, Color* palette = 0, bool release = false, CommonLib::IAllocPtr pAlloc = CommonLib::IAllocPtr());
			CBitmap(const CBitmap& bmp);
			~CBitmap(void);
			CBitmap& operator=(const CBitmap& bmp);

            void Save(CommonLib::IWriteStream *pStream) const;
            void Load(CommonLib::IReadStream *pStream);

			void Save(CommonLib::ISerializeObjPtr pObj, const astr& name) const;
			void Load(CommonLib::ISerializeObjPtr pObj, const astr& name);

		public:
			size_t                 Height() const;
			size_t                 Width() const;
			unsigned char*         Bits();
			const unsigned char*   Bits() const;
			size_t                 LineSize() const;
			size_t                 Size() const;
			size_t                 Bpp() const;
			Color*                 Palette();
			eBitmapFormatType      Type() const;
			Color                  Pixel(size_t row, size_t col);
			bool                   CopyTo(CBitmap* bitmap);
			void                   Attach(unsigned char* bits, size_t width, size_t height, eBitmapFormatType type, Color* palette = 0, bool release = false);
			CBitmap*               Transform(double xScale, double yScale, double angle = 0);
 
			void Init(size_t width, size_t height, eBitmapFormatType type);
		private:
			unsigned char*   m_pBuf;
			size_t           m_nWidth;
			size_t           m_nHeight;
			eBitmapFormatType m_type;
			Color*           m_pPalette;
			bool             m_bRelease;
			CommonLib::IAllocPtr	m_ptrAlloc;
		};
	}
}