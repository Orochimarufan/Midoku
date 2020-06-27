#include "mpvframebuffer.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include <QQuickWindow>

/*namespace detail {
namespace {
static void *get_proc_address_mpv(void *ctx, const char *name)
{
    Q_UNUSED(ctx)

    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx)
        return nullptr;

    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}
}

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
    Mpv *mpv;

public:
    MpvRenderer(Mpv *mpv)
        : mpv{mpv}
    {
    }

    virtual ~MpvRenderer() override
    {}

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        // init GL
        if (!mpv->context)
        {
            mpv_opengl_init_params gl_init_params{get_proc_address_mpv, nullptr, nullptr};
            mpv_render_param params[]{
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
                {MPV_RENDER_PARAM_INVALID, nullptr}
            };

            if (mpv_render_context_create(&mpv->context, mpv->inst, params) < 0)
                throw std::runtime_error("failed to initialize MPV OpenGL context");
            mpv_render_context_set_update_callback(mpv->context, Mpv::on_update, mpv);
        }

        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render()
    {
        mpv->window()->resetOpenGLState();

        QOpenGLFramebufferObject *fbo = framebufferObject();

        mpv_opengl_fbo mpv_fbo {
            .fbo = static_cast<int>(fbo->handle()),
            .w = fbo->width(),
            .h = fbo->height(),
            .internal_format = 0
        };
        int flip_y = 0;

        // TODO
    }
};
}

Mpv::Mpv(QQuickItem *parent)
    : QQuickFramebufferObject (parent),
      inst{mpv_create()},
      context{nullptr}
{
    if (!inst)
        throw std::runtime_error("Could not create mpv context");

    mpv_set_option_string(inst, "terminal", "yes");
    mpv_set_option_string(inst, "msg-level", "all=v");

    if (mpv_initialize(inst) < 0)
        throw std::runtime_error("Could not initialize mpv context");

    mpv_set_option_string(inst, "hwdec", "auto");

    connect(this, &Mpv::onUpdate, this, &Mpv::doUpdate, Qt::QueuedConnection);
}

Mpv::~Mpv()
{
    if (context)
        mpv_render_context_free(context);

    mpv_terminate_destroy(inst);
}

void Mpv::on_update(void *ctx)
{
    Mpv *self = reinterpret_cast<Mpv*>(ctx);
    emit self->onUpdate();
}

void Mpv::doUpdate()
{
    update();
}

QQuickFramebufferObject::Renderer *Mpv::createRenderer() const
{
    window()->setPersistentOpenGLContext(true);
    window()->setPersistentSceneGraph(true);
    return new detail::MpvRenderer(const_cast<Mpv*>(this));
}*/
