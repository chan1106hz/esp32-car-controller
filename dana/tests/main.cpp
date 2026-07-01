#include <QtTest>
#include "tst_checksum.h"
#include "tst_parser.h"

int main(int argc, char *argv[])
{
    int failures = 0;

    TestChecksum tc;
    failures += QTest::qExec(&tc, argc, argv);

    TestParser tp;
    failures += QTest::qExec(&tp, argc, argv);

    return failures > 0 ? 1 : 0;
}
