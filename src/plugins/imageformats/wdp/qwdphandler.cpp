/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include "qwdphandler_p.h"
#include "qimage.h"
#include "qvariant.h"
#include "qvector.h"
#include "qbuffer.h"
#include "JXRGlue.h"

extern bool jxrReadHeader(QIODevice* pDevice, int* width, int* height, float* resolutionX, float* resolutionY, QImage::Format* Imgformat);
extern QImage jxrImageRead(QIODevice* pDevice, ERR* err, long flags);

QT_BEGIN_NAMESPACE

typedef long ERR;

class QWdpHandlerPrivate
{
public:
    enum State {
        Ready,
        ReadHeader,
        Error
    };

    QWdpHandlerPrivate(QWdpHandler *qq)
        : size(QSize(0,0))
        , format(QImage::Format_Invalid)
        , state(Ready)
        , err(WMP_errSuccess)
        , q(qq)
    {}

    ~QWdpHandlerPrivate()
    {}

    bool read(QImage* image);
    bool readWdpHeader(QIODevice* device);

    QSize size;
	QSizeF dpm;
    QImage::Format format;

    State state;
    ERR err;
    QWdpHandler *q;
};


QWdpHandler::QWdpHandler()
    : d(new QWdpHandlerPrivate(this)) 
{}

QWdpHandler::~QWdpHandler()
{
    delete d;
}

bool QWdpHandler::canRead() const
{
    if (d->state == QWdpHandlerPrivate::Ready && !canRead(device()))
        return false;

    if (d->state != QWdpHandlerPrivate::Error) {
        setFormat("wdp");
        return true;
    }
    return false;
}

bool QWdpHandler::canRead(QIODevice *device)
{
    if (!device) {
        qWarning("QWdpHandler::canRead() called with no device");
        return false;
    }

	char buffer[3] = {};
    if (device->peek(buffer, 3) != 3)
        return false;

    const unsigned char jxr_signature[3] = {0x49, 0x49, 0xBC};

    return (memcmp(jxr_signature, buffer, 3) == 0);
}

bool QWdpHandler::read(QImage *image)
{
    if (!canRead())
        return false;
    return d->read(image);
}

bool QWdpHandler::write(const QImage&)
{
    return false;
}

bool QWdpHandler::supportsOption(ImageOption option) const
{
    return option == Size
            || option == ImageFormat
            || option == DotsPerMeter;
}

QVariant QWdpHandler::option(ImageOption option) const
{
	if (option == Size) {
		if (d->readWdpHeader(device()))
			return d->size;
	} else if (option == ImageFormat) {
		if (d->readWdpHeader(device()))
			return d->format;
	} else if (option == DotsPerMeter) {
		if (d->readWdpHeader(device()))
			return d->dpm;
	}
    return QVariant();
}

void QWdpHandler::setOption(ImageOption , const QVariant&)
{

}

QByteArray QWdpHandler::name() const
{
    return "wdp";
}

Q_GUI_EXPORT extern int qt_defaultDpiX();
Q_GUI_EXPORT extern int qt_defaultDpiY();

bool QWdpHandlerPrivate::readWdpHeader(QIODevice* device)
{
    if (state == Ready) {
        state = Error;
        int width = 0;
        int height = 0;

		float dpiX = 0.0f;
		float dpiY = 0.0f;

        if (!jxrReadHeader(device, &width, &height, &dpiX, &dpiY, &format))
            return false;

        size = QSize(width, height);
		
		if (dpiX <= 0.0f)
			dpiX = qt_defaultDpiX();
		if (dpiY <= 0.0f)
			dpiY = qt_defaultDpiY();

		dpm = QSizeF(dpiX * 100.0 / 2.54, dpiY * 100.0 / 2.54);

        state = ReadHeader;

        return true;
    }
    else if (state == Error)
        return false;

    return true;
}

bool QWdpHandlerPrivate::read(QImage* image)
{
    if (state == Ready)
        readWdpHeader(q->device());

    if (state == ReadHeader) {
        q->device()->reset();
        *image = jxrImageRead(q->device(), &err, 0);
		if (err == WMP_errSuccess) {
			image->setDotsPerMeterX(qRound(dpm.width()));
			image->setDotsPerMeterY(qRound(dpm.height()));
		}
        state = (err == WMP_errSuccess) ? Ready : Error;
        return (err == WMP_errSuccess);
    }

    return false;
}

QT_END_NAMESPACE