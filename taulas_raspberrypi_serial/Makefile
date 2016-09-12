#
# Taulas RPI Serial interface
#
# Makefile used to build the software
#
# Copyright 2016 Nicolas Mora <mail@babelouest.org>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation;
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU GENERAL PUBLIC LICENSE for more details.
#
# You should have received a copy of the GNU General Public
# License along with this library.  If not, see <http://www.gnu.org/licenses/>.
#

CC=gcc
CFLAGS=-c -Wall -I$(LIBYDER_LOCATION) -D_REENTRANT $(ADDITIONALFLAGS)
LIBS=-lc -lulfius -lyder -ljansson -lorcania -lpthread

all: taulas-rpi-serial

clean:
	rm -f *.o taulas-rpi-serial valgrind.txt

debug: ADDITIONALFLAGS=-DDEBUG -g -O0

debug: taulas-rpi-serial

taulas-rpi-serial.o: taulas-rpi-serial.c taulas-rpi-serial.h
	$(CC) $(CFLAGS) taulas-rpi-serial.c -DDEBUG -g -O0

arduino-serial-lib.o: arduino-serial-lib.c arduino-serial-lib.h
	$(CC) $(CFLAGS) arduino-serial-lib.c -DDEBUG -g -O0

taulas-rpi-serial: taulas-rpi-serial.o arduino-serial-lib.o
	$(CC) -o taulas-rpi-serial taulas-rpi-serial.o arduino-serial-lib.o $(LIBS)

memcheck: debug
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./taulas-rpi-serial 2>valgrind.txt

install: taulas-rpi-serial
	cp -f taulas-rpi-serial /usr/local/bin
