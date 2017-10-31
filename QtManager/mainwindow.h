#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFont>
#include <QObject>
#include <QStyle>
#include <wiringPi.h>
#include <softPwm.h>
#include "callapp.h"
#include <QDebug>

#include <QStandardItemModel>
#include <QtNetwork>

#include <QStyleFactory>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


public slots:
    void pushButton_Pin21_clicked();

private slots:
    void styleChanged(const QString &styleName);

    void on_dial_2_valueChanged(int value);

    void on_shutdownButton_clicked();

    void on_rebootButton_clicked();

    void cmdButton_clicked();
    void appendOutput();
    void cmdFinished();

    void on_killmeButton_clicked();

    void processPendingDatagrams();
    void queryStates();
    void setCurrentDeviceConfig();    

    void on_dialOutput2Time_sliderMoved(int position);
    void on_devicesView_pressed(const QModelIndex &index);

    void on_comboInput1Edge_currentIndexChanged(const QString &arg1);

    void on_comboOutput2State_currentIndexChanged(const QString &arg1);

    void on_dialOutput2Time_sliderReleased();

    void on_buttonESP01On_clicked();

    void on_buttonESP01Off_clicked();

    void on_buttonESP01RESET_clicked();

    void on_buttonESP01CLEAR_clicked();

private:
    Ui::MainWindow *ui;
    callApp * appRunning;
    void executeFiniteCommand(QString command);

    //DEVICES and NET//
    QUdpSocket *udpSocket=0;
    QTimer *devicesTimer;
    int timerCount=0;
    void initUdpSocket();
    void udpTalkTo(QString msg, QHostAddress host=QHostAddress::Null,
                   bool allInterfaces=true);

    bool validDevice(QStringList msgData);
    void getDeviceStringTime(QString &string, short time);

    QStandardItemModel *model;
    int currentModelIndex;


    QString getStringModel(int row, int column)
        {return model->data(model->index(row,column)).toString();}

    void setStringModel(int row, int column, QString string)
        {model->setData(model->index(row,column),string);}

    int indexOfModel(QString deviceMAC, int columnIndex=0, int indexFrom=0);

    void appendDevice(const QString &id, const QString &ip, const QString &last,
                      const QString &state, const QString &config,
                      int rowIndex=-1);
    void updateDevice(int rowIndex, const QString &ip,
                      const QString &last, const QString &state, const QString &config);

    void setDeviceFrameEnabled();
    void updateCurrentDeviceState();
    void updateCurrentDeviceConfig();
    void deviceESP01SendCMD(QString cmd);
};

#endif // MAINWINDOW_H
