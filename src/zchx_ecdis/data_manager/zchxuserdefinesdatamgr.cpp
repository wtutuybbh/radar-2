﻿#include "zchxuserdefinesdatamgr.h"
#include "profiles.h"
#include "zchxmapwidget.h"
#include "map_layer/zchxmaplayermgr.h"
#include "coastdatainfodialog.h"
#include "dialog/cardmouthinfodialog.h"
#include "dialog/statistcLineinfodialog.h"
#include "dialog/channelinfodialog.h"
#include "dialog/mooringinfodialog.h"
#include "zchxmapframe.h"

using namespace qt;

bool zchxWarningZoneDataMgr::updateActiveItem(const QPoint &pt)
{
    bool sts = zchxTemplateDataMgr::updateActiveItem(pt);
    if(sts && mDisplayWidget) {
        WarningZoneElement* ele = static_cast<WarningZoneElement*>(mDisplayWidget->getCurrentSelectedElement());
        if(ele) mDisplayWidget->signalIsSelected4WarringZone(ele->data());
    }
    return sts;
}

void zchxCoastDataMgr::importData(const std::vector<std::pair<double, double> > & path)
{
    ZCHX::Data::ITF_CoastData zone;
    zone.path = path;
    zone.id = 0;
    zone.beachLength = zchxMapDataUtils::getTotalDistance(path);

    CoastDataInfoDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        zone.name = dlg.getName();
        zone.manageOrganization = dlg.getOrg();
        zone.responsibilityAccount = dlg.getAccount();
        zone.height = dlg.getHeight();

        if(!zone.name.isEmpty())
        {
            //先添加进去, 确保显示
            updateData(zone);
            emit mDisplayWidget->signalCreateCoastDataLINE(zone);
        }
    }
}

void zchxSeabedPipLineDataMgr::importData(const std::vector<std::pair<double, double> > &path)
{
    ZCHX::Data::ITF_SeabedPipeLine zone;
    zone.path = path;
    zone.id = 0;
    zone.lineLength = zchxMapDataUtils::getTotalDistance(path);

    CoastDataInfoDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        zone.name = dlg.getName();
        zone.manageOrganization = dlg.getOrg();
        zone.responsibilityAccount = dlg.getAccount();
        zone.depth = dlg.getHeight();

        if(!zone.name.isEmpty())
        {
            //先添加进去, 确保显示
            updateData(zone);
            emit mDisplayWidget->signalCreateSeabedPipeLineLINE(zone);
        }
    }
}

void zchxChannelDataMgr::importData(const std::vector<std::pair<double, double> > &path)
{
    ZCHX::Data::ITF_Channel zone;
    zone.path = path;
    zone.id = 0;
//    zone.area = getArea(path);

    ChannelInfoDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        zone.name = dlg.getName();
        zone.courseType = dlg.getChannelType();
        zone.isWarn = dlg.getChannelStatus();
        zone.speedLimit = dlg.getMaxSpeed();
        zone.collisionThreshold = dlg.getCollisionMaxDis();
        zone.yawThreshold = dlg.getYawMinDis();
        zone.overtakThreshold = dlg.getOvertak();
        zone.dropAnchorThreshold = dlg.getDropAnchor();
        zone.overtakShortDis = dlg.getOvertakShortDis();
        zone.overtakVerticalDis = dlg.getOvertakVerticalDis();
        zone.acceleraThreshold = dlg.getAcceleraThreshold();
        zone.acceleraDfference = dlg.getAcceleraDfference();
        zone.isConverseWarn = dlg.isConverseWarn();
        zone.isOverloadWarn = dlg.isOverloadWarn();

        zone.fillColor = dlg.getColor();
    }

    if(!zone.name.isEmpty())
    {
        //先添加进去, 确保显示
        updateData(zone);
        emit mDisplayWidget->signalCreateChannelZone(zone);
    }
}

bool zchxChannelDataMgr::updateActiveItem(const QPoint &pt)
{
    bool sts = zchxTemplateDataMgr::updateActiveItem(pt);
    if(sts && mDisplayWidget) {
        ChannelElement* ele = static_cast<ChannelElement*>(mDisplayWidget->getCurrentSelectedElement());
        if(ele) mDisplayWidget->signalIsSelected4ChannelZone(ele->data());
    }
    return sts;
}

void zchxChannelDataMgr::selectChannelLine(int channelId, const ZCHX::Data::ITF_ChannelLine & line)
{
    ChannelElement *ele = item(channelId);
    if(!ele) return;
    ZCHX::Data::ITF_Channel item = ele->data();
    for (int j = 0; j < item.lineList.size(); j++)
    {
        if (item.lineList[j] == line)
        {
            ele->setLineSelected(j, true);
        }
        else
        {
            ele->setLineSelected(j, false);
        }
    }

}

void zchxChannelDataMgr::cancelChannelLine(int channelId)
{
    ChannelElement *ele = item(channelId);
    if(!ele) return;

    ZCHX::Data::ITF_Channel item = ele->data();
    for (int j = 0; j < item.lineList.size(); j++)
    {
        ele->setLineSelected(j, false);
    }
}

void zchxAreaNetDataMgr::importData(const std::vector<std::pair<double, double> > &path)
{
    ZCHX::Data::ITF_AreaNet zone;
    zone.path = path;
    zone.id = 0;
    zone.area = zchxMapDataUtils::getTotalArea(path);

    QInputDialog input_dialog;
    input_dialog.setCancelButtonText(tr("Cancel"));
    input_dialog.setOkButtonText(tr("OK"));
    input_dialog.setWindowTitle(tr("WARRING ZONE"));
    input_dialog.setLabelText(tr("(Default 5 minute warning)Name:"));
    if (input_dialog.exec())
    {
        zone.name = input_dialog.textValue();
    }

    if(!zone.name.isEmpty())
    {
        //先添加进去, 确保显示
        updateData(zone);
        emit mDisplayWidget->signalCreateAreaNetZone(zone);
    }
}

void zchxMooringDataMgr::importData(const std::vector<std::pair<double, double> > &path)
{
    ZCHX::Data::ITF_Mooring zone;
    zone.path = path;
    zone.id = 0;
//    zone.area = getArea(path);

    MooringInfoDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        zone.name = dlg.getName();
        zone.anchorType = dlg.getMooringType();
        zone.isWarn = dlg.getMooringStatus();
        zone.fillColor = dlg.getColor();
        zone.displaceDis = dlg.getDisplaceDis();
        zone.displaceCycle = dlg.getDisplaceCycle();
    }

    if(!zone.name.isEmpty())
    {
        //先添加进去, 确保显示
        updateData(zone);
        emit mDisplayWidget->signalCreateMooringZone(zone);
    }
}

bool zchxMooringDataMgr::updateActiveItem(const QPoint &pt)
{
    bool sts = zchxTemplateDataMgr::updateActiveItem(pt);
    if(sts && mDisplayWidget) {
        MooringElement* ele = static_cast<MooringElement*>(mDisplayWidget->getCurrentSelectedElement());
        if(ele) mDisplayWidget->signalIsSelected4MooringZone(ele->data());
    }
    return sts;
}

void zchxCardMouthDataMgr::importData(const std::vector<std::pair<double, double> > &path)
{
    ZCHX::Data::ITF_CardMouth zone;
    zone.path = path;
    zone.id = 0;

    CardMouthInfoDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        zone.name = dlg.getName();
        zone.caMouCapConType = dlg.getType();
        zone.isAisUnopened = dlg.isAisUnopened();
        zone.isWarn = dlg.getWarning();
        zone.fillColor = dlg.getColor();
        zone.overlength = dlg.getOverlength();
        zone.enterStatus = dlg.getEnterStatus();
        zone.isPhotograph = dlg.getIsPhoto();
    }

    if(!zone.name.isEmpty())
    {
        //先添加进去, 确保显示
        updateData(zone);
        emit mDisplayWidget->signalCreateCardMouthZone(zone);
    }
}

bool zchxCardMouthDataMgr::updateActiveItem(const QPoint &pt)
{
    bool sts = zchxTemplateDataMgr::updateActiveItem(pt);
    if(sts && mDisplayWidget) {
        CardMouthElement* ele = static_cast<CardMouthElement*>(mDisplayWidget->getCurrentSelectedElement());
        if(ele) mDisplayWidget->signalIsSelected4CardMouthZone(ele->data());
    }
    return sts;
}

void zchxStatistcLineDataMgr::importData(const std::vector<std::pair<double, double> > &path)
{
    ZCHX::Data::ITF_StatistcLine zone;
    zone.path = path;
    zone.id = 0;

    StatistcLineInfoDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        zone.name = dlg.getName();
        zone.fillColor = dlg.getColor();
        zone.enterStatus = dlg.getEnterStatus();
    }

    if(!zone.name.isEmpty())
    {
        //先添加进去, 确保显示
        updateData(zone);
        emit mDisplayWidget->signalCreateStatistcLineZone(zone);
    }
}

bool zchxStatistcLineDataMgr::updateActiveItem(const QPoint &pt)
{
    bool sts = zchxTemplateDataMgr::updateActiveItem(pt);
    if(sts && mDisplayWidget) {
        StatistcLineElement* ele = static_cast<StatistcLineElement*>(mDisplayWidget->getCurrentSelectedElement());
        if(ele) mDisplayWidget->signalIsSelected4StatistcLineZone(ele->data());
    }
    return sts;
}

void zchxShipAlarmAscendDataMgr::show(QPainter* painter)
{
    if( !painter || !mDisplayWidget->getLayerMgr()->isLayerVisible(mLayerName) || mData.empty()) return;
    //追溯线
    QPolygonF points;
    for(std::shared_ptr<ShipAlarmAscendElement> ele : mData)
    {
        ZCHX::Data::ITF_ShipAlarmAscend data = ele->data();
        points.append(mDisplayWidget->framework()->LatLon2Pixel(data.lat, data.lon).toPointF());
    }
    if(points.size() >= 2)
    {
        PainterPair chk(painter);
        painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));
        painter->drawPolyline(points);
    }

    zchxTemplateDataMgr::show(painter);
}
