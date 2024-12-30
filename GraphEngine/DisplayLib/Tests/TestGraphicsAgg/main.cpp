#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../Display.h"


int main()
{

    CommonLib::CExcBase exc("");
    GraphEngine::Display::CPen pen;

    GraphEngine::Display::IGraphicsPtr graphics = GraphEngine::Display::IGraphics::CreateCGraphicsAgg( 200, 200);
    graphics->Erase(GraphEngine::Display::Color::Black);
    return 0;

}

