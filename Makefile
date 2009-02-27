PACKAGE	=	tssend
CC = gcc
CFLAGS = -O2 -g -Wall
#LFLAGS = -lpthread
SRCS	= $(OBJS:%.o=%.c)
HEADERS	= rtc.h raw_send.h
OBJS	= tssend.o raw_send.o
FILES	= Makefile  $(HEADERS) $(SRCS) 
VER	= `date +%Y%m%d`
RM	= rm -f

all: $(PACKAGE)
#	chmod +s $(PACKAGE)

$(PACKAGE): $(OBJS)
	$(CC) -o $(PACKAGE) $(OBJS) $(CFLAGS) $(LFLAGS)

$(OBJS): $(SRCS) $(HEADERS)
	$(CC) -c $(SRCS)

clean:
	$(RM) $(PACKAGE) $(OBJS)
	$(RM) core gmon.out *~ #*#

tar:
	@echo $(PACKAGE)-$(VER) > .package
	@$(RM) -r `cat .package`
	@mkdir `cat .package`
	@ln $(FILES) `cat .package`
	tar cvf - `cat .package` | gzip -9 > `cat .package`.tar.gz
	@$(RM) -r `cat .package` .package

install:
	cp $(PACKAGE) /usr/local/bin/

# DO NOT DELETE
