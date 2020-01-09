if HAVE_CUPS
CUPS_DIR=cups	
else
CUPS_DIR=
endif

if HAVE_PAPI
PAPI_DIR=papi
else
PAPI_DIR=
endif

if PLATFORM_WIN32
WIN32_DIR=win32
else
WIN32_DIR=
endif

LPD_DIR=lpd

SUBDIRS=$(CUPS_DIR) $(PAPI_DIR) $(LPD_DIR) $(WIN32_DIR)

EXTRA_DIST = makefile.msc
