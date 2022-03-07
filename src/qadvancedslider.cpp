#include "qadvancedslider.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QDebug>
#include <QStyleOptionSlider>
#include <QColor>
#include <math.h>



class CPrivateSlider : public QSlider
{
public:
    CPrivateSlider(Qt::Orientation fOrientation, QWidget *pParent = 0)
      : QSlider(fOrientation, pParent)
      , m_bstColor("#ff95dc95")
      , m_optColor(0x0, 0xff, 0x0, 0x3c)
      , m_wrnColor(0xff, 0x54, 0x0, 0x3c)
      , m_errColor(0xff, 0x0, 0x0, 0x3c)
      , m_minBst(-1)
      , m_maxBst(-1)
      , m_minOpt(-1)
      , m_maxOpt(-1)
      , m_minWrn(-1)
      , m_maxWrn(-1)
      , m_minErr(-1)
      , m_maxErr(-1)
    {
        /* Make sure ticks *always* positioned below: */
        setTickPosition(QSlider::TicksBelow);
    }

    int positionForValue(int val) const
    {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        opt.subControls = QStyle::SC_All;
        int available = opt.rect.width() - style()->pixelMetric(QStyle::PM_SliderLength, &opt, this);
        return QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, val, available);
    }

    virtual void paintEvent(QPaintEvent *pEvent)
    {
        QPainter p(this);

        QStyleOptionSlider opt;
        initStyleOption(&opt);
        opt.subControls = QStyle::SC_All;

        int available = opt.rect.width() - style()->pixelMetric(QStyle::PM_SliderLength, &opt, this);
        QSize s = size();

         /* Under Windows SC_SliderTickmarks is fully unreliable
         * source of the information we need, providing us with empty rectangle.
         * Under X11 SC_SliderTickmarks is not fully reliable
         * source of the information we need, providing us with different rectangles
         * (correct or incorrect) under different look&feel styles.
         * So we have to calculate tickmarks rectangle ourself: */
        QRect ticks = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this) |
                      style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        ticks.setRect((s.width() - available) / 2, ticks.bottom() + 1, available, s.height() - ticks.bottom() - 1);

        if ((m_minBst != -1 &&
             m_maxBst != -1) &&
            m_minBst != m_maxBst)
        {
            int posMinBst = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_minBst, available);
            int posMaxBst = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_maxBst, available);
            p.fillRect(ticks.x() + posMinBst, ticks.y(), posMaxBst - posMinBst + 1, ticks.height(), m_bstColor);
        }

        if ((m_minOpt != -1 &&
             m_maxOpt != -1) &&
            m_minOpt != m_maxOpt)
        {
            int posMinOpt = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_minOpt, available);
            int posMaxOpt = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_maxOpt, available);
            p.fillRect(ticks.x() + posMinOpt, ticks.y(), posMaxOpt - posMinOpt + 1, ticks.height(), m_optColor);
        }
        if ((m_minWrn != -1 &&
             m_maxWrn != -1) &&
            m_minWrn != m_maxWrn)
        {
            int posMinWrn = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_minWrn, available);
            int posMaxWrn = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_maxWrn, available);
            p.fillRect(ticks.x() + posMinWrn, ticks.y(), posMaxWrn - posMinWrn + 1, ticks.height(), m_wrnColor);
        }
        if ((m_minErr != -1 &&
             m_maxErr != -1) &&
            m_minErr != m_maxErr)
        {
            int posMinErr = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_minErr, available);
            int posMaxErr = QStyle::sliderPositionFromValue(opt.minimum, opt.maximum, m_maxErr, available);
            p.fillRect(ticks.x() + posMinErr, ticks.y(), posMaxErr - posMinErr + 1, ticks.height(), m_errColor);
        }
        p.end();

        QSlider::paintEvent(pEvent);
    }

    QColor m_bstColor;
    QColor m_optColor;
    QColor m_wrnColor;
    QColor m_errColor;

    int m_minBst;
    int m_maxBst;
    int m_minOpt;
    int m_maxOpt;
    int m_minWrn;
    int m_maxWrn;
    int m_minErr;
    int m_maxErr;
};

QAdvancedSlider::QAdvancedSlider(QWidget *pParent /* = 0 */)
  : QWidget(pParent)
{
    init();
}

QAdvancedSlider::QAdvancedSlider(Qt::Orientation fOrientation, QWidget *pParent /* = 0 */)
  : QWidget(pParent)
{
    init(fOrientation);
}

int QAdvancedSlider::value() const
{
    return m_pSlider->value();
}

void QAdvancedSlider::setRange(int minV, int maxV)
{
    m_pSlider->setRange(minV, maxV);
}

void QAdvancedSlider::setMaximum(int val)
{
    m_pSlider->setMaximum(val);
}

int QAdvancedSlider::maximum() const
{
    return m_pSlider->maximum();
}

void QAdvancedSlider::setMinimum(int val)
{
    m_pSlider->setMinimum(val);
}

int QAdvancedSlider::minimum() const
{
    return m_pSlider->minimum();
}

void QAdvancedSlider::setPageStep(int val)
{
    m_pSlider->setPageStep(val);
}

int QAdvancedSlider::pageStep() const
{
    return m_pSlider->pageStep();
}

void QAdvancedSlider::setSingleStep(int val)
{
    m_pSlider->setSingleStep(val);
}

int QAdvancedSlider::singelStep() const
{
    return m_pSlider->singleStep();
}

void QAdvancedSlider::setTickInterval(int val)
{
    m_pSlider->setTickInterval(val);
}

int QAdvancedSlider::tickInterval() const
{
    return m_pSlider->tickInterval();
}

Qt::Orientation QAdvancedSlider::orientation() const
{
    return m_pSlider->orientation();
}

void QAdvancedSlider::setSnappingEnabled(bool fOn)
{
    m_fSnappingEnabled = fOn;
}

bool QAdvancedSlider::isSnappingEnabled() const
{
    return m_fSnappingEnabled;
}

void QAdvancedSlider::setOptimalHint(int min, int max)
{
    m_pSlider->m_minOpt = min;
    m_pSlider->m_maxOpt = max;

    update();
}

void QAdvancedSlider::setBestHint(int min, int max)
{
    m_pSlider->m_minBst = min;
    m_pSlider->m_maxBst = max;

    update();
}

void QAdvancedSlider::setWarningHint(int min, int max)
{
    m_pSlider->m_minWrn = min;
    m_pSlider->m_maxWrn = max;

    update();
}

void QAdvancedSlider::setErrorHint(int min, int max)
{
    m_pSlider->m_minErr = min;
    m_pSlider->m_maxErr = max;

    update();
}

void QAdvancedSlider::setOrientation(Qt::Orientation fOrientation)
{
    m_pSlider->setOrientation(fOrientation);
}

void QAdvancedSlider::setValue (int val)
{
    m_pSlider->setValue(val);
}

void QAdvancedSlider::sltSliderMoved(int val)
{
    val = snapValue(val); 
    m_pSlider->setValue(val);
    emit sliderMoved(val);
}

void QAdvancedSlider::init(Qt::Orientation fOrientation /* = Qt::Horizontal */)
{
    m_fSnappingEnabled = false;

    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 0, 0, 0);
    m_pSlider = new CPrivateSlider(fOrientation, this);
    pMainLayout->addWidget(m_pSlider);

    connect(m_pSlider, SIGNAL(sliderMoved(int)), this, SLOT(sltSliderMoved(int)));
    connect(m_pSlider, SIGNAL(valueChanged(int)), this, SIGNAL(valueChanged(int)));
    connect(m_pSlider, SIGNAL(sliderPressed()), this, SIGNAL(sliderPressed()));
    connect(m_pSlider, SIGNAL(sliderReleased()), this, SIGNAL(sliderReleased()));
}

int QAdvancedSlider::snapValue(int val)
{
    if (m_fSnappingEnabled)
    {
        val = RoundNum(val);
    }
    return val;
}

int QAdvancedSlider::RoundNum(int num)
{
     int rem = num % 10;
     return rem >= 5 ? (num - rem + 10) : (num - rem);
}

