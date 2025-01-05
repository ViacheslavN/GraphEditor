#pragma once
#include "../CommonLib/CommonLib.h"
#include "../CommonLib/SpatialData/IGeoShape.h"
#include "../GisGeometry/Geometry.h"
#include "../CommonLib/utils/PropertySet.h"
#include "../CommonLib/Serialize/SerializeObj.h"
#include "../CommonLib/exception/exc_base.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        enum eWorkspaceType {
            wtUndefined = 0,
            wtShapeFile,
            wtSqlLite,
            wtOracle,
            wtMSSQL,
            wtPostGIS,
            wtRaster
        };

        enum eDatasetType
        {
            dtUndefined    = 0,
            dtTypeTable    = 1,
            dtSpatialTable = 2,
            dtGroupDataset = 4,
            dtTypeRaster   = 8,
            dtTypeAny      = -1
        };

        enum eDataTypes {
            dtUnknown = 0,
            dtNull,
            dtInteger8,
            dtInteger16,
            dtInteger32,
            dtInteger64,
            dtUInteger8,
            dtUInteger16,
            dtUInteger32,
            dtUInteger64,
            dtFloat,
            dtDouble,
            dtString,
            dtWString,
            dtBlob,
            dtRaster,
            dtDate,
            dtGeometry,
            dtSerializedObject,
            dtAnnotation
        };

        enum eSpatialRel {
            srlUndefined = 0,
            srlIntersects = 1,
            srlTouches = 2,
            srlOverlaps = 3,
            srlCrosses = 4,
            srlWithin = 5,
            srlContains = 6,
            srlRelation = 7
        };

        enum eTransactionType {
            ttUndefined = 1,
            ttSelect = 2,
            ttModify = 4,
            ttDDL = 8
        };



        typedef std::shared_ptr<class IWorkspace> IWorkspacePtr;
        typedef std::shared_ptr<class IDatabaseWorkspace> IDatabaseWorkspacePtr;
        typedef std::shared_ptr<class ITransaction> ITransactionPtr;
        typedef std::shared_ptr<class ICursor> ICursorPtr;
        typedef std::shared_ptr<class IInsertCursor> IInsertCursorPtr;
        typedef std::shared_ptr<class IUpdateCursor> IUpdateCursorPtr;
        typedef std::shared_ptr<class IDeleteCursor> IDeleteCursorPtr;
        typedef std::shared_ptr<class IDataset> IDatasetPtr;
        typedef std::shared_ptr<class ITable> ITablePtr;
        typedef std::shared_ptr<class ISpatialTable> ISpatialTablePtr;
        typedef std::shared_ptr<class IRow> IRowPtr;
        typedef std::shared_ptr<class IField> IFieldPtr;
        typedef std::shared_ptr<class ISpatialField> ISpatialFieldPtr;
        typedef std::shared_ptr<class IOIDSet>  IOIDSetPtr;
        typedef std::shared_ptr<class IFields> IFieldsPtr;
        typedef std::shared_ptr<class IFieldSet> IFieldSetPtr;
        typedef std::shared_ptr<class IQueryFilter> IQueryFilterPtr;
        typedef std::shared_ptr<class ISpatialFilter> ISpatialFilterPtr;
        typedef std::shared_ptr<class IGeometryDefinition> IGeometryDefinitionPtr;



        class ITransaction {
        public:
            ITransaction() {}
            virtual ~ ITransaction() {}

            virtual bool Commit() = 0;
            virtual bool Rollback() = 0;
            virtual void GetError(std::string &sText) = 0;
            virtual IInsertCursorPtr CreateInsertCusor(ITablePtr pTable, IFieldSetPtr ptrFileds = nullptr) = 0;
            virtual IUpdateCursorPtr CreateUpdateCusor(ITablePtr pTable, IFieldSetPtr ptrFileds = nullptr, const std::string& whereClause = "") = 0;
            virtual IDeleteCursorPtr CreateDeleteCusor(ITablePtr pTable, IFieldSetPtr ptrFileds = nullptr) = 0;
            virtual void Executequery(const std::string& sqlQuery) = 0;

        };


        class IWorkspace : public CommonLib::ISerialize
        {
        public:
            IWorkspace(){}
            virtual ~IWorkspace(){}
            virtual eWorkspaceType GetWorkspaceType() const = 0;
            virtual uint32_t GetDatasetCount() const = 0;
            virtual IDatasetPtr GetDataset(uint32_t nIdx) const = 0;
            virtual void RemoveDataset(uint32_t nIdx) = 0;
            virtual void RemoveDataset(IDatasetPtr ptrDataset) = 0;
            virtual int32_t GetID() const = 0;
        };


        class IDatabaseWorkspace : public IWorkspace
        {
        public:
            IDatabaseWorkspace(){}
            virtual ~IDatabaseWorkspace(){}
            virtual ITransactionPtr StartTransaction(eTransactionType type) = 0;

            virtual ITablePtr CreateTable(const std::string& name, const std::string& viewName, IFieldsPtr ptrFields) = 0;
            virtual ITablePtr GetTable(const std::string& name) = 0;
            virtual ISpatialTablePtr CreateSpatialTable(const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields, const std::string& shapeFiledName, const std::string& spatialIndexName = "") = 0;
            virtual ISpatialTablePtr GetSpatialTable(const std::string& name) = 0;

        };


        class IRow
        {
        public:
            IRow(){}
            virtual ~IRow(){}

            virtual int32_t  ColumnCount() const = 0;
            virtual std::string ColumnName(int32_t col) const = 0;
            virtual bool ColumnIsNull(int32_t col) const = 0;
            virtual eDataTypes GetColumnType(int32_t col) const = 0;
            virtual int32_t GetColumnBytes(int32_t col) const = 0;
            virtual int64_t  GetRowId() const = 0;

            virtual int8_t ReadInt8(int32_t col) const = 0;
            virtual uint8_t ReadUInt8(int32_t col) const = 0;
            virtual int16_t ReadInt16(int32_t col) const = 0;
            virtual uint16_t ReadUInt16(int32_t col) const = 0;
            virtual int32_t ReadInt32(int32_t col) const = 0;
            virtual uint32_t ReadUInt32(int32_t col) const = 0;
            virtual int64_t ReadInt64(int32_t col) const = 0;
            virtual uint64_t ReadUInt64(int32_t col) const = 0;
            virtual float ReadFloat(int32_t col) const = 0;
            virtual double ReadDouble(int32_t col) const = 0;
            virtual void ReadText(int32_t col, std::string& text) const = 0;
            virtual std::string ReadText(int32_t col) const = 0;
            virtual void ReadTextW(int32_t col, std::wstring& text) const = 0;
            virtual std::wstring ReadTextW(int32_t col) const = 0;
            virtual void ReadBlob(int col, byte_t **pBuf, int32_t& size) const = 0;
            virtual CommonLib::IGeoShapePtr ReadShape(int32_t col) const = 0;

            virtual void BindInt8(int32_t col, int8_t val) = 0;
            virtual void BindUInt8(int32_t col, uint8_t val) = 0;
            virtual void BindInt16(int32_t col, int16_t val) = 0;
            virtual void BindUInt16(int32_t col, uint16_t val) = 0;
            virtual void BindInt32(int32_t col, int32_t val) = 0;
            virtual void BindUInt32(int32_t col, uint32_t val) = 0;
            virtual void BindInt64(int32_t col, int64_t val) = 0;
            virtual void BindUInt64(int32_t col, uint64_t val) = 0;
            virtual void BindFloat(int32_t col, float val)  = 0;
            virtual void BindDouble(int32_t col, double val) = 0;
            virtual void BindText(int32_t col, const std::string& text, bool copy) = 0;
            virtual void BindTextW(int32_t col, const std::wstring& text, bool copy) = 0;
            virtual void BindBlob(int32_t col, const byte_t *pBuf, int32_t size, bool copy) = 0;
            virtual void BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape) = 0;
        };



        class IField
        {
        public:
            IField(){}
            virtual ~IField(){}
            virtual const std::string& GetName() const = 0;
            virtual void                 SetName( const std::string& name) = 0;
            virtual  const std::string&  GetAliasName() const = 0;
            virtual void                 SetAliasName(const  std::string&  name) = 0;
            virtual bool                 GetIsEditable() const = 0;
            virtual void                 SetIsEditable(bool flag) = 0;
            virtual bool                 GetIsNullable() const = 0;
            virtual void                 SetIsNullable(bool flag) = 0;
            virtual bool                 GetIsRequired() const = 0;
            virtual void                 SetIsRequired(bool flag) = 0;
            virtual eDataTypes			 GetType() const = 0;
            virtual void                 SetType(eDataTypes type) = 0;
            virtual int                  GetLength() const = 0;
            virtual void                 SetLength(int length) = 0;
            virtual int                  GetPrecision() const = 0;
            virtual void                 SetPrecision(int precision) = 0;
            virtual int                  GetScale() const = 0;
            virtual void                 SetScale(int scale) = 0;
            virtual bool                 GetIsPrimaryKey() const = 0;
            virtual void                 SetIsPrimaryKey(bool flag) = 0;
            virtual IFieldPtr			 Clone() const = 0;
        };

        class ISpatialField : public IField
        {
        public:
            ISpatialField(){}
            virtual ~ISpatialField(){}

            virtual IGeometryDefinitionPtr  GetGeometryDef() const = 0;
            virtual void            SetGeometryDef(IGeometryDefinitionPtr ptrDef) = 0;
        };


        class  IGeometryDefinition
        {
        public:
            IGeometryDefinition(){}
            virtual ~IGeometryDefinition(){}

            virtual  CommonLib::eShapeType			  GetGeometryType() const = 0;
            virtual void                              SetGeometryType(CommonLib::eShapeType type) = 0;
            virtual bool                              GetHasZ() const = 0;
            virtual void                              SetHasZ(bool flag) = 0;
            virtual bool                              GetHasM() const = 0;
            virtual void                              SetHasM(bool flag) = 0;
            virtual Geometry::ISpatialReferencePtr    GetSpatialReference() const = 0;
            virtual void                              SetSpatialReference(Geometry::ISpatialReferencePtr ptrRef) = 0;
            virtual CommonLib::bbox					  GetBaseExtent() const = 0;
            virtual void                              SetBaseExtent(const CommonLib::bbox& box) = 0;
            virtual IGeometryDefinitionPtr			  Clone() const = 0;
        };


        class IDataset : public CommonLib::ISerialize
        {
        public:
            IDataset(){}
            virtual ~IDataset(){}
            virtual eDatasetType  GetDatasetType() const = 0;
            virtual const std::string&   GetDatasetName() const = 0;
            virtual const std::string&   GetDatasetViewName() const = 0;

        };

        class ITable : public IDataset
        {
        public:
            ITable(){}
            virtual ~ITable(){}
            virtual void						 AddField(IFieldPtr field) = 0;
            virtual void						 DeleteField(const std::string& fieldName) = 0;
            virtual IFieldsPtr					 GetFields() const  = 0;
            virtual void						 SetFields(IFieldsPtr ptrFields)  = 0;
            virtual bool						 HasOIDField() const = 0;
            virtual void						 SetHasOIDField(bool bFlag) = 0;
            virtual const std::string&      	 GetOIDFieldName() const = 0;
            virtual void						 SetOIDFieldName(const std::string& sOIDFieldName) = 0;
            virtual IRowPtr						 GetRow(int64_t id) = 0;
            virtual ICursorPtr					 Search(IQueryFilterPtr filter) = 0;
        };

        class ISpatialTable : public ITable
        {
        public:
            ISpatialTable(){}
            virtual ~ISpatialTable(){}
            virtual CommonLib::eShapeType				 GetGeometryType() const = 0;
            virtual	void								 SetGeometryType(CommonLib::eShapeType shapeType)	= 0;
            virtual const std::string&			         GetShapeFieldName() const = 0;
            virtual void						         SetShapeFieldName(const std::string& sName)  = 0;
            virtual bool								 GetIsAnnoClass() const = 0;
            virtual const std::string&			         GetAnnoFieldName() const = 0;
            virtual void								 SetIsAnnoClass(bool bAnno) = 0;
            virtual void								 SetAnnoFieldName(const std::string& sAnnoName) = 0;
            virtual Geometry::IEnvelopePtr			     GetExtent() const = 0;
            virtual Geometry::ISpatialReferencePtr	     GetSpatialReference() const = 0;
            virtual void								 SetExtent(Geometry::IEnvelopePtr pEnvelope)  = 0;
            virtual void								 SetSpatialReference(Geometry::ISpatialReferencePtr pSpatRef)  = 0;
        };


        class  IOIDSet
        {
        public:

            IOIDSet(){}
            virtual ~IOIDSet(){}
            virtual int  GetCount() const = 0;
            virtual bool Find(int64_t id) const = 0;
            virtual void Reset() = 0;
            virtual bool Next(int64_t& id) = 0;
            virtual void Add(int64_t id) = 0;
            virtual void AddList(const int64_t* id, int count) = 0;
            virtual void Remove(int id) = 0;
            virtual void RemoveList(const int64_t* id, int count) = 0;
            virtual void Clear() = 0;
        };


        class IFields
        {
        public:
            IFields(){}
            virtual ~IFields(){}
            virtual int       GetFieldCount() const = 0;
            virtual void      SetFieldCount(int count) = 0;
            virtual IFieldPtr GetField(int index) const = 0;
            virtual IFieldPtr  GetField(const std::string& name) const = 0;
            virtual void      SetField(int index, IFieldPtr field) = 0;
            virtual void      AddField(IFieldPtr field) = 0;
            virtual void      RemoveField(int index) = 0;
            virtual int       FindField(const std::string& name) const = 0;
            virtual bool      FieldExists(const std::string& name) const = 0;
            virtual void	  Clear() = 0;
            virtual IFieldsPtr		Clone() const = 0;
        };


        class IFieldSet
        {
        public:

            IFieldSet(){}
            virtual ~IFieldSet(){}
            virtual int  GetCount() const = 0;
            virtual int  Find(const std::string& field) const = 0;
            virtual void Reset() = 0;
            virtual bool Next(std::string& field) = 0;
            virtual void Add(const std::string& field) = 0;
            virtual void Remove(const std::string& field) = 0;
            virtual	const std::string& Get(int nIndex) const = 0;
            virtual void Clear() = 0;
        };

        class IQueryFilter
        {
        public:
            IQueryFilter(){}
            virtual ~IQueryFilter(){}
            virtual IFieldSetPtr                        GetFieldSet() const = 0;
            virtual Geometry::ISpatialReferencePtr      GetOutputSpatialReference() const = 0;
            virtual void								SetOutputSpatialReference(Geometry::ISpatialReferencePtr spatRef) = 0;
            virtual const std::string&                  GetWhereClause() const = 0;
            virtual void								SetWhereClause(const std::string& where) = 0;
        };

        class ISpatialFilter : public IQueryFilter
        {
        public:
            ISpatialFilter(){}
            virtual ~ISpatialFilter(){}
            virtual const std::string&    GetShapeField() const = 0;
            virtual void						 SetShapeField(const std::string& name) = 0;
            virtual CommonLib::IGeoShapePtr		 GetShape() const = 0;
            virtual void						 SetShape( CommonLib::IGeoShapePtr ptrShape) = 0;
            virtual CommonLib::bbox				 GetBB() const = 0;
            virtual void						 SetBB(const  CommonLib::bbox& bbox ) = 0;
            virtual double						 GetPrecision() const = 0;
            virtual void						 SetPrecision(double precision) = 0;
            virtual eSpatialRel					 GetSpatialRel() const = 0;
            virtual void						 SetSpatialRel(eSpatialRel rel) = 0;
        };


        class ICursor
        {
        public:
            ICursor(){}
            virtual ~ICursor(){}
            virtual bool NextRow() = 0;
            virtual IRowPtr GetCurrentRow() = 0;
        };

        class  IInsertCursor
        {
        public:
            IInsertCursor(){}
            virtual ~IInsertCursor(){}
            virtual int64_t InsertRow(IRowPtr pRow) = 0;
        };

        class  IUpdateCursor : public ICursor
        {
        public:
            IUpdateCursor(){}
            virtual ~IUpdateCursor(){}
            virtual void UpdateRow(IRowPtr pRow) = 0;
        };

        class IDeleteCursor
        {
        public:
            IDeleteCursor(){}
            virtual ~IDeleteCursor(){}
            virtual void DeleteRow(int64_t oid) = 0;
        };


    }

}