#ifndef TST_CHECKSUM_H
#define TST_CHECKSUM_H

#include <QObject>

// ============================================================
// 异或校验算法测试 —— 对应 mainwindow.cpp 的 calculateChecksum()
// ============================================================

class TestChecksum : public QObject
{
    Q_OBJECT

private slots:
    void test_basic();              // 基本异或校验
    void test_dataframe_send();     // 数据帧发送的校验（AA帧头+5字节）
    void test_feedback_frame();     // 回传帧的校验（55帧头+7字节）
    void test_zero_data();          // 全零数据的校验
    void test_identity();           // a^a = 0 的性质
    void test_manual_calculation(); // 手动计算交叉验证
};

#endif // TST_CHECKSUM_H
