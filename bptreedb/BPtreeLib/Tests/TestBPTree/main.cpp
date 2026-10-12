#include "pch.h"
#include "../../../../CommonLib/CommonLib.h"

int TestBPMapBase();

int main() {

    std::shared_ptr<CommonLib::IlogWriter> pLogWriter(new CommonLib::CConsoleLogWriter());

    Log.InnitLog(std::shared_ptr<CommonLib::IlogLineFormat>(), pLogWriter);

    return TestBPMapBase();
}