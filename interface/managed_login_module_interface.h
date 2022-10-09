// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MANAGEDLOGINMODULEINTERFACE_H
#define MANAGEDLOGINMODULEINTERFACE_H

#include "login_module_interface.h"

namespace dss {
namespace module {

// expend module type for someone who do not like uos login UI
// for this type, greeter and lock would use UI and auth from plugin
class ManagedLoginModuleInterface : public LoginModuleInterface
{
private:
    // new type added for LoginModuleInterface
    virtual ModuleType getType() const override { return FullManagedLoginType; }
};

} // namespace module
} // namespace dss

Q_DECLARE_INTERFACE(dss::module::ManagedLoginModuleInterface, "com.deepin.dde.shell.Modules.ManagedLogin")

#endif // MANAGEDLOGINMODULEINTERFACE_H
