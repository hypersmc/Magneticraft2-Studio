#pragma once

namespace studio {
inline constexpr const char* editorStyle = R"QSS(
QMainWindow, QDialog, QMenuBar, QMenu, QWidget { background: #111820; color: #dbe3e9; }
QMenuBar { padding: 3px 8px; background: #141d26; border-bottom: 1px solid #26323c; }
QMenuBar::item { padding: 6px 10px; }
QMenuBar::item:selected, QMenu::item:selected { background: #354251; color: #ffffff; border-radius: 4px; }
QMenu { border: 1px solid #3c4957; padding: 5px; }
QMenu::item { padding: 6px 32px 6px 16px; }
QToolBar#StudioHeader { background: #19232d; border: 0; border-bottom: 1px solid #2c3945; padding: 9px 12px; spacing: 7px; }
QToolBar#ModelAuthoringToolbar { background: #151f28; border: 0; border-bottom: 1px solid #293644; padding: 5px 12px; spacing: 8px; }
QLabel#Brand { color: #edb17a; font-size: 12pt; font-weight: 800; letter-spacing: 1px; padding-right: 12px; }
QLabel#ProjectHeading { color: #ebf0f3; font-size: 11pt; font-weight: 700; }
QLabel#ProjectContext { color: #899ca9; font-size: 9pt; }
QLabel#MicroLabel { color: #8196a6; font-size: 8pt; font-weight: 700; }
QLabel#ContextTitle { color: #e0b68b; font-size: 9pt; font-weight: 800; letter-spacing: 1px; }
QLabel#ContextInfo { color: #91a4b2; }
QFrame#CentralShell { background: #0f161e; }
QFrame#SceneHeader { background: #151f29; border-bottom: 1px solid #293744; }
QWidget#ToolRail { background: #19232c; border-right: 1px solid #32404c; }
QToolButton[workspaceTab="true"] { background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 9px 13px; color: #9aafbd; font-size: 10pt; font-weight: 650; }
QToolButton[workspaceTab="true"]:hover { color: white; background: #293844; }
QToolButton[workspaceTab="true"]:checked { color: #f7bd85; background: #3b302b; border-color: #865c3b; }
QToolButton[editorTool="true"] { background: transparent; color: #9eb3c1; border: 1px solid transparent; border-radius: 7px; padding: 7px 4px; margin: 4px 6px; font-size: 10pt; }
QToolButton[editorTool="true"]:hover { background: #2b3b49; color: #f0f5f8; }
QToolButton[editorTool="true"]:checked { background: #4a382e; border-color: #c08352; color: #f4be8e; font-weight: 800; }
QToolButton[editorTool="true"]:disabled { color: #5e6e7a; }
QDockWidget { color: #becfda; font-weight: 650; border: none; }
QDockWidget::title { background: #19232d; border-bottom: 1px solid #35424d; padding: 12px 12px; }
QDockWidget::close-button, QDockWidget::float-button { border: none; }
QTabBar::tab { background: #17212b; color: #8397a6; border: 1px solid #30404b; padding: 9px 13px; }
QTabBar::tab:selected { background: #293844; color: #f5bd85; border-top: 2px solid #c58756; }
QSplitter::handle { background: #2d3945; }
QTreeWidget { background: #131c25; border: none; outline: none; font-size: 10pt; }
QTreeWidget::item { padding: 5px 4px; }
QTreeWidget::item:selected { background: #4c3a30; color: #ffffff; border-left: 3px solid #eca76a; }
QTreeWidget::item:hover:!selected { background: #22303e; }
QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox { background: #222f3b; color: #e9eff3; border: 1px solid #3a4a58; border-radius: 5px; padding: 7px 9px; selection-background-color: #9d6742; }
QLineEdit:hover, QDoubleSpinBox:hover, QSpinBox:hover, QComboBox:hover { border-color: #668093; }
QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus { border-color: #daa16f; }
QComboBox QAbstractItemView { background: #202d37; selection-background-color: #865d42; border: 1px solid #536170; }
QPushButton, QToolButton { background: #283744; color: #e2eaf1; border: 1px solid #344857; border-radius: 6px; padding: 8px 12px; }
QPushButton:hover, QToolButton:hover { background: #34485a; border-color: #557084; }
QPushButton:pressed { background: #405668; }
QPushButton#PrimaryAction { color: #24180f; background: #e0a06a; border-color: #e0a06a; font-weight: 800; }
QPushButton#PrimaryAction:hover { background: #f5b985; }
QGroupBox { border: 1px solid #2d3d4a; border-radius: 8px; margin-top: 14px; padding: 12px 10px 10px; font-weight: 700; color: #c6d5df; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; color: #e8b68b; }
QScrollArea { border: 0; }
QStatusBar { background: #17222b; color: #97aab8; border-top: 1px solid #30404a; font-size: 9pt; }
QStatusBar::item { border: none; }
QLabel#InspectorHelp { color: #8d9ea8; font-size: 9pt; line-height: 140%; }
QCheckBox { color: #b5c5d0; spacing: 6px; }
QScrollBar:vertical { background: #15202a; width: 9px; }
QScrollBar::handle:vertical { background: #41525e; min-height: 24px; border-radius: 4px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }

/* Landing page is intentionally an application surface, not a startup dialog. */
QWidget#StartPage, QWidget#StartSurface, QScrollArea#StartScrollArea { background: #111a24; }
QLabel#StartEyebrow { color: #e0a16d; font-size: 11pt; font-weight: 850; letter-spacing: 2px; }
QLabel#StartHeroTitle { color: #f0f4f5; font-size: 28pt; font-weight: 850; }
QLabel#StartHeroSubtitle { color: #a2b5c1; font-size: 11pt; }
QLabel#StartSectionTitle { color: #b3c5ce; font-size: 10pt; font-weight: 800; letter-spacing: 1.5px; padding-top: 4px; }
QFrame#StartCard { background: #1b2a35; border: 1px solid #344756; border-radius: 12px; }
QFrame#StartCard:hover { border-color: #bd845b; background: #20303b; }
QLabel#StartCardGlyph { color: #e1ac78; font-size: 23pt; font-weight: 700; }
QLabel#StartBadge { color: #a4c1d1; font-size: 8pt; font-weight: 800; letter-spacing: 0.8px; }
QLabel#StartCardTitle { color: #f6f3ef; font-size: 16pt; font-weight: 800; }
QLabel#StartCardDescription { color: #b3c2cb; font-size: 10pt; }
QPushButton[primaryStartAction="true"] { background: #e2a46c; color: #23180f; border-color: #ecb584; font-weight: 800; }
QPushButton[primaryStartAction="true"]:hover { background: #f5be8c; }
QFrame#StartExampleCard { background: #18232d; border: 1px solid #2e404b; border-radius: 10px; }
QFrame#StartExampleCard:hover { border-color: #678095; }
QLabel#StartExampleTag { color: #d0a581; font-size: 8pt; font-weight: 800; letter-spacing: 0.8px; }
QLabel#StartExampleTitle { color: #e6eef3; font-size: 12pt; font-weight: 780; }
QLabel#StartExampleDescription { color: #99adb9; font-size: 9pt; }
QListWidget#RecentProjectsList { background: #192530; border: 1px solid #324551; border-radius: 9px; padding: 6px; color: #c9d7df; }
QListWidget#RecentProjectsList::item { padding: 11px 14px; }
QListWidget#RecentProjectsList::item:selected { background: #4a392e; color: white; }
)QSS";
}
