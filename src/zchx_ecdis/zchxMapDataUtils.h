﻿#ifndef ZCHXECDISUTILS_H
#define ZCHXECDISUTILS_H

#include <math.h>
#include <QString>
#include <QDateTime>
#include <QPixmap>
#include "zchxutils.hpp"

typedef         std::pair<double, double>           GPNT;
typedef         std::vector<GPNT>                   GPATH;
typedef         std::vector<QPointF>                PPATH;
namespace qt {

struct MapRangeData{
    ZCHX::Data::Mercator    mLowerLeft;         //左下
    ZCHX::Data::Mercator    mTopRight;          //右上
    double                  mUnitLenth;
};

enum    TILE_ORIGIN_POS{
    TILE_ORIGIN_TOPLEFT = 0,        //左上
    TILE_ORIGIN_BOTTEMLEFT,         //左下
};


//每次加载瓦片地图的参数设定,主要是视窗的墨卡托范围,视窗屏幕坐标大小
struct MapLoadSetting{
    MapRangeData    mMapRange;
    double          mResolution;
    int             mZoom;
    int             mSource; //0:本地1:服务器地址
    int             mTilePos;
    ZCHX::Data::LatLon          mCenter;
};

//瓦片图片信息
struct TileImage {
    QPixmap    mImg;
    int         mPosX;
    int         mPosY;
    QString     mName;

    TileImage(){}

    TileImage(const QPixmap& img, int x, int y, const QString& name = QString())
    {
        mImg = img;
        mPosX = x;
        mPosY = y;
        mName = name;
    }
};

class TileImageList:public QList<TileImage>
{
public:
    TileImageList():QList<TileImage>() {}
};

static double constexpr degreeInMetres = 360.0 / 40008245;

class ZCHX_ECDIS_EXPORT zchxMapDataUtils
{
public:
    zchxMapDataUtils();
    //墨卡托和wgs84互转
    static ZCHX::Data::LatLon mercatorToWgs84LonLat(const ZCHX::Data::Mercator& mercator);
    static ZCHX::Data::Mercator wgs84LonlatToMercator(const ZCHX::Data::LatLon& wgs84 );
    static ZCHX::Data::Mercator wgs84LatLonToMercator(double lat, double lon ) {return wgs84LonlatToMercator(ZCHX::Data::LatLon(lat, lon));}
    static double calResolution(int zoom);
    //角度弧度换算
    static double DegToRad(double deg);
    //地球空间计算
    // 地球半径 单位米.
    static inline double EarthRadiusMeters() { return 6378000; }
    // Length of one degree square at the equator in meters.
    static inline double OneDegreeEquatorLengthMeters() { return 111319.49079; }
    // Distance on unit sphere between (lat1, lon1) and (lat2, lon2).
    // lat1, lat2, lon1, lon2 - in degrees.
    static double DistanceOnSphere(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg);
    // Area on unit sphere for a triangle (ll1, ll2, ll3).
    static double AreaOnSphere(ZCHX::Data::LatLon const & ll1, ZCHX::Data::LatLon const & ll2, ZCHX::Data::LatLon const & ll3);
    static double AreaOnSphere(std::vector<ZCHX::Data::LatLon> vectorPoints);

    // Distance in meteres on Earth between (lat1, lon1) and (lat2, lon2).
    // lat1, lat2, lon1, lon2 - in degrees.
    static double DistanceOnEarth(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg)
    {
      return EarthRadiusMeters() * DistanceOnSphere(lat1Deg, lon1Deg, lat2Deg, lon2Deg);
    }

    static double DistanceOnEarth(ZCHX::Data::LatLon const & ll1, ZCHX::Data::LatLon const & ll2)
    {
      return DistanceOnEarth(ll1.lat, ll1.lon, ll2.lat, ll2.lon);
    }

    static double AreaOnEarth(ZCHX::Data::LatLon const & ll1, ZCHX::Data::LatLon const & ll2, ZCHX::Data::LatLon const & ll3)
    {
      return OneDegreeEquatorLengthMeters() * OneDegreeEquatorLengthMeters() * AreaOnSphere(ll1, ll2, ll3);
    }

    static double AreaOnEarth(std::vector<ZCHX::Data::LatLon> vectorPoints)
    {
      return OneDegreeEquatorLengthMeters() * OneDegreeEquatorLengthMeters() * AreaOnSphere(vectorPoints);
    }

    static double getTotalDistance(const std::vector<std::pair<double, double> > &pointList);
    static double getTotalArea(const std::vector<std::pair<double, double> > &pointList);
    ZCHX::Data::LatLon static getSmPoint(const ZCHX::Data::LatLon & pt, double lonMetresR, double latMetresR);
    static double getDistancePixel(const ZCHX::Data::Point2D& p1, const ZCHX::Data::Point2D & p2);


    //计算向量的角度[P1,P2]
    static inline double AngleTo(const ZCHX::Data::Point2D& p1, const ZCHX::Data::Point2D & p2)
    {
        return atan2(p2.y - p1.y, p2.x - p1.x);
    }

    //计算两个向量的夹角[p, p1] 到[p, p2]. 逆时针旋转. [0, 2pi]
    static inline double TwoVectorsAngle(const ZCHX::Data::Point2D& p0, const ZCHX::Data::Point2D& p1, const ZCHX::Data::Point2D & p2)
    {
        double a = AngleTo(p0, p2) - AngleTo(p0, p1);
        while (a < 0) a += GLOB_PI;
        return a;
    }
    //角度校正
    static double AngleIn2PI(double a);
    static bool is2VectorCross(QPoint& cross, const QLine& line1, const QLine& line2);
    static int  calHLS(int a11, int a12, int a21, int a22);

};



typedef struct tagStartEndPoint{
    ZCHX::Data::Point2D start;
    ZCHX::Data::Point2D end;
}StartEndPoint;

typedef struct tagMultiBeamImg {
    double dMinLon;//多波束数据的最小经度
    double dMinLat;//多波束数据的最小纬度
    double dMaxLon;//多波束数据的最大经度
    double dMaxLat;//多波束数据的最大纬度
    QPixmap multibeamImg;//多波束数据图片
}MultiBeamImg;

enum MapStyle
{
    MapStyleBase = 0x0001,
    MapStyleStandard = 0x0002,
    MapStyleAll = 0x0004,
    MapStyleEcdisDayBright = 0x0008,       //晴天模式
    MapStyleEcdisNight = 0x0010,              //夜晚模式
    MapStyleEcdisDayDUSK = 0x0020,           //黄昏模式
    MapStyleEcdisDayBlackBack = 0x0040, //阴天模式
    MapStyleEcdisDayWhiteBack = 0x0080,  //白昼模式
    // Specifies number of MapStyle enum values, must be last
    MapStyleCount
};

enum MapUnit{
    MapUnitMeter = 1,   //米
    MapUnitFoot,    //英尺
    MapUnitKm,  //千米
    MapUnitNmi, //海里
};
}

Q_DECLARE_METATYPE(qt::TileImage)
Q_DECLARE_METATYPE(qt::TileImageList)

//地图配置文件相关的设定
#define             MAP_INDEX                       "Map_0"
#define             MAX_LINE_LENGTH                 "MaxLineLength"
#define             WARN_FLAH_COLOR_ALPHA           "FlashColorAlpha"
#define             OPEN_MEET                       "OpenMeet"
#define             MAP_UNIT                        "MapUnit"
#define             MAP_STYLE_AUTO_CHANGE           "StyleAutoChange"
#define             MAP_DAY_TIME                    "DayTime"
#define             MAP_NIGHT_TIME                    "NightTime"
#define             MAP_DUSK_TIME                    "DuskTime"
#define             MAP_DEFAULT_LAT                      "Lat"
#define             MAP_DEFAULT_LON                      "Lon"
#define             MAP_DEFAULT_ZOOM                    "Zoom"
#define             MAP_MIN_ZOOM                        "MinZoom"
#define             MAP_MAX_ZOOM                        "MaxZoom"
#define             MAP_DEFAULT_TARGET_ZOOM             "TargetZoom"            //目标居中放大时的倍数
#define             MAP_UPDATE_INTERVAL             "UpdateInterval"            //刷新时间间隔毫秒
#define             MAP_SOURCE                      "MapSource"                 //地图数据来源
#define             MAP_DISPLAY_MENU                "DisplayRightMouseMenu"     //是狗显示右键菜单
#define             IMG_URL                         "ImgUrl"
#define             MAP_FILE_DIR                    "MapFileDir"
#define             IMG_FILE_DIR                    "ImgFileDir"
#define             GOOGLE_MAP_DIR                  "GoogleMapDir"
#define             MAP_BACK_GROUND                 "BackgroundColor"
#define             MAX_RECT_PIXEL_LENGTH           "MaxRadarRectLength"
#define             MAP_USE_RECT_COG                "RadarRectUseCog"
#define             MAP_ZOOM_WITH_CENTER            "ZommWithCenter"

//Ais显示配置
#define             AIS_DISPLAY_SETTING                         "AIS"
#define             AIS_FILL_COLOR                              "FillColor"
#define             AIS_CONCERN_COLOR                           "ConcernColor"
#define             AIS_TEXT_COLOR                              "TextColor"
#define             AIS_BORDER_COLOR                            "BorderColor"
#define             AIS_FORCED_IMAGE                            "ForcedImageDisplay"
#define             AIS_CONCERN_NUM                             "ConcernNum"
#define             AIS_REPLACE_CONCERN                         "AutoReplaceConcern"
#define             AIS_TAIL_TRACK_NUM                          "TailTrackNum"
#define             AIS_REPLACE_TAIL_TRACK                      "AutoReplaceTailTrack"
#define             AIS_HISTORY_TRACK_NUM                       "HistoryTrackNum"
#define             AIS_REPLACE_HISTORY_TRACK                   "AutoReplaceHistoryTrack"
#define             AIS_EXTRAPOLATE_NUM                         "ExtrapolateNum"
#define             AIS_REPLACE_EXTRAPOLATE                       "AutoReplaceExtrapolate"

//雷达显示配置
#define             RADAR_DISPLAY_SETTING                       "Radar"
#define             RADAR_FILL_COLOR                            "FillColor"
#define             RADAR_CONCERN_COLOR                         "ConcernColor"
#define             RADAR_TEXT_COLOR                            "TextColor"
#define             RADAR_BORDER_COLOR                          "BorderColor"
#define             RADAR_DRAW_SHAPE                                "Shape"
#define             RADAR_CONCERN_NUM                             "ConcernNum"
#define             RADAR_REPLACE_CONCERN                         "AutoReplaceConcern"
#define             RADAR_TAIL_TRACK_NUM                          "TailTrackNum"
#define             RADAR_REPLACE_TAIL_TRACK                      "AutoReplaceTailTrack"
#define             RADAR_HISTORY_TRACK_NUM                       "HistoryTrackNum"
#define             RADAR_REPLACE_HISTORY_TRACK                   "AutoReplaceHistoryTrack"

#endif // ZCHXECDISUTILS_H
