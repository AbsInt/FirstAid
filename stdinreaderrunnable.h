#ifndef STDINREADERRUNNABLE
#define STDINREADERRUNNABLE

#include <QCoreApplication>
#include <QEvent>
#include <QRunnable>
#include <stdio.h>

class StdinReadEvent: public QEvent
{
public:
    StdinReadEvent(const QString &text): QEvent(QEvent::User)
                                       , m_text(text)
    {
    }

    QString text() const { return m_text; }

private:
    QString m_text;
};



class StdinReaderRunnable: public QRunnable
{
public:
    StdinReaderRunnable(QObject *receiver): QRunnable()
                                          , m_receiver(receiver)
    {
    }

    void run()
    {
        char buffer[256];

        while (1) {
            if (buffer == fgets(buffer, 256, stdin))
                QCoreApplication::postEvent(m_receiver, new StdinReadEvent(QString::fromLocal8Bit(buffer)), Qt::NormalEventPriority);
            
        }
    }

private:
    QObject *m_receiver;
};



#endif // #ifndef STDINREADERRUNNABLE
