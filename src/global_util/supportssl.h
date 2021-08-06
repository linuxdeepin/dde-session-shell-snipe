#ifndef SUPPORTSSL_H
#define SUPPORTSSL_H

#include <QObject>
#include <QNetworkRequest>
#include <QSslConfiguration>

class SupportSsl : public QObject
{
    Q_OBJECT
public:
    static SupportSsl &instance();
    void supportHttps(QNetworkRequest &request);

private:
    explicit SupportSsl(QObject *parent = nullptr);
    bool loadRootCAFile();

private:
    QSslConfiguration m_sslConfig;
    bool m_init;
};

#endif // SUPPORTSSL_H
