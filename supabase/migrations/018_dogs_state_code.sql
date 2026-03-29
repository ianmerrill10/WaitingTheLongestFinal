-- Add state_code column to dogs table for direct state filtering
-- Previously dogs only linked to states via shelter_id → shelters.state_code
-- which caused state pages to show 0 dogs (column didn't exist on dogs table)

ALTER TABLE dogs ADD COLUMN IF NOT EXISTS state_code text;

-- Populate state_code from the shelter's state_code
UPDATE dogs
SET state_code = s.state_code
FROM shelters s
WHERE dogs.shelter_id = s.id
  AND dogs.state_code IS NULL
  AND s.state_code IS NOT NULL;

-- Create index for fast state-based queries
CREATE INDEX IF NOT EXISTS idx_dogs_state_code ON dogs(state_code);

-- Create composite index for the common query pattern: available dogs in a state
CREATE INDEX IF NOT EXISTS idx_dogs_state_available ON dogs(state_code, is_available);
