#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "ui_mainwindow.h"

#include <QtCore/QDebug>
#include <QMainWindow>
#include <QFont>
#include <QObject>
#include <QStyle>

#include "callapp.h"
#include "ComboBoxDelegate.h"

#include <QDebug>

#include <QStandardItemModel>
#include <QtNetwork>

#include <QStyleFactory>

/*PI
#include <wiringPi.h>
#include <softPwm.h>
*/

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

    void on_configView_pressed(const QModelIndex &index);

    void on_listSave_clicked();

    void on_listLoad_clicked();

    void on_listAd_clicked();

    void on_listDelete_clicked();

    void on_configView_viewportEntered();

    void on_configView_entered(const QModelIndex &index);

    void on_listUp_clicked();

    void on_listDown_clicked();

    void on_listSaveD_clicked();

    void on_listLoadD_clicked();

    void on_listDeleteD_clicked();

    void on_devicesHTML_clicked();

    void on_listClone_clicked();

    void on_configView_doubleClicked(const QModelIndex &index);

private:
    Ui::MainWindow *ui;
    callApp * appRunning;
    void executeFiniteCommand(QString command);

    //DEVICES and NET//
    QUdpSocket *udpSocket;
    QTimer *devicesTimer;
    int timerCount;
    void initUdpSocket();
    void udpTalkTo(QString msg, QHostAddress host=QHostAddress::Null,
                   bool allInterfaces=true);

    bool validDevice(QStringList msgData);
    void getDeviceStringTime(QString &string, short time);

    int getCurrentModelIndex(QTableView *tableView);

    QStandardItemModel *devicesModel;
    int currentModelIndex;

    QStandardItemModel *configModel;
    int currentConfigModelIndex;

    QString getStringModel(QStandardItemModel *model, int row, int column)
        {return model->data(model->index(row,column)).toString();}

    void setStringModel(QStandardItemModel *model, int row, int column, QString string)
        {model->setData(model->index(row,column),string);}

    int indexOfModel(QStandardItemModel *model, QString deviceMAC, int columnIndex=0, int indexFrom=0);

    void loadModel(QStandardItemModel *model, QString fileName);
    void saveModel(QStandardItemModel *model, QString fileName);

    void appendDevice(const QString &id, const QString &ip, const QString &last,
                      const QString &state, const QString &config,
                      int rowIndex=-1);
    void updateDevice(const QString &ip, const QString &last, const QString &state,
                      const QString &config, int rowIndex);

    QString getDeviceState(int row);
    void appendConfigDevice(const QString &id, const QString &nameD,
                                  const QString &widthD, const QString &heightD,
                                  const QString &styleD, int rowIndex=-1);

    void updateConfig(const QString &id, const QString &nameD,
                                  const QString &widthD, const QString &heightD,
                                  const QString &styleD, int rowIndex);

    void setDeviceFrameEnabled();
    void updateCurrentDeviceState();
    void updateCurrentDeviceConfig();
    void deviceESP01SendCMD(QString cmd);

    QString loadTextFile(QString fileName);
    int saveTextFile(QString text, QString fileName);

    void updateDevicesListHtml();
    void updateDelegateDevicesList();
    ComboBoxDelegate* delegate;
};

#endif // MAINWINDOW_H
