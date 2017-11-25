#include "mainwindow.h"
#include <QApplication>


MainWindow *window;

/*PI
int interruptCounter=0;

void interruptionHanlder(void)
{
     qInfo() << "Interruption Hanlder On " << interruptCounter;
     window->pushButton_Pin21_clicked();
     interruptCounter++;
     delay(50);
}
*/

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;
    window=&w;
    w.show();

    /*PI
    pullUpDnControl(28,PUD_UP);
    wiringPiISR(28,INT_EDGE_RISING,&interruptionHanlder);
    */

    return a.exec();
}
