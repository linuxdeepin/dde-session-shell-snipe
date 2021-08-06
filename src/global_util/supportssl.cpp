#include "supportssl.h"

#include <QSsl>
#include <QSslSocket>
#include <QFile>
#include <QDebug>

SupportSsl::SupportSsl(QObject *parent)
    : QObject(parent)
{
    m_sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    m_sslConfig.setProtocol(QSsl::TlsV1_0);
    m_init = loadRootCAFile();
}

bool SupportSsl::loadRootCAFile()
{
    bool ret = false;
    QFile file("/usr/share/dcmc/rootCA.pem");
    if (file.open(QIODevice::ReadOnly)) {
        m_sslConfig.setCaCertificates(QSslCertificate::fromData(file.readAll(), QSsl::Pem));
        file.close();
        ret = true;
    }

    return ret;
}

SupportSsl &SupportSsl::instance()
{
    static SupportSsl ssl;
    return ssl;
}

void SupportSsl::supportHttps(QNetworkRequest &request)
{
    if (!m_init) {  /* 判断若未初始化或失败才会重新加载 */
        m_init = loadRootCAFile();
    }

    request.setSslConfiguration(m_sslConfig);
}
