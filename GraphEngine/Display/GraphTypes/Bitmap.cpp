#include "CommonLib.h"
#include "Bitmap.h"
#include "DisplayMath.h"

#include "../../CommonLib/alloc/simpleAlloc.h"
#include "../../CommonLib/stream/MemoryStream.h"

#include "../agg/agg_renderer_base.h"
#include "../agg/agg_scanline_p.h"
#include "../agg/agg_renderer_scanline.h"
#include "../agg/agg_pixfmt_rgba.h"

#include "../agg/agg_rasterizer_outline.h"
#include "../agg/agg_renderer_primitives.h"
#include "../agg/agg_rasterizer_outline_aa.h"
#include "../agg/agg_renderer_outline_aa.h"
#include "../agg/agg_ellipse.h"
#include "../agg/agg_font_cache_manager.h"
#include "../agg/agg_rasterizer_scanline_aa.h"
#include "../agg/agg_conv_dash.h"
#include "../agg/agg_conv_stroke.h"

#include "../agg/agg_span_allocator.h"
#include "../agg/agg_span_interpolator_linear.h"
#include "../agg/agg_span_image_filter.h"
#include "../agg/agg_image_accessors.h"
#include "../agg/agg_span_image_filter_rgba.h"
#include "../agg/agg_span_pattern_rgba.h"
#include "../agg/agg_trans_affine.h"

namespace GraphEngine
{
	namespace Display
	{
		CBitmap::CBitmap(CommonLib::IAllocPtr pAlloc)  
		{
			m_pBuf = 0;
			m_nWidth = 0;
			m_nHeight = 0;
			m_pPalette = 0;
			m_bRelease = false;
			m_type = BitmapFormatType32bppARGB;

			m_ptrAlloc = pAlloc;

			if (!m_ptrAlloc)
				m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>(new CommonLib::CSimpleAlloc());
		}

		CBitmap::CBitmap(unsigned char* bits, size_t width, size_t height, eBitmapFormatType type,
			Color* palette, bool release, CommonLib::IAllocPtr pAlloc) : m_ptrAlloc(pAlloc)
		{
			m_ptrAlloc = pAlloc;

			if (!m_ptrAlloc)
				m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

			Attach(bits, width, height, type, palette, release);
		}

		CBitmap::CBitmap(CommonLib::IReadStream* pStream, CommonLib::IAllocPtr pAlloc) : m_ptrAlloc(pAlloc)
		{
			m_ptrAlloc = pAlloc;

			if (!m_ptrAlloc)
				m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

			m_nWidth = (size_t)pStream->ReadInt32();
			m_nHeight = (size_t)pStream->ReadInt32();
			m_type = (eBitmapFormatType)pStream->ReadByte();
			Init(m_nWidth, m_nHeight, m_type);
			pStream->Read(m_pBuf, Size());
			if (m_pPalette)
				for (int i = 0; i < 1 << Bpp(); i++)
					m_pPalette[i].Load(pStream);
		}

		CBitmap::CBitmap(size_t width, size_t height, eBitmapFormatType type, CommonLib::IAllocPtr pAlloc) : m_ptrAlloc(pAlloc)
		{
			m_ptrAlloc = pAlloc;

			if (!m_ptrAlloc)
				m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();

			Init(width, height, type);
		}

		void CBitmap::Init(size_t width, size_t height, eBitmapFormatType type)
		{
			m_nWidth = width;
			m_nHeight = height;
			m_type = type;
			m_pPalette = 0;
			m_pBuf = new unsigned char[Size()];
			switch (m_type)
			{
			case BitmapFormatType1bpp:
				m_pPalette = new Color[2];
				break;
			case BitmapFormatType4bpp:
				m_pPalette = new Color[16];
				break;
			case BitmapFormatType8bpp:
				m_pPalette = new Color[256];
				break;
			}
			m_bRelease = true;
		}

		CBitmap::CBitmap(const CBitmap& bmp)
			: m_pPalette(0)
			, m_pBuf(0)
		{
			m_ptrAlloc = bmp.m_ptrAlloc;
			this->operator=(bmp);
		}

		CBitmap::~CBitmap(void)
		{
			if (m_bRelease)
			{
				m_ptrAlloc->Free(m_pBuf);
				if (m_pPalette)
					m_ptrAlloc->Free(m_pPalette);
			}
		}

		CBitmap& CBitmap::operator=(const CBitmap& bmp)
		{
			if (this == &bmp)
				return *this;


			if (m_bRelease && m_pBuf)
			{
				m_ptrAlloc->Free(m_pBuf);
				m_pBuf = 0;

				if (m_pPalette)
					m_ptrAlloc->Free(m_pPalette);

				m_pPalette = 0;
			}
 
			m_ptrAlloc = bmp.m_ptrAlloc;

			m_nWidth = bmp.m_nWidth,
				m_nHeight = bmp.m_nHeight,
				m_type = bmp.m_type;
			if (bmp.m_pBuf == 0)
			{
				m_bRelease = false;
				return *this;
			}
			m_bRelease = true;

			m_pBuf = (byte*)m_ptrAlloc->Alloc(Size());
			memcpy(m_pBuf, bmp.m_pBuf, Size());

			switch (m_type)
			{
			case BitmapFormatType1bpp:
				m_pPalette = (Color*)m_ptrAlloc->Alloc(sizeof(Color) * 2);
				memcpy(m_pPalette, bmp.m_pPalette, sizeof(Color) * 2);
				break;
			case BitmapFormatType4bpp:
				m_pPalette = (Color*)m_ptrAlloc->Alloc(sizeof(Color) * 16);
				memcpy(m_pPalette, bmp.m_pPalette, sizeof(Color) * 16);
				break;
			case BitmapFormatType8bpp:
				m_pPalette = (Color*)m_ptrAlloc->Alloc(sizeof(Color) * 256);
				memcpy(m_pPalette, bmp.m_pPalette, sizeof(Color) * 256);
				break;
			}

			return *this;
		}

		size_t CBitmap::Height() const
		{
			return m_nHeight;
		}

		size_t CBitmap::Width() const
		{
			return m_nWidth;
		}

		unsigned char* CBitmap::Bits()
		{
			return m_pBuf;
		}

		const unsigned char* CBitmap::Bits() const
		{
			return m_pBuf;
		}

		size_t CBitmap::LineSize() const
		{
			return 4 * ((m_nWidth * Bpp() + 31) / 32);
		}

		size_t CBitmap::Size() const
		{
			return LineSize() * m_nHeight;
		}

		size_t CBitmap::Bpp() const
		{
			switch (m_type)
			{
			case BitmapFormatType1bpp:
				return 1;
			case BitmapFormatType4bpp:
				return 4;
			case BitmapFormatType8bpp:
				return 8;
			case BitmapFormatType16bppARGB1555:
			case BitmapFormatType16bppGrayScale:
			case BitmapFormatType16bppRGB555:
			case BitmapFormatType16bppRGB565:
				return 16;
			case BitmapFormatType24bppRGB:
				return 24;
			case BitmapFormatType32bppARGB:
				return 32;
			}
			return 0;
		}

		Color* CBitmap::Palette()
		{
			return m_pPalette;
		}

		eBitmapFormatType CBitmap::Type() const
		{
			return m_type;
		}

		Color CBitmap::Pixel(size_t row, size_t col)
		{
			size_t row_ptr = LineSize() * row;
			size_t ptr = row_ptr + (col * Bpp()) / 8;

			if (Bpp() <= 8) // get from pallete
			{
				size_t idx = 0;
				if (!m_pPalette)
					return Color();
				switch (m_type)
				{
				case BitmapFormatType1bpp:
					idx = ((m_pBuf[ptr] >> (7 - (col % 8))) & 1) ? 1 : 0;
					break;
				case BitmapFormatType4bpp:
					idx = ((col % 2) == 0) ? (m_pBuf[ptr] >> 4) & 0xF : m_pBuf[ptr] & 0xF;
					return 4;
				case BitmapFormatType8bpp:
					idx = m_pBuf[ptr];
					return 8;
				}
				return m_pPalette[idx];
			}
			else
			{
				Color::ColorComponent r = 0, g = 0, b = 0, a = Color::Opaque;
				switch (m_type)
				{
				case BitmapFormatType16bppARGB1555:
					a = m_pBuf[ptr] & 1;
					b = (m_pBuf[ptr] >> 1) & 31;
					g = ((*((int16_t*)&m_pBuf[ptr])) >> 6) & 31;
					r = ((*((int16_t*)&m_pBuf[ptr])) >> 11) & 31;
					break;
				case BitmapFormatType16bppGrayScale: // type Color can store 256 grayscale gradations only
					return Color();
				case BitmapFormatType16bppRGB555:
					b = (m_pBuf[ptr] >> 1) & 31;
					g = (*((int16_t*)&m_pBuf[ptr]) >> 6) & 31;
					r = (*((int16_t*)&m_pBuf[ptr]) >> 11) & 31;
					break;
				case BitmapFormatType16bppRGB565:
					r = ((*((int16_t*)&m_pBuf[ptr])) >> 8) & 0xF8;
					g = ((*((int16_t*)&m_pBuf[ptr])) >> 3) & 0xFC;
					b = ((*((int16_t*)&m_pBuf[ptr])) << 3) & 0xF8;
					break;
				case BitmapFormatType24bppRGB:
					b = m_pBuf[ptr];
					g = m_pBuf[ptr + 1];
					r = m_pBuf[ptr + 2];
					break;
				case BitmapFormatType32bppARGB:
					b = m_pBuf[ptr];
					g = m_pBuf[ptr + 1];
					r = m_pBuf[ptr + 2];
					a = m_pBuf[ptr + 3];
					break;
					//return *((Color*)&buf_[ptr]);
				}
				return Color(r, g, b, a);
			}
		}

		bool CBitmap::CopyTo(CBitmap* bitmap)
		{
			if (bitmap->Height() != Height() || bitmap->Width() != Width())
				return false;

			// TODO: support all bitmap formats
			if (bitmap->Type() != BitmapFormatType32bppARGB)
				return false;

			Color::ColorComponent* bits = bitmap->Bits();
			for (size_t row = 0; row < m_nHeight; row++)
			{
				for (size_t col = 0; col < m_nWidth; col++, bits += 4)
				{
					Color color = Pixel(row, col);
					bits[0] = color.GetB();
					bits[1] = color.GetG();
					bits[2] = color.GetR();
					bits[3] = color.GetA();
				}
			}

			return true;
		}

        void CBitmap::Save(CommonLib::IWriteStream *pStream) const
        {
            try
            {
                CommonLib::CWriteMemoryStream stream;
                stream.Write((uint32_t)m_nWidth);
                stream.Write((uint32_t)m_nHeight);
                stream.Write((byte)m_type);
                stream.Write(m_pBuf, Size());
                if (m_pPalette)
                    for (int i = 0; i < 1 << Bpp(); i++)
                        m_pPalette[i].Save(&stream);

                pStream->Write(&stream);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save bitmap", exc);
            }

        }

        void  CBitmap::Load(CommonLib::IReadStream *pStream)
        {
            try
            {


                pStream->Read(m_nWidth);
                pStream->Read(m_nHeight);
                byte type = 0;
                pStream->Read(type);
                m_type = (eBitmapFormatType)type;
                Init(m_nWidth, m_nHeight, m_type);
                size_t nSize = Size();
                if (nSize)
                {
                    pStream->Read(m_pBuf, nSize);
                    if (m_pPalette)
                    {
                        for (int i = 0; i < 1 << Bpp(); i++)
                        {
                            m_pPalette[i].Load(pStream);
                        }
                    }
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load bitmap", exc);
            }

        }

		void  CBitmap::Save(CommonLib::ISerializeObjPtr pObj, const astr& name) const
		{
			try
			{
                CommonLib::ISerializeObjPtr pBlobNode = pObj->CreateChildNode(name);
				CommonLib::CWriteMemoryStream stream(m_ptrAlloc);
				Save(&stream);
				pBlobNode->SetBlob(stream.Buffer(), stream.Size());
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExc("Failed to save bitmap", exc);
			}
		}

		void CBitmap::Load(CommonLib::ISerializeObjPtr pObj, const astr& name)
		{
			try
			{
                CommonLib::ISerializeObjPtr pBlobNode = pObj->GetChild(name);
				CommonLib::Data::TVecBuffer blob;
				pBlobNode->GetBlob(blob);

				CommonLib::CReadMemoryStream stream;
				stream.AttachBuffer(blob.data(), blob.size());
				return Load(&stream);
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExc("Failed to load bitmap", exc);
			}
		}

		void CBitmap::Attach(unsigned char* bits, size_t width, size_t height, eBitmapFormatType type, Color* palette, bool release)
		{
			m_pBuf = bits;
			m_nWidth = width;
			m_nHeight = height;
			m_type = type;
			m_pPalette = m_pPalette;
			m_bRelease = release;
		}

		CBitmap* CBitmap::Transform(double xScale, double yScale, double angle)
		{
			if (this->Type() != BitmapFormatType32bppARGB)
				return 0;
			size_t newWidth = size_t(this->Width() * xScale);
			size_t newHeight = size_t(this->Height() * yScale);

			CBitmap* ret = new CBitmap(newWidth, newHeight, this->Type(), m_ptrAlloc);
			memset(ret->Bits(), 0, ret->Size());

			int w = (int)this->Width();
			int h = (int)this->Height();
			std::vector<unsigned char> tmpBuf;

			typedef agg::pixfmt_bgra32 pixfmt_t;
			typedef agg::span_allocator<agg::rgba8> span_allocator_t;
			typedef agg::wrap_mode_repeat pattern_wrap_t;
			typedef agg::image_accessor_wrap<agg::pixfmt_bgra32, pattern_wrap_t, pattern_wrap_t> pattern_accessor_t;
			typedef agg::span_pattern_rgba<pattern_accessor_t> span_pattern_generator_t;
			typedef agg::renderer_base<pixfmt_t> renderer_base_t;
			typedef agg::renderer_scanline_aa_solid<renderer_base_t> renderer_t;
			typedef agg::rasterizer_scanline_aa<> rasterizer_t;

			agg::rendering_buffer rbuf_(ret->Bits(), (int)newWidth, (int)newHeight, -4 * (int)newWidth);
			pixfmt_t rendering_buffer_(rbuf_);
			renderer_base_t renderer_base_(rendering_buffer_);
			renderer_t renderer_(renderer_base_);
			rasterizer_t rasterizer_;

			typedef agg::scanline_p8 scanline_t;


			span_allocator_t         span_allocator_;
			agg::pixfmt_bgra32       pattern_rgba32_;
			agg::rendering_buffer    pattern_rbuf_;
			span_pattern_generator_t span_pattern_generator_;

			pattern_rbuf_.attach((agg::int8u*)this->Bits(), w, h, (-w) * 4);
			pattern_rgba32_.attach(pattern_rbuf_);

			typedef agg::image_accessor_clone<agg::pixfmt_bgra32> img_accessor_type;
			img_accessor_type ia(pattern_rgba32_);

			double pol[8] = { 0, 0, (double)newWidth, 0, (double)newWidth, (double)newHeight, 0, (double)newHeight };

			rasterizer_.reset();
			agg::trans_affine mtx2(pol, 0, 0, w, h);
			if (!CDisplayMath::Equals(angle, 0.0))
			{
				mtx2 *= agg::trans_affine_translation(-(newWidth / 2.0), -(newHeight / 2.0));
				mtx2 *= agg::trans_affine_rotation(DEG2RAD(angle));
				mtx2 *= agg::trans_affine_translation(newWidth / 2.0, newWidth / 2.0);
			}
			//mtx2 *= agg::trans_affine_scaling(1 / xScale, 1 / yScale);

			typedef agg::span_interpolator_linear<> interpolator_type;
			interpolator_type interpolator(mtx2);

			typedef agg::span_image_filter_rgba_bilinear<img_accessor_type, interpolator_type> span_gen_type;
			span_gen_type sg(ia, interpolator, 255);

			rasterizer_.move_to_d(0, 0);
			rasterizer_.line_to_d((double)newWidth, 0);
			rasterizer_.line_to_d((double)newWidth, (double)newHeight);
			rasterizer_.line_to_d(0, (double)newHeight);
			rasterizer_.line_to_d(0, 0);

			agg::scanline_p8 scanline_;

			agg::render_scanlines_aa(rasterizer_, scanline_, renderer_base_, span_allocator_, sg);

			return ret;
		}
	}
}