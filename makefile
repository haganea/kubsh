# Компилятор и флаги
CXX := g++
CXXFLAGS := -O2 -std=c++11 -Wall -Wextra

# Имя программы
TARGET := kubsh

# Настройки пакета
PACKAGE_NAME := $(TARGET)
VERSION := 1.0
ARCH := amd64
DEB_FILENAME := $(PACKAGE_NAME)_$(VERSION)_$(ARCH).deb

# Временные директории
BUILD_DIR := deb_build
INSTALL_DIR := $(BUILD_DIR)/usr/local/bin

.PHONY: all build run clean deb install

all: build

build: $(TARGET)

$(TARGET): kubsh.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: build
	./$(TARGET)

deb: $(TARGET) | $(BUILD_DIR) $(INSTALL_DIR)
	# Копируем бинарник
	cp $(TARGET) $(INSTALL_DIR)/
	chmod 755 $(INSTALL_DIR)/$(TARGET)
	
	# Создаем базовую структуру пакета
	mkdir -p $(BUILD_DIR)/DEBIAN
	mkdir -p $(BUILD_DIR)/usr/share/doc/$(PACKAGE_NAME)
	
	# Генерируем контрольный файл
	@echo "Package: $(PACKAGE_NAME)" > $(BUILD_DIR)/DEBIAN/control
	@echo "Version: $(VERSION)" >> $(BUILD_DIR)/DEBIAN/control
	@echo "Section: utils" >> $(BUILD_DIR)/DEBIAN/control
	@echo "Priority: optional" >> $(BUILD_DIR)/DEBIAN/control
	@echo "Architecture: $(ARCH)" >> $(BUILD_DIR)/DEBIAN/control
	@echo "Maintainer: $(USER) <$(USER)@example.com>" >> $(BUILD_DIR)/DEBIAN/control
	@echo "Description: Simple custom shell implementation" >> $(BUILD_DIR)/DEBIAN/control
	@echo " Learning project for shell development in C++" >> $(BUILD_DIR)/DEBIAN/control
	
	# Создаем copyright файл (упрощенный)
	@echo "License: MIT" > $(BUILD_DIR)/usr/share/doc/$(PACKAGE_NAME)/copyright
	
	# Собираем пакет
	dpkg-deb --build $(BUILD_DIR) $(DEB_FILENAME)

$(BUILD_DIR) $(INSTALL_DIR):
	mkdir -p $@

install: build
	cp $(TARGET) /usr/local/bin/

clean:
	rm -rf $(TARGET) $(BUILD_DIR) *.deb

test: build
	@echo "q" | ./$(TARGET) || true
