#ifndef TST_PARSER_H
#define TST_PARSER_H

#include <QObject>
#include <QByteArray>

// ============================================================
// 二进制帧解析测试 —— 对应 mainwindow.cpp 的 parseBinaryFrame()
// ============================================================

// 帧解析结果结构体
struct ParsedFrame {
    bool valid;           // 是否找到有效帧
    quint8 pwmLeft;
    quint8 spdLeft;
    quint8 staLeft;
    quint8 pwmRight;
    quint8 spdRight;
    quint8 staRight;
    quint8 xorByte;
    bool checksumOk;      // 校验是否通过
};

class TestParser : public QObject
{
    Q_OBJECT

private slots:
    void test_single_complete_frame();    // 单独一帧完整8字节
    void test_multiple_frames();          // 连续多帧
    void test_no_header();                // 数据中没有0x55帧头
    void test_header_at_end();            // 帧头在数据末尾，不够8字节
    void test_false_header_in_data();     // 数据域中出现0x55，帧头误识别BUG
    void test_checksum_mismatch();        // 校验不匹配，帧应被丢弃
    void test_all_zero_frame();           // 全零帧
    void test_max_speed();                // 速度=0xFF=255
    void test_positive_reverse_stop();    // 状态值:1正转/0反转/2停止
};

// 模拟 parseBinaryFrame 的核心逻辑（去掉UI依赖）
ParsedFrame findFirstFrame(const QByteArray &data);

#endif // TST_PARSER_H
