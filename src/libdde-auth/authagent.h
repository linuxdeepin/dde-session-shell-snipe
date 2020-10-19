#ifndef AUTHAGENT_H
#define AUTHAGENT_H

#include <QObject>
#include <atomic>

class DeepinAuthFramework;
class AuthAgent : public QObject {
    Q_OBJECT
public:
    enum AuthFlag {
        Password = 1 << 0,
        Fingerprint = 1 << 1,
        Face = 1 << 2,
        ActiveDirectory = 1 << 3
    };

    enum FingerprintStatus {
        MATCH = 0,
        NO_MATCH,
        ERROR,
        RETRY,
        DISCONNECTED
    };

    enum FpRetryStatus {
        SWIPE_TOO_SHORT = 1,
        FINGER_NOT_CENTERED,
        REMOVE_AND_RETRY
    };

    explicit AuthAgent(DeepinAuthFramework *deepin);
    ~AuthAgent();

    void Responsed(const QString& password);
    void Authenticate(const QString& username);
    int GetAuthType();
    DeepinAuthFramework *deepinAuth() { return m_deepinauth; }

signals:
    void displayErrorMsg(const QString &msg);
    void displayTextInfo(const QString &msg);
    void respondResult(const QString &msg);

private:
    static int pamConversation(int num,
                               const struct pam_message** msg,
                               struct pam_response** resp,
                               void* app_data);

private:
    DeepinAuthFramework* m_deepinauth = nullptr;
    std::atomic<bool> m_isCondition;

    QString m_password;
    AuthFlag m_authType;
    QString m_userName;
};

#endif // AUTHAGENT_H
