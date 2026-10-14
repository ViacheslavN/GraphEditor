#pragma once

#include "../GeoDatabase.h"
#include "../WorkspaceBase.h"
#include "BPtreeDBContext.h"

namespace GraphEngine {
    namespace GeoDatabase {

        // Workspace on a bptreedb database (bptreedb/DB/bptreedb.h).
        //
        // Differences from the SQLite workspace:
        //  - the field types are kept exactly: int8..uint64, float, double (SQLite: INTEGER, REAL);
        //    dtGuid is stored as a string, dtDate isn't supported (as in SQLite);
        //  - a string field without a length gets GetDefaultStringLength() bytes (UTF-8), a field with a length
        //    of n characters - 4 * n bytes: bptreedb checks the length on insert;
        //  - two kinds of spatial index: R-tree (default) and Z-order (needs the extent of the data); the kind is
        //    chosen per table (CreateTableWithSpatialIndex with SBPtreeDBSpatialIndexParams) or by the default of
        //    the workspace (SetDefaultSpatialIndex). A Z-order table without an extent gets an R-tree, except in
        //    the bulk load mode: there the index is built at the end and its grid covers the inserted shapes;
        //  - the bulk load mode: the write transactions are DDL ones (exclusive, tables can be created in them),
        //    the spatial index and the unique index of the primary key of the tables created in it are built once
        //    by IInsertCursor::Finish / the commit (sorted keys, packed nodes) instead of row by row;
        //  - one write transaction at a time (bptreedb): a second StartTransaction of a write transaction throws
        //    instead of blocking.
        class CBPtreeDBWorkspace : public IWorkspaceBase<IDatabaseWorkspace>
        {
            CBPtreeDBWorkspace();
            CBPtreeDBWorkspace(const char *pszName, const char *pszDatabasePath, CommonLib::CGuid id, CBPtreeDBContextPtr ptrContext);
        public:
            typedef  IWorkspaceBase<IDatabaseWorkspace> TBase;

            virtual ~CBPtreeDBWorkspace();

            static IDatabaseWorkspacePtr Open(const char *pszName, const char *pszPath, CommonLib::CGuid id,
                                              const bptreedb::SDatabaseParams& params = bptreedb::SDatabaseParams());
            static IDatabaseWorkspacePtr Open(CommonLib::ISerializeObjPtr pObj);
            // creates a new database (an existing file is overwritten)
            static IDatabaseWorkspacePtr Create(const char *pszName, const char *pszPath, CommonLib::CGuid id,
                                                const bptreedb::SDatabaseParams& params = bptreedb::SDatabaseParams());

            virtual ITablePtr CreateTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields);
            virtual ITablePtr CreateTableWithSpatialIndex(const std::string& name,
                                                        const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                        CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference);
            // with the kind of the spatial index
            ITablePtr CreateTableWithSpatialIndex(const std::string& name,
                                                  const std::string& viewName,  const std::string& spatialIndexName, const std::string& shapeFieldName, const std::string& sOIDFieldName, IFieldsPtr ptrFields,
                                                  CommonLib::eShapeType shapeType, Geometry::IEnvelopePtr  ptrExtent, Geometry::ISpatialReferencePtr ptrSpatialReference,
                                                  const SBPtreeDBSpatialIndexParams& spatialIndex);
            virtual ITransactionPtr StartTransaction(eTransactionType type);
            virtual void SetBulkLoadMode(bool bBulk);
            // the rows of the next write transactions come in the order of the indexed values (an export, a sorted
            // load): full index leaves (bptreedb ITransaction::SetInsertOnRight); off - random inserts (editing)
            void SetInsertOnRight(bool bOnRight);

            virtual void Save(CommonLib::ISerializeObjPtr ptrObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr ptrObj);

            // names of the spatial tables of the database (they are also loaded as datasets on open)
            std::vector<std::string> GetSpatialTableNames() const;

            // the spatial index of the new spatial tables (R-tree by default)
            void SetDefaultSpatialIndex(const SBPtreeDBSpatialIndexParams& params);
            SBPtreeDBSpatialIndexParams GetDefaultSpatialIndex() const;
            // UTF-8 bytes of a new string field without a length, 0 - unlimited (stored like blobs)
            void SetDefaultStringLength(uint32_t nLength);
            uint32_t GetDefaultStringLength() const;

            // the kind of the spatial index of a table (bsitRTree / bsitZOrder), false - the table has none
            bool GetSpatialIndexType(const std::string& sTable, eBPtreeDBSpatialIndexType& type) const;

            // the database itself: the bptreedb SDK (SQL, statistics...)
            bptreedb::IDatabasePtr GetDatabase() const;
            const std::string& GetDatabasePath() const { return m_sDatabasePath; }

        protected:
            virtual IDatasetPtr LoadDataset(const std::string& sName);
            virtual IDatasetPtr LoadTable(const std::string& sName);
            virtual IDatasetPtr LoadSpatialTable(const std::string& sName);

        private:
            void LoadSpatialTables();
            static CBPtreeDBContextPtr OpenDatabase(const std::string& sPath, bool bCreate, const bptreedb::SDatabaseParams& params);

        private:

            static std::string m_DatabasePathProps;

            std::string m_sDatabasePath;
            CBPtreeDBContextPtr m_ptrContext;
        };

    }
}
