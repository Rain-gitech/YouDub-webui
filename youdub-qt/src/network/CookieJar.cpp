#include "CookieJar.h"

namespace youdub {

CookieJar::CookieJar(QObject *parent)
    : QNetworkCookieJar(parent) {}

} // namespace youdub
