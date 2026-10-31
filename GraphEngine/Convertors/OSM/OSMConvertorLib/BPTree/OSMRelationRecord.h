#pragma once

// The record of a relation (the value of the relation tree, a blob):
//   varuint  the size of the shape (0 - no shape)
//   bytes    the shape (GeometryCompression, ShapeFormat.h, 1e-7 degrees):
//              type=multipolygon / boundary - a polygon of the rings made of the member ways (outer clockwise, its holes
//              counterclockwise after it); otherwise a polyline, a part per member way with a shape; a relation of nodes
//              only - a multipoint
//   varuint  the member count
//   members  uint8 the type (0 node, 1 way, 2 relation), zigzag varint the id - the id of the previous member,
//            varuint the length of the role, the role (UTF-8)

#include "../../../../../CommonLib/CommonLib.h"
#include "../../../../../CommonLib/exception/exc_base.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            enum eRelationMemberType : uint8_t
            {
                RelationMemberNode     = 0,
                RelationMemberWay      = 1,
                RelationMemberRelation = 2
            };

            struct SRelationMember
            {
                uint8_t     type = RelationMemberNode;
                int64_t     id = 0;
                std::string role;
            };

            struct SRelationRecord
            {
                std::vector<byte_t>          shape;
                std::vector<SRelationMember> members;
            };

            inline void WriteVarUInt(std::vector<byte_t>& out, uint64_t nValue)
            {
                while (nValue >= 0x80)
                {
                    out.push_back((byte_t)(nValue | 0x80));
                    nValue >>= 7;
                }
                out.push_back((byte_t)nValue);
            }

            inline uint64_t ZigZag64(int64_t nValue) { return ((uint64_t)nValue << 1) ^ (uint64_t)(nValue >> 63); }
            inline int64_t UnZigZag64(uint64_t nValue) { return (int64_t)(nValue >> 1) ^ -(int64_t)(nValue & 1); }

            inline uint64_t ReadVarUInt(const byte_t*& p, const byte_t* pEnd)
            {
                uint64_t nValue = 0;
                for (int nShift = 0; nShift < 64; nShift += 7)
                {
                    if (p >= pEnd)
                        throw CommonLib::CExcBase("the relation record is truncated");
                    byte_t b = *p++;
                    nValue |= (uint64_t)(b & 0x7F) << nShift;
                    if ((b & 0x80) == 0)
                        return nValue;
                }
                throw CommonLib::CExcBase("a wrong varint in the relation record");
            }

            inline void DecodeRelationRecord(const byte_t* pData, size_t nSize, SRelationRecord& record)
            {
                const byte_t* p = pData;
                const byte_t* pEnd = pData + nSize;
                uint64_t nShape = ReadVarUInt(p, pEnd);
                if (nShape > (uint64_t)(pEnd - p))
                    throw CommonLib::CExcBase("the shape of the relation record is truncated");
                record.shape.assign(p, p + nShape);
                p += nShape;
                uint64_t nMembers = ReadVarUInt(p, pEnd);
                record.members.clear();
                record.members.reserve((size_t)(std::min)(nMembers, (uint64_t)(pEnd - p)));
                int64_t nPrev = 0;
                for (uint64_t i = 0; i < nMembers; ++i)
                {
                    if (p >= pEnd)
                        throw CommonLib::CExcBase("the members of the relation record are truncated");
                    SRelationMember member;
                    member.type = *p++;
                    nPrev += UnZigZag64(ReadVarUInt(p, pEnd));
                    member.id = nPrev;
                    uint64_t nRole = ReadVarUInt(p, pEnd);
                    if (nRole > (uint64_t)(pEnd - p))
                        throw CommonLib::CExcBase("the role of the relation record is truncated");
                    member.role.assign((const char*)p, (size_t)nRole);
                    p += nRole;
                    record.members.push_back(std::move(member));
                }
            }
        }
    }
}
