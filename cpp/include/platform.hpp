//- DLL exports -------------------------------------------------------------

#if (defined (__WINDOWS__))
#   if defined ZRE_DLL_EXPORT
#       if (!defined (ZRE_EXPORT))
#          define ZRE_EXPORT __declspec(dllexport)
#       endif
#   else
#       if (!defined (ZRE_EXPORT))
#          define ZRE_EXPORT __declspec(dllimport)
#       endif
#   endif
#else
#   define ZRE_EXPORT
#endif