#pragma once

#include <QtGlobal>

#if defined(MILO_LIBRARY)
#  define MILOSHARED_EXPORT Q_DECL_EXPORT
#else
#  define MILOSHARED_EXPORT Q_DECL_IMPORT
#endif
