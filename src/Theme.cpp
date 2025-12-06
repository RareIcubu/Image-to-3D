#include "Theme.h"
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QStyle>

namespace Theme {

void applyDarkPalette(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette dark;
    dark.setColor(QPalette::Window, QColor(53,53,53));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Base, QColor(42,42,42));
    dark.setColor(QPalette::AlternateBase, QColor(66,66,66));
    dark.setColor(QPalette::ToolTipBase, Qt::white);
    dark.setColor(QPalette::ToolTipText, Qt::white);
    dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::Button, QColor(53,53,53));
    dark.setColor(QPalette::ButtonText, Qt::white);
    dark.setColor(QPalette::BrightText, Qt::red);
    dark.setColor(QPalette::Link, QColor(42,130,218));
    dark.setColor(QPalette::Highlight, QColor(42,130,218));
    dark.setColor(QPalette::HighlightedText, Qt::black);

    app.setPalette(dark);

    // small tooltip tweak
    app.setStyleSheet("QToolTip { color: #ffffff; background-color: #2a2a2a; border: 1px solid white; }");
}

void applyLightPalette(QApplication &app)
{
    // Restore default Fusion palette (light)
    app.setStyle(QStyleFactory::create("Fusion"));

    // QApplication::style() returns QStyle*, so include <QStyle> above to make this valid.
    if (QStyle *s = QApplication::style()) {
        app.setPalette(s->standardPalette());
    } else {
        // Fallback: set default-constructed palette
        app.setPalette(QPalette());
    }

    app.setStyleSheet(""); // clear custom stylesheet
}

} // namespace Theme
