#pragma once
#include "../../CommonLib/CommonLib.h"


namespace GraphEngine {
    namespace Geometry {

        class CCodeProjConvertor
        {
        public:
            static std::string CodeToProj4Str(int code);
            static int Proj4StrToCode(const   std::string& sProj4Str);
        };


        }
    }