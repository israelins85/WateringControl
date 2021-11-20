#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHostAddress>
#include <QDebug>


#define CC_NUL  (char(0x00))
#define CC_SOH  (char(0x01))
#define CC_STX  (char(0x02))
#define CC_ETX  (char(0x03))
#define CC_EOT  (char(0x04))
#define CC_ENQ  (char(0x05))
#define CC_ACK  (char(0x06))
#define CC_BEL  (char(0x07))
#define CC_BS   (char(0x08))
#define CC_TAB  (char(0x09))
#define CC_LF   (char(0x0A))
#define CC_VT   (char(0x0B))
#define CC_FF   (char(0x0C))
#define CC_CR   (char(0x0D))
#define CC_SO   (char(0x0E))
#define CC_SI   (char(0x0F))
#define CC_DLE  (char(0x10))
#define CC_DC1  (char(0x11))
#define CC_WAK  CC_DC1
#define CC_DC2  (char(0x12))
#define CC_DC3  (char(0x13))
#define CC_DC4  (char(0x14))
#define CC_NAK  (char(0x15))
#define CC_SYN  (char(0x16))
#define CC_ETB  (char(0x17))
#define CC_CAN  (char(0x18))
#define CC_EM   (char(0x19))
#define CC_SUB  (char(0x1A))
#define CC_ESC  (char(0x1B))
#define CC_FS   (char(0x1C))
#define CC_GS   (char(0x1D))
#define CC_RS   (char(0x1E))
#define CC_US   (char(0x1F))

QString byteArrayToString(QByteArray a_ba) {
    QString l_out;

    foreach (uchar l_chr, a_ba) {
        switch (l_chr) {
            case CC_NUL: l_out += "<NUL>"; break;
            case CC_SOH: l_out += "<SOH>"; break;
            case CC_STX: l_out += "<STX>"; break;
            case CC_ETX: l_out += "<ETX>"; break;
            case CC_EOT: l_out += "<EOT>"; break;
            case CC_ENQ: l_out += "<ENQ>"; break;
            case CC_ACK: l_out += "<ACK>"; break;
            case CC_BEL: l_out += "<BEL>"; break;
            case CC_BS: l_out += "<BS>"; break;
            case CC_TAB: l_out += "<TAB>"; break;
            case CC_LF: l_out += "<LF>"; break;
            case CC_VT: l_out += "<VT>"; break;
            case CC_FF: l_out += "<FF>"; break;
            case CC_CR: l_out += "<CR>"; break;
            case CC_SO: l_out += "<SO>"; break;
            case CC_SI: l_out += "<SI>"; break;
            case CC_DLE: l_out += "<DLE>"; break;
            case CC_DC1: l_out += "<DC1>"; break;
            case CC_DC2: l_out += "<DC2>"; break;
            case CC_DC3: l_out += "<DC3>"; break;
            case CC_DC4: l_out += "<DC4>"; break;
            case CC_NAK: l_out += "<NAK>"; break;
            case CC_SYN: l_out += "<SYN>"; break;
            case CC_ETB: l_out += "<ETB>"; break;
            case CC_CAN: l_out += "<CAN>"; break;
            case CC_EM: l_out += "<EM>"; break;
            case CC_SUB: l_out += "<SUB>"; break;
            case CC_ESC: l_out += "<ESC>"; break;
            case CC_FS: l_out += "<FS>"; break;
            case CC_GS: l_out += "<GS>"; break;
            case CC_RS: l_out += "<RS>"; break;
            case CC_US: l_out += "<US>"; break;
            default: {
                if (l_chr < 127) {
                    l_out += QChar::fromLatin1(l_chr);
                } else {
                    l_out += "\\x" + QString::number(l_chr, 16);
                }
            } break;
        }
    }

    return l_out;
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->edtText->setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::writeToLog(const QString &a_data)
{
    ui->edtLog->insertPlainText(a_data);
    qDebug() << a_data;
}

void MainWindow::on_btnEnviar_clicked()
{
    QHostAddress l_destIp(ui->edtIP->text());
    quint16 l_destPort = ui->edtPort->value();

    if (!m_tcpSocket) {
        m_tcpSocket = new QTcpSocket(this);

        connect(m_tcpSocket , &QTcpSocket::readyRead, this, [this]() {
            qDebug() << m_tcpSocket->bytesAvailable();
            QString l_dataReceived = byteArrayToString(m_tcpSocket->readAll());
            writeToLog(QString("RECV: <<< %1\n...\n").arg(l_dataReceived));
        });
    }

    if (m_tcpSocket->isOpen()) {
        if ((m_tcpSocket->peerAddress() != l_destIp) ||
            (m_tcpSocket->peerPort() != l_destPort)) {
            m_tcpSocket->close();
        }
    }

    if (!m_tcpSocket->isOpen()) {
        ui->statusbar->showMessage("Conectando...");
        m_tcpSocket->connectToHost(l_destIp, l_destPort);
        if (!m_tcpSocket->waitForConnected()) {
            ui->statusbar->showMessage("Falha na conexão!");
            qWarning() << m_tcpSocket->errorString();
            return;
        }
        ui->statusbar->showMessage("Conectado");
    }

    m_tcpSocket->write(ui->edtText->toPlainText().toUtf8());
    m_tcpSocket->write("\n");

    QString l_dataSent = byteArrayToString(ui->edtText->toPlainText().toUtf8() + "\n");

    m_tcpSocket->flush();
    writeToLog(QString("SENT: >>> %1\n...\n").arg(l_dataSent));
    ui->edtText->selectAll();
}
