# - Try to find konqueror library
# Once done this will define
#
#  PLASMA_FOUND - system has libkonq library
#  PLASMA_INCLUDE_DIR - the PLASMA include directory
#  PLASMA_LIBRARY - the libkonq library

#  Original file: FindMarbleWidget.cmake (found in digikam-0.10.0-beta2)
#  copyright 2008 by Patrick Spendrin <ps_ml@gmx.de>
#  Copyright (c) 2009, Alexander Neundorf, <neundorf@kde.org>
#  use this file as you like
#
#  Modifications to find libkonq by Joachim Eibl 2008

find_path(PLASMA_INCLUDE_DIR plasma/plasma_export.h )

find_library(PLASMA_LIBRARY plasma)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PLASMA  DEFAULT_MSG  PLASMA_INCLUDE_DIR PLASMA_LIBRARY )

mark_as_advanced(PLASMA_INCLUDE_DIR PLASMA_LIBRARY)
