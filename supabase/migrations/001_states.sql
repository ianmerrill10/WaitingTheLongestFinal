-- States reference table
CREATE TABLE states (
    code VARCHAR(2) PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    region VARCHAR(20) NOT NULL DEFAULT 'Unknown',
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

COMMENT ON TABLE states IS 'US states reference table for shelter/dog location';
CREATE INDEX idx_states_region ON states(region);
