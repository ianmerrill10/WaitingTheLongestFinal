-- Favorites table (links Supabase Auth users to dogs)
CREATE TABLE favorites (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL,
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(user_id, dog_id)
);

CREATE INDEX idx_favorites_user ON favorites(user_id);
CREATE INDEX idx_favorites_dog ON favorites(dog_id);

-- Euthanasia reports from AI agent
CREATE TABLE euthanasia_reports (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
    shelter_id UUID REFERENCES shelters(id) ON DELETE SET NULL,
    source VARCHAR(50) NOT NULL,
    source_url VARCHAR(500),
    raw_text TEXT,
    parsed_dog_name VARCHAR(100),
    parsed_breed VARCHAR(100),
    parsed_euthanasia_date TIMESTAMPTZ,
    parsed_shelter_name VARCHAR(200),
    confidence DECIMAL(3, 2) NOT NULL DEFAULT 0.0,
    status VARCHAR(20) NOT NULL DEFAULT 'pending',
    reviewed_by UUID,
    reviewed_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

ALTER TABLE euthanasia_reports ADD CONSTRAINT euth_reports_status_valid
    CHECK (status IN ('pending', 'approved', 'rejected', 'auto_applied'));

CREATE INDEX idx_euth_reports_status ON euthanasia_reports(status);
CREATE INDEX idx_euth_reports_dog ON euthanasia_reports(dog_id) WHERE dog_id IS NOT NULL;

-- Rescue notifications log
CREATE TABLE rescue_notifications (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID NOT NULL REFERENCES dogs(id) ON DELETE CASCADE,
    rescue_org_id UUID NOT NULL REFERENCES shelters(id) ON DELETE CASCADE,
    notification_type VARCHAR(20) NOT NULL DEFAULT 'email',
    status VARCHAR(20) NOT NULL DEFAULT 'sent',
    response VARCHAR(20),
    sent_at TIMESTAMPTZ DEFAULT NOW(),
    responded_at TIMESTAMPTZ
);

CREATE INDEX idx_rescue_notif_dog ON rescue_notifications(dog_id);

-- Success stories
CREATE TABLE success_stories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id) ON DELETE SET NULL,
    user_id UUID,
    dog_name VARCHAR(100) NOT NULL,
    title VARCHAR(200) NOT NULL,
    story TEXT NOT NULL,
    photo_urls TEXT[] DEFAULT '{}',
    is_approved BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX idx_stories_approved ON success_stories(is_approved) WHERE is_approved = TRUE;

COMMENT ON TABLE favorites IS 'User favorited dogs';
COMMENT ON TABLE euthanasia_reports IS 'AI agent euthanasia findings with confidence scores';
COMMENT ON TABLE rescue_notifications IS 'Auto-rescue notification log';
COMMENT ON TABLE success_stories IS 'Adopted dog success stories from community';
