#include "CommonLib.h"
#include "Pen.h"
#include "../../CommonLib/stream/MemoryStream.h"


namespace GraphEngine
{
	namespace Display
	{
        CPen::CPen(bool bIsNull)
        {
            m_type = bIsNull ? PenTypeNull : PenTypeSolid;
            m_color = Color::Black;
            m_nWidth = 1;
            m_capType = CapTypeRound;
            m_joinType = JoinTypeRound;
            m_pTexture = 0;
            m_nOffset = 0;
        }
        CPen::CPen(ePenType penType, const Color& color, GUnits nWidth, eCapType capType, eJoinType joinType,
                   GUnits nOffset, BitmapPtr pTexture) : m_type(penType), m_color(color), m_nWidth(nWidth), m_capType(capType),
                                                                        m_joinType(joinType), m_pTexture(pTexture), m_nOffset(nOffset)

        {
        }

        CPen::~CPen()
        {
        }

        void CPen::SetPenType(ePenType penType)
        {
            m_type = penType;
        }
        void CPen::SetColor(const Color& color)
        {
            m_color = color;
        }
        void CPen::SetWidth(GUnits nWidth)
        {
            m_nWidth = nWidth;
        }
        void CPen::SetCapType(eCapType capType)
        {
            m_capType = capType;
        }
        void CPen::SetJoinType(eJoinType joinType)
        {
            m_joinType = joinType;
        }

        void CPen::SetTexture(BitmapPtr pTexture)
        {
            m_pTexture = pTexture;
        }
        void CPen::SetOffset(GUnits nOffset)
        {
            m_nWidth = nOffset;
        }
        void CPen::AddTemplate(GUnits par, GUnits tem)
        {
            m_vecTemplates.push_back(std::make_pair(par, tem));
        }
        void CPen::ClearTmplates()
        {
            m_vecTemplates.clear();
        }


        ePenType CPen::GetPenType() const
        {
            return m_type;
        }

        const Color& CPen::GetColor() const
        {
            return m_color;
        }

        GUnits CPen::GetWidth() const
        {
            return m_nWidth;
        }

        eCapType CPen::GetCapType() const
        {
            return m_capType;
        }

        eJoinType CPen::GetJoinType() const
        {
            return m_joinType;
        }

        BitmapPtr CPen::GetTexture() const
        {
            return m_pTexture;
        }
        GUnits CPen::GetOffset() const
        {
            return m_nOffset;
        }
        const TPenTemplates& CPen::GetTemplates() const
        {
            return m_vecTemplates;
        }

        void CPen::Save(CommonLib::ISerializeObjPtr pObj, const astr& name) const
        {
            try
            {
                CommonLib::ISerializeObjPtr pPenNode = pObj->CreateChildNode(name);

                pPenNode->AddPropertyInt16U("PenType", uint16_t(m_type));
                pPenNode->AddPropertyInt16U("JoinType", uint16_t(m_joinType));
                pPenNode->AddPropertyDouble("Width", m_nWidth);
                pPenNode->AddPropertyInt16U("CapType", uint16_t(m_capType));

                m_color.Save(pPenNode, "Color");

                if(m_pTexture.get() != nullptr)
                    m_pTexture->Save(pPenNode, "Texture");

                if(m_vecTemplates.size())
                {
                    CommonLib::CWriteMemoryStream stream;
                    stream.Write((uint32_t)m_vecTemplates.size());
                    for (size_t i = 0; i < m_vecTemplates.size(); ++i)
                    {
                        stream.Write(m_vecTemplates[i].first);
                        stream.Write(m_vecTemplates[i].second);
                    }

                    CommonLib::ISerializeObjPtr pBlobNode = pPenNode->CreateChildNode("Templates");
                    pBlobNode->SetBlob(stream.Buffer(), stream.Size());

                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save pen", exc);
            }


        }
        void CPen::Load(CommonLib::ISerializeObjPtr pObj, const astr& name)
        {
            try
            {

                CommonLib::ISerializeObjPtr pPenNode = pObj->GetChild(name);

                m_type = (ePenType)pPenNode->GetPropertyInt16U("PenType", PenTypeNull);
                m_joinType = (eJoinType)pPenNode->GetPropertyInt16U("JoinType", JoinTypeMiter);
                m_capType = (eCapType)pPenNode->GetPropertyInt16U("CapType", CapTypeButt);
                m_nWidth = (GUnits)pPenNode->GetPropertyDouble("Width", 1);
                m_color.Load(pPenNode, "Color");

                if(pPenNode->IsChildExists("Texture"))
                {
                    m_pTexture = std::make_shared<CBitmap>();
                    m_pTexture->Load(pPenNode, "Texture");
                }

                if(pPenNode->IsChildExists("Templates"))
                {
                    CommonLib::ISerializeObjPtr pTemplatesNode = pObj->GetChild("Templates");
                    CommonLib::Data::TVecBuffer blob;
                    pTemplatesNode->GetBlob(blob);

                    CommonLib::CReadMemoryStream stream;
                    stream.AttachBuffer(blob.data(), blob.size());
                    uint32_t nSize = stream.ReadIntu32();
                    if(nSize)
                    {
                        m_vecTemplates.reserve(nSize);
                        for (size_t i = 0; i < nSize; ++i)
                        {
                            stream.Read(m_vecTemplates[i].first);
                            stream.Read(m_vecTemplates[i].second);

                        }
                    }

                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load pen", exc);
            }
        }
	}
}