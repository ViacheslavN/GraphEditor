#pragma once

#include "../GeoDatabase.h"
#include "../CursorBase.h"
#include "../../CommonLib/sqlitelib/Database.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteSelectCursor : public ICursorBase<ISelectCursor>
        {
        public:
            typedef ICursorBase<ISelectCursor> TBase;

            CSQLiteSelectCursor(const std::string& sQuery, CommonLib::database::IDatabasePtr ptrDatabase);
            CSQLiteSelectCursor(const std::string& sTableName, IFieldsPtr  ptrSourceFields, IQueryFilterPtr ptrFilter, CommonLib::database::IDatabasePtr ptrDatabase);
            CSQLiteSelectCursor(const std::string& sTableName, IFieldsPtr  ptrSourceFields, IQueryFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrSpatRefSource, CommonLib::database::IStatmentPtr ptrDatabase);


            virtual bool Next();
            virtual bool ColumnIsNull(int32_t col) const;
            virtual int8_t ReadInt8(int32_t col) const;
            virtual uint8_t ReadUInt8(int32_t col) const ;
            virtual int16_t ReadInt16(int32_t col) const ;
            virtual uint16_t ReadUInt16(int32_t col) const ;
            virtual int32_t ReadInt32(int32_t col) const;
            virtual uint32_t ReadUInt32(int32_t col) const;
            virtual int64_t ReadInt64(int32_t col) const;
            virtual uint64_t ReadUInt64(int32_t col) const;
            virtual float ReadFloat(int32_t col) const;
            virtual double ReadDouble(int32_t col) const;
            virtual void ReadText(int32_t col, std::string& text) const;
            virtual std::string ReadText(int32_t col) const;
            virtual void ReadTextW(int32_t col, std::wstring& text) const;
            virtual std::wstring ReadTextW(int32_t col) const;
            virtual void ReadBlob(int col, byte_t **pBuf, int32_t& size) const;
            virtual CommonLib::IGeoShapePtr ReadShape(int32_t col) const;
        protected:
            std::string CreateSQLQuery(IFieldsPtr  ptrSourceFields, IQueryFilterPtr ptrFilter, const std::string& sTableName, const std::string& sSpatialIndex);
        private:
            CommonLib::database::IStatmentPtr m_ptrStatment;
            mutable CommonLib::IGeoShapePtr  m_ptrCacheShape;


        };

    }
    }