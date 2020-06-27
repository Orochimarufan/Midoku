#include "blob.h"

namespace Midoku::Library {
DB_OBJECT_IMPL(Blob);

QImage Blob::toImage() {
    return QImage::fromData(get(data), "PNG");
}

}
