-- init.sql - MariaDB initialization for orm-glib test database
--
-- Copyright 2025 Zach Pobiel
-- SPDX-License-Identifier: AGPL-3.0-or-later
--
-- This script runs automatically when the container starts.
-- The orm_test database is created by the MARIADB_DATABASE environment variable.

-- Grant all privileges on the test database
GRANT ALL PRIVILEGES ON orm_test.* TO 'orm_test'@'%';
FLUSH PRIVILEGES;

-- Create a simple health check table
CREATE TABLE IF NOT EXISTS orm_health_check (
    id INT AUTO_INCREMENT PRIMARY KEY,
    checked_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Insert a record to verify the database is working
INSERT INTO orm_health_check (checked_at) VALUES (NOW());
