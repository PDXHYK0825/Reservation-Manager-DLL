#pragma once

#include <QtGlobal>

#if defined(RESERVATION_EXPORTS)
#define RESERVATION_API Q_DECL_EXPORT
#else
#define RESERVATION_API Q_DECL_IMPORT
#endif
