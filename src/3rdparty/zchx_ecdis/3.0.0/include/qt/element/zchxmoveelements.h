#ifndef ZCHXUSERDEFINEMOVEELEMENTS_H
#define ZCHXUSERDEFINEMOVEELEMENTS_H

#include "moveelement.h"

namespace qt {
//自定义区域
class WarningZoneElement : public zchxMoveElement<ZCHX::Data::ITF_WarringZone>
{
public:
    explicit WarningZoneElement(const ZCHX::Data::ITF_WarringZone &ele, zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_WarringZone>(ele, ZCHX::Data::ELE_WARNING_ZONE, ZCHX::LAYER_DEFINEZONE, w) {}

    QString getDefenceColor(){return m_data.fillColor;}
    QString getWarnColor(){return m_data.warnColor;}
    void drawElement(QPainter* painter);
};

//海岸数据
class CoastElement : public zchxMoveElement<ZCHX::Data::ITF_CoastData>
{
public:
    explicit CoastElement(const ZCHX::Data::ITF_CoastData &ele, zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_CoastData>(ele, ZCHX::Data::ELE_COAST, ZCHX::LAYER_COASTDATA, w) {}
    void drawElement(QPainter* painter);
};

//海底岸线数据
class SeabedPipeLineElement : public zchxMoveElement<ZCHX::Data::ITF_SeabedPipeLine>
{
public:
    explicit SeabedPipeLineElement(const ZCHX::Data::ITF_SeabedPipeLine &ele,  zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_SeabedPipeLine>(ele, ZCHX::Data::ELE_SEABEDPIPLINE, ZCHX::LAYER_SEABEDPIPELINE, w) {}
    void drawElement(QPainter* painter);
};

//地理区域网络
class AreaNetElement : public zchxMoveElement<ZCHX::Data::ITF_AreaNet>
{
public:
    explicit AreaNetElement(const ZCHX::Data::ITF_AreaNet &ele, zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_AreaNet>(ele, ZCHX::Data::ELE_AREA_NET, ZCHX::LAYER_AREANET, w) {}
    void drawElement(QPainter *painter);
};

//卡口
class CardMouthElement : public zchxMoveElement<ZCHX::Data::ITF_CardMouth>
{
public:
    explicit CardMouthElement(const ZCHX::Data::ITF_CardMouth &ele, zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_CardMouth>(ele, ZCHX::Data::ELE_CARD_MOUTH, ZCHX::LAYER_CARDMOUTH, w) {}
    void drawElement(QPainter *painter);
};

//自定义线
class StatistcLineElement : public zchxMoveElement<ZCHX::Data::ITF_StatistcLine>
{
public:
    explicit StatistcLineElement(const ZCHX::Data::ITF_StatistcLine &ele, zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_StatistcLine>(ele, ZCHX::Data::ELE_CARD_MOUTH, ZCHX::LAYER_STATISTCLINE, w) {}
    void drawElement(QPainter *painter);
};

//航道
class ChannelElement : public zchxMoveElement<ZCHX::Data::ITF_Channel>
{
public:
    explicit ChannelElement(const ZCHX::Data::ITF_Channel &ele, zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_Channel>(ele, ZCHX::Data::ELE_CHANNEL, ZCHX::LAYER_CHANNEL, w) {}
    void drawElement(QPainter *painter);
    void setLineSelected(int i, bool selectStatus);
};

//锚泊
class MooringElement : public zchxMoveElement<ZCHX::Data::ITF_Mooring>
{
public:
    explicit MooringElement(const ZCHX::Data::ITF_Mooring &ele, zchxMapWidget* w)
        : zchxMoveElement<ZCHX::Data::ITF_Mooring>(ele, ZCHX::Data::ELE_MOOR, ZCHX::LAYER_MOORING, w) {}
    void drawElement(QPainter *painter);
};
}


#endif // ZCHXUSERDEFINEMOVEELEMENTS_H
