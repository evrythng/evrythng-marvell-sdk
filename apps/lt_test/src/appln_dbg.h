/*
 *  Copyright (C) 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __APPLN_DBG_H__
#define __APPLN_DBG_H__

#include <wmlog.h>

#if APPCONFIG_DEBUG_ENABLE
#define dbg(_fmt_, ...)				\
	wmprintf("[appln] "_fmt_"\n\r", ##__VA_ARGS__)
#else
#define dbg(...)
#endif /* APPCONFIG_DEBUG_ENABLE */


#endif /* ! __APPLN_DBG_H__ */
