#pragma once

#include "GeoDatabase.h"


namespace GraphEngine {
    namespace GeoDatabase {
        class CField : public ISpatialField
        {
        public:
            CField(std::string& sName, std::string& sAliasName, bool bIsEditable,	bool bIsNullable, bool bIsRquired, eDataTypes type,
                   int nLength, int nPrecision, int nScale, IGeometryDefinitionPtr ptrGeometry,  bool bIsPrimaryKey);

            CField();
            virtual ~CField();
            CField(const CField&);
            CField& operator=(const CField&);


            // IField
            virtual const std::string& GetName() const;
            virtual void                 SetName( const std::string& sName);
            virtual  const std::string&  GetAliasName() const;
            virtual void                 SetAliasName(const   std::string&  sAliasName);
            virtual bool                 GetIsEditable() const;
            virtual void                 SetIsEditable(bool bIsEditable);
            virtual bool                 GetIsNullable() const;
            virtual void                 SetIsNullable(bool bIsNullable);
            virtual bool                 GetIsRequired() const;
            virtual void                 SetIsRequired(bool bIsRequired);
            virtual eDataTypes			 GetType() const;
            virtual void                 SetType(eDataTypes type);
            virtual int                  GetLength() const;
            virtual void                 SetLength(int length);
            virtual int                  GetPrecision() const;
            virtual void                 SetPrecision(int precision);
            virtual int                  GetScale() const;
            virtual void                 SetScale(int scale);
            virtual bool                 GetIsPrimaryKey() const;
            virtual void                 SetIsPrimaryKey(bool flag);

            // IShapeField
            virtual IGeometryDefinitionPtr  GetGeometryDef() const;
            virtual void            SetGeometryDef(IGeometryDefinitionPtr ptrGeometryDef);
            virtual IFieldPtr		 Clone() const;
        private:
            void Copy(const CField *pField);
        private:
            std::string		m_sName;
            std::string		m_sAliasName;
            bool					m_bIsEditable;
            bool					m_bIsNullable;
            bool					m_bIsRquired;
            eDataTypes				m_type;
            int						m_nLength;
            int						m_nPrecision;
            int						m_nScale;
            IGeometryDefinitionPtr  m_ptrGeometryDef;
            bool					m_bIsPrimaryKey;

        };
    }
}