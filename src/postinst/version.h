#pragma once

#define POSTINST_VERSION_MAJOR    0
#define POSTINST_VERSION_MINOR    3
#define POSTINST_VERSION_BUILD    66
#define POSTINST_VERSION_REV      0

#ifndef RESOURCE_RC
extern const char postinst_version[];
#endif
#ifdef RESOURCE_RC
#define _STR(x) #x
#define STR(x) _STR(x)

#define postinst_version \
	STR(POSTINST_VERSION_MAJOR) "." \
	STR(POSTINST_VERSION_MINOR) "." \
	STR(POSTINST_VERSION_BUILD) "." \
	STR(POSTINST_VERSION_REV)
#endif
