﻿#ifndef ZMQMONITORTHREAD_H
#define ZMQMONITORTHREAD_H

#include <QThread>
#include <QMap>
#include "zmq.h"

class ZmqMonitorThread : public QThread
{
    Q_OBJECT
public:
    explicit ZmqMonitorThread(void* context, const QString& url, QObject *parent = 0);
protected:
    void run();
signals:
    void signalClientInOut(const QString& ip, int inout);

public slots:
private slots:
    bool GetPeerIPAndPort(int fd, QString& ip, int& port, QString& name);
    int ReadMsg(void* s, zmq_event_t* event, char* ep);
    void* SocketMonitor (void *ctx);

private:
    void*                   mContext;       //ZMQ上下文
    QString                   mUrl;
    QMap<int, QString>        mClientMap;
};

#endif // ZMQMONITORTHREAD_H
