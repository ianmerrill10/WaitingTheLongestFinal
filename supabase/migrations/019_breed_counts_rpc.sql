-- RPC function to get breed counts for available dogs
-- Replaces fetching all 57K+ dog rows into memory
CREATE OR REPLACE FUNCTION get_breed_counts()
RETURNS TABLE(breed text, count bigint)
LANGUAGE sql STABLE
AS $$
  SELECT breed_primary AS breed, COUNT(*) AS count
  FROM dogs
  WHERE is_available = true AND breed_primary IS NOT NULL
  GROUP BY breed_primary
  ORDER BY count DESC;
$$;
