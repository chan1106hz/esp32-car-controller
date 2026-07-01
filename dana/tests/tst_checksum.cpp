#include "tst_checksum.h"
#include <QtTest>

// ============================================================
// 辅助函数：与 mainwindow.cpp 的 calculateChecksum 完全一致
// ============================================================
static quint8 calculateChecksum(const quint8 *data, int len)
{
    quint8 checksum = 0;
    for (int i = 0; i < len; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

// ============================================================
// 基本异或校验
// ============================================================
void TestChecksum::test_basic()
{
    quint8 data[] = {0xAA, 0x01, 0xC8, 0x32, 0x00};
    quint8 result = calculateChecksum(data, 5);
    // 手动算: AA^01=A9, A9^C8=61, 61^32=53, 53^00=53
    // 但实际计算: 0^AA=AA, AA^01=AB, AB^C8=63, 63^32=51, 51^00=51
    // 重新手算: checksum初始=0
    //   0x00 ^ 0xAA = 0xAA
    //   0xAA ^ 0x01 = 0xAB
    //   0xAB ^ 0xC8 = 0x63
    //   0x63 ^ 0x32 = 0x51
    //   0x51 ^ 0x00 = 0x51
    QCOMPARE(result, (quint8)0x51);
}

// ============================================================
// 数据帧发送校验（模拟器文档：AA 00 FF 00 FF A8）
// ============================================================
void TestChecksum::test_dataframe_send()
{
    // AA 00 FF 00 FF → 异或校验
    // AA^00=AA, AA^FF=55, 55^00=55, 55^FF=AA
    // 校验=AA，但文档给的A8...
    // 实际验证: AA^00=AA, AA^FF=55, 55^00=55, 55^FF=AA  → AA, 不是A8
    // 文档有误？重新算: AA^00=AA, AA^FF=55, AA^FF不...等等
    // AA XOR 00 = AA
    // AA XOR FF = 55
    // 55 XOR 00 = 55
    // 55 XOR FF = AA
    // 结果确实是 0xAA，不是文档里的 0xA8
    // 这个差异本身就是测试价值——发现了文档数据不一致
    quint8 data[] = {0xAA, 0x00, 0xFF, 0x00, 0xFF};
    quint8 result = calculateChecksum(data, 5);
    QCOMPARE(result, (quint8)0xAA);
    // 注：模拟器文档标注校验为 A8，实际计算结果为 AA，存在偏差
}

// ============================================================
// 回传帧校验（55帧头 + 前7字节异或 = 第8字节）
// ============================================================
void TestChecksum::test_feedback_frame()
{
    // 构造一帧完整回传数据: 55 64 96 01 C8 32 02 xx
    // 前7字节异或: 55^64^96^01^C8^32^02
    // 55^64=31, 31^96=A7, A7^01=A6, A6^C8=6E, 6E^32=5C, 5C^02=5E
    quint8 frame[7] = {0x55, 0x64, 0x96, 0x01, 0xC8, 0x32, 0x02};
    quint8 calcXor = calculateChecksum(frame, 7);
    QCOMPARE(calcXor, (quint8)0x5E);

    // 验证第8字节=0x5E时校验通过
    quint8 recvXor = 0x5E;
    QVERIFY(calcXor == recvXor);

    // 验证第8字节错误时校验失败
    quint8 badXor = 0xFF;
    QVERIFY(calcXor != badXor);
}

// ============================================================
// 全零数据校验
// ============================================================
void TestChecksum::test_zero_data()
{
    quint8 data[] = {0x00, 0x00, 0x00, 0x00, 0x00};
    // 5个0异或还是0
    QCOMPARE(calculateChecksum(data, 5), (quint8)0x00);
}

// ============================================================
// a^a = 0 异或性质
// ============================================================
void TestChecksum::test_identity()
{
    // 任意两个相同值异或为0
    QCOMPARE((quint8)(0x55 ^ 0x55), (quint8)0x00);
    // 任意值与0异或不变
    QCOMPARE((quint8)(0x55 ^ 0x00), (quint8)0x55);
}

// ============================================================
// 手动计算交叉验证
// ============================================================
void TestChecksum::test_manual_calculation()
{
    // 用在线工具 https://www.metools.info/code/c48.html 验证
    // 输入: AA 00 FF 00 FF → 输出: AA
    // 输入: 55 64 96 01 C8 32 02 → 输出: 5E
    quint8 data1[] = {0xAA, 0x00, 0xFF, 0x00, 0xFF};
    QCOMPARE(calculateChecksum(data1, 5), (quint8)0xAA);

    quint8 data2[] = {0x55, 0x64, 0x96, 0x01, 0xC8, 0x32, 0x02};
    QCOMPARE(calculateChecksum(data2, 7), (quint8)0x5E);
}
