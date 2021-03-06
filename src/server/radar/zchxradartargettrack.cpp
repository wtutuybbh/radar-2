﻿#include "zchxradartargettrack.h"
#include <QDebug>
#include <QGeoCoordinate>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>


#define     PROCESS_MULTI_CALC

extern double polygonArea(const QPolygon& poly);
extern double polygonArea(const QPolygonF& poly);

extern bool   debug_output;
int radar_scan_time_msec = 0;
#define     DEBUG_TRACK_INFO                if(debug_output) qDebug()

QPolygonF  predictionAreaLL2PolygonF(const com::zhichenhaixin::proto::PredictionArea& area)
{
    QPolygonF poly;
    for(int i=0; i<area.area_size(); i++)
    {
        com::zhichenhaixin::proto::Latlon pnt = area.area(i);
        poly.append(latlonToMercator(pnt.latitude(), pnt.longitude()).toPointF());
    }

    return poly;
}
zchxRadarTargetTrack::zchxRadarTargetTrack(const zchxVideoParserSettings& setting, QObject *parent)
    : QThread(parent)
    , mSettings(setting)
    , mMaxEstCount(5)
    , mTargetPredictionInterval(2)
    , mIsOver(false)
    , mLastVideoDataTime(0)
    , mThreadPool(0)
    , mRangeFactor(1.0)
{
    mMaxSpeed = mSettings.user_video_parse.max_target_speed * 1.852 / 3.6;
    mMinNum = (setting.radar_id * 10 + setting.channel_id) * 10000;
    mMaxNum = mMinNum + 9998;
    mRectNum = mMinNum;
    qRegisterMetaType<zchxRadarSurfaceTrack>("const zchxRadarSurfaceTrack&");    
    qRegisterMetaType<videoParseData>("const videoParseData&");
#ifdef PROCESS_MULTI_CALC
    if(!mThreadPool) mThreadPool = new QThreadPool(this);
    mThreadPool->setMaxThreadCount(8);
#endif
}

zchxRadarTargetTrack::~zchxRadarTargetTrack()
{
    if(mThreadPool)
    {
        mThreadPool->clear();
//        mThreadPool->waitForDone();
        mThreadPool->deleteLater();
    }
    mTargetNodeMap.clear();
}

void zchxRadarTargetTrack::appendTrackTask(const zchxRadarRectDefList& task)
{
    QMutexLocker locker(&mTaskMutex);
    mTaskList.append(task);
}

bool zchxRadarTargetTrack::getTask(zchxRadarRectDefList &task)
{
    QMutexLocker locker(&mTaskMutex);
    if(mTaskList.size() == 0) return false;
    task = mTaskList.takeFirst();
    if(mTaskList.size() > 0)
    {
        qDebug()<<"delete unprocessed task size:"<<mTaskList.size();
        mTaskList.clear();
    }

}

void zchxRadarTargetTrack::run()
{
    while (!mIsOver) {
        //获取当前的任务
        zchxRadarRectDefList task;
        if(!getTask(task))
        {
            msleep(1000);
            continue;
        }
        //开始进行处理
        process(task);
    }
}

void zchxRadarTargetTrack::mergeRectTargetInDistance(zchxRadarRectDefList &temp_list, int target_merge_distance)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    //目标预处理.将距离太近的目标进行合并,合并的中心点取二者的重点
    for(int i=0; i<temp_list.size();)
    {
        zchxRadarRectDef &cur = temp_list[i];
        //获取和当前目标距离较近的目标,目标可能多个,这里暂且设定为list,
        zchxRadarRectDefList merge_list;
        for(int k = i+1; k<temp_list.size();)
        {
            zchxRadarRectDef next = temp_list[k];
            //计算两者的距离
            double distance = getDisDeg(cur.mSrcRect.center().latitude(), cur.mSrcRect.center().longitude(),
                                        next.mSrcRect.center().latitude(), next.mSrcRect.center().longitude());
            if(distance < target_merge_distance)
            {
                //目标存在,从原来的队列里边删除
                merge_list.append(next);
                temp_list.removeAt(k);
                continue;
            }
            k++;
        }
        if(merge_list.size() == 0)
        {
            i++;
            continue;
        }
        //合并目标
        merge_list.append(cur);
        Latlon ll = getMergeTargetLL(merge_list);
        changeTargetLL(ll, cur);
    }
}

Latlon zchxRadarTargetTrack::getMergeTargetLL(const zchxRadarRectDefList &list)
{
    double sum_x = 0, sum_y = 0;
    foreach (zchxRadarRectDef temp, list) {
        sum_x += temp.mSrcRect.center().longitude();
        sum_y += temp.mSrcRect.center().latitude();
    }
    if(list.size() > 0)
    {
        sum_x = sum_x / list.size();
        sum_y = sum_y / list.size();
    }

    return Latlon(sum_y, sum_x);
}

void zchxRadarTargetTrack::changeTargetPixCenter(const QPoint &now, zchxRadarRectDef *cur)
{
    if(!cur) return;
    //检查新旧中心的对比，移动坐标系的外形点列
    QPoint old = cur->mCenter;
    QPoint sub = now - old;
    cur->mShapePnts.translate(sub);
    cur->mCenter = now;

    //经纬度
    Latlon ll = cur->mPosConveter.pixel2Latlon(now);
    double dlon = ll.lon - cur->mSrcRect.center().longitude();
    double dlat = ll.lat - cur->mSrcRect.center().latitude();
    cur->mSrcRect.mutable_center()->set_longitude(ll.lon);
    cur->mSrcRect.mutable_center()->set_latitude(ll.lat);
    //将目标的所有经纬度坐标进行同步的变换
    cur->mSrcRect.mutable_boundrect()->mutable_topleft()->set_latitude(cur->mSrcRect.boundrect().topleft().latitude() + dlat);
    cur->mSrcRect.mutable_boundrect()->mutable_topleft()->set_longitude(cur->mSrcRect.boundrect().topleft().longitude() + dlon);
    cur->mSrcRect.mutable_boundrect()->mutable_bottomright()->set_latitude(cur->mSrcRect.boundrect().bottomright().latitude() + dlat);
    cur->mSrcRect.mutable_boundrect()->mutable_bottomright()->set_longitude(cur->mSrcRect.boundrect().bottomright().longitude() + dlon);
    for(int i=0; i<cur->mSrcRect.outline_size(); i++)
    {
        zchxLatlon *ll = cur->mSrcRect.mutable_outline(i);
        ll->set_latitude(ll->latitude() + dlat);
        ll->set_longitude(ll->longitude() + dlon);
    }
    if(cur->mSrcRect.has_seg())
    {
        cur->mSrcRect.mutable_seg()->mutable_start()->set_latitude(cur->mSrcRect.seg().start().latitude() + dlat);
        cur->mSrcRect.mutable_seg()->mutable_start()->set_longitude(cur->mSrcRect.seg().start().longitude() + dlon);
        cur->mSrcRect.mutable_seg()->mutable_end()->set_latitude(cur->mSrcRect.seg().end().latitude() + dlat);
        cur->mSrcRect.mutable_seg()->mutable_end()->set_longitude(cur->mSrcRect.seg().end().longitude() + dlon);
    }

}


void zchxRadarTargetTrack::changeTargetLL(const Latlon &ll, zchxRadarRectDef &cur)
{
    double dlon = ll.lon - cur.mSrcRect.center().longitude();
    double dlat = ll.lat - cur.mSrcRect.center().latitude();
    cur.mSrcRect.mutable_center()->set_longitude(ll.lon);
    cur.mSrcRect.mutable_center()->set_latitude(ll.lat);
    //将目标的所有经纬度坐标进行同步的变换
    cur.mSrcRect.mutable_boundrect()->mutable_topleft()->set_latitude(cur.mSrcRect.boundrect().topleft().latitude() + dlat);
    cur.mSrcRect.mutable_boundrect()->mutable_topleft()->set_longitude(cur.mSrcRect.boundrect().topleft().longitude() + dlon);
    cur.mSrcRect.mutable_boundrect()->mutable_bottomright()->set_latitude(cur.mSrcRect.boundrect().bottomright().latitude() + dlat);
    cur.mSrcRect.mutable_boundrect()->mutable_bottomright()->set_longitude(cur.mSrcRect.boundrect().bottomright().longitude() + dlon);
    for(int i=0; i<cur.mSrcRect.outline_size(); i++)
    {
        zchxLatlon *ll = cur.mSrcRect.mutable_outline(i);
        ll->set_latitude(ll->latitude() + dlat);
        ll->set_longitude(ll->longitude() + dlon);
    }
    if(cur.mSrcRect.has_seg())
    {
        cur.mSrcRect.mutable_seg()->mutable_start()->set_latitude(cur.mSrcRect.seg().start().latitude() + dlat);
        cur.mSrcRect.mutable_seg()->mutable_start()->set_longitude(cur.mSrcRect.seg().start().longitude() + dlon);
        cur.mSrcRect.mutable_seg()->mutable_end()->set_latitude(cur.mSrcRect.seg().end().latitude() + dlat);
        cur.mSrcRect.mutable_seg()->mutable_end()->set_longitude(cur.mSrcRect.seg().end().longitude() + dlon);
    }
    //检查新旧中心的对比，移动坐标系的外形点列
    QPoint old = cur.mCenter;
    QPoint now = cur.mPosConveter.Latlon2Pixel(ll).toPoint();
    QPoint sub = now - old;
    cur.mShapePnts.translate(sub);
    cur.mCenter = now;
}

bool zchxRadarTargetTrack::isDirectionChange(double src, double target)
{
    //计算原角度对应的相反角度的范围,反映到0-360的范围
    int min = int(src - mSettings.user_video_parse.direction_invert_hold);
    int max = int(src + mSettings.user_video_parse.direction_invert_hold);
    if(min < 0)
    {
        //原方向指向右上方向,则有效值的范围是min_+360 ~ 360, 0~max
        if((target >= 0  && target < max) || (target > min+360)) return false;
        return true;
    } else if(max > 360)
    {
        //原方向指向左上方向,则有效值的范围是min ~ 360, 0~max-360
        if((target >= 0  && target < max-360) || (target > min)) return false;
        return true;
    } else
    {
        //最大或者最小都在0-360的范围内
        if(target > min && target < max) return false;
        return true;
    }
    return false;
}

bool zchxRadarTargetTrack::isRectAreaContainsPoint(const zchxRadarRectDef &rect, double lat, double lon)
{
    bool sts = false;
    QPolygonF poly;
    for(int i=0; i<rect.mSrcRect.outline_size();i++)
    {
        zchxLatlon block = rect.mSrcRect.outline(i);
        //将经纬度转换成墨卡托点列
        Mercator m = latlonToMercator(block.latitude(), block.longitude());
        poly.append(m.toPointF());
    }
    if(poly.size() > 0)
    {
        sts = poly.containsPoint(latlonToMercator(lat, lon).toPointF(), Qt::OddEvenFill);
    }
    return sts;
}

void zchxRadarTargetTrack::CounterPossibleChildRect(QList<AreaNodeTable>& areaTableList, QMap<TargetNode*, zchxRadarRectDefList>& result , const zchxRadarRectDefList& rects)
{
    zchxTimeElapsedCounter counter(__FUNCTION__);
    result.clear();
    //先检查每一个区域内落入了哪些目标
    for(int i=0; i<areaTableList.size(); i++)
    {
        AreaNodeTable &table = areaTableList[i];
        for(int k=0; k<rects.size(); k++)
        {
            zchxRadarRectDef def = rects[k];
            Mercator pos = latlonToMercator(def.mSrcRect.center().latitude(), def.mSrcRect.center().longitude());
            if(table.mArea.containsPoint(pos.toPointF(), Qt::OddEvenFill))
            {
                 table.mRectList.append(def);
            }
        }
    }
    //开始统计整理，每一个节点现在有多少个待选节点,有可能存在重复的目标添加
    foreach (AreaNodeTable table, areaTableList)
    {
        foreach (TargetNode* node, table.mNodeList)
        {
            result[node].append(table.mRectList);
        }
    }
}

double zchxRadarTargetTrack::calcDis(const zchxRadarRectDef &p1, const zchxRadarRectDef &p2)
{
    QGeoCoordinate src(p1.mSrcRect.center().latitude(), p1.mSrcRect.center().longitude());

    return src.distanceTo(QGeoCoordinate(p2.mSrcRect.center().latitude(), p2.mSrcRect.center().longitude()));
}

#include <QRunnable>
class nodeRelativeRectRun : public QRunnable
{
public:
    explicit nodeRelativeRectRun(TargetNode* node, zchxRadarTargetTrack* parent, zchxRadarRectDefList* rectlist)
    {
        mNode = node;
        mParent = parent;
        mRectList = rectlist;
        setAutoDelete(true);
    }

    ~nodeRelativeRectRun() {}
    void run()
    {
        zchxRadarRectDefList dis_selected_list, overlap_selected_list;
        if(!mNode) return;

        //先将关联矩形删除
        mNode->mRelativeRectList.clear();
        //将新周期的回波与上一周期的回波进行对比，识别相同位置或者位置小于一个回波长度的归为一个目标
        double min_dis = LONG_MAX;
        foreach (zchxRadarRectDef rect , *mRectList)
        {
            //计算两个目标点的位置距离
            double dis = mParent->calcDis(*(mNode->mDefRect), rect);
            //距离是否小于目标对应的回波宽度
            if(dis > mNode->mDefRect->mSrcRect.boundrect().diameter()) continue;
//            if(dis < min_dis)
//            {
//                min_dis = dis;
//                dis_selected_list.clear();
//                dis_selected_list.append(rect);
//            }
            //检查回波图形是否相交
            QPolygon intersected = mNode->mDefRect->mShapePnts.intersected(rect.mShapePnts);
            if(intersected.size() > 0)
            {
                overlap_selected_list.append(rect);
            }
        }
        //有回波图形和原来的回波图形相交，优先使用
        mNode->mRelativeRectList.append(overlap_selected_list);
        if(mNode->mRelativeRectList.size() == 0)
        {
            //没有相交的回波图形,使用距离最短的回波图形
            mNode->mRelativeRectList.append(dis_selected_list);
        }
        if(mNode->mRelativeRectList.size() == 1)
        {
            mParent->updateVideoCounter(mNode->mRelativeRectList.first().mSrcRect.rectnumber());
        }

        bool sts = mNode->mRelativeRectList.size() != 0;
    }

private:
    zchxRadarTargetTrack*       mParent;
    TargetNode*                 mNode;
    zchxRadarRectDefList*       mRectList;
};



bool zchxRadarTargetTrack::calcNodeMostRelativeRect(TargetNode* node,  const zchxRadarRectDefList& list)
{
    zchxRadarRectDefList dis_selected_list, overlap_selected_list;
    if(!node) return false;

    //先将关联矩形删除
    node->mRelativeRectList.clear();
    //将新周期的回波与上一周期的回波进行对比，识别相同位置或者位置小于一个回波长度的归为一个目标
    double min_dis = LONG_MAX;
    foreach (zchxRadarRectDef rect , list)
    {
        //计算两个目标点的位置距离
        double dis = calcDis(*(node->mDefRect), rect);
        //距离是否小于目标对应的回波宽度
        if(dis > node->mDefRect->mSrcRect.boundrect().diameter()) continue;
//        if(dis < min_dis)
//        {
//            min_dis = dis;
//            dis_selected_list.clear();
//            dis_selected_list.append(rect);
//        }
        //检查回波图形是否相交
        if(node->mDefRect->mShapePnts.intersected(rect.mShapePnts).size() > 0)
        {
            overlap_selected_list.append(rect);
        }
    }
    //有回波图形和原来的回波图形相交，优先使用
    node->mRelativeRectList.append(overlap_selected_list);
    if(node->mRelativeRectList.size() == 0)
    {
        //没有相交的回波图形,使用距离最短的回波图形
        node->mRelativeRectList.append(dis_selected_list);
    }

    if(node->mRelativeRectList.size() == 1)
    {
        updateVideoCounter(node->mRelativeRectList.first().mSrcRect.rectnumber());
    }

    return node->mRelativeRectList.size() != 0;
}

bool updateNextNodeSpeedAndCog(zchxRadarRectDef& target, TargetNode* node, double max_speed, bool speed_check)
{
    //计算距离开始点所经过的距离
    TargetNode* top = node->topNode();
    if(!top) return false;
    QGeoCoordinate source(top->mDefRect->mSrcRect.center().latitude(), top->mDefRect->mSrcRect.center().longitude());
    QGeoCoordinate dest(target.mSrcRect.center().latitude(), target.mSrcRect.center().longitude());
    double delta_time = target.mSrcRect.updatetime() - top->mDefRect->mSrcRect.updatetime();
    double distance = source.distanceTo(dest);
    double sog = 0.0;
    if(delta_time > 0) sog = distance / delta_time;
    if(sog > max_speed) sog = max_speed;

//    //速度校正
//    double refer_sog = node->getReferenceSog();
//    if(refer_sog > 0.1)
//    {
//        sog = (sog + refer_sog) / 2.0;
//    }
//    if(speed_check)
//    {
//        //检查速度是否超出了设定的最大速度, 超过了最大速度就跳过不处理
//        if(sog > max_speed) return false;
//        //检查目标的速度是否超出了前一个节点的速度的2倍以上， 不处理
//        if(node->mDefRect->mSrcRect.sogms() > 0 && sog > node->mDefRect->mSrcRect.sogms() * 1.2) return false;
//    }

    //角度校正
    Latlon refer_ll = node->getReferencePoint();
    double cog = QGeoCoordinate(refer_ll.lat, refer_ll.lon).azimuthTo(dest);

    //更新到目标路径中
    target.mSrcRect.set_cog(cog);
    target.mSrcRect.set_sogms(sog);
    return true;
}

double zchxRadarTargetTrack::calNodeMoveDis(TargetNode *node)
{
    if(!node) return 0.0;
    TargetNode* topNode = node->topNode();
    if(!topNode) return 0.0;
    //计算两个节点之间的距离
    return calcDis(node->mDefRect, topNode->mDefRect);
}

void zchxRadarTargetTrack::determineNodeStatus(TargetNode* node, bool term_chk)
{
//    zchxTimeElapsedCounter counter(__FUNCTION__);
    if(!node) return;
    //开始从最新节点往上遍历，计算目标此刻的运动距离，
    //如果超出了目标的回波图形宽度，就将目标视为运动，否则就是静止
    //获取最起初的第一个目标
    TargetNode* topNode = node->topNode();
    if(topNode == node) return;
    //计算初次发现到现在经历了多少个周期
    uint cur_node_term = node->mVideoTerm;
    if(cur_node_term < topNode->mVideoTerm) cur_node_term += MAX_RADAR_VIDEO_INDEX_T;
    uint sub_term = cur_node_term - topNode->mVideoTerm;
//    if(term_chk && sub_term < mSettings.user_video_parse.confirm_target_cnt) return;
    if(node->mStatus == Node_UnDef)
    {
        //计算两个节点之间的距离
        double dis = calcDis(node->mDefRect, topNode->mDefRect);
        double size = topNode->mDefRect->mSrcRect.boundrect().diameter();
        if(size >= mSettings.user_video_parse.target_move_confirm_dis) size = mSettings.user_video_parse.target_move_confirm_dis;
        int time = node->mDefRect->mSrcRect.updatetime() - topNode->mUpdateTime;
        if(dis >= size)
        {
            qDebug()<<"target "<<node->mSerialNum<<" is confirmed as move obj .distance:"<<dis<<"diameter:"<<size<<" time:"<<time;
            node->mStatus = Node_Moving;
            //打印方位角
            QList<TargetNode*> path = node->path();
            foreach (TargetNode* temp, path) {
                qDebug()<<node->mSerialNum<<temp->mDefRect->mCenter<<temp->mDefRect->mSrcRect.cog()<<temp->mDefRect->mSrcRect.sogms();
            }

//            //通过最小二乘法调整目标的位置
//            QList<TargetNode*> path = node->path();
//            QList<QPointF> src_pnts;
//            foreach (TargetNode* temp, path) {
//                src_pnts.append(temp->mDefRect->mPixCenter);
//            }
//            LeastSquare ls(src_pnts);
//            //开始更新各个点对应的中心位置
//            foreach (TargetNode* temp, path) {
//                //重新计算中心位置
//                double x = temp->mDefRect->mPixCenter.x();
//                double y = ls.getY(x);
//                changeTargetPixCenter(QPointF(x, y), temp->mDefRect);
//            }
            //开始重新更新方位角和速度值


        }
    }
    if(sub_term > mSettings.user_video_parse.max_history_num)
    {
        //删除目标的最起初的节点
//        qDebug()<<"delete top node:"<<topNode;
        TargetNode* child = topNode->mChild;
        if(child) child->mParent.reset();
    }

}

bool zchxRadarTargetTrack::isSamePoint(TargetNode *src, const zchxRadarRectDef &target)
{
    if(!src || !src->mDefRect) return false;
    QPoint point1 = src->mDefRect->mCenter;
    QPoint point2 = target.mCenter;
    return point1 == point2;
}

bool zchxRadarTargetTrack::preCheckTargetValid(TargetNode *src, const zchxRadarRectDef &target)
{
    if(!src) return true;

    //如果目标是第一个节点，那么任意方向都是可行的
    if(!src->mParent)
    {
        DEBUG_TRACK_INFO<<src->mSerialNum<<" is top child. so next target is always true...";
        return true;
    }
    if(src->mDefRect->mCenter == target.mCenter)
    {
        DEBUG_TRACK_INFO<<src->mSerialNum<<" next child is same as curreent. so is ok..";
        return true;
    }
    QPoint pre = src->mParent->mDefRect->mCenter;
    QPoint now = src->mDefRect->mCenter;
    QPoint next = target.mCenter;

    double angle1 = atan2(now.y() - pre.y(), now.x() - pre.x()) * 180 / GLOB_PI;
    if(angle1 < 0) angle1 += 360;
    double angle2 = atan2(next.y() - now.y(), next.x() - now.x())* 180 / GLOB_PI;
    if(angle2 < 0) angle2 += 360;

    DEBUG_TRACK_INFO<<src->mSerialNum<<"pre pnt:"<<pre<<" cur:"<<now<<" next:"<<next<<" pre_anlge:"<<angle1<<" now_angle:"<<angle2;

    //计算下一个目标可能的角度范围[-45, 45]
    int min = int(angle1 - 60);
    int max = int(angle1 + 60);
    if(min < 0)
    {
        if((angle2 >= 0  && angle2 < max) || (angle2 > min+360)) return true;
    } else if(max > 360)
    {
        if((angle2 >= 0  && angle2 < max-360) || (angle2 > min)) return true;
    } else
    {
        //最大或者最小都在0-360的范围内
        if(angle2 > min && angle2 < max) return true;
    }

    return false;

}

void zchxRadarTargetTrack::updateVideoCounter(int index)
{
#ifdef PROCESS_MULTI_CALC
    QMutexLocker locker(&mVideoCounterMutex);
#endif
    int val = mVideoUsedCounter.value(index, 0);
    mVideoUsedCounter[index] = val+1;
}

void zchxRadarTargetTrack::addDelNodeReason(quint32 time, double lat, double lon, const QString &reason, int track)
{
//    QString key = QString("%1:%2").arg(time).arg(track);
    zchxCommon::zchxDelNodeLog log;
    log.time = time;
    log.track = track;
    log.lat = lat;
    log.lon = lon;
    log.reason = reason;
//    qDebug()<<"del node:"<<log.toJson();
    mDelNodeReasonList.append(log);
}

void zchxRadarTargetTrack::processWithPossibleRoute(const zchxRadarRectDefList &task)
{
    zchxTimeElapsedCounter counter(__FUNCTION__);
    if(task.size() == 0) return;
    int cur_cycle_index = task.first().mSrcRect.videocycleindex();
    zchxRadarRectDefList temp_list(task);             //保存的未经处理的所有矩形单元
    quint32 now_time = task.first().mSrcRect.updatetime();
    qDebug()<<"now process list time:"<<QDateTime::fromTime_t(task.first().mSrcRect.updatetime()).toString("yyyy-MM-dd hh:mm:ss")<<task.size();

    //目标最初都认为是静止，当目标的运动长度超出了目标回波的宽度的时候，才认为目标是运动目标
    QTime t;
    t.start();
    //先计算已经存在的目标可能的待选目标
    mVideoUsedCounter.clear();
    if(mThreadPool)
    {
        foreach (QSharedPointer<TargetNode> node, mTargetNodeMap) {
            mThreadPool->start(new nodeRelativeRectRun(node.data(), this, &temp_list));
        }
//        mThreadPool->waitForDone();
        while (mThreadPool->activeThreadCount()) {
            if(mIsOver){
                mThreadPool->clear();
                break;
            }
            msleep(100);
        }
    } else
    {
        foreach (QSharedPointer<TargetNode> node, mTargetNodeMap) {
            if(mIsOver) break;
            calcNodeMostRelativeRect(node.data(),  temp_list);
        }
    }

    qDebug()<<"calculate node relative rect elapsed time: "<<t.elapsed();
    t.start();
    //对已经存在的目标，更新其下一个节点。
    QList<int>  used_index_list;        //标记已经更新过的目标块
    QList<int> keys = mTargetNodeMap.keys();
    foreach (int key, keys)
    {
        if(mIsOver) break;
        QSharedPointer<TargetNode> node = mTargetNodeMap[key];
        if(!node) continue;

        //新的回波周期过来，没有找到可以更新的回波图形,将原来的回波图形复制更新到预推位置
        if(node->mRelativeRectList.size() == 0)
        {
            DEBUG_TRACK_INFO<<"find no next rect "<<node->mSerialNum;
            //检查目标是否需要预推，如果目标连续长时间没有更新，就将目标删除
            if(node->mLastLostTime > 0 && now_time - node->mLastLostTime >= mSettings.user_video_parse.clear_target_time)
            {
                //删除对应的目标
                DEBUG_TRACK_INFO<<"remove target not updated for specified expired time with key:"<<node->mSerialNum;
                mTargetNodeMap.remove(key);
                addDelNodeReason(now_time, node->mDefRect->mSrcRect.center().latitude(), node->mDefRect->mSrcRect.center().longitude(),
                                 QStringLiteral("超时未更新删除"), node->mSerialNum);
                continue;
            }
            //标记目标跟踪丢失的时间
            if(node->mLastLostTime == 0)
            {
                DEBUG_TRACK_INFO<<"target "<<node->mSerialNum<<" lost in this term:"<<cur_cycle_index<<" start prediction now...";
                node->mLastLostTime = now_time;
            }
            //保存前一次回波块的时间，更新目标回波块的时间为当次时间
            int old_time = node->mDefRect->mSrcRect.updatetime();
            node->mDefRect->mSrcRect.set_updatetime(now_time);
            //静止目标就保持不变, 如果是运动目标，则根据时间间隔值执行预推
            if(node->mStatus != Node_Moving)
            {
                DEBUG_TRACK_INFO<<"target "<<node->mSerialNum<<" is not a move target. not prediction. only update time..";
                continue;
            }

            //检查目标是否需要执行预推
            bool exec_prediction = false;
            int prediction_gap = mSettings.user_video_parse.confirm_target_cnt + mSettings.user_video_parse.video_overlap_cnt;
            if(node->mLastPrecitonTerm == 0)
            {
                //初次不预推，标记这种情况下的上一次预推为本次周期的上一周期
                int last_term_index = cur_cycle_index - 1;
                if(last_term_index < 0) last_term_index += MAX_RADAR_VIDEO_INDEX_T;
                node->mLastPrecitonTerm = last_term_index;
            }

            int gap = cur_cycle_index - node->mLastPrecitonTerm;
            if(gap < 0) gap += MAX_RADAR_VIDEO_INDEX_T;
            if(gap >= prediction_gap) exec_prediction = true;
            if(!exec_prediction)
            {
                DEBUG_TRACK_INFO<<"target "<<node->mSerialNum<<" prediction condition not match."<<" last prediciotn trem:"<<node->mLastPrecitonTerm<<" now term:"<<cur_cycle_index<<" gap:"<<gap<<" prediction gap:"<<prediction_gap;
                continue;
            }
            DEBUG_TRACK_INFO<<"start target "<<node->mSerialNum<<" prediction "<<" last prediciotn trem:"<<node->mLastPrecitonTerm<<" now term:"<<cur_cycle_index<<" gap:"<<gap<<" prediction gap:"<<prediction_gap;

            zchxRadarRectDef copy;
            copy.CopyFrom(*(node->mDefRect));
            copy.mSrcRect.set_realdata(false);
            copy.mSrcRect.set_updatetime(now_time);
            copy.mSrcRect.set_videocycleindex(cur_cycle_index);

            //将目标移动预推的位置
            double delta_time = now_time - old_time;
            QGeoCoordinate source(node->mDefRect->mSrcRect.center().latitude(), node->mDefRect->mSrcRect.center().longitude());
            //计算目标的预推位置
            double delta_dis = node->mDefRect->mSrcRect.sogms() * delta_time;
            QGeoCoordinate dest = source.atDistanceAndAzimuth(delta_dis, node->mDefRect->mSrcRect.cog());
            Latlon ll(dest.latitude(), dest.longitude());
            changeTargetLL(ll, copy);
            node->mLastPrecitonTerm = cur_cycle_index;
            node->mRelativeRectList.append(copy);
            DEBUG_TRACK_INFO<<"target "<<node->mSerialNum<<" prediction to pix"<<copy.mCenter<<" and old is:"<<node->mDefRect->mCenter;
        } else
        {
            DEBUG_TRACK_INFO<<"find next rect size: "<<node->mRelativeRectList.size()<<node->mSerialNum;
            if(node->mLastLostTime != 0)
            {
                DEBUG_TRACK_INFO<<"target "<<node->mSerialNum<<" reappear in this term:"<<cur_cycle_index<<" end prediction now...";
            }
            node->mLastLostTime = 0;
            node->mLastPrecitonTerm = 0;
        }

        bool multi_target = node->mRelativeRectList.size() > 1;
        if(multi_target)
        {
            //目标的前一周期回波图形上出现叠加了多个这一周期的回波图形，看起来就是目标回波图形分裂成了多个目标图形
            //这时候就将原来的大的回波图形进行删除，让新的回波图形生成新的目标
            DEBUG_TRACK_INFO<<"remove target split into multi targets with key:"<<node->mSerialNum;
            mTargetNodeMap.remove(key);
            addDelNodeReason(now_time, node->mDefRect->mSrcRect.center().latitude(), node->mDefRect->mSrcRect.center().longitude(),
                             QStringLiteral("目标检索到多个可能目标图形"), node->mSerialNum);
            continue;
        }        

        //根据待选点更新目标的下一个状态位置
        zchxRadarRectDef target = node->mRelativeRectList.takeFirst();
        //检查新的回波位置是否真正的符合要求。这里主要是看看回波是否小回波遇到大回波的情况。
        if(target.mArea > node->mDefRect->mArea * 1.5)
        {
            if(target.mSrcRect.realdata() && mVideoUsedCounter.value(target.mSrcRect.rectnumber(), 0) > 1)
            {
                DEBUG_TRACK_INFO<<"target "<<node->mSerialNum<<" area changed larger than 150. small it to old one. and make it appropriate position";
                //重新构造回波图形
                zchxRadarRectDef copy;
                copy.CopyFrom(*(node->mDefRect));
                copy.mSrcRect.set_realdata(true);
                copy.mSrcRect.set_updatetime(now_time);
                copy.mSrcRect.set_videocycleindex(cur_cycle_index);
                copy.mSrcRect.set_rectnumber(target.mSrcRect.rectnumber());
                if(node->mStatus == Node_Moving)
                {
                    //大回波有可能是原来的几个小回波合并起来的，需要将小回波的更新位置校正到大回波对应的可能位置
                    //这里主要是更新目标的中心位置
                    //计算目标的预推位置
                    double delta_time = now_time - node->mDefRect->mSrcRect.updatetime();
                    QGeoCoordinate source(node->mDefRect->mSrcRect.center().latitude(), node->mDefRect->mSrcRect.center().longitude());
                    double delta_dis = node->mDefRect->mSrcRect.sogms() * delta_time;
                    QGeoCoordinate geo_dest = source.atDistanceAndAzimuth(delta_dis, node->mDefRect->mSrcRect.cog());
                    QPoint geo_pix = target.mPosConveter.Latlon2Pixel(Latlon(geo_dest.latitude(), geo_dest.longitude())).toPoint();
                    //检查预推位置是否在新的回波图形里边,如果没有就将预推位置向新的回波中心靠
                    int loop = 0;
                    while (!target.mShapePnts.containsPoint(geo_pix, Qt::OddEvenFill))
                    {
                        geo_pix.setX( qRound(0.5* (geo_pix.x() + target.mCenter.x())));
                        geo_pix.setY( qRound(0.5* (geo_pix.y() + target.mCenter.y())));
                        if(qAbs(geo_pix.x() - target.mCenter.x()) <= 1 && (qAbs(geo_pix.y() - target.mCenter.y()) <= 1)) break;
                        if(loop >= 1000) break;
                        loop++;
                    }
                    Latlon ll = copy.mPosConveter.pixel2Latlon(geo_pix);
                    changeTargetLL(ll, copy);
                }
                target.CopyFrom(copy);
            }

        } else if(target.mArea <= node->mDefRect->mArea * 0.5)
        {
            DEBUG_TRACK_INFO<<"target "<<node->mSerialNum<<" area changed smaller than 0.5. large it to old one. and keep it position";
            //将目标的面积还原成原来的大小
            QPoint sub = target.mCenter - node->mDefRect->mCenter;
            target.mShapePnts = node->mDefRect->mShapePnts.translated(sub);
            target.mArea = node->mDefRect->mArea;
        }

        //预检查目标的方向是否与原来的方向相同
        if(!preCheckTargetValid(node.data(), target))
        {
            DEBUG_TRACK_INFO<<node->mSerialNum<<" next target direction error. use prevoius rect as newly update..";

            //目标反向了，继续使用就数据
            Latlon ll;
            ll.lat = node->mDefRect->mSrcRect.center().latitude();
            ll.lon = node->mDefRect->mSrcRect.center().longitude();
            changeTargetLL(ll, target);
        }

        //检查新目标和旧目标是否是同一个点
        if(isSamePoint(node.data(), target))
        {
            node->mDefRect->mSrcRect.set_updatetime(now_time);
            DEBUG_TRACK_INFO<<node->mSerialNum<<" same point now. only update rect time..";
        } else
        {
            DEBUG_TRACK_INFO<<node->mSerialNum<<" create a new child:"<<target.mCenter<<" old pos:"<<node->mDefRect->mCenter;
            updateNextNodeSpeedAndCog(target, node.data(), mMaxSpeed, false);
            //获取选择的目标，更新速度等
            QSharedPointer<TargetNode> new_node(new TargetNode(target, node));

            //将最新的节点更新到队列中
            mTargetNodeMap[node->mSerialNum] = new_node;
        }
        determineNodeStatus(mTargetNodeMap[node->mSerialNum].data(), true);


        //将目标添加到已经使用的队列中， 防止同一个回波块出现两个目标
        if(target.mSrcRect.realdata() && !used_index_list.contains(target.mSrcRect.rectnumber()))
        {
            DEBUG_TRACK_INFO <<"add origin rect into used list , not make new node from it."<<target.mSrcRect.rectnumber();
            used_index_list.append(target.mSrcRect.rectnumber());
        }
    }
    qDebug()<<"update exist elapsed time: "<<t.elapsed();
    //将未更新的矩形作为新目标添加
    for(int i=0; i<temp_list.size(); i++)
    {
        if(mIsOver) break;
        zchxRadarRectDef rect = temp_list[i];
        if(used_index_list.contains(rect.mSrcRect.rectnumber())) continue;
        TargetNode *node = new TargetNode(rect);
        node->mSerialNum = getCurrentNodeNum();
        mTargetNodeMap.insert(node->mSerialNum, QSharedPointer<TargetNode>(node));
        DEBUG_TRACK_INFO <<"make new node from origin rect ."<<"node number:"<<node->mSerialNum<<"origin rect number:"<<rect.mSrcRect.rectnumber()<<node->mDefRect->mCenter<<node->mParent<<node->mDefRect->mSrcRect.cog()<<node->mDefRect->mSrcRect.sogms();
    }

    //现在将目标进行输出
    outputTargets();
    qDebug()<<"totol target size:"<<mTargetNodeMap.size();

    //删除2分钟以前的日志
    for(int i=0; i<mDelNodeReasonList.size(); )
    {
        if(now_time - mDelNodeReasonList[i].time >= 20)
        {
            mDelNodeReasonList.removeAt(i);
            continue;
        }
        i++;
    }
    emit signalSendDelNodeLog(QJsonDocument(mDelNodeReasonList.toJson().toArray()).toJson());
}

void zchxRadarTargetTrack::outputTargets()
{
    zchxRadarSurfaceTrack   track_list;         //雷达目标数据输出
    track_list.set_flag(1);
    track_list.set_sourceid("240");
    track_list.set_utc(QDateTime::currentMSecsSinceEpoch());

    //遍历目标进行数据组装
    QList<TargetNode*> output_list;
    for(TargetNodeMap::iterator it = mTargetNodeMap.begin(); it != mTargetNodeMap.end(); it++)
    {
        if(mIsOver) break;
        TargetNode *node = it->data();
        if(!node || !node->mDefRect) continue;
        //构造目标数据
        updateTrackPointWithNode(track_list, node);
    }
    track_list.set_length(track_list.trackpoints_size());

    if(track_list.trackpoints_size())
    {
        emit signalSendTracks(track_list);
    }
}

int zchxRadarTargetTrack::getCurrentNodeNum()
{
    //更新编号,没有找到重复的点,直接添加
    if(mRectNum > mMaxNum)
    {
        mRectNum = mMinNum;
    }

    //检查当前是否还存在空闲的
    while (mRectNum <= mMaxNum) {
        if(mTargetNodeMap.contains(mRectNum)){
            mRectNum++;
            continue;
        }
        break;
    }

    return mRectNum;
}

void zchxRadarTargetTrack::process(const zchxRadarRectDefList &task)
{
    zchxTimeElapsedCounter counter(QString(metaObject()->className()) + " : " + QString(__FUNCTION__));
    processWithPossibleRoute(task);
    return;
}

void zchxRadarTargetTrack::dumpTargetDistance(const QString &tag, double merge_dis)
{
    DEBUG_TRACK_INFO<<"dump target distance now:"<<tag;
    QList<int> keys = mRadarRectMap.keys();
    for(int i=0; i<keys.size(); i++)
    {
        zchxRadarRect cur = mRadarRectMap[keys[i]];
        double cur_lat = cur.current_rect().center().latitude();
        double cur_lon = cur.current_rect().center().longitude();
        for(int k = i+1; k<keys.size(); k++)
        {
            zchxRadarRect next = mRadarRectMap[keys[k]];
            double next_lat = next.current_rect().center().latitude();
            double next_lon = next.current_rect().center().longitude();
            double distance = getDisDeg(cur_lat, cur_lon, next_lat, next_lon);
            if(distance < merge_dis)
            {
                DEBUG_TRACK_INFO<<"found abormal targets:"<<keys[i]<<keys[k]<<"  distance:"<<distance<<" time:"<<cur.current_rect().updatetime()<<next.current_rect().updatetime();
            }
        }
    }
}

void zchxRadarTargetTrack::checkTargetRectAfterUpdate(double merge_dis)
{
    QList<int> keys = mRadarRectMap.keys();
    for(int i=0; i<keys.size(); i++)
    {
        if(!mRadarRectMap.contains(keys[i])) continue;
        zchxRadarRect cur = mRadarRectMap[keys[i]];
        double cur_lat = cur.current_rect().center().latitude();
        double cur_lon = cur.current_rect().center().longitude();
        for(int k = i+1; k<keys.size(); k++)
        {
            if(!mRadarRectMap.contains(keys[k])) continue;
            zchxRadarRect next = mRadarRectMap[keys[k]];
            double next_lat = next.current_rect().center().latitude();
            double next_lon = next.current_rect().center().longitude();
            double distance = getDisDeg(cur_lat, cur_lon, next_lat, next_lon);
            if(distance < merge_dis)
            {
                if(cur.dir_confirmed() ^ next.dir_confirmed())
                {
                    DEBUG_TRACK_INFO<<"found abormal targets:"<<keys[i]<<keys[k]<<"  distance:"<<distance<<" confirmed:"<<cur.dir_confirmed()<<next.dir_confirmed()<<" remove not confirmed one.";

                    if(cur.dir_confirmed() && !next.dir_confirmed())
                    {
                        mRadarRectMap.remove(keys[k]);
                        continue;
                    }
                    if((!cur.dir_confirmed()) && next.dir_confirmed())
                    {
                        mRadarRectMap.remove(keys[i]);
                        break;
                    }
                } else
                {
                    DEBUG_TRACK_INFO<<"found abormal targets:"<<keys[i]<<keys[k]<<"  distance:"<<distance<<" time:"<<cur.current_rect().updatetime()<<next.current_rect().updatetime()<<" remove old one.";

                    if(next.current_rect().updatetime() <= cur.current_rect().updatetime())
                    {
                        mRadarRectMap.remove(keys[k]);
                    } else
                    {
                        mRadarRectMap.remove(keys[i]);
                        break;
                    }
                }


            }
        }
    }
}


void zchxRadarTargetTrack::appendUserDefObj(const zchxCommon::UserSpecifiedObj &obj)
{
    if(obj.mTrackNum <= 0) return;
    mUserDefObj[obj.mTrackNum] = obj;
}

void zchxRadarTargetTrack::removeUserDefObj(const zchxCommon::UserSpecifiedObj &obj)
{
    mUserDefObj.remove(obj.mTrackNum);
}

void zchxRadarTargetTrack::updateParseSetting(const zchxVideoParserSettings &setting)
{
    mSettings = setting;
//    mTargetNodeMap.clear();
}

void zchxRadarTargetTrack::updateTrackPointWithNode(zchxRadarSurfaceTrack& list, TargetNode *node)
{
    if(!node) return;
    //确认目标是否输出
    if(node->mStatus == Node_UnDef && !mSettings.user_video_parse.output_point) return;
    if(node->mStatus == Node_Moving && node->mDefRect->mSrcRect.sogms() < mSettings.user_video_parse.output_target_min_speed * 1.852 / 3.6) return ;


    int node_number = node->mSerialNum;
    zchxTrackPoint *trackObj = list.add_trackpoints();
    if(!trackObj) return;
    zchxRadarRectDef *target = node->mDefRect;

    //编号
    trackObj->set_radarsiteid(QString("%1_%2").arg(mSettings.radar_id).arg(mSettings.channel_id).toUtf8().data());
    trackObj->set_tracknumber(node_number);
    trackObj->set_objtype(1);
    //当前目标
    trackObj->mutable_current()->CopyFrom(target->mSrcRect);    
    trackObj->mutable_current()->mutable_fixedimg()->set_width(calNodeMoveDis(node));

    bool is_move_obj = (node->mStatus == Node_Moving);
    //更新当前目标对应的回波判定的相交图形。
    trackObj->mutable_current()->mutable_outline()->Clear();

    if(is_move_obj)
    {
        QPolygon src_poly;
        if(node->mParent)
        {
            src_poly = node->mParent->mDefRect->mShapePnts;
        }
        for(int i=0; i<src_poly.size(); i++)
        {
            QPoint pnt = src_poly[i];
            com::zhichenhaixin::proto::Latlon* add = trackObj->mutable_current()->mutable_outline()->Add();
            Latlon ll = node->mDefRect->mPosConveter.pixel2Latlon(QPointF(pnt.x(), pnt.y()));
            add->set_latitude(ll.lat);
            add->set_longitude(ll.lon);
        }

        //历史轨迹数据按照时间顺序进行输出，时间越早则靠后。top节点在最后面
        TargetNode* cur = node;
        while (cur) {
            zchxRadarRectDefSrc *history = trackObj->add_tracks();
            history->CopyFrom(cur->mDefRect->mSrcRect);
            cur = cur->mParent.data();
        }
        trackObj->mutable_current()->set_sogknot(target->mSrcRect.sogms() * 3.6 / 1.852);  //输出的速度为节
    } else
    {
        trackObj->mutable_current()->set_sogknot(0.0);  //输出的速度为节
        trackObj->mutable_current()->set_sogms(0.0);
        trackObj->mutable_current()->set_cog(0.0);
    }
    trackObj->set_trackconfirmed(is_move_obj? true : false);
}

