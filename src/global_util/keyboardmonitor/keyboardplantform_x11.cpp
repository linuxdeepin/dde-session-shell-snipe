// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardplantform_x11.h"
#include "constants.h"

#ifdef ENABLE_DSS_SNIPE
#include <QGuiApplication>
#else
#include <QX11Info>
#endif
#include <QDebug>

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <stdio.h>

static int xi2_opcode;

int KeyboardPlatformX11::xinput_version(Display *display)
{
    XExtensionVersion *version;
    static int vers = -1;

    if (vers != -1)
        return vers;

    version = XGetExtensionVersion(display, INAME);

    if (version && (version != (XExtensionVersion*) NoSuchExtension)) {
        vers = version->major_version;
        XFree(version);
    }

    /* Announce our supported version so the server treats us correctly. */
    if (vers >= XI_2_Major)
    {
        const char *forced_version;
        int maj = 2, min = 2;

        forced_version = getenv("XINPUT_XI2_VERSION");
        if (forced_version) {
            if (sscanf(forced_version, "%d.%d", &maj, &min) != 2) {
                fprintf(stderr, "Invalid format of XINPUT_XI2_VERSION "
                                "environment variable. Need major.minor\n");
                exit(1);
            }
            printf("Overriding XI2 version to: %d.%d\n", maj, min);
        }

        XIQueryVersion(display, &maj, &min);
    }

    return vers;
}

void KeyboardPlatformX11::select_events(Display* display)
{
    XIEventMask m;
    m.deviceid = XIAllMasterDevices;
    m.mask_len = XIMaskLen(XI_LASTEVENT);
    m.mask = (unsigned char*)calloc(m.mask_len, sizeof(char));

    //    XISetMask(m.mask, XI_RawKeyPress);
    XISetMask(m.mask, XI_RawKeyRelease);
    //    XISetMask(m.mask, XI_RawButtonPress);
    //    XISetMask(m.mask, XI_RawButtonRelease);
    //    XISetMask(m.mask, XI_RawMotion);
    //    XISetMask(m.mask, XI_RawTouchBegin);
    //    XISetMask(m.mask, XI_RawTouchUpdate);
    //    XISetMask(m.mask, XI_RawTouchEnd);

    XISelectEvents(display, DefaultRootWindow(display), &m, 1);

    free(m.mask);
    XSync(display, False);
}

int KeyboardPlatformX11::listen(Display *display)
{
    Window root = DefaultRootWindow(display);
    int root_x, root_y, nouse;
    Window noused_window;
    unsigned int mask;

    while(1)
    {
        XEvent ev;
        XGenericEventCookie *cookie = static_cast<XGenericEventCookie*>(&ev.xcookie);
        XNextEvent(display, static_cast<XEvent*>(&ev));

        if (XGetEventData(display, cookie) &&
                cookie->type == GenericEvent &&
                cookie->extension == xi2_opcode)
        {
            XQueryPointer(display, root, &noused_window, &noused_window, &root_x, &root_y, &nouse, &nouse, &mask);
            XIRawEvent* event = static_cast<XIRawEvent*>(cookie->data);
            switch (cookie->evtype)
            {
            case XI_RawButtonPress:
                /*printf("Button press: Detail(%d), X(%d), Y(%d), Mask(%u)\n", event->detail, root_x, root_y, mask);*/
                break;
            case XI_RawButtonRelease:
                /*printf("Button release: Detail(%d), X(%d), Y(%d), Mask(%u)\n", event->detail, root_x, root_y, mask);*/
                break;
            case XI_RawKeyPress:
                break;
            case XI_RawKeyRelease:
                if (event->detail == 66) { // check if the key pressed is capslock first.
                    emit capsLockStatusChanged(isCapsLockOn());
                } else if (event->detail == 77) {
                    emit numLockStatusChanged(isNumLockOn());
                }
                /*printf("Key release: Detail(%d), X(%d), Y(%d), Mask(%u)\n", event->detail, root_x, root_y, mask);*/
                break;
            case XI_RawTouchBegin:
                //                printf("Touch begin: Detail(%d), X(%d), Y(%d), Mask(%u)\n", event->detail, root_x, root_y, mask);
                break;
            case XI_RawTouchUpdate:
                //                printf("Touch update: Detail(%d), X(%d), Y(%d), Mask(%u)\n", event->detail, root_x, root_y, mask);
                break;
            case XI_RawTouchEnd:
                //                printf("Touch end: Detail(%d), X(%d), Y(%d), Mask(%u)\n", event->detail, root_x, root_y, mask);
                break;
            }
        }

        XFreeEventData(display, cookie);
    }
    return EXIT_SUCCESS;
}

#ifndef ENABLE_DSS_SNIPE
KeyboardPlatformX11::KeyboardPlatformX11(QObject *parent)
    : KeyBoardPlatform(parent)
{

}

bool KeyboardPlatformX11::isCapsLockOn()
{
    bool result = false;
    unsigned int n = 0;
    static Display *d = QX11Info::display();
    if (d != nullptr) {
        XkbGetIndicatorState(d, XkbUseCoreKbd, &n);
        result = (n & 0x01) != 0;
    }
    return result;
}

bool KeyboardPlatformX11::isNumLockOn()
{
    bool result;
    unsigned int n = 0;
    static Display* d = QX11Info::display();
    if (!d)
        return false;

    XkbGetIndicatorState(d, XkbUseCoreKbd, &n);
    result = (n & 0x02) != 0;

    return result;
}

bool KeyboardPlatformX11::setNumLockStatus(const bool &on)
{
    Display* d = QX11Info::display();
    if (!d)
        return false;

    XKeyboardState x;
    XGetKeyboardControl(d, &x);
    const bool numLockEnabled = x.led_mask & 2;

    if (numLockEnabled == on) {
        return true;
    }

    // Get the keycode for XK_Caps_Lock keysymbol
    unsigned int keycode = XKeysymToKeycode(d, XK_Num_Lock);

    // Simulate Press
    int pressExit = XTestFakeKeyEvent(d, keycode, True, CurrentTime);
    XFlush(d);

    // Simulate Release
    int releseExit = XTestFakeKeyEvent(d, keycode, False, CurrentTime);
    XFlush(d);

    return pressExit == 0 && releseExit == 0;
}

void KeyboardPlatformX11::run()
{
    Display* display = XOpenDisplay(nullptr);
    int event, error;

    if (!XQueryExtension(display, "XInputExtension", &xi2_opcode, &event, &error)) {
        fprintf(stderr, "XInput2 not available.\n");
        return;
    }

    if (!xinput_version(display)) {
        fprintf(stderr, "XInput2 extension not available\n");
        return;
    }

    select_events(display);
    listen(display);
}

void KeyboardPlatformX11::ungrabKeyboard()
{
    static Display *d = QX11Info::display();
    if (d == NULL) {
        fprintf(stderr, "display pointer is NULL when try ungrab keyboard.\n");
        return;
    }

    XUngrabKeyboard(d, CurrentTime);
}
#else
KeyboardPlatformX11::KeyboardPlatformX11(QObject *parent)
    : KeyBoardPlatform(parent)
{
    if (auto x11Application = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
        m_display = x11Application->display();
}

bool KeyboardPlatformX11::isCapsLockOn()
{
    bool result = false;
    unsigned int n = 0;
    if (m_display) {
        XkbGetIndicatorState(m_display, XkbUseCoreKbd, &n);
        result = (n & 0x01) != 0;
    }

    return result;
}

bool KeyboardPlatformX11::isNumLockOn()
{
    bool result = false;
    unsigned int n = 0;
    if (!m_display)
        return false;

    XkbGetIndicatorState(m_display, XkbUseCoreKbd, &n);
    result = (n & 0x02) != 0;

    return result;
}

bool KeyboardPlatformX11::setNumLockStatus(const bool &on)
{
    if (!m_display)
        return false;

    XKeyboardState x;
    XGetKeyboardControl(m_display, &x);
    const bool numLockEnabled = x.led_mask & 2;

    if (numLockEnabled == on) {
        return true;
    }

    // Get the keycode for XK_Caps_Lock keysymbol
    unsigned int keycode = XKeysymToKeycode(m_display, XK_Num_Lock);

    // Simulate Press
    int pressExit = XTestFakeKeyEvent(m_display, keycode, True, CurrentTime);
    XFlush(m_display);

    // Simulate Release
    int releseExit = XTestFakeKeyEvent(m_display, keycode, False, CurrentTime);
    XFlush(m_display);

    return pressExit == 0 && releseExit == 0;
}

void KeyboardPlatformX11::run()
{
    Display* display = XOpenDisplay(nullptr);
    int event, error;

    if (!XQueryExtension(display, "XInputExtension", &xi2_opcode, &event, &error)) {
        fprintf(stderr, "XInput2 not available.\n");
        return;
    }

    if (!xinput_version(display)) {
        fprintf(stderr, "XInput2 extension not available\n");
        return;
    }

    select_events(display);
    listen(display);
}

void KeyboardPlatformX11::ungrabKeyboard()
{
    if (!m_display) {
        fprintf(stderr, "display pointer is NULL when try ungrab keyboard.\n");
        return;
    }

    XUngrabKeyboard(m_display, CurrentTime);
}
#endif