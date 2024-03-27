# light-status - percentage configuration utility
# See LICENSE file for copyright and license details.

include config.mk

OUT_DIR = out
DIST_DIR = dist

SRC = 
HEADERS = \
	bitstream.h \
	bytestream.h \
	decoder.h \
	defs.h \
	mem.h \
	reverse.h \
	utils.h \
	video.h \
	vlc.h

OBJ = $(addprefix ${OUT_DIR}/,${SRC:.c=.o})
DIST_ASSETS = LICENSE Makefile README.md config.mk ${HEADERS} ${SRC}

all: options build-lib

options:
	@echo ${BIN_NAME} build options:
	@echo ""
	@echo "CFLAGS   = ${CFLAGS} ${DEFFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@echo ""

${OUT_DIR}:
	mkdir -p $@ $@/build/{lib,include}

# ${OUT_DIR}/%.o: %.c ${HEADERS} config.mk | ${OUT_DIR}
# 	${CC} -c ${CFLAGS} ${DEFFLAGS} $< -o $@
# 
# ${OUT_DIR}/build/lib/${BIN_NAME}: ${OBJ}
# 	${CC} -o $@ ${OBJ} ${LDFLAGS}


build-lib: | ${OUT_DIR} # | ${OUT_DIR}/build/lib/${BIN_NAME}
	cp -r ${HEADERS} ${OUT_DIR}/build/include

clean:
	rm -rf ${OUT_DIR}
	rm -rf ${DIST_DIR}

dist:
	mkdir -p ${DIST_DIR}/${BIN_NAME}-${VERSION}
	cp -r ${DIST_ASSETS} ${DIST_DIR}/${BIN_NAME}-${VERSION}
	cd ${DIST_DIR}; \
		tar -cf ${BIN_NAME}-${VERSION}.tar ${BIN_NAME}-${VERSION}; \
		gzip ${BIN_NAME}-${VERSION}.tar; \
		rm -rf ${BIN_NAME}-${VERSION}

.PHONY: all options clean build-lib dist
