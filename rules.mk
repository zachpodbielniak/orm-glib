# rules.mk - Common build rules for orm-glib
#
# Copyright 2025 Zach Pobiel
# SPDX-License-Identifier: AGPL-3.0-or-later

# Source files by module
CORE_SRCS := \
	src/core/orm-enums.c \
	src/core/orm-error.c \
	src/core/orm-value.c

TYPES_SRCS := \
	src/types/orm-sql-type.c \
	src/types/orm-integer.c \
	src/types/orm-string.c \
	src/types/orm-text.c \
	src/types/orm-boolean.c \
	src/types/orm-float.c \
	src/types/orm-datetime.c \
	src/types/orm-blob.c

SCHEMA_SRCS := \
	src/schema/orm-metadata.c \
	src/schema/orm-table.c \
	src/schema/orm-column.c \
	src/schema/orm-primary-key.c \
	src/schema/orm-foreign-key.c \
	src/schema/orm-index.c

# Engine layer
ENGINE_SRCS := \
	src/engine/orm-engine.c \
	src/engine/orm-connection.c \
	src/engine/orm-transaction.c \
	src/engine/orm-result.c \
	src/engine/orm-row.c

# Dialect base files
DIALECT_SRCS := \
	src/dialect/orm-dialect.c \
	src/dialect/orm-type-compiler.c \
	src/dialect/orm-ddl-compiler.c

# Driver layer: backend I/O behind an abstraction, plus the scheme registry
DRIVER_SRCS := \
	src/driver/orm-driver.c \
	src/driver/orm-driver-connection.c \
	src/driver/orm-driver-result.c \
	src/driver/orm-driver-registry.c

# Per-backend drivers (conditional)
ifeq ($(ENABLE_SQLITE),1)
SQLITE_DRIVER_SRCS := src/driver/sqlite/orm-sqlite-driver.c
else
SQLITE_DRIVER_SRCS :=
endif

ifeq ($(ENABLE_POSTGRES),1)
POSTGRES_DRIVER_SRCS := src/driver/postgres/orm-postgres-driver.c
else
POSTGRES_DRIVER_SRCS :=
endif

ifeq ($(ENABLE_MYSQL),1)
MYSQL_DRIVER_SRCS := src/driver/mysql/orm-mysql-driver.c
else
MYSQL_DRIVER_SRCS :=
endif

# SQLite dialect (conditional)
ifeq ($(ENABLE_SQLITE),1)
SQLITE_DIALECT_SRCS := \
	src/dialect/sqlite/orm-sqlite-dialect.c \
	src/dialect/sqlite/orm-sqlite-type-compiler.c \
	src/dialect/sqlite/orm-sqlite-ddl-compiler.c
else
SQLITE_DIALECT_SRCS :=
endif

# PostgreSQL dialect (conditional)
ifeq ($(ENABLE_POSTGRES),1)
POSTGRES_DIALECT_SRCS := \
	src/dialect/postgres/orm-postgres-dialect.c \
	src/dialect/postgres/orm-postgres-type-compiler.c \
	src/dialect/postgres/orm-postgres-ddl-compiler.c
else
POSTGRES_DIALECT_SRCS :=
endif

# MySQL dialect (conditional)
ifeq ($(ENABLE_MYSQL),1)
MYSQL_DIALECT_SRCS := \
	src/dialect/mysql/orm-mysql-dialect.c \
	src/dialect/mysql/orm-mysql-type-compiler.c \
	src/dialect/mysql/orm-mysql-ddl-compiler.c
else
MYSQL_DIALECT_SRCS :=
endif

# SQL expression language
SQL_SRCS := \
	src/sql/orm-expression.c \
	src/sql/orm-column-element.c \
	src/sql/orm-table-clause.c \
	src/sql/orm-binary-expression.c \
	src/sql/orm-literal.c \
	src/sql/orm-select.c \
	src/sql/orm-insert.c \
	src/sql/orm-update.c \
	src/sql/orm-delete.c

# ORM layer
ORM_SRCS := \
	src/orm/orm-serializable.c \
	src/orm/orm-property.c \
	src/orm/orm-relationship.c \
	src/orm/orm-mapper.c \
	src/orm/orm-identity-map.c \
	src/orm/orm-session.c \
	src/orm/orm-query.c

# All source files (Phase 1 through Phase 6)
LIB_SRCS := $(CORE_SRCS) $(TYPES_SRCS) $(SCHEMA_SRCS) $(DIALECT_SRCS) $(SQLITE_DIALECT_SRCS) $(POSTGRES_DIALECT_SRCS) $(MYSQL_DIALECT_SRCS) $(DRIVER_SRCS) $(SQLITE_DRIVER_SRCS) $(POSTGRES_DRIVER_SRCS) $(MYSQL_DRIVER_SRCS) $(SQL_SRCS) $(ENGINE_SRCS) $(ORM_SRCS)

# Object files
LIB_OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(LIB_SRCS))

# Base header files for installation
PUBLIC_HEADERS_BASE := \
	src/orm.h \
	src/orm-version.h \
	src/core/orm-types.h \
	src/core/orm-enums.h \
	src/core/orm-error.h \
	src/core/orm-value.h \
	src/types/orm-sql-type.h \
	src/types/orm-integer.h \
	src/types/orm-string.h \
	src/types/orm-text.h \
	src/types/orm-boolean.h \
	src/types/orm-float.h \
	src/types/orm-datetime.h \
	src/types/orm-blob.h \
	src/schema/orm-metadata.h \
	src/schema/orm-table.h \
	src/schema/orm-column.h \
	src/schema/orm-primary-key.h \
	src/schema/orm-foreign-key.h \
	src/schema/orm-index.h \
	src/dialect/orm-dialect.h \
	src/dialect/orm-type-compiler.h \
	src/dialect/orm-ddl-compiler.h \
	src/driver/orm-driver.h \
	src/driver/orm-driver-connection.h \
	src/driver/orm-driver-result.h \
	src/driver/orm-driver-registry.h \
	src/sql/orm-expression.h \
	src/sql/orm-column-element.h \
	src/sql/orm-table-clause.h \
	src/sql/orm-binary-expression.h \
	src/sql/orm-literal.h \
	src/sql/orm-select.h \
	src/sql/orm-insert.h \
	src/sql/orm-update.h \
	src/sql/orm-delete.h \
	src/engine/orm-engine.h \
	src/engine/orm-connection.h \
	src/engine/orm-transaction.h \
	src/engine/orm-result.h \
	src/engine/orm-row.h \
	src/orm/orm-serializable.h \
	src/orm/orm-property.h \
	src/orm/orm-relationship.h \
	src/orm/orm-mapper.h \
	src/orm/orm-identity-map.h \
	src/orm/orm-session.h \
	src/orm/orm-query.h

# SQLite dialect headers (conditional)
ifeq ($(ENABLE_SQLITE),1)
SQLITE_DIALECT_HEADERS := \
	src/dialect/sqlite/orm-sqlite-dialect.h \
	src/dialect/sqlite/orm-sqlite-type-compiler.h \
	src/dialect/sqlite/orm-sqlite-ddl-compiler.h
else
SQLITE_DIALECT_HEADERS :=
endif

# PostgreSQL dialect headers (conditional)
ifeq ($(ENABLE_POSTGRES),1)
POSTGRES_DIALECT_HEADERS := \
	src/dialect/postgres/orm-postgres-dialect.h \
	src/dialect/postgres/orm-postgres-type-compiler.h \
	src/dialect/postgres/orm-postgres-ddl-compiler.h
else
POSTGRES_DIALECT_HEADERS :=
endif

# MySQL dialect headers (conditional)
ifeq ($(ENABLE_MYSQL),1)
MYSQL_DIALECT_HEADERS := \
	src/dialect/mysql/orm-mysql-dialect.h \
	src/dialect/mysql/orm-mysql-type-compiler.h \
	src/dialect/mysql/orm-mysql-ddl-compiler.h
else
MYSQL_DIALECT_HEADERS :=
endif

# Combined header files for installation
PUBLIC_HEADERS := $(PUBLIC_HEADERS_BASE) $(SQLITE_DIALECT_HEADERS) $(POSTGRES_DIALECT_HEADERS) $(MYSQL_DIALECT_HEADERS)

# Test fixture sources (compiled with each test)
TEST_FIXTURE_SRCS := \
	tests/test-fixtures.c \
	tests/test-model.c

# Test sources (excluding fixtures)
TEST_SRCS := \
	tests/test-value.c \
	tests/test-types.c \
	tests/test-enums.c \
	tests/test-schema.c \
	tests/test-engine.c \
	tests/test-dialect-factory.c \
	tests/test-driver.c \
	tests/test-result-types.c \
	tests/test-isolation.c \
	tests/test-dialect-sqlite.c \
	tests/test-expression.c \
	tests/test-select.c \
	tests/test-orm.c \
	tests/test-session.c \
	tests/test-query.c \
	tests/test-integration.c

TEST_BINS := $(patsubst tests/%.c,$(TEST_DIR)/%,$(TEST_SRCS))

# Example common sources (shared models)
EXAMPLE_COMMON_SRCS := examples/common/example-models.c

# SQLite examples (conditional)
ifeq ($(ENABLE_SQLITE),1)
SQLITE_EXAMPLE_SRCS := \
	examples/sqlite/basic-crud.c \
	examples/sqlite/query-builder.c \
	examples/sqlite/relationships.c
SQLITE_EXAMPLE_BINS := \
	$(EXAMPLE_DIR)/sqlite-basic-crud \
	$(EXAMPLE_DIR)/sqlite-query-builder \
	$(EXAMPLE_DIR)/sqlite-relationships
else
SQLITE_EXAMPLE_SRCS :=
SQLITE_EXAMPLE_BINS :=
endif

# PostgreSQL examples (conditional)
ifeq ($(ENABLE_POSTGRES),1)
POSTGRES_EXAMPLE_SRCS := \
	examples/postgres/basic-crud.c \
	examples/postgres/query-builder.c \
	examples/postgres/relationships.c
POSTGRES_EXAMPLE_BINS := \
	$(EXAMPLE_DIR)/postgres-basic-crud \
	$(EXAMPLE_DIR)/postgres-query-builder \
	$(EXAMPLE_DIR)/postgres-relationships
else
POSTGRES_EXAMPLE_SRCS :=
POSTGRES_EXAMPLE_BINS :=
endif

# MariaDB/MySQL examples (conditional)
ifeq ($(ENABLE_MYSQL),1)
MARIADB_EXAMPLE_SRCS := \
	examples/mariadb/basic-crud.c \
	examples/mariadb/query-builder.c \
	examples/mariadb/relationships.c
MARIADB_EXAMPLE_BINS := \
	$(EXAMPLE_DIR)/mariadb-basic-crud \
	$(EXAMPLE_DIR)/mariadb-query-builder \
	$(EXAMPLE_DIR)/mariadb-relationships
else
MARIADB_EXAMPLE_SRCS :=
MARIADB_EXAMPLE_BINS :=
endif

# All example binaries
EXAMPLE_BINS := $(SQLITE_EXAMPLE_BINS) $(POSTGRES_EXAMPLE_BINS) $(MARIADB_EXAMPLE_BINS)

# Common compile rule
$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Test compile rule - link fixture sources with each test
$(TEST_DIR)/%: tests/%.c $(TEST_FIXTURE_SRCS) $(BUILD_DIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Itests $< $(TEST_FIXTURE_SRCS) -o $@ \
		-L$(BUILD_DIR) -l$(LIB_NAME)-$(API_VERSION) $(LDFLAGS)

# SQLite example compile rules
$(EXAMPLE_DIR)/sqlite-%: examples/sqlite/%.c $(EXAMPLE_COMMON_SRCS) $(BUILD_DIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Iexamples $< $(EXAMPLE_COMMON_SRCS) -o $@ \
		-L$(BUILD_DIR) -l$(LIB_NAME)-$(API_VERSION) $(LDFLAGS)

# PostgreSQL example compile rules
$(EXAMPLE_DIR)/postgres-%: examples/postgres/%.c $(EXAMPLE_COMMON_SRCS) $(BUILD_DIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Iexamples $< $(EXAMPLE_COMMON_SRCS) -o $@ \
		-L$(BUILD_DIR) -l$(LIB_NAME)-$(API_VERSION) $(LDFLAGS)

# MariaDB example compile rules
$(EXAMPLE_DIR)/mariadb-%: examples/mariadb/%.c $(EXAMPLE_COMMON_SRCS) $(BUILD_DIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Iexamples $< $(EXAMPLE_COMMON_SRCS) -o $@ \
		-L$(BUILD_DIR) -l$(LIB_NAME)-$(API_VERSION) $(LDFLAGS)
