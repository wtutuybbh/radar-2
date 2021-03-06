﻿#include "zchxradarvideoparser.h"
#include "BR24.hpp"
#include <QDebug>
#include <QtCore/qmath.h>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
//#include <zlib.h>
#include <QDir>
#include <QBuffer>
#include "zchxradarvideoprocessor.h"
#include "zchxradartargettrack.h"

static uint8_t BR24MARK[] = {0x00, 0x44, 0x0d, 0x0e};
int g_lastRange = 0;

#define         PROCESS_CYCLE_DATA
#define         DOPPLER_USED

#ifndef NAVICO_SPOKES
#define NAVICO_SPOKES 2048
#endif

#ifndef NAVICO_SPOKE_LEN
#define NAVICO_SPOKE_LEN 1024
#endif

#if SPOKES_MAX < NAVICO_SPOKES
#undef SPOKES_MAX
#define SPOKES_MAX NAVICO_SPOKES
#endif
#if SPOKE_LEN_MAX < NAVICO_SPOKE_LEN
#undef SPOKE_LEN_MAX
#define SPOKE_LEN_MAX NAVICO_SPOKE_LEN
#endif


enum LookupSpokeEnum {
  LOOKUP_SPOKE_LOW_NORMAL,
  LOOKUP_SPOKE_LOW_BOTH,
  LOOKUP_SPOKE_LOW_APPROACHING,
  LOOKUP_SPOKE_HIGH_NORMAL,
  LOOKUP_SPOKE_HIGH_BOTH,
  LOOKUP_SPOKE_HIGH_APPROACHING
};

static uint8_t lookupData[6][256];

void zchxRadarVideoParser::InitializeLookupData()
{
  if (lookupData[5][255] == 0) {
    for (int j = 0; j <= UINT8_MAX; j++) {
      uint8_t low = (j & 0x0f) << 4;
      uint8_t high = (j & 0xf0);

      lookupData[LOOKUP_SPOKE_LOW_NORMAL][j] = low;
      lookupData[LOOKUP_SPOKE_HIGH_NORMAL][j] = high;

      switch (low) {
        case 0xf0:
          lookupData[LOOKUP_SPOKE_LOW_BOTH][j] = 0xff;
          lookupData[LOOKUP_SPOKE_LOW_APPROACHING][j] = 0xff;
          break;

        case 0xe0:
          lookupData[LOOKUP_SPOKE_LOW_BOTH][j] = 0xfe;
          lookupData[LOOKUP_SPOKE_LOW_APPROACHING][j] = low;
          break;

        default:
          lookupData[LOOKUP_SPOKE_LOW_BOTH][j] = low;
          lookupData[LOOKUP_SPOKE_LOW_APPROACHING][j] = low;
      }

      switch (high) {
        case 0xf0:
          lookupData[LOOKUP_SPOKE_HIGH_BOTH][j] = 0xff;
          lookupData[LOOKUP_SPOKE_HIGH_APPROACHING][j] = 0xff;
          break;

        case 0xe0:
          lookupData[LOOKUP_SPOKE_HIGH_BOTH][j] = 0xfe;
          lookupData[LOOKUP_SPOKE_HIGH_APPROACHING][j] = high;
          break;

        default:
          lookupData[LOOKUP_SPOKE_HIGH_BOTH][j] = high;
          lookupData[LOOKUP_SPOKE_HIGH_APPROACHING][j] = high;
      }
    }

//    {
//        for(int i=0; i<6; i++)
//        {
//            QList<uint8_t> data;
//            for (int j = 0; j <= UINT8_MAX; j++) {
//                data.append(lookupData[i][j]);
//            }
//            qDebug()<<"lookupData["<<i<<"]:"<<data;
//        }
//    }
  }
}

zchxRadarVideoParser::zchxRadarVideoParser(zchxRadarOutputDataMgr* mgr, const zchxVideoParserSettings& parse, QObject *parent)
    : QObject(parent),
      mStartAzimuth(-1),
      mRadarOutMgr(mgr),
      mRadarType(zchxCommon::RADAR_UNKNOWN),
      mDopplerVal(0),
      mParseParam(parse),
      mWorkThread(0),
      mTermSpokeCount(0),
      mNextSpoke(-1),
      mTermIndex(0)
{
    qRegisterMetaType<QMap< int,QList<QPointF> >>("QMap< int,QList<QPointF> >");
    qRegisterMetaType<zchxRadarRectList>("const zchxRadarRectList&");
    qRegisterMetaType<zchxRadarRectDefList>("const zchxRadarRectDefList&");
    qRegisterMetaType<zchxRadarRectMap>("const zchxRadarRectMap&");
    qRegisterMetaType<QMap<int,QList<TrackNode>>>("QMap<int,QList<TrackNode>>");
    qRegisterMetaType<zchxRadarRouteNodes>("const zchxRadarRouteNodes&");

    InitializeLookupData();
    //回波块解析
    bool process_sync = true;
    mVideoProcessor =  new ZCHXRadarVideoProcessor(parse);
    mTrackProcessor = new zchxRadarTargetTrack(parse, this);

    connect(mVideoProcessor,SIGNAL(signalSendVideoPixmap(QImage)), this,SLOT(slotSendVideoImage(QImage)));
    //处理矩形回波块    
    connect(mTrackProcessor, SIGNAL(signalSendTracks(zchxRadarSurfaceTrack)),
            this, SLOT(slotSendTracks(zchxRadarSurfaceTrack)));
    connect(mTrackProcessor, SIGNAL(signalSendRectData(zchxRadarRectMap)),
            this, SLOT(slotSendRectList(zchxRadarRectMap)));
    connect(mTrackProcessor, SIGNAL(signalSendRoutePath(zchxRadarRouteNodes)),
            this, SLOT(slotSendRadarNodeRoute(zchxRadarRouteNodes)));
    mVideoProcessor->start();
#ifndef TRACK_THREAD
    mVideoProcessor->setTracker(mTrackProcessor);
#else
    connect(mVideoProcessor,SIGNAL(signalSendRects(zchxRadarRectDefList)),
            mTrackProcessor,SLOT(appendTask(zchxRadarRectDefList)));
    mTrackProcessor->start();
#endif

    if(!parent)
    {
        mWorkThread = new QThread;
        mWorkThread->setStackSize(64000000);
        moveToThread(mWorkThread);
        mWorkThread->start();
    }
}


zchxRadarVideoParser::~zchxRadarVideoParser()
{
    if(mVideoProcessor)
    {
        qDebug()<<"now wait for processor terminate...";
        mVideoProcessor->setOver(true);
        mVideoProcessor->terminate();
        mVideoProcessor->wait();
        qDebug()<<"start delete ...";
        delete mVideoProcessor;
        mVideoProcessor = NULL;
    }
    if(mTrackProcessor)
    {

        qDebug()<<"now wait for TrackProcessor delete...";
        mTrackProcessor->deleteLater();
        mTrackProcessor = 0;
//        delete mTrackProcessor;
//        mTrackProcessor = 0;
        qDebug()<<"now end TrackProcessor delete...";
    }
    qDebug()<<"stop parse thread...";
    if(mWorkThread)
    {
        if(mWorkThread->isRunning())
        {
            qDebug()<<"quit parse thread...";
            mWorkThread->quit();
        }
        qDebug()<<"terminate parse thread...";
        mWorkThread->terminate();
        qDebug()<<"delete parse thread...";
        mWorkThread->deleteLater();
    }
    qDebug()<<"video parse end now....";
}
void zchxRadarVideoParser::slotRecvVideoData(const QByteArray &sRadarData)
{
    if(mRadarType == zchxCommon::RADAR_UNKNOWN) return;
    const char *buf = sRadarData.constData();
    int len = sRadarData.size();
//    qDebug()<<"analysisLowranceRadarSlot len:"<<len;
    BR24::Constants::radar_frame_pkt *packet = (BR24::Constants::radar_frame_pkt *)buf;//正常大小是17160

    //cout<<sizeof(BR24::Constants::radar_frame_pkt);//结构体大小是固定的64328 定的120个spoke(536*120)+frame_hdr(8)
    //qDebug()<<" packet len:"<<(int)sizeof(packet->frame_hdr);
    if (len < (int)sizeof(packet->frame_hdr)) {
        qDebug()<<"The packet is so small it contains no scan_lines, quit!";
        return;
    }
    //cout<<len<<sizeof(packet->frame_hdr)<<sizeof(BR24::Constants::radar_line);//第 178 行 (17160 - 8) / 536 = 32
    int scanlines_in_packet = (len - sizeof(packet->frame_hdr)) / sizeof(BR24::Constants::radar_line);
    if (scanlines_in_packet != 32) {
        qDebug()<<QString::fromUtf8("此包没有32条扫描线，丢包！");
        return;
    }

    QList<int> lineData;
    double range_factor;
    for (int scanline = 0; scanline < scanlines_in_packet; scanline++) {
        BR24::Constants::radar_line *line = &packet->line[scanline];

        // Validate the spoke
        int spoke = (line->common.scan_number[0] | (line->common.scan_number[1] << 8)) % SPOKES;
        //cout<<spoke;

        if (line->common.headerLen != 0x18) {
            qDebug()<<"strange header length(normal:24):" << line->common.headerLen;
            mNextSpoke = (spoke + 1) % SPOKES;
            continue;
        }

        //probably status: 02 valid data; 18 spin up
        if (line->common.status != 0x02 && line->common.status != 0x12) {
            qDebug()<<"strange status:" << line->common.status;
        }
        if (mNextSpoke >= 0 && spoke != mNextSpoke) {
            qDebug()<<"error spoke:"<<spoke<<mNextSpoke<<" with radar : channel "<<mParseParam.radar_id<<mParseParam.channel_id;
        }
        mNextSpoke = (spoke + 1) % SPOKES;
        //qDebug()<<"current spoke:"<<spoke<<" next:"<<m_next_spoke;
        int range_raw = 0;
        int angle_raw = 0;
        short int heading_raw = 0;
        double range_meters = 0;

        heading_raw = (line->common.heading[1] << 8) | line->common.heading[0];
        if(mParseParam.use_video_radius)
        {
            switch (mRadarType) {
            case zchxCommon::RADAR_BR24:
            {
                range_raw = ((line->br24.range[2] & 0xff) << 16 | (line->br24.range[1] & 0xff) << 8 | (line->br24.range[0] & 0xff));
                angle_raw = (line->br24.angle[1] << 8) | line->br24.angle[0];
                range_meters = (int)((double)range_raw * 10.0 / sqrt(2.0));
                break;
            }

            case zchxCommon::RADAR_3G:
            case zchxCommon::RADAR_4G: {
                short int large_range = (line->br4g.largerange[1] << 8) | line->br4g.largerange[0];
                short int small_range = (line->br4g.smallrange[1] << 8) | line->br4g.smallrange[0];
                angle_raw = (line->br4g.angle[1] << 8) | line->br4g.angle[0];
                if (large_range == 0x80) {
                    if (small_range == -1) {
                        range_meters = 0;  // Invalid range received
                    } else {
                        range_meters = small_range / 4;
                    }
                } else {
                    range_meters = large_range * 64;
                }
                break;
            }

            case zchxCommon::RADAR_6G:
            {
                uint16_t large_range = (line->br4g.largerange[1] << 8) | line->br4g.largerange[0];
                uint16_t small_range = (line->br4g.smallrange[1] << 8) | line->br4g.smallrange[0];
                angle_raw = (line->br4g.angle[1] << 8) | line->br4g.angle[0];
                if (large_range == 0x80) {
                    if (small_range == 0xffff) {
                        range_meters = 0;  // Invalid range received
                    } else {
                        range_meters = small_range / 4;
                    }
                } else {
                    range_meters = large_range * small_range / 512;
                }
                break;
            }

            default:
                break;
            }
        } else
        {
            if (memcmp(line->br24.mark, BR24MARK, sizeof(BR24MARK)) == 0) {
                // BR24 and 3G mode
                range_raw = ((line->br24.range[3] & 0xff) << 24 | (line->br24.range[2] & 0xff) << 16 | (line->br24.range[1] & 0xff) << 8 | (line->br24.range[0] & 0xff));
                angle_raw = (line->br24.angle[1] << 8) | line->br24.angle[0];
                range_meters = (int)((double)range_raw * 10.0 / sqrt(2.0));
                //range_meters = 7249.92;//1_写死距离因子,和半径

                qDebug()<<"br24";

                // test code:
                if (g_lastRange != range_meters) {
                    qDebug()<<QString("BR24 and 3G mode: range_raw = %1, range_meters = %2").arg(range_raw).arg(range_meters);

                    g_lastRange = range_meters;
                }
            } else
            {
                // 4G mode
                short int large_range = (line->br4g.largerange[1] << 8) | line->br4g.largerange[0];
                short int small_range = (line->br4g.smallrange[1] << 8) | line->br4g.smallrange[0];
                //cout<<"large_range=" << (line->br4g.largerange[1] << 8) <<(line->br4g.largerange[0]) ;
                //cout<<"small_range=" << (line->br4g.smallrange[1] << 8) <<(line->br4g.smallrange[0]);
                angle_raw = (line->br4g.angle[1] << 8) | line->br4g.angle[0];
                if (large_range == 0x80 && (line->br4g.smallrange[1] << 8)<32768) {
                    //if (large_range == 0x80  ) {
                    if (small_range == -1) {
                        range_raw = 0;  // Invalid range received
                    } else {
                        //cout<<"small_range"<<small_range<<"large_range"<<large_range;
                        range_raw = small_range;
                        range_meters = range_raw / 4;
                        //if(range_meters < 2600) range_meters =2600;
                    }
                }
                else
                {
                    range_meters = mParseParam.manual_radius;
                }
            }
        }

        int azimuth_cell = mParseParam.line_num;
        bool radar_heading_valid = (((heading_raw) & ~(HEADING_TRUE_FLAG | (azimuth_cell - 1))) == 0);
        double heading;
        if (radar_heading_valid) {
            double heading_rotation = (((heading_raw) + 2 * azimuth_cell) % azimuth_cell);
            double heading_degrees = ((heading_rotation) * (double)DEGREES_PER_ROTATION / azimuth_cell);
            heading = (fmod(heading_degrees + 2 * DEGREES_PER_ROTATION, DEGREES_PER_ROTATION));
        } else {
            //ZCHXLOG_DEBUG("radar_heading_valid=" << radar_heading_valid );
        }
        //角度值强制变成偶数
        int origin_angle_raw = angle_raw;
        if(0){
            //雷达扫描线检测
            static QMap<int, int> counterMap;
            if(counterMap.contains(spoke) && counterMap[spoke] != origin_angle_raw)
            {
                qDebug()<<"error spoke angle find now:"<<spoke<<origin_angle_raw<<counterMap[spoke];
            } else
            {
                counterMap[spoke] = origin_angle_raw;
            }

        }

//        qDebug()<<"spoke:"<<spoke<<" angle:"<<angle_raw;
        if(angle_raw % 2)
        {
            angle_raw += 1;
        }
        if(angle_raw == 4096) angle_raw = 0;
//        angle_raw = MOD_ROTATION2048(angle_raw / 2);  //让方向和一圈的扫描线个数保持一致(2048)
//        qDebug()<<"spoke:"<<spoke<<" angle:"<<angle_raw;
        double start_range = 0.0 ;        

        //qDebug()<<"range_meter:"<<range_meters<<" cellNum:"<<uCellNum<<" range_factor"<<range_factor;

        lineData.clear();
        //double dAzimuth = angle_raw * (360.0 / (uLineNum / 2)) + uHeading;
        //cout<<"dAzimuth:"<<dAzimuth<<"angle_raw"<<angle_raw<<"uHeading"<<uHeading; //1_扫描方位,angle_raw(0-2047),uHeading(180)

        //赋值振幅
#ifndef DOPPLER_USED
        for (int range = 0; range < uCellNum; range++) {
            lineData.append((int)(line->data[range]));
        }
#else
        int doppler = mDopplerVal;
        if (doppler < 0 || doppler > 2) {
            doppler = 0;
        }
        uint8_t *lookup_low = lookupData[LOOKUP_SPOKE_LOW_NORMAL + doppler];
        uint8_t *lookup_high = lookupData[LOOKUP_SPOKE_HIGH_NORMAL + doppler];
        for (int i = 0; i < mParseParam.cell_num; i++) {
            int temp = line->data[i];
            lineData.append(lookup_low[temp]);
            lineData.append(lookup_high[temp]);
        }
#endif
        //cout<<"spoke:"<<spoke<<" angle:"<<angle_raw<<heading_raw;
        range_factor = range_meters/lineData.size();
        //检查是否是经过了一次扫描周期,如果是,发出数据开始解析
        if(mStartAzimuth >= 0 && mStartAzimuth == angle_raw)
        {
            qDebug()<<"cycle lowrance data end:"<<QDateTime::currentDateTime().toString("hh:mm:ss zzz")<<" elaped:"<<mCounterT.elapsed()<<" spoke count:"<<mTermSpokeCount<<QThread::currentThread();
            processVideoData(true);
            mRadarVideoMap1T.clear();
            mStartAzimuth = -1;
            mTermSpokeCount = 0;
        }

        //cout<<"angle_raw"<<angle_raw;
        RADAR_VIDEO_DATA objVideoData;
//        objVideoData.m_uSourceID = m_uSourceID; //1 接入雷达编号
        objVideoData.m_uSystemAreaCode = 1; //系统区域代码
        objVideoData.m_uSystemIdentificationCode = 1; //系统识别代码
        objVideoData.m_uMsgIndex = spoke; //消息唯一序列号
        objVideoData.m_uAzimuth = angle_raw; //扫描方位
        objVideoData.m_dStartRange = start_range;//扫描起始距离
        objVideoData.m_dRangeFactor = range_factor;//1_距离因子
        objVideoData.m_uTotalCellNum = lineData.size();//1_一条线上有多少个点
        objVideoData.m_dCentreLon = mParseParam.center_lon; //中心经度
        objVideoData.m_dCentreLat = mParseParam.center_lon; //中心纬度
        objVideoData.m_uLineNum = mParseParam.line_num; //1_总共线的个数
        objVideoData.m_uHeading = mParseParam.head;//雷达方位
        objVideoData.mLineData = lineData;
        objVideoData.m_time = QDateTime::currentMSecsSinceEpoch();
        //半径
        mScanRadius = range_meters;
        mRadarVideoMap1T[objVideoData.m_uMsgIndex % 2048] = objVideoData;
        mTermSpokeCount++;
        if(mStartAzimuth == -1)
        {
            qDebug()<<"new cycle lowrance data now:"<<QDateTime::currentDateTime().toString("hh:mm:ss zzz");
            mCounterT.start();
            mStartAzimuth = angle_raw;
        }
    }

    if(mRadarOutMgr) mRadarOutMgr->updateRadarRadiusAndFactor(mParseParam.channel_id, mScanRadius, range_factor, QString::number(mParseParam.radar_id));

}

//发送雷达回波余辉图片
void zchxRadarVideoParser::slotSendVideoImage(const QImage &videoPixmap)
{
    if(!mRadarOutMgr) return;
    //回波图片转二进制
    QByteArray videoArray;
    QBuffer videoBuffer(&videoArray);
    videoBuffer.open(QIODevice::WriteOnly);
    videoPixmap.save(&videoBuffer ,"PNG");

    zchxRadarVideoSingleImage img;
    img.set_channelid(mParseParam.channel_id);
    img.set_radarid(QString::number(mParseParam.radar_id).toStdString());
    img.set_radarname(mParseParam.name.toStdString());
    img.mutable_center()->set_latitude(mParseParam.center_lat);
    img.mutable_center()->set_longitude(mParseParam.center_lon);
    img.set_utc(QDateTime::currentMSecsSinceEpoch());
    img.set_height(videoPixmap.height());
    img.set_width(videoPixmap.width());
    img.set_radius(mScanRadius);
    img.set_imagedata(videoArray.data(),videoArray.size());

    mRadarOutMgr->updateVideoImage(mParseParam.channel_id, img);
}


//zmq发送雷达回波矩形图片
void zchxRadarVideoParser::slotSendRectList(const zchxRadarRectMap& map)
{
//    if((!mRadarOutMgr) || map.size() == 0) return;
//    zchxRadarRects *totalRadar_Rects = new zchxRadarRects;
//    foreach (zchxRadarRect rect, map) {
//        zchxRadarRect *mRadarRect = totalRadar_Rects->add_rect_list();
//        mRadarRect->CopyFrom(rect);
//        //没有确定的点不绘制历史
//        if(!mRadarRect->dir_confirmed())
//        {
//            mRadarRect->clear_history_rect_list();
//        }
//    }
//    qint64 utc = QDateTime::currentMSecsSinceEpoch();
//    totalRadar_Rects->set_length(map.size());
//    totalRadar_Rects->set_utc(utc);
//    //通过zmq发送
//    mRadarOutMgr->appendData(zchxRadarUtils::protoBufMsg2ByteArray(totalRadar_Rects), mRadarRectTopic, mRadarRectPort);
//    delete totalRadar_Rects;
}

//zmq发送雷达回波矩形图片
void zchxRadarVideoParser::slotSendRadarNodeRoute(const zchxRadarRouteNodes& list)
{
//    if((!mRadarOutMgr) || list.node_list_size() == 0) return;

//    zchxRadarRouteNodes* nodes = new zchxRadarRouteNodes(list);
//    qDebug()<<"send data node size:"<<list.node_list_size();
//    //通过zmq发送
//    mRadarOutMgr->appendData(zchxRadarUtils::protoBufMsg2ByteArray(nodes), "RadarRoute", mRadarTrackPort);
//    delete nodes;
}


void zchxRadarVideoParser::processVideoData(bool rotate)
{
    if(mVideoProcessor == 0) return;
    if (mRadarVideoMap1T.isEmpty())  return;


    zchxRadarVideoTask task;
    task.m_RadarVideo = mRadarVideoMap1T;
    task.m_Range = mScanRadius;
    task.m_Rotate = rotate;
    mTermIndex = (++mTermIndex) % MAX_RADAR_VIDEO_INDEX_T;
    task.m_IndexT = mTermIndex;

    mVideoProcessor->appendSrcData(task);
}

//发送合并的雷达目标
void zchxRadarVideoParser::slotSendTracks(const zchxRadarSurfaceTrack& tracks)
{
   if(mRadarOutMgr)
   {
       mRadarOutMgr->updateTracks(mParseParam.channel_id, tracks);
   }
}


void zchxRadarVideoParser::slotSetGpsData(double lat, double lon)
{
    mParseParam.center_lat = lat;
    mParseParam.center_lon = lon;
}


void zchxRadarVideoParser::slotSetRadarType(int type)
{
    mRadarType = type;
}

bool zchxRadarVideoParser::isSameParseSetting(const zchxVideoParserSettings &setting)
{
    return mParseParam == setting;
}

void zchxRadarVideoParser::updateParseSetting(const zchxVideoParserSettings &setting)
{
    mParseParam = setting;
    if(mVideoProcessor) mVideoProcessor->updateParseSetting(setting);
    if(mTrackProcessor) mTrackProcessor->updateParseSetting(setting);
}

