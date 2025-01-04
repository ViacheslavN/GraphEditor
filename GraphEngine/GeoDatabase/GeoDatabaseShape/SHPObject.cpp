#include "SHPObject.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        CSHPObject::CSHPObject(SHPObject*   pObject) : m_pObject(pObject)
        {

        }

        CSHPObject::~CSHPObject()
        {
            if(m_pObject)
            {
                SHPDestroyObject(m_pObject);
                m_pObject = 0;
            }
        }

        int CSHPObject::GetSHPType() const
        {
            return m_pObject->nSHPType;
        }

        int CSHPObject::GetVertices() const
        {
            return m_pObject->nVertices;
        }

        int CSHPObject::GetParts() const
        {
            return m_pObject->nParts;
        }

        double CSHPObject::GetPadfX(int idx) const
        {
            return m_pObject->padfX[idx];
        }

        double CSHPObject::GetPadfY(int idx) const
        {
            return m_pObject->padfY[idx];
        }

        bool CSHPObject::HasZ() const
        {
            return  m_pObject->padfZ != NULL;
        }

        bool CSHPObject::HasM() const
        {
            return  m_pObject->padfM != NULL;
        }

        double CSHPObject::GetPadfZ(int idx) const
        {
            return  m_pObject->padfZ[idx];
        }

        double CSHPObject::GetPadfM(int idx) const
        {
            return  m_pObject->padfM[idx];
        }

        int CSHPObject::GetPanPartStart(int idx) const
        {
            return m_pObject->panPartStart[idx];
        }

        bool CSHPObject::HasPanPartType() const
        {
            return m_pObject->panPartType != NULL;
        }

        int CSHPObject::GetPanPartType(int idx) const
        {
            return m_pObject->panPartType[idx];
        }

    }
    }