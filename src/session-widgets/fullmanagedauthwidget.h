#ifndef FULLMANAGEDAUTHWIDGET_H
#define FULLMANAGEDAUTHWIDGET_H

#include "auth_widget.h"
#include "userinfo.h"
#include "authcommon.h"

class AuthModule;
class KbLayoutWidget;
class KeyboardMonitor;

class FullManagedAuthWidget : public AuthWidget
{
    Q_OBJECT
public:
    explicit FullManagedAuthWidget(QWidget *parent = nullptr);
    ~FullManagedAuthWidget() override;

    void setModel(const SessionBaseModel *model) override;
    void setAuthType(const int type) override;
    void setAuthState(const int type, const int state, const QString &message) override;
    inline bool isPluginLoaded() const { return m_isPluginLoaded; }

public slots:
    void onRequestChangeAuth(const int authType);

protected:
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;

private:
    void initUI();
    void initConnections();
    void initCustomAuth();
    void checkAuthResult(const int type, const int state) override;

private:
    QVBoxLayout *m_mainLayout;
    AuthCommon::AuthType m_currentAuthType;
    static QList<FullManagedAuthWidget *> FullManagedAuthWidgetObjs;
    bool m_inited;
    bool m_isPluginLoaded;
};

#endif // FULLMANAGEDAUTHWIDGET_H
