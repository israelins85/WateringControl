#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void writeToLog(const QString& a_data);

private slots:
    void on_btnEnviar_clicked();

private:
    Ui::MainWindow *ui;
    QTcpSocket* m_tcpSocket = nullptr;
};
#endif // MAINWINDOW_H
