// GisGeometryLibJni - wrapper over GisGeometry for C# (P/Invoke) and Kotlin (JNI). Skeleton.

#if defined(_WIN32)
#  define GE_API extern "C" __declspec(dllexport)
#else
#  define GE_API extern "C" __attribute__((visibility("default")))
#endif

GE_API int GisGeometryLibJni_GetApiVersion()
{
    return 1;
}
