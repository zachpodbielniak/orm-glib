# Makefile - Build system for orm-glib
#
# Copyright 2025 Zach Pobiel
# SPDX-License-Identifier: AGPL-3.0-or-later

# Include configuration and rules
include config.mk
include rules.mk

# Default target
.PHONY: all
all: lib

# Build library
.PHONY: lib
lib: $(BUILD_DIR)/$(LIB_SHARED) $(BUILD_DIR)/$(LIB_STATIC)

# Shared library
$(BUILD_DIR)/$(LIB_SHARED): $(LIB_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -shared -Wl,-soname,$(LIB_SONAME) -o $@ $^ $(LDFLAGS)
	cd $(BUILD_DIR) && ln -sf $(LIB_SHARED) $(LIB_SONAME)

# Static library
$(BUILD_DIR)/$(LIB_STATIC): $(LIB_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(AR) rcs $@ $^

# Build tests
.PHONY: test tests
test: tests
tests: lib $(TEST_BINS)
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		LD_LIBRARY_PATH=$(BUILD_DIR) $$test || exit 1; \
	done
	@echo "All tests passed!"

# Build examples
.PHONY: examples
examples: lib $(EXAMPLE_BINS)

# Generate GObject introspection data
.PHONY: gir
gir: $(BUILD_DIR)/Orm-$(API_VERSION).gir $(BUILD_DIR)/Orm-$(API_VERSION).typelib

$(BUILD_DIR)/Orm-$(API_VERSION).gir: $(LIB_SRCS) $(PUBLIC_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(GIR_SCANNER) --warn-all \
		--namespace=Orm \
		--nsversion=$(API_VERSION) \
		--identifier-prefix=Orm \
		--symbol-prefix=orm \
		--include=GLib-2.0 \
		--include=GObject-2.0 \
		--include=Gio-2.0 \
		--library=$(LIB_NAME)-$(API_VERSION) \
		--library-path=$(BUILD_DIR) \
		--pkg glib-2.0 \
		--pkg gobject-2.0 \
		--pkg gio-2.0 \
		-DORM_COMPILATION \
		--output=$@ \
		$(PUBLIC_HEADERS) $(LIB_SRCS)

$(BUILD_DIR)/Orm-$(API_VERSION).typelib: $(BUILD_DIR)/Orm-$(API_VERSION).gir
	$(GIR_COMPILER) $< -o $@

# Install
.PHONY: install
install: lib
	@echo "Installing to $(DESTDIR)$(PREFIX)..."
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)/core
	install -d $(DESTDIR)$(INCLUDEDIR)/types
	install -d $(DESTDIR)$(INCLUDEDIR)/schema
	install -d $(DESTDIR)$(INCLUDEDIR)/migration
	install -d $(DESTDIR)$(PKGCONFIGDIR)
ifeq ($(BUILD_SHARED),1)
	install -m 755 $(BUILD_DIR)/$(LIB_SHARED) $(DESTDIR)$(LIBDIR)/
	cd $(DESTDIR)$(LIBDIR) && ln -sf $(LIB_SHARED) $(LIB_SONAME)
endif
ifeq ($(BUILD_STATIC),1)
	install -m 644 $(BUILD_DIR)/$(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
endif
	install -m 644 src/orm.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 src/orm-version.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 src/core/*.h $(DESTDIR)$(INCLUDEDIR)/core/
	install -m 644 src/types/*.h $(DESTDIR)$(INCLUDEDIR)/types/
	install -m 644 src/schema/*.h $(DESTDIR)$(INCLUDEDIR)/schema/
	install -m 644 src/migration/*.h $(DESTDIR)$(INCLUDEDIR)/migration/
	sed -e 's,@prefix@,$(PREFIX),' \
	    -e 's,@libdir@,$(LIBDIR),' \
	    -e 's,@includedir@,$(INCLUDEDIR),' \
	    -e 's,@VERSION@,$(VERSION),' \
	    -e 's,@API_VERSION@,$(API_VERSION),' \
	    orm-glib.pc.in > $(DESTDIR)$(PKGCONFIGDIR)/$(LIB_NAME)-$(API_VERSION).pc
ifeq ($(BUILD_GIR),1)
	install -d $(DESTDIR)$(GIRDIR)
	install -d $(DESTDIR)$(TYPELIBDIR)
	install -m 644 $(BUILD_DIR)/Orm-$(API_VERSION).gir $(DESTDIR)$(GIRDIR)/
	install -m 644 $(BUILD_DIR)/Orm-$(API_VERSION).typelib $(DESTDIR)$(TYPELIBDIR)/
endif
	@echo "Installation complete!"

# Uninstall
.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SHARED)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SONAME)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_STATIC)
	rm -rf $(DESTDIR)$(INCLUDEDIR)
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(LIB_NAME)-$(API_VERSION).pc
	rm -f $(DESTDIR)$(GIRDIR)/Orm-$(API_VERSION).gir
	rm -f $(DESTDIR)$(TYPELIBDIR)/Orm-$(API_VERSION).typelib

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# Show configuration
.PHONY: info
info:
	@echo "orm-glib build configuration"
	@echo "============================"
	@echo "Version:        $(VERSION)"
	@echo "API Version:    $(API_VERSION)"
	@echo "Prefix:         $(PREFIX)"
	@echo "CC:             $(CC)"
	@echo "CFLAGS:         $(CFLAGS)"
	@echo "LDFLAGS:        $(LDFLAGS)"
	@echo ""
	@echo "Build options:"
	@echo "  BUILD_SHARED: $(BUILD_SHARED)"
	@echo "  BUILD_STATIC: $(BUILD_STATIC)"
	@echo "  BUILD_GIR:    $(BUILD_GIR)"
	@echo "  BUILD_TESTS:  $(BUILD_TESTS)"
	@echo "  DEBUG:        $(DEBUG)"
	@echo "  ASAN:         $(ASAN)"
	@echo ""
	@echo "Database drivers:"
	@echo "  SQLITE:       $(ENABLE_SQLITE)"
	@echo "  POSTGRES:     $(ENABLE_POSTGRES)"
	@echo "  MYSQL:        $(ENABLE_MYSQL)"

# Help target
.PHONY: help
help:
	@echo "orm-glib build targets"
	@echo "======================"
	@echo ""
	@echo "  make              - Build library (shared and static)"
	@echo "  make lib          - Build library"
	@echo "  make test         - Build and run tests"
	@echo "  make examples     - Build examples"
	@echo "  make gir          - Generate GObject introspection data"
	@echo "  make install      - Install library and headers"
	@echo "  make uninstall    - Remove installed files"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make info         - Show build configuration"
	@echo ""
	@echo "Build options (set via environment or make args):"
	@echo "  DEBUG=1           - Enable debug build"
	@echo "  ASAN=1            - Enable address sanitizer"
	@echo "  UBSAN=1           - Enable undefined behavior sanitizer"
	@echo "  ENABLE_SQLITE=1   - Enable SQLite support (default)"
	@echo "  ENABLE_POSTGRES=1 - Enable PostgreSQL support"
	@echo "  ENABLE_MYSQL=1    - Enable MySQL support"
	@echo "  PREFIX=/path      - Installation prefix"
	@echo ""
	@echo "Container management (for testing PostgreSQL/MariaDB):"
	@echo "  make container-postgres-start  - Start PostgreSQL on port 15432"
	@echo "  make container-postgres-stop   - Stop PostgreSQL container"
	@echo "  make container-mariadb-start   - Start MariaDB on port 13306"
	@echo "  make container-mariadb-stop    - Stop MariaDB container"
	@echo "  make containers-start          - Start all database containers"
	@echo "  make containers-stop           - Stop all database containers"
	@echo "  make containers-status         - Show container status"
	@echo "  make containers-clean          - Remove containers and images"

# Dependency tracking
-include $(LIB_OBJS:.o=.d)

$(OBJ_DIR)/%.d: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MM -MT $(OBJ_DIR)/$*.o $< > $@

# =============================================================================
# Container Management (for PostgreSQL and MariaDB test databases)
# =============================================================================

CONTAINER_RUNTIME := podman
POSTGRES_CONTAINER := orm-postgres
POSTGRES_IMAGE := orm-postgres
POSTGRES_PORT := 15432
MARIADB_CONTAINER := orm-mariadb
MARIADB_IMAGE := orm-mariadb
MARIADB_PORT := 13306

.PHONY: container-postgres-build
container-postgres-build:
	@echo "Building PostgreSQL container..."
	$(CONTAINER_RUNTIME) build -t $(POSTGRES_IMAGE) -f containers/postgres/Containerfile containers/postgres

.PHONY: container-postgres-start
container-postgres-start: container-postgres-build
	@echo "Starting PostgreSQL container on port $(POSTGRES_PORT)..."
	@$(CONTAINER_RUNTIME) rm -f $(POSTGRES_CONTAINER) 2>/dev/null || true
	$(CONTAINER_RUNTIME) run -d --name $(POSTGRES_CONTAINER) -p $(POSTGRES_PORT):5432 $(POSTGRES_IMAGE)
	@echo "PostgreSQL available at: postgresql://orm_test:orm_test_pass@localhost:$(POSTGRES_PORT)/orm_test"
	@echo "Waiting for PostgreSQL to be ready..."
	@sleep 3

.PHONY: container-postgres-stop
container-postgres-stop:
	@echo "Stopping PostgreSQL container..."
	@$(CONTAINER_RUNTIME) stop $(POSTGRES_CONTAINER) 2>/dev/null || true
	@$(CONTAINER_RUNTIME) rm $(POSTGRES_CONTAINER) 2>/dev/null || true

.PHONY: container-postgres-logs
container-postgres-logs:
	$(CONTAINER_RUNTIME) logs -f $(POSTGRES_CONTAINER)

.PHONY: container-mariadb-build
container-mariadb-build:
	@echo "Building MariaDB container..."
	$(CONTAINER_RUNTIME) build -t $(MARIADB_IMAGE) -f containers/mariadb/Containerfile containers/mariadb

.PHONY: container-mariadb-start
container-mariadb-start: container-mariadb-build
	@echo "Starting MariaDB container on port $(MARIADB_PORT)..."
	@$(CONTAINER_RUNTIME) rm -f $(MARIADB_CONTAINER) 2>/dev/null || true
	$(CONTAINER_RUNTIME) run -d --name $(MARIADB_CONTAINER) -p $(MARIADB_PORT):3306 $(MARIADB_IMAGE)
	@echo "MariaDB available at: mysql://orm_test:orm_test_pass@localhost:$(MARIADB_PORT)/orm_test"
	@echo "Waiting for MariaDB to be ready..."
	@sleep 5

.PHONY: container-mariadb-stop
container-mariadb-stop:
	@echo "Stopping MariaDB container..."
	@$(CONTAINER_RUNTIME) stop $(MARIADB_CONTAINER) 2>/dev/null || true
	@$(CONTAINER_RUNTIME) rm $(MARIADB_CONTAINER) 2>/dev/null || true

.PHONY: container-mariadb-logs
container-mariadb-logs:
	$(CONTAINER_RUNTIME) logs -f $(MARIADB_CONTAINER)

.PHONY: containers-start
containers-start: container-postgres-start container-mariadb-start
	@echo "All database containers started."

.PHONY: containers-stop
containers-stop: container-postgres-stop container-mariadb-stop
	@echo "All database containers stopped."

.PHONY: containers-status
containers-status:
	@echo "Container status:"
	@$(CONTAINER_RUNTIME) ps -a --filter name=$(POSTGRES_CONTAINER) --filter name=$(MARIADB_CONTAINER)

.PHONY: containers-clean
containers-clean: containers-stop
	@echo "Removing container images..."
	@$(CONTAINER_RUNTIME) rmi $(POSTGRES_IMAGE) 2>/dev/null || true
	@$(CONTAINER_RUNTIME) rmi $(MARIADB_IMAGE) 2>/dev/null || true
	@echo "Container cleanup complete."
