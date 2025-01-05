#include "Field.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CField::CField(std::string& sName, std::string& sAliasName, bool bIsEditable,	bool bIsNullable, bool bIsRquired, eDataTypes type,
                       int nLength, int nPrecision, int nScale, IGeometryDefinitionPtr ptrGeometry,   bool bIsPrimaryKey) :
                m_sName(sName), m_sAliasName(sAliasName), m_bIsEditable(bIsEditable), m_bIsNullable(bIsNullable), m_bIsRquired(bIsRquired), m_type(type),
                m_nLength(nLength), m_nPrecision(nPrecision), m_nScale(nScale), m_ptrGeometryDef(ptrGeometry),  m_bIsPrimaryKey(bIsPrimaryKey)
        {

        }
        CField::CField() : m_bIsEditable(true), m_bIsNullable(true), m_bIsRquired(false), m_type(dtUnknown), m_nLength(0), m_nPrecision(0), m_nScale(0), m_bIsPrimaryKey(false)

        {

        }

        CField::~CField()
        {

        }

        CField::CField(const CField& field)
        {
            Copy(&field);
        }

        CField& CField::operator=(const CField& field)
        {
            Copy(&field);
            return *this;
        }

        void CField::Copy(const CField *pField)
        {
            if(!pField)
                return;

            SetName(pField->GetName());
            SetAliasName(pField->GetAliasName());
            SetIsEditable(pField->GetIsEditable());
            SetIsNullable(pField->GetIsNullable());
            SetIsRequired(pField->GetIsRequired());
            SetType(pField->GetType());
            SetLength(pField->GetLength());
            SetPrecision(pField->GetPrecision());
            SetScale(pField->GetScale());

            if(pField->GetGeometryDef().get())
                SetGeometryDef(pField->GetGeometryDef()->Clone());

            SetIsPrimaryKey(pField->GetIsPrimaryKey());

        }

        const std::string& CField::GetName() const
        {
            return m_sName;
        }
        void   CField::SetName( const std::string& sName)
        {
            m_sName = sName;
        }
        const std::string&  CField::GetAliasName() const
        {
            return m_sAliasName;
        }
        void  CField::SetAliasName(const std::string&  sAliasName)
        {
            m_sAliasName = sAliasName;
        }
        bool  CField::GetIsEditable() const
        {
            return m_bIsEditable;
        }
        void  CField::SetIsEditable(bool bIsEditable)
        {
            m_bIsEditable = bIsEditable;
        }
        bool  CField::GetIsNullable() const
        {
            return m_bIsNullable;
        }
        void  CField::SetIsNullable(bool bIsNullable)
        {
            m_bIsNullable = bIsNullable;
        }
        bool  CField::GetIsRequired() const
        {
            return m_bIsRquired;
        }
        void  CField::SetIsRequired(bool bIsRquired)
        {
            m_bIsRquired = bIsRquired;
        }
        eDataTypes  CField::GetType() const
        {
            return m_type;
        }
        void  CField::SetType(eDataTypes type)
        {
            m_type = type;
        }
        int  CField::GetLength() const
        {
            return m_nLength;
        }
        void  CField::SetLength(int length)
        {
            m_nLength = length;
        }
        int        CField::GetPrecision() const
        {
            return m_nPrecision;
        }
        void  CField::SetPrecision(int precision)
        {
            m_nPrecision = precision;
        }
        int  CField::GetScale() const
        {
            return m_nScale;
        }
        void CField::SetScale(int scale)
        {
            m_nScale = scale;
        }



// IShapeField
        IGeometryDefinitionPtr  CField::GetGeometryDef() const
        {
            return m_ptrGeometryDef;
        }
        void  CField::SetGeometryDef(IGeometryDefinitionPtr ptrGeometryDef)
        {
            m_ptrGeometryDef = ptrGeometryDef;
        }


        bool  CField::GetIsPrimaryKey() const
        {
            return m_bIsPrimaryKey;
        }
        void   CField::SetIsPrimaryKey(bool bFlag)
        {
            m_bIsPrimaryKey = bFlag;
        }

        IFieldPtr	CField::Clone() const
        {
            ISpatialFieldPtr pField = std::make_shared<CField>();
            pField->SetName(GetName());
            pField->SetAliasName(GetAliasName());
            pField->SetIsEditable(GetIsEditable());
            pField->SetIsNullable(GetIsNullable());
            pField->SetIsRequired(GetIsRequired());
            pField->SetType(GetType());
            pField->SetLength(GetLength());
            pField->SetPrecision(GetPrecision());
            pField->SetScale(GetScale());

            pField->SetIsPrimaryKey(GetIsPrimaryKey());
            if(GetGeometryDef().get() != NULL)
                pField->SetGeometryDef(GetGeometryDef()->Clone());

            return pField;

        }

    }
}