#include "callapp.h"

callApp::callApp(QString command, QWidget *parent) :
    QWidget(parent)
{
    myProcess = new QProcess;
    myProcess->start(command);
    connect(myProcess,SIGNAL(finished(int)),this,SIGNAL(appClosed()));
    connect(myProcess,SIGNAL(readyReadStandardError()),this,SLOT(newStderr()));
    connect(myProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(newStdout()));
}

bool callApp::isRuning()
{
    if(myProcess!=0)
    {
        if(myProcess->state()==QProcess::Running)
            return 1;
    }
    return 0;
}

void callApp::newStderr()
{
    myProcess->setReadChannel(QProcess::StandardError);
    data = myProcess->readAll();
    emit newData();
}

void callApp::newStdout()
{
    myProcess->setReadChannel(QProcess::StandardOutput);
    data = myProcess->readAll();
    emit newData();
}

void callApp::deleteProcess()
{
    if(myProcess!=0)
    {
        myProcess->terminate();
        myProcess->deleteLater();
    }
}

void callApp::writeData(QString outData)
{
    if(isRuning())
        myProcess->write(outData.toLatin1().data());
}
