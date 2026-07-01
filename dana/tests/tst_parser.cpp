#include "tst_parser.h"
#include <QtTest>

// ============================================================
// 模拟 parseBinaryFrame 核心逻辑（去掉 UI 依赖，纯逻辑测试）
// ============================================================
static quint8 calcXor7(const quint8 *d) {
    return d[0] ^ d[1] ^ d[2] ^ d[3] ^ d[4] ^ d[5] ^ d[6];
}

ParsedFrame findFirstFrame(const QByteArray &data)
{
    ParsedFrame f;
    f.valid = false;

    int index = 0;
    while (index <= data.size() - 8) {
        if ((quint8)data[index] == 0x55) {
            const quint8 *d = (const quint8*)data.constData() + index;
            f.pwmLeft  = d[1];
            f.spdLeft  = d[2];
            f.staLeft  = d[3];
            f.pwmRight = d[4];
            f.spdRight = d[5];
            f.staRight = d[6];
            f.xorByte  = d[7];

            quint8 expected = calcXor7(d);
            f.checksumOk = (expected == f.xorByte);
            f.valid = true;
            return f;
        }
        index++;
    }
    return f;  // valid = false
}

// ============================================================
// 单独一帧完整8字节
// ============================================================
void TestParser::test_single_complete_frame()
{
    QByteArray data = QByteArray::fromHex("55" "64" "96" "01" "C8" "32" "02" "5E");
    ParsedFrame f = findFirstFrame(data);

    QVERIFY(f.valid);
    QCOMPARE(f.pwmLeft,  (quint8)0x64);
    QCOMPARE(f.spdLeft,  (quint8)0x96);   // 左轮速度=150
    QCOMPARE(f.staLeft,  (quint8)0x01);   // 正转
    QCOMPARE(f.pwmRight, (quint8)0xC8);
    QCOMPARE(f.spdRight, (quint8)0x32);   // 右轮速度=50
    QCOMPARE(f.staRight, (quint8)0x02);   // 停止
    QVERIFY(f.checksumOk);
}

// ============================================================
// 连续多帧
// ============================================================
void TestParser::test_multiple_frames()
{
    // 两帧拼接在一起
    QByteArray data = QByteArray::fromHex(
        "55" "64" "96" "01" "C8" "32" "02" "5E"   // 帧1
        "55" "C8" "32" "02" "64" "96" "01" "5E"    // 帧2（左右轮数据互换）
    );
    ParsedFrame f = findFirstFrame(data);
    QVERIFY(f.valid);
    QCOMPARE(f.spdLeft,  (quint8)0x96);   // 帧1的左轮速度
    QCOMPARE(f.spdRight, (quint8)0x32);   // 帧1的右轮速度
}

// ============================================================
// 数据中没有0x55帧头
// ============================================================
void TestParser::test_no_header()
{
    QByteArray data = QByteArray::fromHex("00" "01" "02" "03" "04" "05" "06" "07");
    ParsedFrame f = findFirstFrame(data);
    QVERIFY(!f.valid);   // 找不到帧头，valid应为false
}

// ============================================================
// 帧头在末尾，不够8字节
// ============================================================
void TestParser::test_header_at_end()
{
    // 帧头不在数据开头，parser 需要跳过前面字节扫描到它
    // 前4字节是无效数据，第5字节开始是0x55帧头，后面刚好8字节
    QByteArray data = QByteArray::fromHex(
        "00" "00" "00" "00"                         // 无效前导字节
        "55" "64" "96" "01" "C8" "32" "02" "5E"    // 完整帧
    );
    // data.size()=12, while index <= 12-8=4
    // index=0:00≠55, 1:00≠55, 2:00≠55, 3:00≠55, 4:55==55 → 命中
    ParsedFrame f = findFirstFrame(data);
    QVERIFY(f.valid);
    QCOMPARE(f.spdLeft, (quint8)0x96);
    QVERIFY(f.checksumOk);
}

// ============================================================
// 数据域中出现0x55 —— 帧头误识别 BUG 演示
// ============================================================
void TestParser::test_false_header_in_data()
{
    // 场景：parser 失步后从中间字节开始扫
    // 模拟从一半开始读串口数据：前面有无效字节，中间才是数据域中的0x55
    QByteArray data = QByteArray::fromHex(
        "00" "00"          // 无效前导（模拟失步）
        "96" "01" "C8"     // 帧数据的前半部分
        "55"               // ← 这是数据域中的 0x55（如 SPD_R=85），不是帧头！
        "02" "51"          // 这是该位置后面正巧有的字节
        "00" "00" "00" "00" "00" "00"  // 填充
    );
    // data.size()=16, while index <= 8
    // 扫描到 index=5 时遇到 0x55，把它当帧头
    // 提取: 55 02 51 00 00 00 00 00 → PWM_L=02, SPD_L=0x51=81 (垃圾数据)
    ParsedFrame f = findFirstFrame(data);

    QVERIFY(f.valid);                // parser 以为找到了帧
    QCOMPARE(f.pwmLeft, (quint8)0x02);
    QCOMPARE(f.spdLeft, (quint8)0x51);
    // 校验不通过（后面字节是垃圾）
    QVERIFY(!f.checksumOk);
    // 这就是帧头误识别 Bug：parser 把数据域的 0x55 当帧头
}

// ============================================================
// 校验不匹配
// ============================================================
void TestParser::test_checksum_mismatch()
{
    // 构造一帧，但把最后一个字节（校验位）故意写错
    QByteArray data = QByteArray::fromHex("55" "64" "96" "01" "C8" "32" "02" "FF");
    ParsedFrame f = findFirstFrame(data);

    QVERIFY(f.valid);           // 帧结构完整
    QVERIFY(!f.checksumOk);     // 但校验不通过，应被丢弃
}

// ============================================================
// 全零帧（停止状态）
// ============================================================
void TestParser::test_all_zero_frame()
{
    // 全零帧：PWM=0, Speed=0, STA=0(反转), 校验 = 55^0^0^0^0^0^0 = 55
    QByteArray data = QByteArray::fromHex("55" "00" "00" "00" "00" "00" "00" "55");
    ParsedFrame f = findFirstFrame(data);

    QVERIFY(f.valid);
    QVERIFY(f.checksumOk);
    QCOMPARE(f.spdLeft,  (quint8)0x00);
    QCOMPARE(f.spdRight, (quint8)0x00);
    QCOMPARE(f.staLeft,  (quint8)0x00);   // 0=反转（但速度为0，无意义）
}

// ============================================================
// 最大速度
// ============================================================
void TestParser::test_max_speed()
{
    // 速度=255(0xFF)，校验=55^FF^FF^01^FF^FF^01
    // 55^FF=AA, AA^FF=55, 55^01=54, 54^FF=AB, AB^FF=54, 54^01=55
    QByteArray data = QByteArray::fromHex("55" "FF" "FF" "01" "FF" "FF" "01" "55");
    ParsedFrame f = findFirstFrame(data);

    QVERIFY(f.valid);
    QVERIFY(f.checksumOk);
    QCOMPARE(f.spdLeft,  (quint8)0xFF);   // 255
    QCOMPARE(f.spdRight, (quint8)0xFF);   // 255
}

// ============================================================
// 状态值: 1=正转, 0=反转, 2=停止
// ============================================================
void TestParser::test_positive_reverse_stop()
{
    // 构造三帧，分别测试正转(1)、反转(0)、停止(2)
    // 帧1: 正转 STA=1, 校验=55^64^32^01^64^32^01=...
    // 55^64=31, 31^32=03, 03^01=02, 02^64=66, 66^32=54, 54^01=55
    QByteArray data1 = QByteArray::fromHex("55" "64" "32" "01" "64" "32" "01" "55");
    ParsedFrame f1 = findFirstFrame(data1);
    QCOMPARE(f1.staLeft,  (quint8)0x01);   // 正转
    QCOMPARE(f1.staRight, (quint8)0x01);   // 正转

    // 帧2: 反转 STA=0
    QByteArray data2 = QByteArray::fromHex("55" "00" "00" "00" "00" "00" "00" "55");
    ParsedFrame f2 = findFirstFrame(data2);
    QCOMPARE(f2.staLeft,  (quint8)0x00);   // 反转

    // 帧3: 停止 STA=2, 校验=55^64^32^02^64^32^02
    // 55^64=31, 31^32=03, 03^02=01, 01^64=65, 65^32=57, 57^02=55
    QByteArray data3 = QByteArray::fromHex("55" "64" "32" "02" "64" "32" "02" "55");
    ParsedFrame f3 = findFirstFrame(data3);
    QCOMPARE(f3.staLeft,  (quint8)0x02);   // 停止
    QCOMPARE(f3.staRight, (quint8)0x02);   // 停止
}
