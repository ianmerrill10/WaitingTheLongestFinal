-- ============================================================================
-- Migration: 009_foster_placements.sql
-- Purpose: Create foster_placements table for tracking dog foster care assignments
-- Author: Agent 2 - WaitingTheLongest Build System
-- Created: 2024-01-28
--
-- DEPENDENCIES:
--   001_extensions.sql (uuid-ossp)
--   003_shelters.sql (shelters table for FK)
--   004_dogs.sql (dogs table for FK)
--   008_foster_homes.sql (foster_homes table for FK)
--
-- PLAN REFERENCE: Foster-First Pathway from zany-munching-waffle.md
-- Tracks foster placements and outcomes for the 14x adoption rate improvement
--
-- ROLLBACK: DROP TABLE IF EXISTS foster_placements CASCADE;
-- ============================================================================

-- ============================================================================
-- FOSTER_PLACEMENTS TABLE
-- Purpose: Track individual foster care placements of dogs with foster homes
-- Critical for:
--   1. Measuring foster-to-adoption conversion rates
--   2. Managing active foster relationships
--   3. Tracking support provided to fosters
--   4. Analyzing which foster matches work best
-- ============================================================================
CREATE TABLE foster_placements (
    -- ========================================================================
    -- PRIMARY IDENTIFIER
    -- ========================================================================
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- ========================================================================
    -- RELATIONSHIPS
    -- ========================================================================
    foster_home_id UUID NOT NULL REFERENCES foster_homes(id) ON DELETE RESTRICT,
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE RESTRICT,
    shelter_id UUID NOT NULL REFERENCES shelters(id) ON DELETE RESTRICT,

    -- ========================================================================
    -- TIMELINE
    -- ========================================================================
    -- Placement dates
    start_date DATE NOT NULL,
    expected_end_date DATE,                          -- Originally planned end
    actual_end_date DATE,                            -- When it actually ended

    -- Request/approval tracking
    requested_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    approved_at TIMESTAMP WITH TIME ZONE,
    approved_by UUID,                                -- User ID who approved

    -- ========================================================================
    -- STATUS
    -- ========================================================================
    status VARCHAR(30) NOT NULL DEFAULT 'pending',   -- See constraint for values

    -- If ended early, why?
    end_reason VARCHAR(50),                          -- See constraint for values
    end_notes TEXT,

    -- ========================================================================
    -- OUTCOME TRACKING
    -- ========================================================================
    outcome VARCHAR(50),                             -- Final outcome: adopted_by_foster, adopted_other, returned, transferred, deceased

    -- Foster-to-adopt conversion ("foster fail" - the best kind of fail)
    foster_adopted BOOLEAN DEFAULT FALSE,            -- Did foster adopt the dog?
    adoption_date DATE,                              -- If adopted, when?

    -- If adopted by someone else
    new_adopter_found_by_foster BOOLEAN DEFAULT FALSE,  -- Did foster help find adopter?

    -- ========================================================================
    -- SUPPORT PROVIDED
    -- ========================================================================
    -- Supplies
    supplies_provided BOOLEAN DEFAULT FALSE,
    supplies_list TEXT[],                            -- What was provided
    supplies_value DECIMAL(8, 2),                    -- Estimated value

    -- Veterinary
    vet_visits INTEGER DEFAULT 0,
    vet_expenses_covered DECIMAL(8, 2) DEFAULT 0,
    vet_notes TEXT,

    -- Training support
    training_support_provided BOOLEAN DEFAULT FALSE,
    training_type VARCHAR(50),                       -- basic, behavioral, medical
    training_notes TEXT,

    -- Financial support
    monthly_stipend DECIMAL(8, 2) DEFAULT 0,         -- If any monthly payment
    total_support_provided DECIMAL(8, 2) DEFAULT 0,

    -- ========================================================================
    -- COMMUNICATION
    -- ========================================================================
    -- Check-ins
    checkin_count INTEGER DEFAULT 0,
    last_checkin_date DATE,
    next_checkin_date DATE,
    checkin_notes TEXT,

    -- Issues/concerns
    issues_reported TEXT[],                          -- Array of reported issues
    has_open_issues BOOLEAN DEFAULT FALSE,

    -- ========================================================================
    -- FOSTER FEEDBACK
    -- ========================================================================
    foster_rating INTEGER,                           -- Foster's rating of experience (1-5)
    foster_feedback TEXT,                            -- Foster's comments
    would_foster_again BOOLEAN,                      -- Would they foster another dog?
    feedback_submitted_at TIMESTAMP WITH TIME ZONE,

    -- ========================================================================
    -- SHELTER FEEDBACK
    -- ========================================================================
    shelter_rating INTEGER,                          -- Shelter's rating of foster (1-5)
    shelter_feedback TEXT,
    shelter_feedback_by UUID,                        -- User ID who submitted
    shelter_feedback_at TIMESTAMP WITH TIME ZONE,

    -- ========================================================================
    -- DOG PROGRESS TRACKING
    -- ========================================================================
    -- Behavior improvements
    behavior_improvement_notes TEXT,
    training_progress_notes TEXT,

    -- Socialization
    socialization_progress TEXT,
    good_with_kids_updated BOOLEAN,                  -- Discovered in foster
    good_with_dogs_updated BOOLEAN,
    good_with_cats_updated BOOLEAN,

    -- Medical
    medical_progress_notes TEXT,
    weight_at_start DECIMAL(5, 1),
    weight_at_end DECIMAL(5, 1),

    -- ========================================================================
    -- MATCHING DATA (for improving future matches)
    -- ========================================================================
    match_score INTEGER,                             -- Predicted match quality (0-100)
    match_factors JSONB,                             -- What factors influenced the match

    -- ========================================================================
    -- TIMESTAMPS
    -- ========================================================================
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- TABLE DOCUMENTATION
-- ============================================================================
COMMENT ON TABLE foster_placements IS 'Individual foster care placements tracking dogs in foster homes. Critical for measuring foster-to-adoption rates and optimizing foster matching.';

-- Primary
COMMENT ON COLUMN foster_placements.id IS 'UUID primary key - auto-generated';
COMMENT ON COLUMN foster_placements.foster_home_id IS 'Foreign key to foster_homes table';
COMMENT ON COLUMN foster_placements.dog_id IS 'Foreign key to dogs table';
COMMENT ON COLUMN foster_placements.shelter_id IS 'Foreign key to shelters table - originating shelter';

-- Timeline
COMMENT ON COLUMN foster_placements.start_date IS 'Date foster placement began';
COMMENT ON COLUMN foster_placements.expected_end_date IS 'Originally planned end date';
COMMENT ON COLUMN foster_placements.actual_end_date IS 'Actual end date (NULL if ongoing)';
COMMENT ON COLUMN foster_placements.requested_at IS 'When foster request was submitted';
COMMENT ON COLUMN foster_placements.approved_at IS 'When request was approved';
COMMENT ON COLUMN foster_placements.approved_by IS 'User ID who approved the placement';

-- Status
COMMENT ON COLUMN foster_placements.status IS 'Current status: pending, approved, active, completed, cancelled, on_hold';
COMMENT ON COLUMN foster_placements.end_reason IS 'If ended, why: completed, adopted_by_foster, adopted_other, returned_to_shelter, transferred, foster_unable, dog_deceased';
COMMENT ON COLUMN foster_placements.end_notes IS 'Additional notes about end of placement';

-- Outcome
COMMENT ON COLUMN foster_placements.outcome IS 'Final outcome: adopted_by_foster, adopted_other, returned, transferred, deceased, ongoing';
COMMENT ON COLUMN foster_placements.foster_adopted IS 'TRUE if foster adopted the dog ("foster fail")';
COMMENT ON COLUMN foster_placements.adoption_date IS 'If adopted, the adoption date';
COMMENT ON COLUMN foster_placements.new_adopter_found_by_foster IS 'TRUE if foster helped find a different adopter';

-- Support
COMMENT ON COLUMN foster_placements.supplies_provided IS 'TRUE if supplies were provided';
COMMENT ON COLUMN foster_placements.supplies_list IS 'List of supplies provided';
COMMENT ON COLUMN foster_placements.supplies_value IS 'Estimated value of supplies';
COMMENT ON COLUMN foster_placements.vet_visits IS 'Number of vet visits during foster';
COMMENT ON COLUMN foster_placements.vet_expenses_covered IS 'Vet expenses paid by shelter/rescue';
COMMENT ON COLUMN foster_placements.vet_notes IS 'Notes about veterinary care';
COMMENT ON COLUMN foster_placements.training_support_provided IS 'TRUE if training support was provided';
COMMENT ON COLUMN foster_placements.training_type IS 'Type of training: basic, behavioral, medical';
COMMENT ON COLUMN foster_placements.training_notes IS 'Notes about training';
COMMENT ON COLUMN foster_placements.monthly_stipend IS 'Monthly stipend amount if any';
COMMENT ON COLUMN foster_placements.total_support_provided IS 'Total monetary support provided';

-- Communication
COMMENT ON COLUMN foster_placements.checkin_count IS 'Number of check-ins completed';
COMMENT ON COLUMN foster_placements.last_checkin_date IS 'Date of last check-in';
COMMENT ON COLUMN foster_placements.next_checkin_date IS 'Scheduled next check-in date';
COMMENT ON COLUMN foster_placements.checkin_notes IS 'Notes from check-ins';
COMMENT ON COLUMN foster_placements.issues_reported IS 'Array of issues reported during foster';
COMMENT ON COLUMN foster_placements.has_open_issues IS 'TRUE if there are unresolved issues';

-- Foster feedback
COMMENT ON COLUMN foster_placements.foster_rating IS 'Foster rating of experience (1-5)';
COMMENT ON COLUMN foster_placements.foster_feedback IS 'Foster comments about experience';
COMMENT ON COLUMN foster_placements.would_foster_again IS 'Would foster another dog';
COMMENT ON COLUMN foster_placements.feedback_submitted_at IS 'When foster submitted feedback';

-- Shelter feedback
COMMENT ON COLUMN foster_placements.shelter_rating IS 'Shelter rating of foster (1-5)';
COMMENT ON COLUMN foster_placements.shelter_feedback IS 'Shelter comments about foster';
COMMENT ON COLUMN foster_placements.shelter_feedback_by IS 'User who submitted shelter feedback';
COMMENT ON COLUMN foster_placements.shelter_feedback_at IS 'When shelter submitted feedback';

-- Progress
COMMENT ON COLUMN foster_placements.behavior_improvement_notes IS 'Notes on behavior improvements';
COMMENT ON COLUMN foster_placements.training_progress_notes IS 'Notes on training progress';
COMMENT ON COLUMN foster_placements.socialization_progress IS 'Notes on socialization progress';
COMMENT ON COLUMN foster_placements.good_with_kids_updated IS 'Updated info about kids compatibility';
COMMENT ON COLUMN foster_placements.good_with_dogs_updated IS 'Updated info about dog compatibility';
COMMENT ON COLUMN foster_placements.good_with_cats_updated IS 'Updated info about cat compatibility';
COMMENT ON COLUMN foster_placements.medical_progress_notes IS 'Notes on medical progress';
COMMENT ON COLUMN foster_placements.weight_at_start IS 'Dog weight at start of foster';
COMMENT ON COLUMN foster_placements.weight_at_end IS 'Dog weight at end of foster';

-- Matching
COMMENT ON COLUMN foster_placements.match_score IS 'Predicted match quality score (0-100)';
COMMENT ON COLUMN foster_placements.match_factors IS 'JSONB of factors that influenced the match';

-- Timestamps
COMMENT ON COLUMN foster_placements.created_at IS 'Record creation timestamp';
COMMENT ON COLUMN foster_placements.updated_at IS 'Last update timestamp';

-- ============================================================================
-- INDEXES
-- ============================================================================
-- Relationship lookups
CREATE INDEX idx_foster_placements_foster_home ON foster_placements(foster_home_id);
CREATE INDEX idx_foster_placements_dog ON foster_placements(dog_id);
CREATE INDEX idx_foster_placements_shelter ON foster_placements(shelter_id);

-- Status filtering
CREATE INDEX idx_foster_placements_status ON foster_placements(status);

-- Active placements
CREATE INDEX idx_foster_placements_active ON foster_placements(foster_home_id, status)
    WHERE status = 'active';

-- Outcome tracking
CREATE INDEX idx_foster_placements_outcome ON foster_placements(outcome)
    WHERE outcome IS NOT NULL;
CREATE INDEX idx_foster_placements_adopted ON foster_placements(foster_adopted)
    WHERE foster_adopted = TRUE;

-- Date ranges
CREATE INDEX idx_foster_placements_start_date ON foster_placements(start_date);
CREATE INDEX idx_foster_placements_end_date ON foster_placements(actual_end_date)
    WHERE actual_end_date IS NOT NULL;

-- Check-in due
CREATE INDEX idx_foster_placements_checkin_due ON foster_placements(next_checkin_date)
    WHERE status = 'active' AND next_checkin_date IS NOT NULL;

-- Open issues
CREATE INDEX idx_foster_placements_has_issues ON foster_placements(has_open_issues)
    WHERE has_open_issues = TRUE;

-- ============================================================================
-- TRIGGER: Auto-update updated_at timestamp
-- ============================================================================
CREATE OR REPLACE FUNCTION update_foster_placements_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_foster_placements_updated_at
    BEFORE UPDATE ON foster_placements
    FOR EACH ROW
    EXECUTE FUNCTION update_foster_placements_updated_at();

-- ============================================================================
-- TRIGGER: Update foster home statistics when placement changes
-- ============================================================================
CREATE OR REPLACE FUNCTION update_foster_home_stats()
RETURNS TRIGGER AS $$
BEGIN
    -- When placement becomes active
    IF NEW.status = 'active' AND (OLD.status IS NULL OR OLD.status != 'active') THEN
        UPDATE foster_homes
        SET current_dogs = current_dogs + 1
        WHERE id = NEW.foster_home_id;
    END IF;

    -- When placement ends
    IF NEW.status = 'completed' AND OLD.status = 'active' THEN
        UPDATE foster_homes
        SET current_dogs = GREATEST(0, current_dogs - 1),
            fosters_completed = fosters_completed + 1,
            total_foster_days = total_foster_days + COALESCE(NEW.actual_end_date - NEW.start_date, 0)
        WHERE id = NEW.foster_home_id;

        -- Update average
        UPDATE foster_homes
        SET average_foster_days = total_foster_days::DECIMAL / NULLIF(fosters_completed, 0)
        WHERE id = NEW.foster_home_id;

        -- Track foster-to-adopt conversions
        IF NEW.foster_adopted = TRUE THEN
            UPDATE foster_homes
            SET fosters_converted_to_adoption = fosters_converted_to_adoption + 1
            WHERE id = NEW.foster_home_id;
        END IF;
    END IF;

    -- When placement is cancelled/returned
    IF NEW.status IN ('cancelled', 'returned') AND OLD.status = 'active' THEN
        UPDATE foster_homes
        SET current_dogs = GREATEST(0, current_dogs - 1)
        WHERE id = NEW.foster_home_id;

        IF NEW.status = 'returned' THEN
            UPDATE foster_homes
            SET fosters_returned = fosters_returned + 1
            WHERE id = NEW.foster_home_id;
        END IF;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_foster_placements_update_stats
    AFTER UPDATE OF status, foster_adopted ON foster_placements
    FOR EACH ROW
    EXECUTE FUNCTION update_foster_home_stats();

-- ============================================================================
-- TRIGGER: Update dog status when foster placement changes
-- ============================================================================
CREATE OR REPLACE FUNCTION update_dog_on_foster_change()
RETURNS TRIGGER AS $$
BEGIN
    -- When placement becomes active, update dog status
    IF NEW.status = 'active' AND (OLD.status IS NULL OR OLD.status != 'active') THEN
        UPDATE dogs
        SET status = 'foster',
            is_available = FALSE
        WHERE id = NEW.dog_id;
    END IF;

    -- When placement ends and dog returned to shelter
    IF NEW.status = 'completed' AND NEW.outcome = 'returned' THEN
        UPDATE dogs
        SET status = 'available',
            is_available = TRUE
        WHERE id = NEW.dog_id;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_foster_placements_update_dog
    AFTER INSERT OR UPDATE OF status, outcome ON foster_placements
    FOR EACH ROW
    EXECUTE FUNCTION update_dog_on_foster_change();

-- ============================================================================
-- CONSTRAINTS
-- ============================================================================
-- Valid status values
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_status_valid
    CHECK (status IN ('pending', 'approved', 'active', 'completed', 'cancelled', 'on_hold'));

-- Valid end reason values
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_end_reason_valid
    CHECK (end_reason IS NULL OR end_reason IN (
        'completed', 'adopted_by_foster', 'adopted_other', 'returned_to_shelter',
        'transferred', 'foster_unable', 'dog_deceased', 'foster_moved', 'medical_emergency'
    ));

-- Valid outcome values
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_outcome_valid
    CHECK (outcome IS NULL OR outcome IN (
        'adopted_by_foster', 'adopted_other', 'returned', 'transferred', 'deceased', 'ongoing'
    ));

-- Valid training types
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_training_type_valid
    CHECK (training_type IS NULL OR training_type IN ('basic', 'behavioral', 'medical', 'specialized'));

-- Valid rating ranges
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_foster_rating_valid
    CHECK (foster_rating IS NULL OR (foster_rating >= 1 AND foster_rating <= 5));
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_shelter_rating_valid
    CHECK (shelter_rating IS NULL OR (shelter_rating >= 1 AND shelter_rating <= 5));

-- Match score range
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_match_score_valid
    CHECK (match_score IS NULL OR (match_score >= 0 AND match_score <= 100));

-- Non-negative values
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_vet_visits_valid
    CHECK (vet_visits >= 0);
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_checkin_count_valid
    CHECK (checkin_count >= 0);

-- Dates logic
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_dates_valid
    CHECK (expected_end_date IS NULL OR expected_end_date >= start_date);
ALTER TABLE foster_placements ADD CONSTRAINT foster_placements_actual_date_valid
    CHECK (actual_end_date IS NULL OR actual_end_date >= start_date);
