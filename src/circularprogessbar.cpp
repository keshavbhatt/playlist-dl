/*
 *@author: 缪庆瑞
 *@date:   2019.6.17
 *@brief:  圆形进度条
 * 该类使用QPainter在widget上绘制，通过外圆和内圆分割出一个环形进度条。
 * 使用drawArc()在环形内绘制弧度展示当前进度，并在内圆中心绘制显示的文本。
 *
 * 注：该部件继承自QWidget,默认不会解析样式表(需要重写paintEvent()加些特殊处理，详见
 * stylesheet帮助文档QWidget部分)。所以即便该进度条继承了父部件设置的特殊样式也不会
 * 生效。除了paintEvent()画的部分之外，其他部分都会沿用QWidget默认属性(背景透明)，这也
 * 方便了将圆形进度条更好的嵌入其他部件中。
 */
#include "circularprogessbar.h"
#include <QDebug>

CircularProgessBar::CircularProgessBar(QWidget *parent) : QWidget(parent)
{
    this->currentProgressValue = 0;
    this->isShowDashBaseBrush = false;
    this->isShowGradient = false;
    //初始化使用默认的属性参数
    setCircleRatio();
    setCircleBrush();
    setCirclePen();
    setArcPenBrush();
    setTextProperty();
    this->resize(300,300);
}
/*
 *@brief:   设置圆形(弧形/外圆/内圆/)占比
 *@author:  缪庆瑞
 *@date:    2019.6.17
 *@param:   arcRatio:弧形占比  即drawArc()的画笔宽度占外圆与内圆所分割圆环宽度的比值
 * 弧形占比取值建议(0.0-2.0)，0<arcRatio<1:弧形在圆环内部，>1:弧形覆盖圆环
 *@param:   outerCircleRatio:外圆占比(0.0-1.0)  即外圆半径占部件可绘制最大圆半径的比值
 *@param:   innerCircleRatio:内圆占比(0.0-1.0)  即内圆半径占部件可绘制最大圆半径的比值
 */
void CircularProgessBar::setCircleRatio(const qreal &arcRatio,
                                        const qreal &outerCircleRatio, const qreal &innerCircleRatio)
{
    this->arcRatio = arcRatio;
    this->outerCircleRatio = outerCircleRatio;
    this->innerCircleRatio = innerCircleRatio;
}
/*
 *@brief:   设置圆形(外圆/内圆)画笔
 *@author:  缪庆瑞
 *@date:    2019.6.19
 *@param:   outerCirclePen:外圆画笔
 *@param:   innerCirclePen:内圆画笔
 */
void CircularProgessBar::setCirclePen(const QPen &outerCirclePen,
                                      const QPen &innerCirclePen)
{
    this->outerCirclePen = outerCirclePen;
    this->innerCirclePen = innerCirclePen;
}
/*
 *@brief:   设置圆形(外圆/内圆)画刷
 *@author:  缪庆瑞
 *@date:    2019.6.17
 *@param:   outerCircleBrush:外圆画刷(填充颜色)
 *@param:   innerCircleBrush:内圆画刷(填充颜色)
 */
void CircularProgessBar::setCircleBrush(const QBrush &outerCircleBrush,
                                        const QBrush &innerCircleBrush)
{
    this->outerCircleBrush = outerCircleBrush;
    this->innerCircleBrush = innerCircleBrush;
}
/*
 *@brief:   设置弧形(drawArc())画笔的画刷，该方法默认在构造函数内会被调用
 *@author:  缪庆瑞
 *@date:    2019.6.20
 *@param:   arcPenBrush:画笔使用的画刷
 */
void CircularProgessBar::setArcPenBrush(const QBrush &arcPenBrush)
{
    /* PenCapStyle这里选择FlatCap(不覆盖端点的方形线端)，因为默认的SquareCap会覆盖端点
     * 并扩展一半的线宽造成进度显示不准确*/
    this->arcPen.setCapStyle(Qt::FlatCap);
    this->arcPen.setBrush(arcPenBrush);
}
/*
 *@brief:   设置弧形(drawArc())画笔的PenStyle(自定义虚线模式，即画笔画出的是虚线)
 *@author:  缪庆瑞
 *@date:    2019.6.19
 *@param:   dashs:虚线模式(设置实线和空白各自所占的长度),详见setDashPattern()帮助
 *@param:   isShowDashBaseBrush:是否展示虚线底色
 *@param:   dashBaseBrush:虚线底色(由画刷保存)
 */
void CircularProgessBar::setArcPenDashPattern(const QVector<qreal> &dashs,
                                           const bool &isShowDashBaseBrush, const QBrush &dashBaseBrush)
{
    /* QPen有多种画笔风格(PenStyle),要实现画笔的虚线模式通常可以使用DashLine或者DotLine。
     * 但由于我们的需求是用虚线画弧形，当进度条满值时会形成一个圆，利用上面的画笔风格可
     * 能会出现虚线首尾交叉的现象，另外也无法自由的控制实线和空白的长度。
     *
     * 这里调用setDashPattern()，会自动将画笔的PenStyle设置为CustomDashLine，实线和空白
     * 各自所占的长度比例通过QVector<qreal>设置。一般情况下dashs数组只设置两个元素即可，
     * 第一个元素表示实线长度，第二个元素表示空白长度。
     * 例:QVector<qreal> &dashs; dashs<<0.42<<0.43;
     *
     * 注:实际应用时不同占比(setCircleRatio)的圆形进度条需要不同的dashs，否则可能出现在绘制
     * 圆时虚线首尾交叉的现象。不过目前还没有推导出dashs元素与占比的具体关系，所以在应用时
     * 需要多测几组dashs数据。
     */
    arcPen.setDashPattern(dashs);
    this->isShowDashBaseBrush = isShowDashBaseBrush;
    this->dashBaseBrush = dashBaseBrush;
}
/*
 *@brief:   设置弧形(drawArc())画笔颜色渐变
 *@author:  缪庆瑞
 *@date:    2019.6.20
 *@param:   conicalGrad:锥形渐变色
 *@param:   isShowGradient:是否显示渐变色
 */
void CircularProgessBar::setArcPenGradient(const QConicalGradient &conicalGrad, const bool &isShowGradient)
{
    this->conicalGrad = conicalGrad;
    this->isShowGradient = isShowGradient;
}
/*
 *@brief:   设置圆形进度条中间显示文本的属性参数
 *@author:  缪庆瑞
 *@date:    2019.6.17
 *@param:   color:文本颜色
 *@param:   font:文本字体
 */
void CircularProgessBar::setTextProperty(const QColor &color, const QFont &font)
{
    this->textColor = color;
    this->textFont = font;
}
/*
 *@brief:   设置圆形进度条中间显示文本内容
 *@author:  缪庆瑞
 *@date:    2019.6.17
 *@param:   text:文本
 */
void CircularProgessBar::setText(const QString &text)
{
    this->text = text;
    update();
}
/*
 *@brief:   设置圆形进度条进度值
 *@author:  缪庆瑞
 *@date:    2019.6.17
 *@param:   progressValue: 0-100
 */
void CircularProgessBar::setProgressValue(const qreal &progressValue)
{
    this->currentProgressValue = progressValue;
    update();
}
/*
 *@brief:   绘图事件处理函数
 *@author:  缪庆瑞
 *@date:    2019.6.17
 *@param:   event: 传递的绘图事件
 */
void CircularProgessBar::paintEvent(QPaintEvent *event)
{
    //qDebug()<<"CircularProgessBar:paintEvent()............................";
    QWidget::paintEvent(event);
    int width = this->width();
    int height = this->height();
    //确定部件的中心(圆心)
    circleCenter.setX(width/2.0);
    circleCenter.setY(height/2.0);
    //确定可正常显示圆的最大半径
    int maxDiameter = width > height ? height : width;
    maxRadius = maxDiameter*0.5;
    //定义painter,设置属性
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);//反锯齿

    //绘制圆形进度条
    drawCircleProgressBar(painter);

}
/*
 *@brief:   绘制圆形进度条
 * 注:该方法内部涉及一些浮点运算以及图形绘制，所以频繁执行时CPU占用会相对高一些
 *@author:  缪庆瑞
 *@date:    2019.6.17
 *@param:   painter: 引用paintEvent()中的对象
 */
void CircularProgessBar::drawCircleProgressBar(QPainter &painter)
{
    qreal outerCircleRadius = maxRadius*outerCircleRatio;
    qreal innerCircleRadius = maxRadius*innerCircleRatio;
    qreal annulusWidth = outerCircleRadius - innerCircleRadius;//圆环宽度
    qreal arcPenWidth = annulusWidth*arcRatio;//圆弧(画笔)宽度
    qreal arcRadius = annulusWidth/2.0+innerCircleRadius;//圆弧所在圆半径
    //圆弧所在矩形
    QRectF arcRectF(circleCenter.x()-arcRadius,circleCenter.y()-arcRadius,arcRadius*2,arcRadius*2);

    /*外圆*/
    painter.setPen(outerCirclePen);
    painter.setBrush(outerCircleBrush);
    painter.drawEllipse(circleCenter,outerCircleRadius,outerCircleRadius);
    /*内圆*/
    painter.setPen(innerCirclePen);
    painter.setBrush(innerCircleBrush);
    painter.drawEllipse(circleCenter,innerCircleRadius,innerCircleRadius);
    /*弧形*/
    /* 圆形进度条一般以12点钟方向(90*16)为起点，顺时针转(负值)。
     * 57.6=360*16/100;即一个百分点对应的值*/
    arcPen.setWidthF(arcPenWidth);//自适应画笔宽度
    if(isShowDashBaseBrush)//使用虚线底色绘制一遍进度条背景
    {
        QBrush penBrush = arcPen.brush();//先记录一下当前弧形画笔的画刷
        arcPen.setBrush(dashBaseBrush);
        painter.setPen(arcPen);
        painter.drawArc(arcRectF,90*16,-360*16);
        arcPen.setBrush(penBrush);//还原画刷
    }
    if(isShowGradient)
    {
        conicalGrad.setCenter(circleCenter);//设置锥形渐变的圆心
        arcPen.setBrush(QBrush(conicalGrad));
    }
    painter.setPen(arcPen);
    painter.drawArc(arcRectF,90*16,-(int)(currentProgressValue*57.6));
    /*文本*/
    /* drawText绘制的文本颜色由画笔决定，但文本字体的大小无法通过画笔宽度设置，需要
     * 设置相应的字体*/
    painter.setPen(QPen(textColor));
    painter.setFont(textFont);
    painter.drawText(arcRectF,Qt::AlignCenter,text);
}
