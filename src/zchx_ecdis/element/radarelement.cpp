﻿#include "radarelement.h"
#include "zchxmapframe.h"
#include "profiles.h"
#include "zchxmapwidget.h"
#include "radarrectelement.h"
#include "map_layer/zchxmaplayermgr.h"

#define MIN_SIDE_LEN 10
#define MAX_SIDE_LEN 30
//#define MAX_DIAMETER 500

namespace qt
{
RadarAreaElement::RadarAreaElement(double radarY, double radarX, int centerLineAngel, int radius, int maxScanRangeANgle, int numberofChannele, int maxWakePointsNumber, zchxMapWidget* v)
    :Element(radarY,radarX, v, ZCHX::Data::ELE_RADAR_AREA, ZCHX::LAYER_RADAR_AREA)
{
    setRadarX(radarX);
    setRadarY(radarY);
    setCenterLineAngel(centerLineAngel);
    setRadius(radius);
    setMaxScanRangeANgle(maxScanRangeANgle);
    setNumberofChannele(numberofChannele);
    setMaxWakePointsNumber(maxWakePointsNumber);

    m_data.radarX = radarX;
    m_data.radarY = radarY;
    m_data.centerLineAngel = centerLineAngel;
    m_data.radius = radius;
    m_data.maxScanRangeANgle = maxScanRangeANgle;
    m_data.numberofChannele = numberofChannele;
    m_data.maxWakePointsNumber = maxWakePointsNumber;
}

RadarAreaElement::RadarAreaElement(const ZCHX::Data::ITF_RadarArea &ele, zchxMapWidget* v)
    :Element(ele.radarY,ele.radarX, v, ZCHX::Data::ELE_RADAR_AREA, ZCHX::LAYER_RADAR_AREA)
{
    setRadarX(ele.radarY);
    setRadarY(ele.radarX);
    setCenterLineAngel(ele.centerLineAngel);
    setRadius(ele.radius);
    setMaxScanRangeANgle(ele.maxScanRangeANgle);
    setNumberofChannele(ele.numberofChannele);
    setMaxWakePointsNumber(ele.maxWakePointsNumber);
    m_data = ele;
}

double RadarAreaElement::radarX() const
{
    return m_radarX;
}

void RadarAreaElement::setRadarX(double radarX)
{
    m_radarX = radarX;
}

double RadarAreaElement::radarY() const
{
    return m_radarY;
}

void RadarAreaElement::setRadarY(double radarY)
{
    m_radarY = radarY;
}

int RadarAreaElement::centerLineAngel() const
{
    return m_centerLineAngel;
}

void RadarAreaElement::setCenterLineAngel(int centerLineAngel)
{
    m_centerLineAngel = centerLineAngel;
}

int RadarAreaElement::radius() const
{
    return m_radius;
}

void RadarAreaElement::setRadius(int radius)
{
    m_radius = radius;
}

int RadarAreaElement::maxScanRangeANgle() const
{
    return m_maxScanRangeANgle;
}

void RadarAreaElement::setMaxScanRangeANgle(int maxScanRangeANgle)
{
    m_maxScanRangeANgle = maxScanRangeANgle;
}

int RadarAreaElement::numberofChannele() const
{
    return m_numberofChannele;
}

void RadarAreaElement::setNumberofChannele(int numberofChannele)
{
    m_numberofChannele = numberofChannele;
}

int RadarAreaElement::maxWakePointsNumber() const
{
    return m_maxWakePointsNumber;
}

void RadarAreaElement::setMaxWakePointsNumber(int maxWakePointsNumber)
{
    m_maxWakePointsNumber = maxWakePointsNumber;
}

ZCHX::Data::ITF_RadarArea RadarAreaElement::data() const
{
    return m_data;
}

QPolygonF RadarAreaElement::getShapePnts(zchxMapFrameWork *framework, double angleFromNorth)
{
    QPolygonF polygon;
    if(framework)
    {
        double startAng = this->centerLineAngel() - this->maxScanRangeANgle() / 2;
        double lenAng =   this->maxScanRangeANgle();
        ZCHX::Data::Point2D centerPos = framework->LatLon2Pixel(this->radarY(),this->radarX());

        //通过推算经纬度计算半径
        ZCHX::Data::LatLon drll(0, 0);
        ZCHX::Utils::distbear_to_latlon(this->radarY(),this->radarX(),this->radius() * 1000,-90 + startAng, drll.lat, drll.lon);
        ZCHX::Data::Point2D drPos = framework->LatLon2Pixel(drll);
        double dr = sqrt(abs(drPos.y - centerPos.y) * abs(drPos.y-centerPos.y) + abs(drPos.x - centerPos.x) * abs(drPos.x - centerPos.x));

        QRectF rectangle(centerPos.x-dr,centerPos.y-dr, dr*2, dr*2);
        int startAngle = (90 - startAng - angleFromNorth);
        int spanAngle = (-lenAng);
        //计算弧线上的点，添加到多边形
        polygon.append(QPointF(centerPos.x, centerPos.y));
        double pntsize = 100;//假定弧线上取100个点
        for(int i=0; i<=pntsize; i++)
        {
            double degree = startAngle + i/pntsize * spanAngle;
            double alpha = degree / 180.0 * GLOB_PI;
            //qDebug()<<"degree:"<<degree<<" alpha:"<<alpha;
            polygon.append(QPointF(centerPos.x + dr * cos(alpha), centerPos.y - dr* sin(alpha)));
        }
        polygon.append(QPointF(centerPos.x, centerPos.y));
        //qDebug()<<"polygon:"<<polygon;
    }

    return polygon;
}

bool RadarAreaElement::contains(zchxMapFrameWork *framework, double angleFromNorth, double x, double y) const
{
    if(!framework)
        return false;
    QPolygonF polygon;
    double startAng = this->centerLineAngel() - this->maxScanRangeANgle() / 2;
    double lenAng =   this->maxScanRangeANgle();
    ZCHX::Data::Point2D centerPos = framework->LatLon2Pixel(this->radarY(),this->radarX());

    //通过推算经纬度计算半径
    ZCHX::Data::LatLon drll(0, 0);
    ZCHX::Utils::distbear_to_latlon(this->radarY(),this->radarX(),this->radius() * 1000,-90 + startAng, drll.lat, drll.lon);
    ZCHX::Data::Point2D drPos = framework->LatLon2Pixel(drll);
    double dr = sqrt(abs(drPos.y - centerPos.y) * abs(drPos.y-centerPos.y) + abs(drPos.x - centerPos.x) * abs(drPos.x - centerPos.x));

    QRectF rectangle(centerPos.x-dr,centerPos.y-dr, dr*2, dr*2);
    int startAngle = (90 - startAng - angleFromNorth);
    int spanAngle = (-lenAng);
    //计算弧线上的点，添加到多边形
    polygon.append(QPointF(centerPos.x, centerPos.y));
    double pntsize = 100;//假定弧线上取100个点
    for(int i=0; i<=pntsize; i++)
    {
        double degree = startAngle + i/pntsize * spanAngle;
        double alpha = degree / 180.0 * GLOB_PI;
        polygon.append(QPointF(centerPos.x + dr * cos(alpha), centerPos.y - dr*sin(alpha)));
    }
    polygon.append(QPointF(centerPos.x, centerPos.y));
    return polygon.containsPoint(QPointF(x, y), Qt::OddEvenFill);
}

//雷达点
//RadarPointElement::RadarPointElement(const double &lat, const double &lon, zchxMapWidget* f)
//    :Element(lat,lon, f, ZCHX::Data::ELE_RADAR_POINT, ZCHX::LAYER_RADAR_CURRENT)
//{
//    m_shape = Radar_Ellipse;
//    m_showRadarLabel = false;
//    initFromSettings();
//}

RadarPointElement::RadarPointElement(const ZCHX::Data::ITF_RadarPoint &ele, zchxMapWidget* f)
    :Element(ele.getLat(),ele.getLon(), f, ZCHX::Data::ELE_RADAR_POINT, ZCHX::LAYER_RADAR_CURRENT, ele.warnStatusColor)
{
    m_shape = Radar_Ellipse;
    m_showRadarLabel = false;
    setData(ele);
    initFromSettings();
}

RadarPointElement::RadarPointElement(const RadarPointElement &pt)
    : Element(pt)
    , m_data(pt.m_data)
    , m_shape(pt.m_shape)
    , m_showRadarLabel(false)
{
    initFromSettings();
}

std::pair<double, double> RadarPointElement::getPoint()
{
    return std::pair<double, double>(elelat,elelon);
}

const ZCHX::Data::ITF_RadarPoint &RadarPointElement::getData() const
{
    return m_data;
}

void RadarPointElement::setData(const ZCHX::Data::ITF_RadarPoint &data)
{
    elelat = data.getLat();
    elelon = data.getLon();
    displayLat = elelat;
    displayLon = elelon;
    m_data = data;
    //将目标的历史轨迹作为子item添加
    m_children.clear();
    ZCHX::Data::ITF_RadarRect child;
    child.mRadarSiteId = data.radarSiteID;
    child.mIsEstObj = data.currentRect.isRealData;
    child.mNodeNumber = data.trackNumber;
    child.mCurrentRect = data.currentRect;
    child.mHistoryRects.append(data.historyRects);
    addChild(std::shared_ptr<RadarRectGlowElement>(new RadarRectGlowElement(child, mView)));
}


uint RadarPointElement::getStatus() const
{
    return m_data.warnStatus;
}

void RadarPointElement::setStatus(const uint &value)
{
    m_data.warnColor = value;
}

bool RadarPointElement::getNeedDrawBox() const
{
    return (m_data.trackby == "1");
}

void RadarPointElement::setNeedDrawBox(bool needDrawBox)
{
    if(needDrawBox)
    {
        m_data.trackby = "1";
    }
    else
    {
        m_data.trackby.clear();
    }
}

void RadarPointElement::initFromSettings()
{
    //初始化雷达颜色设定
    mFillingColor.setNamedColor(Profiles::instance()->value(RADAR_DISPLAY_SETTING, RADAR_FILL_COLOR, QColor(Qt::green).name()).toString());
    mTextColor.setNamedColor(Profiles::instance()->value(RADAR_DISPLAY_SETTING, RADAR_TEXT_COLOR, QColor(Qt::black).name()).toString());
    mConcernColor.setNamedColor(Profiles::instance()->value(RADAR_DISPLAY_SETTING, RADAR_CONCERN_COLOR, QColor(Qt::red).name()).toString());;
    mBorderColor.setNamedColor(Profiles::instance()->value(RADAR_DISPLAY_SETTING, RADAR_BORDER_COLOR, QColor(Qt::black).name()).toString());
    m_shape = Profiles::instance()->value(RADAR_DISPLAY_SETTING, RADAR_DRAW_SHAPE, 1).toInt();
    m_showRadarLabel = false;

    Element::initFromSettings();
    //qDebug()<<"radar ini seetings."<<mDrawAsAis<<mRadarShape<<mFillingColor.name()<<mTextColor.name()<<mConcernColor.name()<<mBorderColor.name();
}

QString RadarPointElement::getRadarImgeBySize()
{
    if(!m_data.directionConfirmed) return QString(":/radarType/雷达点迹.png");
    if (m_data.diameter <= 80)
    {
        return QString(":/radarType/雷达小.png");
    }
    else if (m_data.diameter > 80 && m_data.diameter <= 200)
    {
        return QString(":/radarType/雷达大.png");
    }
    else if (m_data.diameter > 200)
    {
        return QString(":/radarType/雷达大.png");
    }

    return QString(":/radarType/雷达大.png");
}

void RadarPointElement::drawElement(QPainter *painter)
{    
    if(!painter || !mView->getLayerMgr()->isLayerVisible(ZCHX::LAYER_RADAR_CURRENT) || !mView->framework())
    {
        return;
    }
    //首先画儿子图元
    Element::drawElement(painter);
    //画报警背景
    if(getStatus() > 0) drawFlashRegion(painter, getCurrentPos(), getStatus(), getData().warnStatusColor);
    //qDebug()<<"radar property:"<<getData().trackNumber<<getDrawAsAis()<<getDrawShape()<<getFillingColor().name()<<getBorderColor().name();


    //检查轨迹点是否已经赋值,没赋值有可能有异常,暂时就不处理
//    if(getPath().size() == 0) return;
    //取得当前图元对应的屏幕坐标位置
    QPointF pos = getCurrentPos();
    int curScale = mView->framework()->Zoom() < 7 ? 5 : 10;
    int sideLen = getSideLen();
    double angleFromNorth = mView->framework()->GetRotateAngle(); //计算当前正北方向的方向角
    QColor fillColor = getIsConcern() ? getConcernColor() : getFillingColor();
    //地图缩放比例小于10级的情况只画点
    if(mView->framework()->Zoom() < 10)
    {
        PainterPair chk(painter);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fillColor);
        if(getDrawShapeAsRect())
        {
            painter->drawRect(QRect(pos.x() - 2, pos.y() - 2, 4, 4));
        } else
        {
            painter->drawEllipse(pos.x() - 2, pos.y() - 2, 4, 4);
        }
        return;
    }
    //开始处理雷达点具体画法
    QRect elementRect; //目标盒子
    //这个RADARPLAN是什么类型,暂时未知
    if(getRadarObjType() == ZCHX::Data::RadarPointUndef)
    {
        PainterPair chk(painter);
        if(getIsActive())
        {
            //画选中的外框
            QRect planeRect(pos.x()-curScale-2,pos.y()-curScale-2,curScale+4,curScale+4);
            elementRect = planeRect.adjusted(-2, -2, 2, 2);
            painter->setPen(Qt::black);
            painter->drawRect(planeRect);
        }
        //目标填充(黄色)
        painter->setBrush(Qt::yellow);
        QRectF rect(pos.x() - curScale, pos.y() - curScale, curScale, curScale);
        painter->drawRect(rect);
    }
    else if(getRadarObjType() == ZCHX::Data::RadarPointNormal)
    {
        QRect shipRect(0,0,sideLen,sideLen);
        shipRect.moveCenter(QPoint(pos.x(), pos.y()) );
        //qDebug()<<"draw as ais:"<<getDrawAsAis();
        if(getDrawAsAis()) //ship
        {
            QPixmap shipPic = ZCHX::Utils::getImage(":/element/ship.png", Qt::yellow, mView->framework()->Zoom());
            shipRect.setSize(shipPic.size());
            shipRect.moveCenter(QPoint(pos.x(), pos.y()) );
            painter->drawPixmap(shipRect, shipPic);
        } else
        {
            PainterPair chk(painter);
            painter->setPen(mBorderColor);
            painter->setBrush(fillColor);
            painter->translate(pos.x(),pos.y());
            painter->rotate((int)(getData().currentRect.cog + angleFromNorth) % 360);
            painter->translate(-pos.x(), -pos.y());
            if(getDrawShapeAsRect())
            {
                painter->drawRect(pos.x()-3,pos.y()-5,6,10);
            } else
            {
//                painter->drawEllipse(pos.x() - sideLen / 2, pos.y() - sideLen / 2, sideLen, sideLen);
                QString imgUrl = getRadarImgeBySize();
                if (!imgUrl.isEmpty())
                {
                    QPixmap pixmap(imgUrl);
                    painter->drawPixmap(pos.x() - pixmap.width() / 2, pos.y() - pixmap.height() / 2, pixmap.width(), pixmap.height(), pixmap);
                }
            }
        }

        elementRect = shipRect.adjusted(-2, -2, 2, 2);

        if(getIsActive())
        {
            //画目标选择的情况
            PainterPair chk(painter);
            QPen pen(Qt::red,2,Qt::DashDotLine); //选中使用点划线
            painter->setBrush(Qt::NoBrush);
            painter->setPen(pen);
            painter->drawRect(pos.x() -sideLen * 1.5, pos.y() - sideLen * 1.5,sideLen * 3, sideLen * 3);
        }

        //绘制交汇
        if( getIsOpenMeet() && getData().radarMeetVec.size()>0)
        {
            //qDebug()<<"!!!!!!!!!!";
            PainterPair chk(painter);
            QPen pen(Qt::red, 2, Qt::DashLine);
            painter->setPen(pen);
            for(int j = 0;j<getData().radarMeetVec.size();j++)
            {
                ZCHX::Data::RadarMeet meetItem = getData().radarMeetVec.at(j);
                ZCHX::Data::Point2D meetPos = mView->framework()->LatLon2Pixel(meetItem.lat,meetItem.lon);
                uint time_hour = meetItem.UTC / 3600;
                uint time_minute = meetItem.UTC / 60 - time_hour * 60;
                uint time_second = meetItem.UTC % 60;
                QString darwText = QObject::tr("Time: %1H %2M %3S; Diatance: %L4m")
                        .arg(time_hour)
                        .arg(time_minute, 2, 10, QLatin1Char('0'))
                        .arg(time_second, 2, 10, QLatin1Char('0'))
                        .arg(meetItem.disrance, 0, 'f', 0);
                painter->drawLine(meetPos.x, meetPos.y, pos.x(), pos.y());
                painter->drawText(pos.x() - 10, pos.y() - sideLen / 2, darwText);
            }
        }
        //绘制方向线和文本
        drawText(painter, pos, sideLen);
    }
    //是否需要套框
    if(getNeedDrawBox())
    {
        //套框使用虚线
        QPen penBox(Qt::red,2,Qt::DashLine);
        PainterPair chk(painter);
        painter->setPen(penBox);
        painter->drawRect(elementRect);
    }

    // 预警状态
    int warnStatus = getStatus();
    if(warnStatus > 0)
    {
        QString warnName = ZCHX::Utils::getWarnName(warnStatus);

        QPixmap devicePix;
        devicePix.load(":/navig64/warning.png");
        painter->drawPixmap(pos.x() - 20 - devicePix.width() / 2, pos.y() - 30 - devicePix.height() / 2,
                            devicePix.width(), devicePix.height(), devicePix);

        painter->setPen(QPen(Qt::red, 1));
        QRect rectangle = QRect(pos.x() - 120, pos.y() - 40, 300, 30);
        painter->drawText(rectangle, Qt::AlignCenter, warnName);

    }
}

void RadarPointElement::drawText(QPainter *painter, QPointF pos, int sideLen)
{
    if(!mView->framework()) return;
    double angleFromNorth = mView->framework()->GetRotateAngle(); //计算当前正北方向的方向角
    PainterPair chk(painter);
    if (m_showRadarLabel)
    {
        painter->setPen(mTextColor);
        QString radarName = QObject::tr("T%1").arg(QString::number(getData().trackNumber));
        QFont font("新宋体", 10, QFont::Normal, false);
        painter->setFont(font);
        painter->drawText(pos.x()+2+sideLen / 2,pos.y() + sideLen / 2,radarName);
    }
    if(!m_data.directionConfirmed) return;
    painter->setPen(mBorderColor);
    painter->translate(pos.x(),pos.y());
    painter->rotate((int)(getData().currentRect.cog + angleFromNorth) % 360);
    painter->translate(-pos.x(), -pos.y());
    painter->drawLine(pos.x(),pos.y(),pos.x(),pos.y() -  sideLen * (getData().currentRect.sogKnot  + 0.5));
}

void RadarPointElement::drawTrack(QPainter *painter)
{
//    const std::vector<std::pair<double, double>>& path = getPath();
//    if(path.empty()) return;

//    std::vector<QPointF> pts = mView->framework()->convert2QtPonitList(path);
//    PainterPair chk(painter);
//    painter->setPen(QPen(Qt::black,1,Qt::DashLine));
//    painter->drawPolyline(&pts[0],pts.size());
//    return;
}



void RadarPointElement::clicked(bool isDouble)
{
    if(!mView) return;
    if(isDouble) {
        mView->signalIsDoubleClicked4RadarPoint(getData());
    } else {
        mView->signalIsSelected4RadarPoint(getData(), false);
    }
}

void RadarPointElement::showToolTip(const QPoint &pos)
{
    ZCHX::Data::ITF_RadarPoint info = getData();
    QString pos_text = QObject::tr("跟踪号: T%1\n").arg(info.trackNumber);
    pos_text += QObject::tr("经度: %1\n").arg(FLOAT_STRING(info.getLat(), 6));
    pos_text += QObject::tr("纬度: %1\n").arg(FLOAT_STRING(info.getLon(), 6));;
    pos_text += QObject::tr("更新时间: %1\n").arg(QDateTime::fromTime_t(info.currentRect.updateTime).toString("MM/dd/yyyy HH:mm:ss"));
    pos_text += QObject::tr("方位角（正北方向）: %1\n").arg(FLOAT_STRING(info.currentRect.cog, 0));
    pos_text += QObject::tr("速度(节): %1\n").arg(FLOAT_STRING(info.currentRect.sogKnot, 2));
    pos_text += QObject::tr("长度(米): %1\n").arg(FLOAT_STRING(info.currentRect.boundRect.diameter, 2));
    pos_text += QObject::tr("运动(米): %1").arg(FLOAT_STRING(info.currentRect.referWidth, 2));
    QToolTip::showText(pos, pos_text);
}

void RadarPointElement::setShowRadarLabel(bool showRadarLabel)
{
    m_showRadarLabel = showRadarLabel;
}

int RadarPointElement::getSideLen()
{
    bool fixRadarSize = Profiles::instance()->value(RADAR_DISPLAY_SETTING, "FixRadarSize").toBool();
    if (fixRadarSize)
    {
        return MIN_SIDE_LEN;
    }

    QPointF center = framework()->LatLon2Pixel(m_data.getLat(), m_data.getLon()).toPointF();
    ZCHX::Data::LatLon templl = ZCHX::Utils::distbear_to_latlon(m_data.getLat(), m_data.getLon(), m_data.diameter, 0);
    QPointF tempPos = framework()->LatLon2Pixel(templl.lat, templl.lon).toPointF();
    double radius = abs(tempPos.y() - center.y());

    if (radius < MIN_SIDE_LEN)
    {
        radius = MIN_SIDE_LEN;
    }
    else if (radius > MAX_SIDE_LEN)
    {
        radius = MAX_SIDE_LEN;
    }

    return radius;
}
}
