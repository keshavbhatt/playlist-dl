#ifndef QADVANCEDSLIDER_H
#define QADVANCEDSLIDER_H

#include <QSlider>

class CPrivateSlider;

class QAdvancedSlider: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue)

public:
    QAdvancedSlider(QWidget *pParent = 0);
    QAdvancedSlider(Qt::Orientation fOrientation, QWidget *pParent = 0);

    int value() const;

    void setRange(int minV, int maxV);

    void setMaximum(int val);
    int maximum() const;

    void setMinimum(int val);
    int minimum() const;

    void setPageStep(int val);
    int pageStep() const;

    void setSingleStep(int val);
    int singelStep() const;

    void setTickInterval(int val);
    int tickInterval() const;

    Qt::Orientation orientation() const;

    void setSnappingEnabled(bool fOn);
    bool isSnappingEnabled() const;

    void setOptimalHint(int min, int max);
    void setWarningHint(int min, int max);
    void setErrorHint(int min, int max);
    void setBestHint(int min, int max);

public slots:

    void setOrientation(Qt::Orientation fOrientation);
    void setValue(int val);

signals:
    void valueChanged(int);
    void sliderMoved(int);
    void sliderPressed();
    void sliderReleased();

private slots:

    void sltSliderMoved(int val);
    int RoundNum(int num);
    void init(Qt::Orientation fOrientation = Qt::Horizontal);
    int snapValue(int val);

private:
    CPrivateSlider *m_pSlider;
    bool m_fSnappingEnabled;
};

#endif // QADVANCEDSLIDER_H

