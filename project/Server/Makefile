#/************************************************************************************
#***
#***	File Author: Dell, 2021年 11月 22日 星期一 14:33:18 CST
#***
#************************************************************************************/

INSTALL_DIR := /usr/local

TARGET := redos

SOURCE := redos.cpp

INCS	:= -Iinclude 

LIBS	:= -Wl,-Bdynamic -lssl -lcrypto -lpthread

CXXFLAGS := -fPIC -Wall -Wextra
LDFLAGS := -fPIC -O3

# DEFINES := -D_GLIBCXX_USE_CXX11_ABI=1 -std=c++17 -std=gnu++17
DEFINES := -std=c++17

#****************************************************************************
# Makefile code common to all platforms
#****************************************************************************
CXXFLAGS := ${CXXFLAGS} ${DEFINES}

OBJECT := $(addsuffix .o,$(basename ${SOURCE}))

all: ${TARGET} 
# install

#****************************************************************************
# Compile block
#****************************************************************************
${TARGET}: ${OBJECT}
	${CXX} -o ${TARGET} ${LDFLAGS} ${OBJECT} ${LIBS}

install:
	sudo install -m 755 ${TARGET} ${INSTALL_DIR}/bin
	sudo cp redos.service /lib/systemd/system/
	sudo systemctl daemon-reload

uninstall:
	sudo service redos stop
	sudo rm -rf ${INSTALL_DIR}/bin/${TARGET}
	sudo rm -rf /lib/systemd/system/redos.service

#****************************************************************************
# common rules
#****************************************************************************
%.o : %.cpp
	${CXX} ${CXXFLAGS} ${INCS} -c $< -o $@

#****************************************************************************
# Depend block
#****************************************************************************
depend:

clean:
	rm -f core ${OBJECT}
	rm -f core ${TARGET}
