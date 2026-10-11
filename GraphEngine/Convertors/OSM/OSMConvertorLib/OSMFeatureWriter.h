#pragma once
#include "OSMConvertorLib.h"
#include "OSMSchema.h"
#include "OSMTableWriter.h"
#include "OSMTags.h"
#include "OSMGeometry.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// The writing of the result tables, shared by the conversion of an OSM file (OSMConvertSession) and
// the export of the split files (OSMSplitExport): the feature tables of the layers, the routes / restrictions,
// the layers added to the map.

namespace GraphEngine {
    namespace Convertors {

        typedef std::vector<std::pair<std::string, std::string> > TOSMTagList;

        // the tags of a stored object as readosm tags (for COSMTags)
        class COSMStoredTags
        {
        public:
            COSMStoredTags() {}
            explicit COSMStoredTags(const TOSMTagList& tags) { Set(tags); }
            void Set(const TOSMTagList& tags)
            {
                m_tags.clear();
                m_tags.reserve(tags.size());
                for(size_t i = 0; i < tags.size(); ++i)
                {
                    readosm_tag tag = {tags[i].first.c_str(), tags[i].second.c_str()};
                    m_tags.push_back(tag);
                }
            }
            COSMTags Tags() const { return COSMTags(m_tags.empty() ? nullptr : &m_tags[0], (int)m_tags.size()); }
            const readosm_tag* Data() const { return m_tags.empty() ? nullptr : m_tags.data(); }

        private:
            std::vector<readosm_tag> m_tags;
        };

        // a member of a relation: n / w / r
        struct SOSMMember
        {
            char        cType;
            int64_t     nId;
            const char* pszRole;
        };

        inline char OSMMemberTypeChar(int nReadosmType)
        {
            return nReadosmType == READOSM_MEMBER_NODE ? 'n' : (nReadosmType == READOSM_MEMBER_WAY ? 'w' : 'r');
        }

        inline bool OSMParseInteger(const char* pszValue, int64_t& nValue)
        {
            if(!pszValue)
                return false;
            while(*pszValue == ' ')
                ++pszValue;
            const char* p = pszValue;
            if(*p == '-' || *p == '+')
                ++p;
            if(*p < '0' || *p > '9')
                return false;
            nValue = strtoll(pszValue, nullptr, 10);
            return true;
        }

        inline bool OSMIsYes(const char* pszValue)
        {
            return pszValue && (strcmp(pszValue, "yes") == 0 || strcmp(pszValue, "true") == 0 || strcmp(pszValue, "1") == 0);
        }

        inline char OSMMemberType(int nType)
        {
            switch(nType)
            {
                case READOSM_MEMBER_NODE: return 'n';
                case READOSM_MEMBER_WAY:  return 'w';
                default:                  return 'r';
            }
        }

        // a feature table of a thematic layer, its columns (IOSMLayer::GetColumn) without the excluded ones:
        // osm_id, osm_type, name, class (the key), type (the value), the fields of the schema, the OSM keys (text)
        class COSMFeatureWriter
        {
        public:
            COSMFeatureWriter(const SOSMLayerDef& def, const IOSMLayer& layer, const CLayerColumnExcludeList& excludeList, const std::string& sNameKey,
                           GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace, GeoDatabase::ITransactionPtr ptrTransaction,
                           Geometry::ISpatialReferencePtr ptrSpatRef, std::shared_ptr<GeometryCompression::CShapeCompressor> ptrCompressor,
                           const std::string& sTempDir) :
                    m_sNameKey(sNameKey)
            {
                // a map made without the columns (saved by an old version): the columns of the schema
                std::vector<SOSMColumn> columns;
                for(int i = 0; i < layer.GetColumnCount(); ++i)
                    columns.push_back(layer.GetColumn(i));
                if(columns.empty())
                    columns = COSMSchema::LayerColumns(def);

                std::vector<COSMTableWriter::SField> vecFields;
                for(size_t i = 0; i < columns.size(); ++i)
                {
                    const SOSMColumn& column = columns[i];
                    if(!column.bRequired && excludeList.IsColumnExcluded(layer.GetName(), column.sName))
                        continue;

                    int nField = (int)vecFields.size();
                    if(column.sKey.empty())
                    {
                        // the columns of the converter
                        if(column.sName == COSMSchema::ColumnOsmId)
                            m_nIdField = nField;
                        else if(column.sName == COSMSchema::ColumnOsmType)
                            m_nTypeField = nField;
                        else if(column.sName == COSMSchema::ColumnName)
                            m_nNameField = nField;
                        else if(column.sName == COSMSchema::ColumnClass)
                            m_nClassField = nField;
                        else if(column.sName == COSMSchema::ColumnType)
                            m_nValueField = nField;
                        else
                            continue;   // unknown: no column
                    }
                    else
                    {
                        // a field of the schema has its kind (integer, bool ...), the other OSM keys are text
                        const SOSMFieldDef* pField = nullptr;
                        for(size_t f = 0; f < def.vecFields.size() && !pField; ++f)
                        {
                            if(def.vecFields[f].sName == column.sName && def.vecFields[f].sKey == column.sKey)
                                pField = &def.vecFields[f];
                        }
                        if(pField)
                            m_vecSchemaFields.push_back(SSchemaField{nField, pField->sKey, pField->kind});
                        else
                            m_mapKeyFields[column.sKey] = nField;
                    }
                    vecFields.push_back({column.sName, column.bInteger ? GeoDatabase::dtInteger64 : GeoDatabase::dtString});
                }

                CommonLib::eShapeType shapeType = def.geometryType == OSMGeometryPoint ? CommonLib::shape_type_point :
                                                  def.geometryType == OSMGeometryLine ? CommonLib::shape_type_polyline : CommonLib::shape_type_polygon;
                m_ptrWriter = std::make_shared<COSMTableWriter>(ptrWorkspace, ptrTransaction, layer.GetTableName(), vecFields, shapeType, ptrSpatRef);
                m_ptrWriter->SetCompressor(ptrCompressor);
                // packed once at the end, much faster than row by row; up to 512 MB of boxes per layer in memory
                // (the buffer grows with the layer: the planet buildings make ~40 sorted runs instead of ~320)
                m_ptrWriter->SetBulkSpatialIndex(sTempDir, 512ull * 1024 * 1024);
            }

            void Write(int64_t nId, char cType, const SOSMMatch& match, const COSMTags& tags, CommonLib::IGeoShapePtr ptrShape)
            {
                SetFields(nId, cType, match, tags);
                m_ptrWriter->SetShape(ptrShape);
                m_ptrWriter->Insert();
            }

            // the geometry is a compressed blob (the parallel ways pass)
            void WriteBlob(int64_t nId, char cType, const SOSMMatch& match, const COSMTags& tags, const uint8_t* pBlob, size_t nBlobSize)
            {
                SetFields(nId, cType, match, tags);
                m_ptrWriter->SetShapeBlob(pBlob, nBlobSize);
                m_ptrWriter->Insert();
            }

            void SetFields(int64_t nId, char cType, const SOSMMatch& match, const COSMTags& tags)
            {
                if(m_nIdField >= 0)
                    m_ptrWriter->SetInt64(m_nIdField, nId);
                if(m_nTypeField >= 0)
                    m_ptrWriter->SetText(m_nTypeField, std::string(1, cType));
                if(m_nNameField >= 0)
                {
                    const char* pszName = m_sNameKey.empty() ? nullptr : tags.Get(m_sNameKey.c_str());
                    if(!pszName || !*pszName)
                        pszName = tags.Get("name");
                    if(pszName && *pszName)
                        m_ptrWriter->SetText(m_nNameField, pszName);
                }
                if(m_nClassField >= 0)
                    m_ptrWriter->SetText(m_nClassField, match.sKey);
                if(m_nValueField >= 0)
                    m_ptrWriter->SetText(m_nValueField, match.sValue);

                for(size_t i = 0; i < m_vecSchemaFields.size(); ++i)
                {
                    const SSchemaField& field = m_vecSchemaFields[i];
                    const char* pszValue = tags.Get(field.sKey.c_str());
                    switch(field.kind)
                    {
                        case OSMFieldText:
                            if(pszValue && *pszValue)
                                m_ptrWriter->SetText(field.nField, pszValue);
                            break;
                        case OSMFieldInteger:
                        {
                            int64_t nValue = 0;
                            if(OSMParseInteger(pszValue, nValue))
                                m_ptrWriter->SetInt64(field.nField, nValue);
                            break;
                        }
                        case OSMFieldBool:
                            m_ptrWriter->SetInt64(field.nField, OSMIsYes(pszValue) ? 1 : 0);
                            break;
                        case OSMFieldOneway:
                            m_ptrWriter->SetInt64(field.nField, OSMIsYes(pszValue) ? 1 :
                                    (pszValue && (strcmp(pszValue, "-1") == 0 || strcmp(pszValue, "reverse") == 0)) ? -1 : 0);
                            break;
                    }
                }

                // the OSM key columns: one lookup per tag of the object
                if(!m_mapKeyFields.empty())
                {
                    for(int i = 0; i < tags.Count(); ++i)
                    {
                        const char* pszKey = tags.Key(i);
                        const char* pszValue = tags.Value(i);
                        if(!pszKey || !pszValue || !*pszValue)
                            continue;
                        m_sKey.assign(pszKey);
                        auto it = m_mapKeyFields.find(m_sKey);
                        if(it != m_mapKeyFields.end())
                            m_ptrWriter->SetText(it->second, pszValue);
                    }
                }
            }

            COSMTableWriterPtr Writer() const { return m_ptrWriter; }

        private:
            struct SSchemaField
            {
                int           nField;
                std::string   sKey;
                eOSMFieldKind kind;
            };

            std::string         m_sNameKey;
            int                 m_nIdField = -1;
            int                 m_nTypeField = -1;
            int                 m_nNameField = -1;
            int                 m_nClassField = -1;
            int                 m_nValueField = -1;
            std::vector<SSchemaField> m_vecSchemaFields;
            std::unordered_map<std::string, int> m_mapKeyFields;   // OSM key -> field
            std::string         m_sKey;
            COSMTableWriterPtr  m_ptrWriter;
        };

        // a table writer with the columns of COSMSchema::TableColumns, the excluded ones aren't created
        // (the values set for them are ignored)
        inline COSMTableWriterPtr OSMCreateTableWriter(GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace, GeoDatabase::ITransactionPtr ptrTransaction,
                                             const IOSMTable& table, const CLayerColumnExcludeList& excludeList)
        {
            std::vector<SOSMColumn> columns = COSMSchema::TableColumns(table.GetTableType());
            std::vector<COSMTableWriter::SField> vecFields;
            for(size_t i = 0; i < columns.size(); ++i)
            {
                COSMTableWriter::SField field;
                field.sName = columns[i].sName;
                field.type = columns[i].bInteger ? GeoDatabase::dtInteger64 : GeoDatabase::dtString;
                field.bExcluded = !columns[i].bRequired && excludeList.IsColumnExcluded(table.GetName(), columns[i].sName);
                vecFields.push_back(field);
            }
            return std::make_shared<COSMTableWriter>(ptrWorkspace, ptrTransaction, table.GetTableName(), vecFields);
        }

        typedef std::shared_ptr<COSMFeatureWriter> COSMFeatureWriterPtr;

        // the indexes of the columns of the overview layers of the layer (COSMMapStyle::OverviewLayers): a small scale
        // layer reads only its rows; in the transaction of the export, returns the seconds
        double OSMCreateOverviewIndexes(GeoDatabase::ITransactionPtr ptrTransaction, const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable);

        // routes: osm_id, route, name, ref, network, operator, colour, route_from, route_to; members: route_id, seq, member_type, member_id, role
        void OSMWriteRoute(COSMTableWriter& writer, COSMTableWriter& members, int64_t nId, const COSMTags& tags, const char* pszRoute,
                           const std::string& sNameLanguage, const SOSMMember* pMembers, int nMembers);
        // restrictions: osm_id, restriction, from_way, via_type, via_id, to_way, except_for
        void OSMWriteRestriction(COSMTableWriter& writer, int64_t nId, const COSMTags& tags, const char* pszRestriction,
                                 const SOSMMember* pMembers, int nMembers);

        // the converted layers are added to the map in the cartographic order (COSMMapStyle::LayerRank)
        class COSMMapLayers
        {
        public:
            COSMMapLayers(Cartography::IMapPtr ptrMap, const COSMProjection& projection) : m_ptrMap(ptrMap), m_projection(projection) {}
            // ptrStyle - a feature layer saved with the symbols (the split metadata), null - the default style
            // the layer and its small scale layers (COSMMapStyle::OverviewLayers)
            void AddLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable, CommonLib::ISerializeObjPtr ptrStyle = CommonLib::ISerializeObjPtr());

        private:
            Cartography::IFeatureLayerPtr CreateLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable, CommonLib::ISerializeObjPtr ptrStyle) const;
            void Insert(Cartography::ILayerPtr ptrLayer, int nRank, int nAfter);

            Cartography::IMapPtr m_ptrMap;
            COSMProjection       m_projection;
            std::vector<std::pair<int, Cartography::ILayerPtr> > m_vecLayers;   // added layers with their rank
        };
    }
}
