#include "DataSource.h"
#include "LogFlux.h"

#include <QColor>
#include <QPalette>
#include <QStyleFactory>
#include <QtWidgets/QApplication>

#ifdef _WIN32
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")

// Some SDKs don't define this yet
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static void enableDarkTitleBar(HWND hwnd)
{
    if (!hwnd)
        return;

    BOOL value = TRUE;

    // Try the modern attribute first (Win10 1809+)
    if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value))))
    {
        // Fallback for older builds (rare now, but safe)
        const DWORD fallback = 19;
        DwmSetWindowAttribute(hwnd, fallback, &value, sizeof(value));
    }
}
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Setup dark theme
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Highlight, QColor(142, 45, 197));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    // Explicitly set the Disabled color group so disabled widgets look "grayed"
    darkPalette.setColor(QPalette::Disabled, QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
    darkPalette.setColor(QPalette::Disabled, QPalette::Base, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 110, 110));
    darkPalette.setColor(QPalette::Disabled, QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
    darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(130, 130, 130));

    app.setPalette(darkPalette);
    // without this the dark palette would be partially ignored.
    app.setStyle(QStyleFactory::create("Fusion"));

    app.setWindowIcon(QIcon(":/images/app.png"));

    LogFlux window;
    window.show();

#ifdef _WIN32
    // Must be called after show() — the native HWND is not created until
    // the window is first shown, so winId() would return 0 before that.
    HWND hwnd = reinterpret_cast<HWND>(window.winId());
    enableDarkTitleBar(hwnd);
#endif

    return app.exec();
}
