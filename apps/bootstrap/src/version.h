#pragma once

#if !defined(APP_VERSION)
#if defined(GLI_DEBUG)
#define APP_VERSION "Debug Build"
#elif defined(GLI_DEVELOPMENT)
#define APP_VERSION "Development Build"
#else
#define APP_VERSION "Release Build"
#endif
#endif // !defined(APP_VERSION)
