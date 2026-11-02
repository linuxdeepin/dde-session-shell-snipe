// SPDX-FileCopyrightText: 2011 - 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <security/pam_ext.h>
#include <security/pam_modules.h>
#include <security/_pam_types.h>
#include <sys/syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

// PAM 认证函数
PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    // pre-greeter.sh脚本用于检查是否存在需要在greeter之前执行的程序，返回0：存在 返回非0：不存在
    const char *shell_path = "/usr/share/dde-session-shell/greeters.d/pre-greeter";
    FILE *file = fopen(shell_path, "r");
    if (file != NULL) {
        fclose(file);
        pid_t pid = fork();
        if (pid == 0) {
            execl(shell_path, shell_path, (char *)NULL);
            _exit(127);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            int error_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            pam_syslog(pamh, LOG_INFO, "run %s return %d", shell_path, error_code);
            // 如果脚本没有返回错误,则需要中断自动登录，跳转至greeter
            return error_code == 0 ? PAM_PERM_DENIED : PAM_SUCCESS;
        } else {
            pam_syslog(pamh, LOG_ERR, "fork failed for %s", shell_path);
        }
    }
    pam_syslog(pamh, LOG_INFO, "%s not exist, continue auto login ", shell_path);
    return PAM_SUCCESS;
}

// PAM 设置环境函数
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    // 在这里设置凭据（如果需要）
    return PAM_SUCCESS;
}