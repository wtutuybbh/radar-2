#ifndef CAMERAELE_H
#define CAMERAELE_H

#include "fixelement.h"

namespace qt {
class CameraElement : public FixElement<ZCHX::Data::ITF_CameraDev>
{
public:
    explicit CameraElement(const ZCHX::Data::ITF_CameraDev & data, zchxMapWidget* frame);
    ZCHX::Data::CAMERATYPE getType() const;

    uint getStatus() const;
    void setStatus(uint status);

    bool isNormal() const;
    bool isBug() const;
    bool isError() const;

    QPixmap getImage() const;
    double getMaxTrackRange() const;

    void updateGeometry(QPointF, qreal){}
    void drawElement(QPainter *painter);
    void setParent(Element* ele);
    Element* getParent();
    void clicked(bool isDouble);
private:
    Element     *mParent;               //相机悬挂的目标
};

}

#endif // CAMERAELE_H
