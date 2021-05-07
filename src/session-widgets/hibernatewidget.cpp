#include "hibernatewidget.h"

HibernateWidget::HibernateWidget(QWidget *parent)
    :SessionBaseWindow(parent)
    , rotateIcon(new DLoadingIndicator (this) )
    , label (new QLabel(this))
    , widget (new QWidget (this))
    , vlayout( new QVBoxLayout (this))
{
    label->setText(tr("is dormant, please wait a moment..."));
    label->adjustSize();

    rotateIcon->smooth();
    rotateIcon->setImageSource(QPixmap("://img/spinner_white.svg"));
    rotateIcon->setAttribute(Qt::WA_TranslucentBackground, true);
    rotateIcon->setAniDuration(3000);
    rotateIcon->setLoading(true);  //开始旋转
    rotateIcon->setFrameStyle(QFrame::NoFrame);

    vlayout->addStretch();
    vlayout->addWidget(rotateIcon, 1, Qt::AlignmentFlag::AlignHCenter);
    vlayout->addWidget(label);
    vlayout->setAlignment(Qt::AlignmentFlag::AlignHCenter);  //居中
    vlayout->addStretch();

    widget->setLayout(vlayout);
    setCenterContent(widget);
    widget->setStyleSheet("background-color:transparent;");  //设置透明
}

void HibernateWidget::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
       //为窗口添加一个半透明的矩形遮罩
    const QRect trueRect(QPoint(0, 0), QSize(size() * devicePixelRatioF()));
    painter.fillRect(QRect(0,0, trueRect.width(), trueRect.height()),QColor(0,0,0,100));
}
