#include "ShortcutHelpDialog.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
/**
 * @brief 由 UTF-8 窄字符串构造 QString
 * @param text UTF-8 编码的窄字符串字面量
 * @return 对应的 QString
 * @note 统一使用该函数而不是 QStringLiteral：本工程以 /source-charset:utf-8 编译，
 *       窄字符串字面量的字节为正确的 UTF-8，但实测 MSVC 19.29 对 QStringLiteral
 *       内部的 u"" 字面量仍按系统代码页解释，导致中文变成乱码，改用 fromUtf8 可稳定正确。
 */
inline QString textUtf8(const char *text)
{
    return QString::fromUtf8(text);
}

/** 说明文本统一使用的字体族：优先微软雅黑，保证中文在大屏上的可读性 */
const char *const kFontFamily = "Microsoft YaHei";

/** 快捷键徽标通用样式：等宽字体、青色文字、半透明蓝色底、圆角 */
const char *const kKeyStyle =
    "QLabel{"
    "color:#8FE3FF;"
    "background:rgba(47,134,222,0.20);"
    "border:1px solid rgba(96,186,255,0.38);"
    "border-radius:12px;"
    "padding:12px 20px;"
    "font-family:Consolas,'Microsoft YaHei';"
    "font-size:28px;"
    "font-weight:bold;"
    "}";

/** 功能说明文字样式：浅蓝灰，弱于标题与快捷键 */
const char *const kDescStyle =
    "QLabel{"
    "color:#C3D8EE;"
    "background:transparent;"
    "border:none;"
    "font-family:'Microsoft YaHei';"
    "font-size:28px;"
    "}";
}  // namespace

ShortcutHelpDialog::ShortcutHelpDialog(QWidget *parent) : QWidget(parent)
{
    // 无边框 + 置顶 + 工具窗口：不显示系统标题栏、边框与关闭按钮，也不占用任务栏
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    // 背景透明，用于绘制圆角卡片与四周的外发光阴影
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(textUtf8("快捷键说明"));
    // 允许接收键盘事件，以便支持 Esc 关闭
    setFocusPolicy(Qt::StrongFocus);

    buildUi();

    // 外部点击检测定时器：以较小间隔轮询全局左键状态，及时响应落在浮层外的点击
    outsideClickTimer = new QTimer(this);
    outsideClickTimer->setInterval(40);
    connect(outsideClickTimer, &QTimer::timeout, this,
            &ShortcutHelpDialog::checkGlobalClickOutside);

    // 安装到 qApp 上：无论点击的是浮层还是本程序的其他窗口，都能收到鼠标按下事件
    qApp->installEventFilter(this);
}

ShortcutHelpDialog::~ShortcutHelpDialog()
{
    if (qApp != nullptr)
    {
        qApp->removeEventFilter(this);
    }
}

void ShortcutHelpDialog::buildUi()
{
    // 外层留白用于显示卡片的外发光阴影
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(24, 24, 24, 24);
    outerLayout->setSpacing(0);

    // 卡片本身承担深蓝渐变背景与圆角，不绘制任何边框
    card = new QFrame(this);
    card->setObjectName(QStringLiteral("helpCard"));
    // 字体放大后需要更大的最小宽度，避免说明文字被折行
    card->setMinimumWidth(1300);
    card->setStyleSheet(QStringLiteral("#helpCard{"
                                       "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
                                       "stop:0 #12365F,stop:0.45 #0B2444,stop:1 #061428);"
                                       "border:none;"
                                       "border-radius:14px;}"));
    outerLayout->addWidget(card);

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(0, 132, 255, 110));
    card->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(44, 32, 44, 24);
    cardLayout->setSpacing(22);

    // ---------- 标题行：左侧青色渐变竖条 + 标题 ----------
    QHBoxLayout *titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(18);

    QFrame *accent = new QFrame(card);
    accent->setFixedSize(7, 40);
    accent->setStyleSheet(QStringLiteral("background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                                         "stop:0 #5BE1FF,stop:1 #1E7BE0);"
                                         "border:none;border-radius:3px;"));
    titleLayout->addWidget(accent);

    QLabel *titleLabel = new QLabel(textUtf8("快捷键说明"), card);
    titleLabel->setStyleSheet(QStringLiteral("QLabel{color:#EAF5FF;background:transparent;"
                                             "border:none;font-family:'%1';font-size:38px;"
                                             "font-weight:bold;letter-spacing:4px;}")
                                  .arg(QString::fromUtf8(kFontFamily)));
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    cardLayout->addLayout(titleLayout);

    // ---------- 标题与列表之间的分隔线 ----------
    QFrame *divider = new QFrame(card);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QStringLiteral("background:rgba(96,186,255,0.20);border:none;"));
    cardLayout->addWidget(divider);

    // ---------- 快捷键列表 ----------
    QGridLayout *grid = new QGridLayout();
    grid->setHorizontalSpacing(40);
    grid->setVerticalSpacing(20);
    grid->setColumnMinimumWidth(0, 380);
    grid->setColumnStretch(1, 1);

    addShortcutRow(grid, 0, QStringLiteral("Alt + M"),
                   textUtf8("显示 / 关闭多屏控制管理界面（软件清单、屏幕布局与分配）"));
    addShortcutRow(grid, 1, QStringLiteral("Alt + H"),
                   textUtf8("显示 / 关闭本快捷键说明"));
    addShortcutRow(grid, 2, QStringLiteral("Alt + R"),
                   textUtf8("重新设置各窗体显示位置，全部复位到各自的默认屏幕"));
    addShortcutRow(grid, 3, textUtf8("Alt + 1 ~ 7\nAlt + 小键盘 1 ~ 7"),
                   textUtf8("切换屏幕：目标屏的显示内容与主屏对调，其余屏幕复位"));
    addShortcutRow(grid, 4, QStringLiteral("Alt + Q\nAlt + F4"),
                   textUtf8("关闭所有窗体并退出程序"));
    addShortcutRow(grid, 5, QStringLiteral("Alt + F5"),
                   textUtf8("重新加载鼠标所在屏幕当前标签页的应用，其它标签页不受影响"));
    addShortcutRow(grid, 6, textUtf8("Alt + Tab\nAlt + Shift + Tab"),
                   textUtf8("切换鼠标所在屏幕的标签页：向后 / 向前循环切换"));

    cardLayout->addLayout(grid);
    cardLayout->addStretch();

    // ---------- 底部提示 ----------
    QLabel *hintLabel = new QLabel(
        textUtf8("点击弹框外部区域或按 Esc 关闭，快捷键为全局生效，无需先激活窗口"), card);
    hintLabel->setStyleSheet(QStringLiteral("QLabel{color:#6E93B8;background:transparent;border:none;"
                                            "font-family:'%1';font-size:22px;}")
                                 .arg(QString::fromUtf8(kFontFamily)));
    hintLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cardLayout->addWidget(hintLabel);
}

void ShortcutHelpDialog::addShortcutRow(QGridLayout *grid, int row, const QString &keys,
                                        const QString &desc)
{
    QLabel *keyLabel = new QLabel(keys, card);
    keyLabel->setStyleSheet(QString::fromUtf8(kKeyStyle));
    keyLabel->setAlignment(Qt::AlignCenter);
    // 与卡片同高自适应，避免多行快捷键文本被截断
    keyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QLabel *descLabel = new QLabel(desc, card);
    descLabel->setStyleSheet(QString::fromUtf8(kDescStyle));
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    grid->addWidget(keyLabel, row, 0);
    grid->addWidget(descLabel, row, 1);
}

void ShortcutHelpDialog::toggle()
{
    if (isVisible())
    {
        hide();
        return;
    }
    showAtCursorScreen();
}

void ShortcutHelpDialog::showAtCursorScreen()
{
    // 先按内容确定尺寸，再做居中计算
    adjustSize();

    // 在鼠标所在屏幕弹出，多屏场景下避免说明窗口出现在另一块屏幕上
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen != nullptr)
    {
        const QRect area = screen->availableGeometry();
        move(area.center().x() - width() / 2, area.center().y() - height() / 2);
    }

    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);

    // 显示期间开启全局点击检测；复位按键状态，避免上一次长按残留在按下态
    leftButtonPressed = false;
    outsideClickTimer->start();
}

bool ShortcutHelpDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (!isVisible())
    {
        return false;
    }

    // 点击浮层外部区域自动隐藏：用全局坐标判断，避免落到子控件时误判为“外部”
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (!geometry().contains(mouseEvent->globalPos()))
        {
            hide();
        }
    }
    // 点击切到本程序之外的窗口（外部程序或 CEF 原生窗口）不会产生 Qt 鼠标事件，
    // 但浮层会失去激活状态，据此同样做隐藏处理
    else if (event->type() == QEvent::WindowDeactivate && watched == this)
    {
        hide();
    }

    // 不拦截事件，保证其他控件与外部程序的交互不受影响
    return false;
}

void ShortcutHelpDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        hide();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ShortcutHelpDialog::hideEvent(QHideEvent *event)
{
    // 浮层隐藏后停止轮询，避免无谓的定时器唤醒
    if (outsideClickTimer != nullptr)
    {
        outsideClickTimer->stop();
    }
    leftButtonPressed = false;
    QWidget::hideEvent(event);
}

void ShortcutHelpDialog::checkGlobalClickOutside()
{
#ifdef Q_OS_WIN
    // GetAsyncKeyState 的最高位表示当前是否处于按下状态；
    // 点击可能落在 CEF 原生窗口或外部程序窗口上，这些点击不会产生 Qt 事件，只能依赖系统全局状态判断
    const bool pressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (pressed && !leftButtonPressed)
    {
        // 只响应按下的瞬间，并且仅当光标落在浮层矩形之外时才隐藏
        if (!geometry().contains(QCursor::pos()))
        {
            hide();
        }
    }
    leftButtonPressed = pressed;
#endif
}
