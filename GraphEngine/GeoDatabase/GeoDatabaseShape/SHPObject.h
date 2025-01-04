#pragma once
#include "../GeoDatabase.h"
#include "../../ThirdParty/ShapeLib/shapefil.h"


namespace GraphEngine
{
    namespace GeoDatabase {

        typedef std::shared_ptr<class CSHPObject> CSHPObjectPtr;

        class CSHPObject
        {
        public:
            CSHPObject(SHPObject*   pObject);
            ~CSHPObject();

            int GetSHPType() const;
            int GetVertices() const;
            int GetParts() const;
            double GetPadfX(int idx) const;
            double GetPadfY(int idx) const;
            bool HasZ() const;
            bool HasM() const;
            double GetPadfZ(int idx) const;
            double GetPadfM(int idx) const;
            int GetPanPartStart(int idx) const;
            bool HasPanPartType() const;
            int GetPanPartType(int idx) const;

        private:
            SHPObject*   m_pObject;
        };


    }
}
