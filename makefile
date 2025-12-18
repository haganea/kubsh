# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra
FUSE_FLAGS = -I/usr/include/fuse3 -lfuse3

# Настройки приложения
APP_NAME = kubsh
APP_VERSION = 1.0.0
BUILD_DIR = build
DEB_DIR = $(BUILD_DIR)/$(APP_NAME)_$(APP_VERSION)_amd64
DEB_FILE = kubsh.deb

# Исходные файлы
SRCS = kubsh.cpp vfs.cpp
OBJS = $(SRCS:.cpp=.o)

# Основная цель
all: $(APP_NAME)

# Сборка исполняемого файла
$(APP_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(APP_NAME) $(OBJS) $(FUSE_FLAGS)

# Компиляция .cpp файлов
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(FUSE_FLAGS) -c $< -o $@

# Запуск шелла
run: $(APP_NAME)
	./$(APP_NAME)

# Подготовка структуры для deb-пакета
prepare-deb: $(APP_NAME)
	@echo "Подготовка структуры для deb-пакета..."
	@# Полностью очищаем старую структуру с sudo (если нужно)
	@sudo rm -rf $(DEB_DIR) 2>/dev/null || true
	@rm -rf $(DEB_DIR) 2>/dev/null || true
	@mkdir -p $(DEB_DIR)/DEBIAN
	@mkdir -p $(DEB_DIR)/usr/local/bin
	@cp $(APP_NAME) $(DEB_DIR)/usr/local/bin/
	@chmod +x $(DEB_DIR)/usr/local/bin/$(APP_NAME)
	
	@echo "Создание control файла..."
	@echo "Package: $(APP_NAME)" > $(DEB_DIR)/DEBIAN/control
	@echo "Version: $(APP_VERSION)" >> $(DEB_DIR)/DEBIAN/control
	@echo "Section: utils" >> $(DEB_DIR)/DEBIAN/control
	@echo "Priority: optional" >> $(DEB_DIR)/DEBIAN/control
	@echo "Architecture: amd64" >> $(DEB_DIR)/DEBIAN/control
	@echo "Maintainer: Your Name <your.email@example.com>" >> $(DEB_DIR)/DEBIAN/control
	@echo "Description: Simple custom shell" >> $(DEB_DIR)/DEBIAN/control
	@echo " A simple custom shell implementation for learning purposes." >> $(DEB_DIR)/DEBIAN/control

# Сборка deb-пакета
deb: prepare-deb
	@echo "Сборка deb-пакета..."
	dpkg-deb --build $(DEB_DIR)
	@mv $(BUILD_DIR)/$(APP_NAME)_$(APP_VERSION)_amd64.deb $(DEB_FILE)
	@echo "Пакет создан: $(DEB_FILE)"

# Тестирование в Docker контейнере
test: deb
	@echo "Запуск теста в Docker контейнере..."
	@-docker run --rm \
		-v $(DEB_FILE):/mnt/kubsh.deb \
		--device /dev/fuse \
		--cap-add SYS_ADMIN \
		--security-opt apparmor:unconfined \
		ghcr.io/xardb/kubshfuse:master 2>/dev/null || true

# Очистка
clean:
	rm -rf $(BUILD_DIR) $(APP_NAME) *.deb $(OBJS)

# Установка пакета (требует sudo)
install: deb
	sudo dpkg -i $(DEB_FILE)

# Удаление пакета
uninstall:
	sudo dpkg -r $(APP_NAME)

# Показать справку
help:
	@echo "Доступные команды:"
	@echo "  make all      - собрать программу"
	@echo "  make deb      - создать deb-пакет"
	@echo "  make install  - установить пакет"
	@echo "  make uninstall - удалить пакет"
	@echo "  make clean    - очистить проект"
	@echo "  make run      - запустить шелл"
	@echo "  make test     - собрать и запустить тест в Docker"
	@echo "  make help     - показать эту справку"

.PHONY: all run prepare-deb deb install uninstall clean help test
