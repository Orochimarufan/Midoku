#pragma once

#include <QQuickFramebufferObject>

#include <mpv/render_gl.h>

#include "mpv.h"


namespace detail {
class MpvRenderer;
}


/*class MpvFramebuffer : public QQuickFramebufferObject
{
    Q_OBJECT

    Mpv *mpv;
    mpv_render_context *context;

    friend class detail::MpvRenderer;

public:
    MpvFramebuffer(QQuickItem *parent=nullptr);
    virtual ~MpvFramebuffer();
    virtual Renderer *createRenderer() const;

    static void on_update(void *ctx);
};*/
