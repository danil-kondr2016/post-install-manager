#include "version.h"

#define _STR(x) #x
#define STR(x) _STR(x)

#define POSTINST_VERSION "$(#)" \
	STR(POSTINST_VERSION_MAJOR) "." \
	STR(POSTINST_VERSION_MINOR) "." \
	STR(POSTINST_VERSION_BUILD) "." \
	STR(POSTINST_VERSION_REV)

const char postinst_version[] = POSTINST_VERSION;
