CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
GTKFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTKLIBS = $(shell pkg-config --libs gtk+-3.0)

SRC = src/main.cpp
TARGET = msuper-gui
PACKAGE_NAME = msuper-gui
VERSION = 2.0.0
ARCH = amd64
DEB_FILE = $(PACKAGE_NAME)_$(VERSION)_$(ARCH).deb

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(GTKFLAGS) -o $(TARGET) $(SRC) $(GTKLIBS)

clean:
	rm -f $(TARGET)
	rm -f $(DEB_FILE)
	rm -rf debian

run:
	./$(TARGET)

deb: $(TARGET)
	./build-deb.sh

install-deb: deb
	sudo dpkg -i $(DEB_FILE)

uninstall-deb:
	sudo dpkg -r $(PACKAGE_NAME)
