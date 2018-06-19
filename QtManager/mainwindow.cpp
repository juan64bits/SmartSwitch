#include "mainwindow.h"
#include "ui_mainwindow.h"

#define BROADCAST_REFRESH   1000
#define BROADCAST_PORT      45454
#define DATE_TIME_FORMAT    "dd/MM/yyyy HH:mm:ss"
#define TIME_FORMAT         "HH:mm:ss"
#define CONFIG_FILENAME     "/home/juan64bits/Projects/testing/config"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    foreach (QString styleName, QStyleFactory::keys()) {
        ui->style->addItem(styleName);
        if (style()->objectName().toLower() == styleName.toLower())
            ui->style->setCurrentIndex(ui->style->count() - 1);
    }

    /*PI
    wiringPiSetup () ;
    softPwmCreate (29,0,100);
    */
    connect(ui->style, SIGNAL(activated(QString)),
            this, SLOT(styleChanged(QString)));
    connect(ui->pushButton_Pin21,SIGNAL(clicked(bool)),
            this,SLOT(pushButton_Pin21_clicked()));
    connect(ui->runButton,SIGNAL(clicked(bool)),
            this,SLOT(cmdButton_clicked()));

/*DEVICES VIEW*/
    devicesModel = new QStandardItemModel(0,5,parent);
    devicesModel->setHeaderData(0, Qt::Horizontal, QObject::tr("Device ID"));
    devicesModel->setHeaderData(1, Qt::Horizontal, QObject::tr("Device IP"));
    devicesModel->setHeaderData(2, Qt::Horizontal, QObject::tr("Last Seen"));
    devicesModel->setHeaderData(3, Qt::Horizontal, QObject::tr("Status"));
    devicesModel->setHeaderData(4, Qt::Horizontal, QObject::tr("Config"));

    ui->devicesView->setModel(devicesModel);
    currentModelIndex=-1;
    udpSocket = new QUdpSocket(this);
    timerCount=0;
    initUdpSocket();

    devicesTimer = new QTimer(this);
    connect(devicesTimer, SIGNAL(timeout()), this, SLOT(queryStates()));
    devicesTimer->start(BROADCAST_REFRESH);

/* DEVICES CONFIG (PLAN) */
    configModel = new QStandardItemModel(0,5,parent);
    configModel->setHeaderData(0, Qt::Horizontal, QObject::tr("Device ID"));
    configModel->setHeaderData(1, Qt::Horizontal, QObject::tr("Device Name"));
    configModel->setHeaderData(2, Qt::Horizontal, QObject::tr("Width"));
    configModel->setHeaderData(3, Qt::Horizontal, QObject::tr("Height"));
    configModel->setHeaderData(4, Qt::Horizontal, QObject::tr("Style"));
    ui->configView->setModel(configModel);
    currentConfigModelIndex=-1;
}

void  MainWindow::initUdpSocket()
{
    udpSocket->bind(BROADCAST_PORT, QUdpSocket::ShareAddress);
    connect(udpSocket, SIGNAL(readyRead()),
            this, SLOT(processPendingDatagrams()));
    ui->netLog->appendPlainText("***Broadcast listening***\n");
}

void MainWindow::appendDevice(const QString &id, const QString &ip,
                              const QString &last, const QString &state,
                              const QString &config, int rowIndex)
{
    if(rowIndex==-1)
        rowIndex=devicesModel->rowCount();
    devicesModel->insertRow(rowIndex);
    setStringModel(devicesModel,rowIndex, 0, id);
    setStringModel(devicesModel,rowIndex, 1, ip);
    setStringModel(devicesModel,rowIndex, 2, last);
    setStringModel(devicesModel,rowIndex, 3, state);
    setStringModel(devicesModel,rowIndex, 4, config);
}


void MainWindow::updateDevice(const QString &ip,
                              const QString &last, const QString &state,
                              const QString &config, int rowIndex)
{
    setStringModel(devicesModel,rowIndex, 1, ip);
    setStringModel(devicesModel,rowIndex, 2, last);
    setStringModel(devicesModel,rowIndex, 3, state);
    setStringModel(devicesModel,rowIndex, 4, config);
}

void MainWindow::appendConfigDevice(const QString &id, const QString &nameD,
                              const QString &widthD, const QString &heightD,
                              const QString &styleD, int rowIndex)
{
    if(rowIndex==-1)
        rowIndex=configModel->rowCount();
    configModel->insertRow(rowIndex);
    setStringModel(configModel,rowIndex, 0, id);
    setStringModel(configModel,rowIndex, 1, nameD);
    setStringModel(configModel,rowIndex, 2, widthD);
    setStringModel(configModel,rowIndex, 3, heightD);
    setStringModel(configModel,rowIndex, 4, styleD);
}

void MainWindow::updateConfig(const QString &id, const QString &nameD,
                              const QString &widthD, const QString &heightD,
                              const QString &styleD, int rowIndex)
{
    setStringModel(configModel,rowIndex, 0, id);
    setStringModel(configModel,rowIndex, 1, nameD);
    setStringModel(configModel,rowIndex, 2, widthD);
    setStringModel(configModel,rowIndex, 3, heightD);
    setStringModel(configModel,rowIndex, 4, styleD);
}

void MainWindow::queryStates()
{    
    udpTalkTo(tr("%1:UPD:NOW").arg(QTime::currentTime().toString(TIME_FORMAT)));

    /*Check connections*/
    QDateTime currentTime= QDateTime::currentDateTime();
    for(int i=0; i<devicesModel->rowCount();i++)
    {        
        QDateTime lastSeen=QDateTime::fromString(
                    getStringModel(devicesModel,i,2),DATE_TIME_FORMAT);

        if(lastSeen.msecsTo(currentTime)>BROADCAST_REFRESH*3 && getStringModel(devicesModel,i,1)!="OFFLINE")
        {
            setStringModel(devicesModel,i, 1, "OFFLINE");
            if(i==currentModelIndex)
                setDeviceFrameEnabled();
        }
    }
}

void MainWindow::deviceESP01SendCMD(QString cmd)
{
    int index = currentModelIndex;
    QStringList currentConfig = getStringModel(devicesModel,index,4).split(':');
    QString currentAddress= getStringModel(devicesModel,index,1);

    if(currentConfig.at(0)=="ESP01" && currentConfig.size() == 3
            && currentAddress != "IP CONFLICT")
    {
qDebug() << "sending command" << cmd << " to " << currentAddress;
        udpTalkTo(tr("%1:SET:%2").arg(QTime::currentTime().toString(TIME_FORMAT))
                    .arg(cmd),QHostAddress(currentAddress));
    }
}

void MainWindow::setCurrentDeviceConfig()
{
    int index = currentModelIndex;
    QStringList currentConfig = getStringModel(devicesModel,index,4).split(':');
    QString currentAddress= getStringModel(devicesModel,index,1);

    if(currentConfig.at(0)=="ESP01" && currentConfig.size() == 3
            && currentAddress != "IP CONFLICT")
    {
        QString newConfig = "C1,I";

        if(ui->comboInput1Edge->currentText()=="Falling")
            newConfig+="F,O";
        else if(ui->comboInput1Edge->currentText()=="Rising")
            newConfig+="R,O";
        else
            newConfig+="X,O";

        if(ui->comboOutput2State->currentText()=="Low")
            newConfig+="L,";
        else if(ui->comboOutput2State->currentText()=="High")
            newConfig+="H,";
        else
            newConfig+="X,";

        newConfig+=QString::number(ui->dialOutput2Time->value(),10);

        if(currentConfig.at(1) != newConfig)
        {
            //Set and update current device configuration
qDebug() << "Setting" << newConfig << currentAddress;
            currentConfig.replace(1,newConfig);
            setStringModel(devicesModel,index, 4,currentConfig.join(':'));
            udpTalkTo(tr("%1:SET:%2").arg(QTime::currentTime().toString(TIME_FORMAT))
                      .arg(newConfig),QHostAddress(currentAddress));
        }
    }

}

int MainWindow::indexOfModel(QStandardItemModel *model, QString string, int columnIndex, int indexFrom)
{
    if(indexFrom>=model->rowCount())
        return -1;

    for(int i=indexFrom; i<model->rowCount();i++)
        if(getStringModel(model,i,columnIndex)==string)
            return i;

    return -1;
}

void MainWindow::loadModel(QStandardItemModel *model, QString fileName)
{
    QFile file(fileName);
    if(file.open(QIODevice::ReadOnly))
    {
        model->removeRows(0,model->rowCount());
        QDataStream stream(&file);
        int rowCount, columnCount;
        stream >> rowCount;
        stream >> columnCount;

        for(int row = 0; row < rowCount; row++)
        {
            if(model->rowCount()<=row)
                model->insertRow(row);
            for(int column = 0; column < columnCount; column++)
            {
                QString item;
                stream >> item;
                setStringModel(model,row,column,item);
            }
        }
    }
}

void MainWindow::saveModel(QStandardItemModel *model, QString fileName)
{
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly))
    {
        QDataStream stream(&file);
        int rowCount = model->rowCount();
        int columnCount = model->columnCount();
        stream << rowCount;
        stream << columnCount;

        for(int row = 0; row < rowCount; row++)
        {
            for(int column = 0; column < columnCount; column++)
            {
                stream << getStringModel(model,row,column);
            }
        }
    }
    else
    {
        qInfo()<<"Failed opening file to save." << fileName;
    }
}


void MainWindow::setDeviceFrameEnabled()
{
    ui->frameESP01->setEnabled(getStringModel(devicesModel,currentModelIndex,1)!="OFFLINE");
}

void MainWindow::updateCurrentDeviceState()
{
    //Update state
    QString deviceInputValue = "UNKNOWN";
    QString deviceOutputState  = "UNKNOWN";
    QString deviceOutputTimeChange  = "UNKNOWN";

    QList<QString> stateList = getStringModel(devicesModel,currentModelIndex,3).split(',');
    if(stateList.size()==4)
    {
        if(stateList.at(1)=="IH")
            deviceInputValue = "HIGH";
        else if(stateList.at(1)=="IL")
            deviceInputValue = "LOW";

        if(stateList.at(2)=="OH")
            deviceOutputState = "HIGH";
        else if(stateList.at(2)=="OL")
            deviceOutputState = "LOW";

        bool ok;
        short time = stateList.at(3).toShort(&ok,16);
        if(ok)
            getDeviceStringTime(deviceOutputTimeChange,time);

    }
    ui->labelInput1Value->setText(deviceInputValue);
    ui->labelOutput2State->setText(deviceOutputState);
    ui->labelOutput2TimeChange->setText(deviceOutputTimeChange);
}

void MainWindow::updateCurrentDeviceConfig()
{
    //Update Config
    QString deviceInputEdge  = "UNKNOWN";
    QString deviceOutputSet  = "UNKNOWN";
    int outputTime=-1;
    QString deviceMAC = getStringModel(devicesModel,currentModelIndex,0);

    QList<QString> configSplit = getStringModel(devicesModel,currentModelIndex,4).split(':');

    if(configSplit.size()==3)
    {
        deviceMAC = configSplit.at(0) + " @ " + deviceMAC;
        QList<QString> configList = configSplit.at(1).split(',');
        if(configList.size()==4)
        {
            if(configList.at(1)=="IR")
                deviceInputEdge = "Rising";
            else if(configList.at(1)=="IF")
                deviceInputEdge = "Falling";

            if(configList.at(2)=="OH")
                deviceOutputSet = "High";
            else if(configList.at(2)=="OL")
                deviceOutputSet = "Low";

            bool ok;
            short time = configList.at(3).toShort(&ok,16);
            if(ok)
            {
                outputTime=time;
                QString newText;
                getDeviceStringTime(newText,time);
                ui->labelOutput2Time->setText(newText);
            }
            else
                ui->labelOutput2Time->setText("UNKNOWN");

        }
    }
    ui->labelDeviceMAC->setText(deviceMAC);
    ui->comboInput1Edge->setEditText(deviceInputEdge);
    ui->comboOutput2State->setEditText(deviceOutputSet);
    ui->dialOutput2Time->setValue(outputTime);        
}

void MainWindow::getDeviceStringTime(QString &string, short time)
{
    if(time==0)
        string = "Never change";
    else if(time <= 60)
        string = QString::number(time) + " sec";
    else if(time > 60 && time<255)
        if(time >120)
            string = QString::number((time-60)/60) + " hr " +
                     QString::number((time-60)%60) + " min";
        else
            string = QString::number(time-60) + " min";
    else if(time == 255)
        string = "Set for ever";
}

bool MainWindow::validDevice(QStringList msgData)
{
    //Quickly check!!
    if(msgData.size()!=4)
        return false;
    //A valid MAC?
    if(msgData.at(0).split(':').size()!=6)
        return false;
    //Valid type response:
    if(msgData.at(1) != "ACK" && msgData.at(1) != "UPD")
        return false;
    //valid status:
    if(msgData.at(2).split(',').size()!=4)
        return false;
    //valid config:
    if(msgData.at(3).split(':').size()!=3)
        return false;

    return true;
}

void MainWindow::processPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        QHostAddress t_hostAddress;
        quint16 hostPort;
        datagram.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(datagram.data(), datagram.size(),
                                &t_hostAddress,&hostPort);
        QString hostAddress=QHostAddress(t_hostAddress.toIPv4Address()).toString();

        //Process data
        QStringList msgData = QString(datagram).split('#');
        if(validDevice(msgData))
        {            
            //Data process
            QString hostMAC = msgData.at(0); //MAC
            QString hostACK = msgData.at(1); //CMD - ACK
            QString hostSTT = msgData.at(2); //STATE
            QString hostCFG = msgData.at(3); //CONFIG
            QString hostLastSeen = QDateTime::currentDateTime()
                               .toString(DATE_TIME_FORMAT);

            //Update devices data
            int macIndex=indexOfModel(devicesModel,hostMAC,0);
            if(macIndex==-1)
            {
                //New device, append
                appendDevice(hostMAC,hostAddress,hostLastSeen,hostSTT,hostCFG);
            }
            else
            {
                QString t_hostSTT = getStringModel(devicesModel,macIndex,3);
                QString t_hostCFG = getStringModel(devicesModel,macIndex,4);
                //Device already exists, update
                updateDevice(hostAddress,hostLastSeen,hostSTT,hostCFG,macIndex);

                if(macIndex==currentModelIndex)
                {
                    setDeviceFrameEnabled();
                    if(t_hostSTT!= hostSTT)
                        updateCurrentDeviceState();
                    if(t_hostCFG!= hostCFG)
                        updateCurrentDeviceConfig();
                }
            }

            //IP Address List Depuration//
            int ipIndex=indexOfModel(devicesModel,hostAddress,1);
            while(ipIndex!=-1)
            {
                if(getStringModel(devicesModel,ipIndex,0)!= hostMAC)
                    setStringModel(devicesModel,ipIndex, 1, "IP CONFLICT");
                ipIndex=indexOfModel(devicesModel,hostAddress,1,ipIndex+1);
            }

            /*TODO: process ACK*/
        }

    }
}

void MainWindow::udpTalkTo(QString msg, QHostAddress host, bool allInterfaces)
{
    QByteArray datagram = msg.toUtf8();
    if(host!=QHostAddress::Null)
    {
        udpSocket->writeDatagram(datagram,host, BROADCAST_PORT);
    }
    else
    {
        if(!allInterfaces)
        {
            udpSocket->writeDatagram(datagram.data(), datagram.size(),
                                     QHostAddress::Broadcast, BROADCAST_PORT);
        }
        else
        {   //TO EVERYONE ON ALL INTERFACES
            QList<QNetworkInterface> ifaces=QNetworkInterface::allInterfaces();
            for (int i = 0; i < ifaces.size(); i++)
            {
                QList<QNetworkAddressEntry> addrs = ifaces[i].addressEntries();
                for (int j = 0; j < addrs.size(); j++)
                    if((addrs[j].ip().protocol()==QAbstractSocket::IPv4Protocol)
                            && (addrs[j].broadcast().toString() != ""))
                       udpSocket->writeDatagram(datagram.data(),datagram.size(),
                                          addrs[j].broadcast(), BROADCAST_PORT);
            }
        }
      }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::executeFiniteCommand(QString command)
{
    QEventLoop waitToFinish;
    callApp cmd(command);
    connect(&cmd, SIGNAL(appClosed()), &waitToFinish, SLOT(quit()));
    waitToFinish.exec();
    cmd.deleteProcess();
}

void MainWindow::cmdButton_clicked()
{

    appRunning = new callApp(ui->cmdLineEdit->text());
    connect(appRunning,SIGNAL(newData()),
            this,SLOT(appendOutput()));
    connect(appRunning,SIGNAL(appClosed()),
            this,SLOT(cmdFinished()));
}
void MainWindow::cmdFinished()
{
    if(appRunning!=0)
        appRunning->deleteProcess();
    appRunning=0;
}

void MainWindow::appendOutput()
{
    ui->outputTextEdit->appendPlainText(appRunning->readData());
}

static void setStyleHelper(QWidget *widget, QStyle *style)
{
    widget->setStyle(style);
    widget->setPalette(style->standardPalette());
    foreach (QObject *child, widget->children()) {
        if (QWidget *childWidget = qobject_cast<QWidget *>(child))
            setStyleHelper(childWidget, style);
    }
}

void MainWindow::styleChanged(const QString &styleName)
{
    QStyle *style = QStyleFactory::create(styleName);
    if (style)
        setStyleHelper(this, style);
}

void MainWindow::pushButton_Pin21_clicked()
{

    if(ui->pushButton_Pin21->text()=="On Pin 21")
    {

        ui->pushButton_Pin21->setText("Off Pin 21");
        ui->dial_2->setValue(100);
    }
    else
    {

        ui->pushButton_Pin21->setText("On Pin 21");
        ui->dial_2->setValue(0);
    }

}



void MainWindow::on_dial_2_valueChanged(int value)
{
    /*PI
    softPwmWrite (29,value);
    */
}

void MainWindow::on_shutdownButton_clicked()
{
    executeFiniteCommand("sudo shutdown -h now");
}

void MainWindow::on_rebootButton_clicked()
{
    executeFiniteCommand("sudo reboot");
}

void MainWindow::on_killmeButton_clicked()
{
    executeFiniteCommand("killall -9 qSmartSwitch");
}

void MainWindow::on_dialOutput2Time_sliderMoved(int position)
{
    QString newText;
    getDeviceStringTime(newText,position);
    ui->labelOutput2Time->setText(newText);
}


void MainWindow::on_comboInput1Edge_currentIndexChanged(const QString &arg1)
{
qDebug() << "from Changed EDGE";
    if(arg1=="Rising" || arg1=="Falling")
        setCurrentDeviceConfig();
}

void MainWindow::on_comboOutput2State_currentIndexChanged(const QString &arg1)
{
qDebug() << "from Changed STATE";
    if(arg1=="High" || arg1=="Low")
        setCurrentDeviceConfig();
}

void MainWindow::on_dialOutput2Time_sliderReleased()
{
    QString newText;
    getDeviceStringTime(newText,ui->dialOutput2Time->value());
    ui->labelOutput2Time->setText(newText);
qDebug() << "from Released DIAL";
    setCurrentDeviceConfig();
}

void MainWindow::on_buttonESP01On_clicked()
{
    deviceESP01SendCMD( "FORCEON");
}

void MainWindow::on_buttonESP01Off_clicked()
{
    deviceESP01SendCMD( "FORCEOFF");
}

void MainWindow::on_buttonESP01RESET_clicked()
{
    deviceESP01SendCMD("RESETDEVICE");
}

void MainWindow::on_buttonESP01CLEAR_clicked()
{
    deviceESP01SendCMD("CLEAREPROM");
}

void MainWindow::on_configView_pressed(const QModelIndex &index)
{
    currentConfigModelIndex=index.row();
}

void MainWindow::on_configView_entered(const QModelIndex &index)
{
    currentConfigModelIndex=index.row();
}

void MainWindow::updateCurrentIndexConfig()
{
    QModelIndexList listIndexes = ui->configView->selectionModel()->selection().indexes();
    if(listIndexes.count()>0)
        currentConfigModelIndex = listIndexes.first().row();
    else
       currentConfigModelIndex=-1;
}

void MainWindow::on_configView_viewportEntered()
{
qInfo()<<"View Entered";
}

void MainWindow::on_devicesView_pressed(const QModelIndex &index)
{
    currentModelIndex=index.row();
    setDeviceFrameEnabled();
    updateCurrentDeviceState();
    updateCurrentDeviceConfig();
}


void MainWindow::on_listSave_clicked()
{
    saveModel(configModel,CONFIG_FILENAME);
}

void MainWindow::on_listLoad_clicked()
{
    loadModel(configModel,CONFIG_FILENAME);
}

void MainWindow::on_listAd_clicked()
{
    appendConfigDevice("MAC"+QString::number(configModel->rowCount()),"NAME","100%","100px","border:solid 1px white;");
}

void MainWindow::on_listDelete_clicked()
{
    updateCurrentIndexConfig();
    if(currentConfigModelIndex>=0)
    {
        configModel->removeRow(currentConfigModelIndex);
    }
}


void MainWindow::on_listUp_clicked()
{
    updateCurrentIndexConfig();
    if(currentConfigModelIndex>0)
    {
        QList<QStandardItem*> itemList = configModel->takeRow(currentConfigModelIndex);
        configModel->insertRow(currentConfigModelIndex-1,itemList);
        ui->configView->selectRow(currentConfigModelIndex-1);
    }
}

void MainWindow::on_listDown_clicked()
{
    updateCurrentIndexConfig();
    if(currentConfigModelIndex<configModel->rowCount()-1 && currentConfigModelIndex != -1)
    {
        QList<QStandardItem*> itemList = configModel->takeRow(currentConfigModelIndex);
        configModel->insertRow(currentConfigModelIndex+1,itemList);
        ui->configView->selectRow(currentConfigModelIndex+1);
    }
}
