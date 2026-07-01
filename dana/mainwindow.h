// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_refreshButton_clicked();        // 刷新串口列表
    void on_openButton_clicked();           // 打开/关闭串口
    void readSerialData();                  // 读取串口数据
    void sendStringCommand(const QString &cmd); // 发送字符串命令（自动加换行）

    // 字符串命令按钮槽函数
    void on_forwardButton_clicked();
    void on_backwardButton_clicked();
    void on_leftButton_clicked();
    void on_rightButton_clicked();
    void on_stopButton_clicked();
    void on_pwmSendButton_clicked();        // PWM调速

    // 数据帧命令（快速转向，带异或校验）
    void on_quickLeftButton_clicked();
    void on_quickRightButton_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;                    // 串口对象
    QString configFilePath;                 // 配置文件路径

    void loadConfig();                      // 从config.txt加载端口和波特率
    void saveConfig(const QString &port, int baud); // 保存配置（可选）
    void updateUiForSerialOpen(bool open);  // 更新界面状态
    void parseAndDisplaySpeed(const QString &data); // 解析速度数据并显示
    void sendDataFrame(quint8 cmd, quint8 pwmLeft, quint8 pwmRight); // 发送6字节数据帧
    void parseBinaryFrame(const QByteArray &data); // 解析模拟器回传的二进制数据帧
    quint8 calculateChecksum(const quint8 *data, int len); // 异或校验计算

    void setupStyleSheet();                 // 设置界面样式表（美化）
};

#endif // MAINWINDOW_H
