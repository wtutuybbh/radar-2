#include "zchxcameraviewdatamgr.h"

namespace qt {
zchxCameraViewDataMgr::zchxCameraViewDataMgr(zchxMapWidget* w, QObject *parent) : zchxEcdisDataMgr(w, ZCHX::DATA_MGR_CAMERA_VIEW, parent)
{

}

void zchxCameraViewDataMgr::show(QPainter* painter)
{
    if(!painter || !mDisplayWidget->getLayerMgr()->isLayerVisible(ZCHX::LAYER_CAMERA_VIEW)) return ;
    QMap<QString, std::shared_ptr<CameraViewElement>>::iterator it = mData.begin();
    for(; it != mData.end(); ++it)
    {
        std::shared_ptr<CameraViewElement> item = it.value();
        if(item) item->drawElement(painter);
    }
}

void zchxCameraViewDataMgr::setData(const QList<ZCHX::Data::ITF_CameraView> &list)
{
    foreach (ZCHX::Data::ITF_CameraView data, list) {
         std::shared_ptr<CameraViewElement> ele = mData[data.getName()];
         if(ele) {
             ele->setData(data);
         } else {
             mData[data.getName()] = std::shared_ptr<CameraViewElement>(new CameraViewElement(data, mDisplayWidget));
         }
     }
}

bool zchxCameraViewDataMgr::updateActiveItem(const QPoint &pt)
{
    return false;
}
}
