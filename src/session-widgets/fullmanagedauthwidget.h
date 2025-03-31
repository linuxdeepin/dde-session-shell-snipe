#ifndef FULLMANAGEDAUTHWIDGET_H
#define FULLMANAGEDAUTHWIDGET_H

#include "auth_widget.h"
#include "userinfo.h"
#include "authcommon.h"
#include "pluginconfigmap.h"

class AuthModule;
class KbLayoutWidget;
class KeyboardMonitor;

namespace dss {
namespace module {
class LoginModuleInterface;
};
}; // namespace dss

class FullManagedAuthWidget : public AuthWidget
    , public PluginConfig
{
    Q_OBJECT
public:
    explicit FullManagedAuthWidget(QWidget *parent = nullptr);
    ~FullManagedAuthWidget() override;

    void setModel(const SessionBaseModel *model) override;
    void setAuthType(const AuthCommon::AuthFlags type) override;
    void setAuthState(const AuthCommon::AuthType type, const AuthCommon::AuthState state, const QString &message) override;
    bool isPluginLoaded() const;
    virtual bool isUserSwitchButtonVisiable() const override;

public slots:
    void onRequestChangeAuth(const AuthCommon::AuthType authType);

protected:
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;

private:
    void initUI();
    void hideInternalControls();
    void initConnections();
    void initCustomAuth();
    void checkAuthResult(const AuthCommon::AuthType type, const AuthCommon::AuthState state) override;

private:
    QVBoxLayout *m_mainLayout;
    AuthCommon::AuthType m_currentAuthType;
    static QList<FullManagedAuthWidget *> FullManagedAuthWidgetObjs;
    bool m_inited;
    bool m_isPluginLoaded;
};

#endif // FULLMANAGEDAUTHWIDGET_H
