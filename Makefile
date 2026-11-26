TARGET = gta_grid_3d
OBJS = main.o

CXXFLAGS = -O2 -G0 -Wall -std=c++17
LIBS = -lSDL2 -lpspgu -lpspgum -lpspge -lpspaudio -lpspctrl -lpsphprm -lpsppower

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = GTA Grid 3D

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
