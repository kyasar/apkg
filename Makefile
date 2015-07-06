EXECUTABLE     = apkg
TMPHOST        = .host
TMPTARGET      = .target
LIB            = libapkg.so
SOURCES        = apkg.c 
LIBSOURCES     = libapkg.c depends.c status.c util.c list.c 
OBJECTS        = $(SOURCES:.c=.o)
LIBOBJECTS     = $(LIBSOURCES:.c=.o)
CFLAGS         = -Wall -g -O2
LIBSOFLAGS     = -shared -g -W1,-soname, 
LDHOSTFLAGS    = -Wl -L./$(TMPHOST) -lapkg
LDTARGETFLAGS  = -Wl -L./$(TMPTARGET) -lapkg
LIBFULLNAME    = $(LIB)

include ../Makefile.in.common

all: check_hostcc check_targetcc $(EXECUTABLE) 

.gitversion.h: $(SOURCES) $(LIBSOURCES)
	@echo "Updating $@"
	@VER=$$(setgitversion . 2>/dev/null) ; echo "#define GITVERSION \"$$VER\"" > $@

$(EXECUTABLE): check_dirs $(LIB) $(OBJECTS)
	$(TARGET_CC)  $(LDTARGETFLAGS) $(TMPTARGET)/*.o -o $(TMPTARGET)/$@
	$(HOST_CC)    $(LDHOSTFLAGS)   $(TMPHOST)/*.o   -o $(TMPHOST)/$@

.c.o:
	$(TARGET_CC)  $(CFLAGS) -c $< -o $(TMPTARGET)/$@
	$(HOST_CC)    $(CFLAGS) -c $< -o $(TMPHOST)/$@

$(LIB): $(LIBOBJECTS)
	$(TARGET_CC)  $(LIBSOFLAGS) $(TMPTARGET)/*.o -o $(TMPTARGET)/$@
	$(HOST_CC)    $(LIBSOFLAGS) $(TMPHOST)/*.o   -o $(TMPHOST)/$@
	
check_dirs:
	rm -rf $(TMPHOST) $(TMPTARGET)
	mkdir  $(TMPHOST) $(TMPTARGET)
	
check_hostcc:
ifndef HOST_CC
	@echo 'Error: HOST_CC not set, specify compiler for host system'
	@exit 1
endif

check_targetcc:
ifndef TARGET_CC
	@echo 'Error: TARGET_CC not set, specify compiler for target system'
	@exit 1
endif

check_installdir:
ifndef INSTALL_DIR
	@echo 'Error: INSTALL_DIR not set, specify directory for installing'
	@exit 1
endif

install: check_installdir
	mkdir -p $(INSTALL_DIR)/usr/bin/atlas
	mkdir -p $(INSTALL_DIR)/usr/lib
	install -Dm 755 $(TMPTARGET)/$(EXECUTABLE)     $(INSTALL_DIR)/usr/bin/atlas/$(EXECUTABLE)
	install -Dm 755 $(TMPTARGET)/$(LIB)    $(INSTALL_DIR)/usr/lib/
	mkdir -p /usr/bin
	mkdir -p /usr/lib
	install -Dm 755 $(TMPHOST)/$(EXECUTABLE)       /usr/bin/$(EXECUTABLE)
	install -Dm 755 libapkg.h libapkg_defs.h       /usr/include/
	install -Dm 755 $(TMPHOST)/$(LIB)      /usr/lib/$(LIB)
	
install_staging: check_installdir
	mkdir -p $(INSTALL_DIR)/usr/lib
	install -Dm 755 $(TMPTARGET)/$(LIB)         $(INSTALL_DIR)/usr/lib/
	install -Dm 755 libapkg.h libapkg_defs.h    $(INSTALL_DIR)/usr/include/
	
clean:
	rm -rf .depend .gitversion.h $(TMPTARGET) $(TMPHOST)

.depend: .gitversion.h $(SOURCES)
	@$(CC) -MM $(SOURCES) > .depend

-include .depend
