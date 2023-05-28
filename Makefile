.POSIX:

BIN=	wish
SRC=	src/wish.c src/parse.c src/builtin.c src/str.c
OBJ=	${SRC:.c=.o}

CC?=	cc

CFLAGS+= -Iinclude

all: ${BIN} include/*.h

${BIN}: ${OBJ}
	${CC} ${LDFLAGS} ${OBJ} -o $@

src/.c.o:
	${CC} -c ${CFLAGS} $<

clean:
	rm -f ${BIN} ${OBJ}

.PHONY: all clean
