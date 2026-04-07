#ifndef QT_COMPATIBLE_H
#define QT_COMPATIBLE_H

#ifdef COMPILE_ON_QT6
    #define QtStringSkipEmptyParts Qt::SkipEmptyParts
#else
    #define QtStringSkipEmptyParts QString::SkipEmptyParts
#endif

#endif // QT_COMPATIBLE_H
