-- init.sql - PostgreSQL initialization for orm-glib test database
--
-- Copyright 2025 Zach Pobiel
-- SPDX-License-Identifier: AGPL-3.0-or-later
--
-- This script runs automatically when the container starts.
-- The orm_test database is created by the POSTGRES_DB environment variable.

-- Grant all privileges on the test database
GRANT ALL PRIVILEGES ON DATABASE orm_test TO orm_test;

-- Create a simple health check table
CREATE TABLE IF NOT EXISTS orm_health_check (
    id SERIAL PRIMARY KEY,
    checked_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Insert a record to verify the database is working
INSERT INTO orm_health_check (checked_at) VALUES (NOW());
