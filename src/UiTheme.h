#pragma once

#include <QApplication>
#include <QFont>
#include <QPalette>

namespace UiTheme {
inline void apply(QApplication &app)
{
    app.setStyle("Fusion");
    QFont font = app.font();
#ifdef Q_OS_WIN
    font.setFamily(QStringLiteral("Microsoft YaHei UI"));
#elif defined(Q_OS_MACOS)
    font.setFamily(QStringLiteral("PingFang SC"));
#endif
    font.setPointSize(10);
    app.setFont(font);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f5f6f8"));
    palette.setColor(QPalette::WindowText, QColor("#202630"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f5f7fa"));
    palette.setColor(QPalette::Text, QColor("#202630"));
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, QColor("#202630"));
    palette.setColor(QPalette::Highlight, QColor("#007aff"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#9199a6"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9199a6"));
    app.setPalette(palette);
    app.setStyleSheet(QStringLiteral(R"(
        QWidget { color: #202630; }
        QWidget#workspace, QWidget#inspectorContent { background: #f5f6f8; }
        QWidget#fileSidebar { background: #eef0f4; border-radius: 12px; }
        QWidget#chartPanel { background: white; border-radius: 12px; }
        QFrame#appHeader { background: #f5f6f8; border: none; }
        QLabel[role="title"] { font-size: 21px; font-weight: 600; }
        QLabel[role="section"] { font-size: 15px; font-weight: 600; }
        QLabel[role="secondary"] { color: #647083; }
        QLabel[role="badge"] { color: #0968cc; background: #e5f0ff; border-radius: 8px; padding: 5px 9px; }
        QLabel#fileName { font-weight: 600; }
        QPushButton, QToolButton { background: white; border: 1px solid #d9dee6; border-radius: 7px; padding: 7px 10px; }
        QPushButton:hover, QToolButton:hover { background: #f0f5fc; border-color: #a9bfdc; }
        QPushButton:pressed, QToolButton:pressed { background: #dfebfb; }
        QPushButton[role="primary"] { color: white; background: #007aff; border-color: #007aff; font-weight: 600; }
        QPushButton[role="primary"]:hover { background: #086cda; }
        QPushButton[role="primary"]:pressed { background: #0058b8; }
        QPushButton:disabled { color: #9299a4; background: #e9ecf1; border-color: #e0e4ea; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: white; border: 1px solid #d9dee6; border-radius: 6px; padding: 6px; min-height: 20px; selection-background-color: #007aff; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid #007aff; }
        QComboBox::drop-down { border: none; width: 22px; }
        QListWidget { background: #eef0f4; border: none; outline: none; }
        QListWidget::item { padding: 10px 8px; margin: 2px 0; border-radius: 7px; }
        QListWidget::item:hover { background: #e3e8f0; }
        QListWidget::item:selected { background: #dceaff; color: #075db5; }
        QGroupBox { background: white; border: 1px solid #e1e5eb; border-radius: 10px; margin-top: 14px; padding: 16px 12px 12px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; color: #465264; }
        QTabWidget::pane { border: none; background: transparent; }
        QTabBar::tab { background: #e9ecf1; color: #526071; border-radius: 6px; padding: 8px 10px; margin: 2px; }
        QTabBar::tab:selected { background: white; color: #0868cc; }
        QTabBar::tab:hover { color: #007aff; }
        QCheckBox { spacing: 7px; padding: 3px 0; }
        QCheckBox::indicator { width: 16px; height: 16px; }
        QSplitter::handle { background: transparent; }
        QSplitter::handle:hover { background: #dce8f8; }
        QScrollArea { background: transparent; border: none; }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 2px; }
        QScrollBar::handle:vertical { background: #c5cbd5; border-radius: 3px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QStatusBar { background: #f5f6f8; color: #647083; padding: 4px 12px; }
        QStatusBar::item { border: none; }
        QToolTip { background: #263445; color: white; border: none; padding: 6px; }
    )"));
}
}
