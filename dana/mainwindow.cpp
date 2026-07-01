// mainwindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QRegularExpression>
#include <QDateTime>

// 常用波特率列表
static const QList<qint32> baudRates = {9600, 19200, 38400, 57600, 115200, 230400};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(nullptr)
    , configFilePath("config.txt")
{
    ui->setupUi(this);
    setWindowTitle("ESP32智能小车控制系统");
    // 应用美化样式表
    setupStyleSheet();

    // 初始化串口对象
    serial = new QSerialPort(this);

    // 填充波特率下拉框
    for (qint32 baud : baudRates) {
        ui->baudComboBox->addItem(QString::number(baud), baud);
    }

    // 信号槽连接：串口数据接收
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);

    // 加载配置文件（端口和波特率）
    loadConfig();

    // 刷新可用串口列表
    on_refreshButton_clicked();

    // 初始默认关闭状态，需要用户点击打开
    updateUiForSerialOpen(false);
}

MainWindow::~MainWindow()
{
    if (serial && serial->isOpen()) {
        serial->close();
    }
    delete ui;
}

// 从config.txt读取配置，格式：第一行端口名，第二行波特率
void MainWindow::loadConfig()
{
    QFile file(configFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 配置文件不存在，使用默认值
        ui->portComboBox->setCurrentText("COM3");
        int index = ui->baudComboBox->findData(115200);
        if (index >= 0) ui->baudComboBox->setCurrentIndex(index);
        return;
    }

    QTextStream in(&file);
    QString port = in.readLine().trimmed();
    QString baudStr = in.readLine().trimmed();
    file.close();

    if (!port.isEmpty()) {
        ui->portComboBox->setCurrentText(port);
    }
    bool ok;
    int baud = baudStr.toInt(&ok);
    if (ok) {
        int index = ui->baudComboBox->findData(baud);
        if (index >= 0) ui->baudComboBox->setCurrentIndex(index);
    }
}

// 保存当前配置
void MainWindow::saveConfig(const QString &port, int baud)
{
    QFile file(configFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << port << "\n" << baud;
        file.close();
    }
}

// 刷新可用串口列表
void MainWindow::on_refreshButton_clicked()
{
    QString current = ui->portComboBox->currentText();
    ui->portComboBox->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        ui->portComboBox->addItem(info.portName());
    }
    if (!current.isEmpty() && ui->portComboBox->findText(current) >= 0) {
        ui->portComboBox->setCurrentText(current);
    } else if (ui->portComboBox->count() > 0) {
        ui->portComboBox->setCurrentIndex(0);
    }
}

// 打开/关闭串口
void MainWindow::on_openButton_clicked()
{
    if (serial->isOpen()) {
        serial->close();
        updateUiForSerialOpen(false);
        ui->recvTextEdit->append("[系统] 串口已关闭");
        return;
    }

    QString portName = ui->portComboBox->currentText();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择或输入串口端口号");
        return;
    }

    qint32 baudRate = ui->baudComboBox->currentData().toInt();
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (serial->open(QIODevice::ReadWrite)) {
        updateUiForSerialOpen(true);
        ui->recvTextEdit->append(QString("[系统] 成功打开串口 %1 波特率 %2").arg(portName).arg(baudRate));

        saveConfig(portName, baudRate);
    } else {
        QMessageBox::critical(this, "错误", "无法打开串口: " + serial->errorString());
        ui->recvTextEdit->append("[系统] 打开串口失败");
    }
}

// 串口状态改变时更新UI控件可用性
void MainWindow::updateUiForSerialOpen(bool open)
{
    ui->openButton->setText(open ? "关闭串口" : "打开串口");
    ui->portComboBox->setEnabled(!open);
    ui->baudComboBox->setEnabled(!open);
    ui->refreshButton->setEnabled(!open);
    // 控制命令按钮在串口打开时才可用
    ui->forwardButton->setEnabled(open);
    ui->backwardButton->setEnabled(open);
    ui->leftButton->setEnabled(open);
    ui->rightButton->setEnabled(open);
    ui->stopButton->setEnabled(open);
    ui->pwmSendButton->setEnabled(open);
    ui->quickLeftButton->setEnabled(open);
    ui->quickRightButton->setEnabled(open);
}

// 发送字符串命令，自动追加换行符\n
void MainWindow::sendStringCommand(const QString &cmd)
{
    if (!serial || !serial->isOpen()) {
        QMessageBox::warning(this, "警告", "串口未打开，无法发送命令");
        return;
    }
    QByteArray data = cmd.toUtf8() + "\n";
    qint64 bytesWritten = serial->write(data);
    if (bytesWritten == data.size()) {
        ui->recvTextEdit->append("[发送] " + cmd);
    } else {
        ui->recvTextEdit->append("[错误] 发送命令失败: " + cmd);
    }
}

// 读取串口数据并显示，同时尝试解析速度信息
void MainWindow::readSerialData()
{
    if (!serial) return;
    QByteArray data = serial->readAll();
    if (data.isEmpty()) return;

    // 显示十六进制原始数据（便于调试）
    QString hexDisp;
    for (int i = 0; i < data.size(); ++i) {
        hexDisp += QString("%1 ").arg((quint8)data[i], 2, 16, QLatin1Char('0'));
    }
    ui->recvTextEdit->append("[RECV HEX] " + hexDisp);

    // 解析二进制帧（模拟器专用）
    parseBinaryFrame(data);

    // 同时也按文本方式尝试解析（兼容可能有的字符串回传）
//    QString received = QString::fromUtf8(data);
//    if (!received.isEmpty()) {
//        ui->recvTextEdit->moveCursor(QTextCursor::End);
//        ui->recvTextEdit->insertPlainText(received);
//        ui->recvTextEdit->ensureCursorVisible();

//        QStringList lines = received.split('\n', Qt::SkipEmptyParts);
//        for (const QString &line : lines) {
//            parseAndDisplaySpeed(line);
//        }
//    }
}

void MainWindow::parseBinaryFrame(const QByteArray &data)
{                               //data 是从串口收到的原始数据
    int index = 0;
    while (index <= data.size() - 8) {
        //data.size() 是收到数据的长度（字节数）。
        //因为每一帧固定 8 个字节，所以只要还有 8 个字节可读，就继续处理。

        if ((quint8)data[index] == 0x55) {
        //帧头是固定值 0x55（十进制85），用来标记一帧的起点。
        //找到帧头后，提取后面的7个字节,按照协议规定的偏移位置取数
            quint8 pwmLeft   = (quint8)data[index+1];
            quint8 spdLeft   = (quint8)data[index+2];//左轮速度
            quint8 staLeft   = (quint8)data[index+3];//左轮状态
            quint8 pwmRight  = (quint8)data[index+4];
            quint8 spdRight  = (quint8)data[index+5];//右轮速度
            quint8 staRight  = (quint8)data[index+6];//右轮状态
            quint8 recvXor   = (quint8)data[index+7];//校验

            // 计算正确的异或校验（前7个字节）
            quint8 calcXor = 0x55 ^ pwmLeft ^ spdLeft ^ staLeft ^ pwmRight ^ spdRight ^ staRight;

            bool checksumOk = (calcXor == recvXor);
            if (!checksumOk) {
                ui->recvTextEdit->append(QString("[警告] 校验不符: 计算=0x%1 接收=0x%2，帧已丢弃")
                                          .arg(calcXor, 2, 16, QLatin1Char('0'))
                                          .arg(recvXor, 2, 16, QLatin1Char('0')));
                index += 8;   // 跳过这8字节，继续找下一帧
                continue;
            }

            // 校验通过才更新显示
            ui->leftSpeedLabel->setText(QString::number(spdLeft));
            ui->rightSpeedLabel->setText(QString::number(spdRight));

            // 状态转换
            QString staLeftText, staRightText;
            switch (staLeft) {
                case 1: staLeftText = "正转"; break;
                case 0: staLeftText = "反转"; break;
                case 2: staLeftText = "停止"; break;
                default: staLeftText = "未知";
            }
            switch (staRight) {
                case 1: staRightText = "正转"; break;
                case 0: staRightText = "反转"; break;
                case 2: staRightText = "停止"; break;
                default: staRightText = "未知";
            }

            ui->leftStatusLabel->setText(staLeftText);
            ui->rightStatusLabel->setText(staRightText);

            ui->recvTextEdit->append(QString("[小车状态] 左轮: PWM=%1 速度=%2 %3 | 右轮: PWM=%4 速度=%5 %6")
                                      .arg(pwmLeft).arg(spdLeft).arg(staLeftText)
                                      .arg(pwmRight).arg(spdRight).arg(staRightText));

            index += 8;
        } else {
            index++;    //如果当前字节不是 0x55，就跳到下一个字节继续找（index++）
        }
    }
}
// 从字符串中提取左右轮速度，支持格式: "L=150 R=200" 或 "Speed L:123 R:234"
void MainWindow::parseAndDisplaySpeed(const QString &data)
{
    // 正则表达式匹配左轮和右轮速度值（整数）
    QRegularExpression re("L[=:\\s]*([0-9]+).*R[=:\\s]*([0-9]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(data);
    if (match.hasMatch()) {
        int leftSpeed = match.captured(1).toInt();
        int rightSpeed = match.captured(2).toInt();
        ui->leftSpeedLabel->setText(QString::number(leftSpeed));
        ui->rightSpeedLabel->setText(QString::number(rightSpeed));
        // 同时将最新速度附加显示到状态栏或文本区（可选）
        ui->recvTextEdit->append(QString("[速度] 左:%1  右:%2").arg(leftSpeed).arg(rightSpeed));
    }
}

// 发送6字节数据帧，带异或校验（第6字节 = byte0^byte1^byte2^byte3^byte4）
void MainWindow::sendDataFrame(quint8 cmd, quint8 pwmLeft, quint8 pwmRight)
{
    if (!serial || !serial->isOpen()) {
        QMessageBox::warning(this, "警告", "串口未打开，无法发送数据帧");
        return;
    }

    quint8 frame[6];
    frame[0] = 0xAA;        // 帧头0xAA
    frame[1] = cmd;         // 命令: 0x01快速左转，0x02快速右转
    frame[2] = pwmLeft;     // 左轮PWM值
    frame[3] = pwmRight;    // 右轮PWM值
    frame[4] = 0x00;        // 保留字节
    frame[5] = calculateChecksum(frame, 5);

    QByteArray sendData(reinterpret_cast<char*>(frame), 6);
    qint64 bytesWritten = serial->write(sendData);
    if (bytesWritten == 6) {
        ui->recvTextEdit->append(QString("[数据帧] 命令:%1 L_PWM:%2 R_PWM:%3 校验:0x%4")
                                 .arg(cmd, 2, 16, QLatin1Char('0'))
                                 .arg(pwmLeft).arg(pwmRight)
                                 .arg(frame[5], 2, 16, QLatin1Char('0')));
    } else {
        ui->recvTextEdit->append("[错误] 数据帧发送失败");
    }
}

// 异或校验：前len个字节逐一异或
quint8 MainWindow::calculateChecksum(const quint8 *data, int len)
{
    quint8 checksum = 0;
    for (int i = 0; i < len; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

// ---------- 字符串命令槽函数 ----------
void MainWindow::on_forwardButton_clicked()
{
    sendStringCommand("FORWARD");
}

void MainWindow::on_backwardButton_clicked()
{
    sendStringCommand("BACKWARD");
}

void MainWindow::on_leftButton_clicked()
{
    sendStringCommand("LEFT");
}

void MainWindow::on_rightButton_clicked()
{
    sendStringCommand("RIGHT");
}

void MainWindow::on_stopButton_clicked()
{
    sendStringCommand("STOP");
}

void MainWindow::on_pwmSendButton_clicked()
{
    int pwmValue = ui->pwmLineEdit->text().toInt();
    if (pwmValue < 0 || pwmValue > 255) {
        QMessageBox::warning(this, "警告", "PWM值范围0-255");
        return;
    }
    sendStringCommand(QString("PWM %1").arg(pwmValue));
}

// ---------- 数据帧命令（快速转向）----------
// 快速左转：左轮大PWM正转，右轮小PWM（或反转，这里用较大差值实现快速左转）
void MainWindow::on_quickLeftButton_clicked()
{
    // 命令0x01表示快速左转，左轮PWM=200，右轮PWM=50，产生差速左转
    sendDataFrame(0x01, 200, 50);
}

// 快速右转：左轮小PWM，右轮大PWM
void MainWindow::on_quickRightButton_clicked()
{
    sendDataFrame(0x02, 50, 200);
}

void MainWindow::setupStyleSheet()
{
    QString styleSheet = R"(
        /* 全局样式 */
        QMainWindow {
            background-color: #f0f2f5;
            font-family: "Segoe UI", "Microsoft YaHei", "PingFang SC", sans-serif;
        }

        /* 分组框样式 */
        QGroupBox {
            font-weight: bold;
            border: 1px solid #dcdfe6;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 10px;
            background-color: #ffffff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 16px;
            padding: 0 8px;
            color: #2c3e50;
        }

        /* 标签通用样式 */
        QLabel {
            color: #2c3e50;
            font-size: 13px;
        }

        /* 输入框、下拉框通用样式 */
        QLineEdit, QComboBox {
            background-color: #ffffff;
            border: 1px solid #dcdfe6;
            border-radius: 6px;
            padding: 5px 8px;
            font-size: 13px;
            selection-background-color: #409eff;
        }
        QLineEdit:focus, QComboBox:focus, QComboBox:editable:focus {
            border-color: #409eff;
            outline: none;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #909399;
            margin-right: 5px;
        }

        /* 按钮通用样式 */
        QPushButton {
            border: none;
            border-radius: 6px;
            padding: 6px 16px;
            font-weight: 600;
            font-size: 13px;
            color: #ffffff;
            background-color: #409eff;
            cursor: pointer;
        }
        QPushButton:hover {
            background-color: #66b1ff;
        }
        QPushButton:pressed {
            background-color: #3a8ee6;
        }
        QPushButton:disabled {
            background-color: #c0c4cc;
            color: #ffffff;
        }

        /* 特殊功能按钮颜色 */
        QPushButton#openButton {
            background-color: #67c23a;
        }
        QPushButton#openButton:hover {
            background-color: #85ce61;
        }
        QPushButton#stopButton {
            background-color: #f56c6c;
        }
        QPushButton#stopButton:hover {
            background-color: #f78989;
        }
        QPushButton#quickLeftButton, QPushButton#quickRightButton {
            background-color: #e6a23c;
        }
        QPushButton#quickLeftButton:hover, QPushButton#quickRightButton:hover {
            background-color: #ebb563;
        }

        /* 速度显示标签特殊样式 */
        QLabel#leftSpeedLabel, QLabel#rightSpeedLabel,
        QLabel#leftStatusLabel, QLabel#rightStatusLabel {
            background-color: #f5f7fa;
            border: 1px solid #e4e7ed;
            border-radius: 6px;
            padding: 4px 8px;
            font-weight: bold;
            font-size: 14px;
            min-width: 60px;
        }

        /* 文本编辑区样式 */
        QTextEdit {
            background-color: #ffffff;
            border: 1px solid #dcdfe6;
            border-radius: 8px;
            padding: 8px 12px;
            font-family: "JetBrains Mono", "Consolas", monospace;
            font-size: 12px;
            line-height: 1.5;
            selection-background-color: #409eff;
        }
        QTextEdit:focus {
            border-color: #409eff;
        }

        /* 菜单栏样式 */
        QMenuBar {
            background-color: #ffffff;
            border-bottom: 1px solid #e4e7ed;
            padding: 2px;
        }
        QMenuBar::item {
            background-color: transparent;
            padding: 4px 10px;
            margin: 0px;
        }
        QMenuBar::item:selected {
            background-color: #ecf5ff;
            color: #409eff;
        }
        QMenu {
            background-color: #ffffff;
            border: 1px solid #e4e7ed;
            border-radius: 6px;
        }
        QMenu::item {
            padding: 6px 24px;
        }
        QMenu::item:selected {
            background-color: #ecf5ff;
            color: #409eff;
        }

        /* 状态栏样式 */
        QStatusBar {
            background-color: #ffffff;
            border-top: 1px solid #e4e7ed;
            color: #606266;
        }
    )";

    this->setStyleSheet(styleSheet);
}
