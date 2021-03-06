/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2004-5   The R Development Core Team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <R.h>
#include <Rinternals.h>
#include <Graphics.h>
#include <Rdevices.h>

#include "grDevices.h"
#include <R_ext/Rdynload.h>

#define CALLDEF(name, n)  {#name, (DL_FUNC) &name, n}

static R_CallMethodDef CallEntries[] = {
    CALLDEF(Type1FontInUse, 1),
    {"L_nullDevice", (DL_FUNC) &L_nullDevice, 0},
    {NULL, NULL, 0}
};

#define EXTDEF(name, n)  {#name, (DL_FUNC) &name, n}

static R_ExternalMethodDef ExtEntries[] = {
    EXTDEF(PicTeX, 6),
    EXTDEF(PostScript, 16),
    EXTDEF(XFig, 11),
    EXTDEF(PDF, 13),
#ifdef WIN32
    EXTDEF(devga, 15),
    EXTDEF(savePlot, 3),
#else
    EXTDEF(Quartz, -1),
#endif
    {NULL, NULL, 0}
};

void R_init_grDevices(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, CallEntries, NULL, ExtEntries);
    R_useDynamicSymbols(dll, FALSE);
}
