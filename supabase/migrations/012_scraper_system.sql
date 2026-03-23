-- Migration 012: Scraper System
-- Adds columns to shelters table for universal website scraping

ALTER TABLE shelters ADD COLUMN IF NOT EXISTS website_platform VARCHAR(50) DEFAULT 'unknown';
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS adoptable_page_url VARCHAR(500);
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS platform_shelter_id VARCHAR(100);
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS last_scraped_at TIMESTAMPTZ;
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS scrape_status VARCHAR(30) DEFAULT 'pending';
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS scrape_error TEXT;
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS dogs_found_last_scrape INTEGER DEFAULT 0;
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS scrape_count INTEGER DEFAULT 0;
ALTER TABLE shelters ADD COLUMN IF NOT EXISTS next_scrape_at TIMESTAMPTZ;

-- Indexes for efficient scraper queries
CREATE INDEX IF NOT EXISTS idx_shelters_platform ON shelters(website_platform);
CREATE INDEX IF NOT EXISTS idx_shelters_scrape_status ON shelters(scrape_status);
CREATE INDEX IF NOT EXISTS idx_shelters_next_scrape ON shelters(next_scrape_at) WHERE next_scrape_at IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_shelters_last_scraped ON shelters(last_scraped_at);
CREATE INDEX IF NOT EXISTS idx_shelters_state_scrape ON shelters(state_code, scrape_status);
