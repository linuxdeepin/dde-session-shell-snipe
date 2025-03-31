#ifndef ABSTRACTFULLBACKGROUND_H
#define ABSTRACTFULLBACKGROUND_H

#include <QPointer>
#include <QScreen>

class AbstractFullBackgroundInterface
{
public:
    virtual void setScreen(QPointer<QScreen> screen, bool isVisible = true) = 0;
};

#endif // ABSTRACTFULLBACKGROUND_H
