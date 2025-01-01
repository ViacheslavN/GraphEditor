#include "CodeProj.h"
#include "esri.inc"

namespace GraphEngine {
    namespace Geometry {
        int CCodeProjConvertor::Proj4StrToCode(const std::string& sProj4Str)
        {
            int count = sizeof (ESRI)/sizeof (ESRI[0]);
            for (int i = 0; i < count; ++i)
            {
                if (sProj4Str == ESRI[i].pProjParams) //TO DO  compare hash
                {
                    return ESRI[i].id;
                }
            }

            return 0;
        }

        std::string CCodeProjConvertor::CodeToProj4Str(int prjCode) {
            int count = sizeof(ESRI) / sizeof(ESRI[0]);
            for (int i = 0; i < count; ++i) {
                if (prjCode == ESRI[i].id)
                    return ESRI[i].pProjParams;
            }
            return "";
        }
    }
    }