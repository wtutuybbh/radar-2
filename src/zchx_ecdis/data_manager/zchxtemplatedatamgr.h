﻿#ifndef ZCHXTEMPLATEDATAMGR_H
#define ZCHXTEMPLATEDATAMGR_H

#include "zchxecdisdatamgr.h"
namespace qt {
template<typename K, typename T>
class zchxTemplateDataMgr : public zchxEcdisDataMgr
{
public:
    zchxTemplateDataMgr(zchxMapWidget* w, int type, const QString& layer, QObject *parent = 0):
        zchxEcdisDataMgr(w, type, parent), mLayerName(layer), mIsDrawNow(false) {}
    //
    virtual void show(QPainter* painter)
    {
        QMutexLocker locker(&m_mutex);

        if( !painter || !mDisplayWidget->getLayerMgr()->isLayerVisible(mLayerName) || mData.empty())
        {
            return;
        }

        for(std::shared_ptr<K> ele : mData)
        {
            if(ele.get() == mDisplayWidget->getCurrentSelectedElement()){
                ele->setIsActive(true);
            } else {
                ele->setIsActive(false);
            }
            ele->drawElement(painter);
        }
    }

    Element* selectItem(const QPoint &pt)
    {
        QMutexLocker locker(&m_mutex);

        if(!mDisplayWidget->getLayerMgr()->isLayerVisible(mLayerName)) return 0;
        for(std::shared_ptr<K> ele : mData)
        {
            //检查AIS图元本身是否选中
            if(ele->contains(pt))
            {
                return ele.get();
            }
        }
        return 0;
    }

    //
    virtual bool updateActiveItem(const QPoint &pt)
    {
        if(!mDisplayWidget->getLayerMgr()->isLayerVisible(mLayerName) ||
           !isPickupAvailable()) return false;
        Element* ele = selectItem(pt);
        if(ele)
        {
            mDisplayWidget->setCurrentSelectedItem(ele);
            return true;
        }
        return false;
    }

    //数据管理
    QList<T> getData()
    {
        QMutexLocker locker(&m_mutex);

        QList<T> list;
        for(std::shared_ptr<K> ele : mData){
            list.push_back(ele->data());
        }
        return list;
    }

    void setData(const T& data)
    {
        setData(QList<T>() <<data);
    }

    void setData(const QList<T> &list)
    {
        QMutexLocker locker(&m_mutex);

        //不能直接clear,主要是防止当前图元正处于选择状态
        for(std::shared_ptr<K> ele : mData){
            ele->setIsUpdate(false);
        }
        updateData(list);
        //删除已经过时的图元
        for(int i=0; i<mData.size();)
        {
            std::shared_ptr<K> ele = mData[i];
            if(!ele) continue;
            if(!ele->getIsUpdate())
            {
                if(ele.get() == mDisplayWidget->getCurrentSelectedElement())
                {
                    mDisplayWidget->setCurrentSelectedItem(0);
                }
                mData.removeOne(ele);
                continue;
            }
            i++;
        }
    }

    void updateData(const QList<T> &list)
    {
        for(const T &zone : list)
        {
            updateData(zone);
        }
    }

    void updateData(const T &zone)
    {
//        qDebug()<<"current name:"<<zone.getName();
        K *zoneItem = item(zone.getName().toStdString());
        //更新已有的
        if(zoneItem)
        {
            zoneItem->setData(zone);
        }
        else
        {
            mData.push_back(std::shared_ptr<K>(new K(zone, mDisplayWidget/*->framework()*/)));
        }
    }

    void removeData(const T &zone)
    {
        QMutexLocker locker(&m_mutex);

        for(std::shared_ptr<K> ele : mData)
        {
            if(zone.name == QString::fromStdString(ele->name())/* || zone.id == ele->id()*/)
            {
                mData.removeOne(ele);
                break;
            }
        }
    }

    virtual void importData(const std::vector<std::pair<double, double> > & path) {}

    K* item(const std::string &name)
    {
        for(std::shared_ptr<K> ele : mData)
        {
            if(!ele) continue;
//            qDebug()<<"ele:"<<QString::fromStdString(ele->name());
            if(ele->name().compare(name) == 0)
            {
                return ele.get();
            }
        }
        return NULL;
    }

    K* item(int id)
    {
        for(std::shared_ptr<K> ele : mData)
        {
            if(ele->data().id == id)
            {
                return ele.get();
            }
        }
        return NULL;
    }

    bool data4id(int uuid, T &info)
    {
        QMutexLocker locker(&m_mutex);

        for(std::shared_ptr<K> ele : mData)
        {
            if(ele->data().id == uuid)
            {
                info = ele->data();
                return true;
            }
        }
        return false;
    }

    bool dataByName(const QString &name, T &info)
    {
        return dataByName(name.toStdString(), info);
    }

    bool dataByName(const std::string &name, T &info)
    {
        QMutexLocker locker(&m_mutex);

        for(std::shared_ptr<K> ele : mData)
        {
            if(ele->name().compare(name) == 0)
            {
                info = ele->data();
                return true;
            }
        }
        return false;
    }

    QString layerName() const {return mLayerName;}

    void setIsDraw(bool sts) {mIsDrawNow = sts;}
    bool isDraw() const {return mIsDrawNow;}

protected:
    QList<std::shared_ptr<K>> mData;        //告警区域数据也叫防区
    QMutex                    m_mutex;

    QString                   mLayerName;
    bool                      mIsDrawNow;   //当前是否正在进行鼠标操作
};
}

#endif // ZCHXTEMPLATEDATAMGR_H
