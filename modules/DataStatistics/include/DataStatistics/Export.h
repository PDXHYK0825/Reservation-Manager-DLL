#pragma once

#include <QtGlobal>

#if defined(DATA_STATISTICS_EXPORTS)
#define DATA_STATISTICS_API Q_DECL_EXPORT
#else
#define DATA_STATISTICS_API Q_DECL_IMPORT
#endif
