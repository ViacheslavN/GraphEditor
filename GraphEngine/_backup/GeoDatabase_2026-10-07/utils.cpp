#include "Utils.h"

namespace GraphEngine {
    namespace GeoDatabase {

        void CGeoDatabaseUtils::FillRow(ISelectCursor* ptrCursor,  IRowPtr ptrRow)
        {
            try
            {
                for (int i = 0; i < ptrCursor-> ColumnCount(); ++i)
                {
                    switch(ptrCursor->GetColumnType(i))
                    {
                        case dtNull:
                            ptrRow->SetNull(i);
                            break;
                        case dtInteger8:
                            ptrRow->SetInt8(i, ptrCursor->ReadInt8(i));
                            break;
                        case dtUInteger8:
                            ptrRow->SetUInt8(i, ptrCursor->ReadUInt8(i));
                            break;
                        case dtInteger16:
                            ptrRow->SetInt16(i, ptrCursor->ReadInt16(i));
                            break;
                        case dtUInteger16:
                            ptrRow->SetUInt16(i, ptrCursor->ReadUInt16(i));
                            break;
                        case dtInteger32:
                            ptrRow->SetUInt32(i, ptrCursor->ReadUInt32(i));
                            break;
                        case dtUInteger32:
                            ptrRow->SetUInt32(i, ptrCursor->ReadUInt32(i));
                            break;
                        case dtInteger64:
                            ptrRow->SetInt64(i, ptrCursor->ReadInt64(i));
                            break;
                        case dtUInteger64:
                            ptrRow->SetUInt64(i, ptrCursor->ReadUInt64(i));
                            break;
                        case dtFloat:
                            ptrRow->SetDouble(i, ptrCursor->ReadDouble(i));
                            break;
                        case dtDouble:
                            ptrRow->SetDouble(i, ptrCursor->ReadDouble(i));
                            break;
                        case dtString:
                            ptrRow->SetText(i, ptrCursor->ReadText(i));
                            break;
                        case dtGeometry:
                            ptrRow->SetShape(i, ptrCursor->ReadShape(i));
                            break;
                        case dtBlob:
                        {
                            CommonLib::CVariantPtr ptrVariant = ptrRow->GetValue(i);
                            if(ptrVariant->IsNull())
                            {
                                CommonLib::Data::CBlobPtr ptrBlob = std::make_shared<CommonLib::Data::CBlob>();
                                ptrVariant->SetVal(ptrBlob);
                            }
                            byte_t *pBuf = nullptr;
                            int32_t size = 0;
                            ptrCursor->ReadBlob(i, &pBuf, size);
                            CommonLib::Data::CBlobPtr ptrBlob =  ptrRow->GetValue(i)->Get<CommonLib::Data::CBlobPtr>();
                            ptrBlob->Copy(pBuf, size);
                        }
                            break;
                        case dtGuid:
                            ptrRow->SetGuid(i, ptrCursor->ReadGuid(i));
                            break;
                        default:
                            throw CommonLib::CExcBase("Not support type: {0}", (int)ptrCursor->GetColumnType(i));
                            break;
                    }
                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExcT("CShapefileRowCursor failed to fill row ",  exc);
                throw;
            }
        }



    }
}