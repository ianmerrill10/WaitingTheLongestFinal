# WaitingTheLongest.com - Complete Build Plan

## Mission Statement

**Find good, safe, loving homes for as many shelter dogs as possible.**

This isn't just a database of dogs. It's a matchmaker, storyteller, barrier-remover, amplifier, and support system. Every feature exists to serve one goal: getting dogs out of shelters and into loving homes.

---

## Core Philosophy

| Principle | Implementation |
|-----------|----------------|
| **Emotional Connection** | LED counters, video profiles, stories - make people FEEL |
| **Smart Matching** | Don't just browse - match the RIGHT dog to the RIGHT person |
| **Remove Every Barrier** | Housing help, transport, fee sponsorship, resources |
| **Amplify Reach** | Every visitor becomes an advocate through easy sharing |
| **Prevent Returns** | Post-adoption support keeps dogs in their new homes |
| **Highlight the Overlooked** | Senior dogs, black dogs, bully breeds deserve extra love |

---

## Feature Overview

### Core Features
1. Real-time LED wait time counters (YY:MM:DD:HH:MM:SS)
2. All 50 states coverage with state-by-state pages
3. Searchable database with robust filtering
4. Agency portal for verified shelters/rescues

### Adoption-Maximizing Features
5. **Lifestyle Matching Quiz** - "Dogs that match YOUR life"
6. **Video Profiles** - 30-second videos convert 3-5x better than photos
7. **One-Click Social Sharing** - Turn every visitor into an advocate
8. **Sponsor-a-Fee Program** - Let donors pay adoption fees
9. **Post-Adoption Support** - Automated check-ins prevent returns
10. **Overlooked Angels** - Spotlight for black dogs, seniors, bully breeds
11. **Pet-Friendly Housing Directory** - Remove the "my landlord says no" barrier
12. **Transport Network** - Connect rural shelter dogs to distant adopters
13. **Adopter Resource Library** - Training guides, adjustment tips
14. **Shelter Analytics Dashboard** - Help shelters improve their listings

---

## Data Sources

| Priority | Source | Access |
|----------|--------|--------|
| **Primary** | RescueGroups.org API | Free, unlimited requests |
| **Secondary** | Best Friends Network | Partner data feed |
| **Verification** | IRS 501(c)(3) database | 40,085 organizations |
| **Breed Data** | TheDogAPI | 4 API keys available |

**IMPORTANT: There is NO Petfinder API. Do not add Petfinder as a data source. Their public API has been shut down.**

---

## Confirmed Decisions

| Decision | Choice |
|----------|--------|
| LED Counter Format | YY:MM:DD:HH:MM:SS (6 segments) |
| Agency Registration | Free (no fees or deposits) |
| Adoption Flow | Redirect to shelter's website |
| Primary Language | C++ (Drogon framework) |
| Frontend Exception | HTML/CSS/JS for browser-side only |

---

## Existing Data Assets

| Asset | Records | Location |
|-------|---------|----------|
| IRS Animal Organizations | 40,085 | `irs_animal_orgs.json` |
| Master Shelter Database | 1,170 verified | `master_shelter_database.json` |
| RescueGroups Organizations | 5,050 | `rescuegroups_orgs.json` |
| Best Friends Network | 6,012 | `bf_network_orgs.json` |
| Dog Breeds | 206+ | `dog_breeds.json` |
| State-Specific Shelters | All 50 states | `{STATE}_shelters.json` |
| Complete Shelter CSV | 40,060 addresses | `US_Animal_Shelters_Complete_List.csv` |

**Working API Keys:**
- TheDogAPI: 4 keys for rate distribution
- Google OAuth: Configured for authentication
- Amazon Associates: `waitingthelon-20` (active)

**Domains Owned:**
- waitingthelongest.com
- waitedthelongest.com

---

## Technology Stack

### Core (C++)
| Component | Technology | Justification |
|-----------|------------|---------------|
| Web Framework | **Drogon** (C++17) | High-performance, native WebSocket, HTTP/2 |
| Database | **PostgreSQL 16** + libpqxx | Proven, RAII connection pool |
| Cache | **Redis** | Sessions, API caching, rate limiting |
| Build System | **CMake 3.16+** | Standard C++ tooling |
| Image Processing | **libvips** | Fast image resizing for share cards |
| Email | **libcurl** + SendGrid API | Post-adoption sequences |

### Required Non-C++ (Browser-Side Only)

| Component | Language | Why |
|-----------|----------|-----|
| Frontend UI | HTML/CSS/JS | Browsers only execute JavaScript |
| LED Counter Animation | CSS/JS | Real-time DOM manipulation |
| WebSocket Client | JavaScript | Browser WebSocket API |
| Interactive Map | SVG/JS | Click handlers, hover effects |
| Video Player | HTML5/JS | Native browser video |
| Social Share Buttons | JS | Platform APIs require JavaScript |

**Note:** Server-side is 100% C++. Only browser rendering requires HTML/CSS/JS.

---

## Project Structure

```
WaitingTheLongest/
├── CMakeLists.txt
├── config.json
├── README.md
│
├── src/
│   ├── main.cc
│   │
│   ├── controllers/
│   │   ├── api/
│   │   │   ├── DogsController.cc
│   │   │   ├── SheltersController.cc
│   │   │   ├── StatesController.cc
│   │   │   ├── SearchController.cc
│   │   │   ├── MatchingController.cc      # Lifestyle quiz & matching
│   │   │   ├── SponsorController.cc       # Fee sponsorship
│   │   │   ├── HousingController.cc       # Pet-friendly housing
│   │   │   ├── TransportController.cc     # Transport requests
│   │   │   ├── ResourcesController.cc     # Adopter resources
│   │   │   └── ShareController.cc         # Social sharing & image gen
│   │   │
│   │   ├── auth/
│   │   │   ├── AuthController.cc
│   │   │   ├── AgencyPortalController.cc
│   │   │   └── AdopterAccountController.cc # Adopter accounts
│   │   │
│   │   ├── websocket/
│   │   │   └── WaitTimeWebSocket.cc
│   │   │
│   │   └── admin/
│   │       ├── AnalyticsController.cc     # Shelter analytics
│   │       └── ModerationController.cc
│   │
│   ├── services/
│   │   ├── WaitTimeCalculator.cc
│   │   ├── DogService.cc
│   │   ├── SearchService.cc
│   │   ├── AuthService.cc
│   │   ├── VerificationService.cc
│   │   ├── MatchingService.cc             # Lifestyle matching algorithm
│   │   ├── SponsorshipService.cc          # Fee sponsorship logic
│   │   ├── ShareImageGenerator.cc         # Generate shareable images
│   │   ├── EmailService.cc                # Post-adoption sequences
│   │   ├── TransportService.cc            # Transport coordination
│   │   ├── AnalyticsService.cc            # Shelter performance metrics
│   │   └── OverlookedService.cc           # Identify overlooked dogs
│   │
│   ├── aggregators/
│   │   ├── RescueGroupsAggregator.cc
│   │   ├── BestFriendsClient.cc
│   │   └── AggregatorManager.cc
│   │
│   ├── workers/
│   │   ├── ApiSyncWorker.cc
│   │   ├── WaitTimeUpdateWorker.cc
│   │   ├── EmailSequenceWorker.cc         # Send scheduled emails
│   │   ├── AnalyticsWorker.cc             # Calculate daily metrics
│   │   └── OverlookedWorker.cc            # Flag overlooked dogs
│   │
│   ├── db/
│   │   └── ConnectionPool.cc
│   │
│   └── utils/
│       ├── TimeUtils.cc
│       ├── CryptoUtils.cc
│       ├── JsonUtils.cc
│       └── ImageUtils.cc                  # Image processing helpers
│
├── static/
│   ├── css/
│   │   ├── main.css
│   │   ├── led-counter.css
│   │   ├── quiz.css                       # Matching quiz styles
│   │   └── share-card.css                 # Shareable card styles
│   │
│   ├── js/
│   │   ├── websocket-client.js
│   │   ├── led-counter.js
│   │   ├── search.js
│   │   ├── matching-quiz.js               # Quiz logic
│   │   ├── video-player.js                # Video profile player
│   │   ├── social-share.js                # Share functionality
│   │   └── housing-search.js              # Housing directory
│   │
│   ├── images/
│   │   ├── share-templates/               # Share card backgrounds
│   │   └── badges/                        # Overlooked angels badges
│   │
│   └── videos/                            # Sample/promo videos
│
├── sql/
│   ├── 001_core_schema.sql
│   ├── 002_matching_schema.sql            # Quiz & matching tables
│   ├── 003_sponsor_schema.sql             # Sponsorship tables
│   ├── 004_housing_schema.sql             # Housing directory
│   ├── 005_transport_schema.sql           # Transport network
│   ├── 006_adoption_support_schema.sql    # Post-adoption tracking
│   ├── 007_analytics_schema.sql           # Metrics tables
│   ├── 008_indexes.sql
│   ├── 009_seed_states.sql
│   └── 010_import_existing_data.sql
│
├── email_templates/
│   ├── welcome.html
│   ├── post_adoption_day1.html
│   ├── post_adoption_week1.html
│   ├── post_adoption_month1.html
│   ├── post_adoption_month3.html
│   ├── post_adoption_year1.html
│   ├── sponsor_thank_you.html
│   └── transport_confirmed.html
│
├── data/
│   ├── master_shelter_database.json
│   ├── dog_breeds.json
│   ├── irs_animal_orgs.json
│   ├── collected_shelters/
│   └── housing/                           # Pet-friendly housing data
│
├── tests/
├── docs/
└── docker/
```

---

## Database Schema

### Core Tables

```sql
-- ============================================
-- CORE TABLES
-- ============================================

CREATE TABLE states (
    id SERIAL PRIMARY KEY,
    code CHAR(2) NOT NULL UNIQUE,
    name VARCHAR(50) NOT NULL,
    total_dogs INTEGER DEFAULT 0,
    avg_wait_days DECIMAL(10,2) DEFAULT 0,
    is_active BOOLEAN DEFAULT FALSE,
    activated_at TIMESTAMP
);

CREATE TABLE shelters (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    external_id VARCHAR(100),
    source VARCHAR(50) NOT NULL,
    name VARCHAR(255) NOT NULL,
    organization_type VARCHAR(50),
    ein_number VARCHAR(20),
    email VARCHAR(255),
    phone VARCHAR(20),
    website VARCHAR(500),
    address VARCHAR(255),
    city VARCHAR(100),
    state_id INTEGER REFERENCES states(id),
    zip_code VARCHAR(10),
    latitude DECIMAL(10,8),
    longitude DECIMAL(11,8),
    verification_status VARCHAR(20) DEFAULT 'pending',
    is_active BOOLEAN DEFAULT TRUE,

    -- Analytics fields
    total_adoptions INTEGER DEFAULT 0,
    avg_days_to_adoption DECIMAL(10,2),
    profile_views INTEGER DEFAULT 0,

    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE dogs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    external_id VARCHAR(100),
    source VARCHAR(50) NOT NULL,
    shelter_id UUID REFERENCES shelters(id),
    name VARCHAR(100) NOT NULL,
    breed_primary VARCHAR(100),
    breed_secondary VARCHAR(100),
    breed_mixed BOOLEAN DEFAULT FALSE,
    age_category VARCHAR(20),
    age_months INTEGER,
    size VARCHAR(20),
    gender VARCHAR(10),
    color_primary VARCHAR(50),
    coat_length VARCHAR(20),
    weight_lbs DECIMAL(5,1),
    status VARCHAR(20) DEFAULT 'adoptable',

    -- Wait time tracking
    intake_date DATE NOT NULL,
    wait_start_date TIMESTAMP NOT NULL,

    -- Compatibility (critical for matching)
    good_with_children BOOLEAN,
    good_with_dogs BOOLEAN,
    good_with_cats BOOLEAN,
    energy_level VARCHAR(20),           -- 'low', 'medium', 'high'
    exercise_needs VARCHAR(20),         -- 'minimal', 'moderate', 'high'
    apartment_suitable BOOLEAN,
    house_trained BOOLEAN,
    crate_trained BOOLEAN,
    leash_trained BOOLEAN,
    special_needs BOOLEAN DEFAULT FALSE,
    special_needs_description TEXT,

    -- Media
    photo_urls TEXT[],
    primary_photo_url VARCHAR(500),
    video_url VARCHAR(500),             -- Video profile URL
    description TEXT,
    story TEXT,                         -- Emotional backstory
    adoption_url VARCHAR(500),
    adoption_fee DECIMAL(10,2),

    -- Overlooked flags
    is_overlooked BOOLEAN DEFAULT FALSE,
    overlooked_reason VARCHAR(50),      -- 'black', 'senior', 'bully_breed', 'bonded_pair', 'special_needs'
    is_featured BOOLEAN DEFAULT FALSE,

    -- Fee sponsorship
    fee_sponsored BOOLEAN DEFAULT FALSE,
    sponsor_id UUID,

    -- Analytics
    profile_views INTEGER DEFAULT 0,
    share_count INTEGER DEFAULT 0,

    last_sync_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE dog_breeds (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    breed_group VARCHAR(50),
    height_imperial VARCHAR(20),
    weight_imperial VARCHAR(20),
    life_span VARCHAR(30),
    temperament TEXT,
    energy_level VARCHAR(20),
    exercise_needs VARCHAR(20),
    grooming_needs VARCHAR(20),
    good_with_kids BOOLEAN,
    good_with_dogs BOOLEAN,
    good_with_cats BOOLEAN,
    apartment_friendly BOOLEAN,
    first_time_owner_friendly BOOLEAN,
    image_url VARCHAR(500),
    description TEXT
);
```

### User & Adopter Tables

```sql
-- ============================================
-- USER TABLES
-- ============================================

CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255),
    oauth_provider VARCHAR(50),         -- 'google', 'facebook'
    oauth_id VARCHAR(255),

    first_name VARCHAR(100),
    last_name VARCHAR(100),
    phone VARCHAR(20),

    -- Role: 'adopter', 'agency', 'admin', 'superadmin'
    role VARCHAR(20) DEFAULT 'adopter',
    shelter_id UUID REFERENCES shelters(id),

    -- Adopter-specific
    zip_code VARCHAR(10),
    has_adopted BOOLEAN DEFAULT FALSE,
    adoption_date DATE,
    adopted_dog_id UUID,

    is_active BOOLEAN DEFAULT TRUE,
    email_verified BOOLEAN DEFAULT FALSE,

    -- Preferences
    email_notifications BOOLEAN DEFAULT TRUE,

    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Saved searches for alerts
CREATE TABLE saved_searches (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),
    name VARCHAR(100),

    -- Search criteria
    state_codes TEXT[],
    breed VARCHAR(100),
    age_category VARCHAR(20),
    size VARCHAR(20),
    gender VARCHAR(10),
    good_with_children BOOLEAN,
    good_with_dogs BOOLEAN,
    good_with_cats BOOLEAN,

    -- Notification settings
    notify_email BOOLEAN DEFAULT TRUE,
    notify_frequency VARCHAR(20) DEFAULT 'daily', -- 'instant', 'daily', 'weekly'

    created_at TIMESTAMP DEFAULT NOW()
);

-- Favorited dogs
CREATE TABLE favorites (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),
    dog_id UUID REFERENCES dogs(id),
    created_at TIMESTAMP DEFAULT NOW(),
    UNIQUE(user_id, dog_id)
);
```

### Lifestyle Matching Tables

```sql
-- ============================================
-- LIFESTYLE MATCHING
-- ============================================

CREATE TABLE lifestyle_profiles (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),

    -- Living situation
    home_type VARCHAR(20),              -- 'apartment', 'condo', 'house', 'farm'
    has_yard BOOLEAN,
    yard_size VARCHAR(20),              -- 'small', 'medium', 'large'
    has_fence BOOLEAN,

    -- Household
    has_children BOOLEAN,
    children_ages TEXT[],               -- ['toddler', 'elementary', 'teen']
    has_other_dogs BOOLEAN,
    other_dogs_count INTEGER,
    has_cats BOOLEAN,
    has_other_pets BOOLEAN,
    other_pets_description TEXT,

    -- Lifestyle
    activity_level VARCHAR(20),         -- 'sedentary', 'moderate', 'active', 'very_active'
    hours_home_daily INTEGER,           -- Hours home per day
    work_from_home BOOLEAN,
    travel_frequency VARCHAR(20),       -- 'rarely', 'monthly', 'weekly'

    -- Experience
    dog_experience VARCHAR(20),         -- 'none', 'some', 'experienced'
    willing_to_train BOOLEAN,

    -- Preferences
    preferred_size TEXT[],              -- ['small', 'medium', 'large']
    preferred_age TEXT[],               -- ['puppy', 'young', 'adult', 'senior']
    preferred_energy VARCHAR(20),       -- 'low', 'medium', 'high'
    ok_with_special_needs BOOLEAN,
    max_adoption_fee DECIMAL(10,2),

    -- Housing restrictions
    has_breed_restrictions BOOLEAN,
    restricted_breeds TEXT[],
    weight_limit_lbs INTEGER,

    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Match scores (calculated and cached)
CREATE TABLE match_scores (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    profile_id UUID REFERENCES lifestyle_profiles(id),
    dog_id UUID REFERENCES dogs(id),
    score DECIMAL(5,2),                 -- 0-100 match score
    score_breakdown JSONB,              -- Detailed scoring factors
    calculated_at TIMESTAMP DEFAULT NOW(),
    UNIQUE(profile_id, dog_id)
);
```

### Sponsorship Tables

```sql
-- ============================================
-- FEE SPONSORSHIP
-- ============================================

CREATE TABLE sponsors (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),  -- NULL for anonymous

    name VARCHAR(255),
    email VARCHAR(255),
    is_anonymous BOOLEAN DEFAULT FALSE,
    display_name VARCHAR(255),          -- What to show: "Sarah M." or "Anonymous"

    -- Totals
    total_sponsored DECIMAL(10,2) DEFAULT 0,
    dogs_sponsored INTEGER DEFAULT 0,

    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE sponsorships (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    sponsor_id UUID REFERENCES sponsors(id),
    dog_id UUID REFERENCES dogs(id),

    amount DECIMAL(10,2) NOT NULL,
    covers_full_fee BOOLEAN,

    -- Payment
    payment_status VARCHAR(20),         -- 'pending', 'completed', 'refunded'
    payment_id VARCHAR(255),            -- Stripe/PayPal transaction ID

    -- Status
    status VARCHAR(20) DEFAULT 'active', -- 'active', 'used', 'expired', 'refunded'
    used_at TIMESTAMP,
    adopter_id UUID REFERENCES users(id),

    message TEXT,                       -- Optional message from sponsor

    created_at TIMESTAMP DEFAULT NOW()
);

-- Corporate sponsors
CREATE TABLE corporate_sponsors (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    company_name VARCHAR(255) NOT NULL,
    logo_url VARCHAR(500),
    website VARCHAR(500),
    contact_email VARCHAR(255),

    monthly_budget DECIMAL(10,2),
    dogs_per_month INTEGER,
    states TEXT[],                      -- States they sponsor in, or NULL for all

    total_sponsored DECIMAL(10,2) DEFAULT 0,
    total_dogs INTEGER DEFAULT 0,

    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT NOW()
);
```

### Housing Directory Tables

```sql
-- ============================================
-- PET-FRIENDLY HOUSING
-- ============================================

CREATE TABLE pet_friendly_housing (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    -- Property info
    name VARCHAR(255) NOT NULL,
    property_type VARCHAR(50),          -- 'apartment', 'condo', 'house', 'townhouse'
    management_company VARCHAR(255),

    -- Location
    address VARCHAR(255),
    city VARCHAR(100) NOT NULL,
    state_id INTEGER REFERENCES states(id),
    zip_code VARCHAR(10),
    latitude DECIMAL(10,8),
    longitude DECIMAL(11,8),

    -- Contact
    phone VARCHAR(20),
    email VARCHAR(255),
    website VARCHAR(500),

    -- Pet policy
    dogs_allowed BOOLEAN DEFAULT TRUE,
    cats_allowed BOOLEAN,
    max_pets INTEGER,
    weight_limit_lbs INTEGER,
    breed_restrictions TEXT[],
    pet_deposit DECIMAL(10,2),
    monthly_pet_rent DECIMAL(10,2),

    -- Details
    pet_amenities TEXT[],               -- ['dog_park', 'pet_wash', 'trails']
    notes TEXT,

    -- Verification
    verified BOOLEAN DEFAULT FALSE,
    verified_at TIMESTAMP,
    reported_count INTEGER DEFAULT 0,

    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- User-submitted housing reports
CREATE TABLE housing_submissions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),
    housing_id UUID REFERENCES pet_friendly_housing(id), -- NULL if new

    -- Submitted info
    property_name VARCHAR(255),
    city VARCHAR(100),
    state_code CHAR(2),
    dogs_allowed BOOLEAN,
    notes TEXT,

    status VARCHAR(20) DEFAULT 'pending', -- 'pending', 'approved', 'rejected'
    reviewed_at TIMESTAMP,

    created_at TIMESTAMP DEFAULT NOW()
);
```

### Transport Network Tables

```sql
-- ============================================
-- TRANSPORT NETWORK
-- ============================================

CREATE TABLE transport_volunteers (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES users(id),

    name VARCHAR(255) NOT NULL,
    email VARCHAR(255) NOT NULL,
    phone VARCHAR(20),

    -- Coverage
    home_state_id INTEGER REFERENCES states(id),
    home_zip VARCHAR(10),
    max_distance_miles INTEGER,
    willing_to_cross_state BOOLEAN,
    coverage_states INTEGER[],          -- State IDs they'll travel to

    -- Vehicle
    vehicle_type VARCHAR(50),           -- 'sedan', 'suv', 'truck', 'van'
    can_transport_large BOOLEAN,
    has_crate BOOLEAN,

    -- Availability
    available_weekdays BOOLEAN,
    available_weekends BOOLEAN,

    -- Stats
    transports_completed INTEGER DEFAULT 0,
    miles_driven INTEGER DEFAULT 0,

    is_active BOOLEAN DEFAULT TRUE,
    background_check_date DATE,

    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE transport_requests (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id),
    requester_id UUID REFERENCES users(id), -- Adopter requesting

    -- Route
    pickup_shelter_id UUID REFERENCES shelters(id),
    pickup_address TEXT,
    pickup_city VARCHAR(100),
    pickup_state_id INTEGER REFERENCES states(id),
    pickup_zip VARCHAR(10),

    destination_address TEXT,
    destination_city VARCHAR(100),
    destination_state_id INTEGER REFERENCES states(id),
    destination_zip VARCHAR(10),

    distance_miles INTEGER,

    -- Schedule
    preferred_date DATE,
    flexible_dates BOOLEAN,

    -- Status
    status VARCHAR(20) DEFAULT 'pending', -- 'pending', 'matched', 'confirmed', 'in_transit', 'completed', 'cancelled'
    volunteer_id UUID REFERENCES transport_volunteers(id),

    -- Completion
    completed_at TIMESTAMP,
    notes TEXT,

    created_at TIMESTAMP DEFAULT NOW()
);

-- For relay transports (multiple volunteers)
CREATE TABLE transport_legs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    request_id UUID REFERENCES transport_requests(id),
    volunteer_id UUID REFERENCES transport_volunteers(id),

    leg_order INTEGER,
    pickup_location TEXT,
    dropoff_location TEXT,
    distance_miles INTEGER,

    status VARCHAR(20) DEFAULT 'pending',
    completed_at TIMESTAMP,

    created_at TIMESTAMP DEFAULT NOW()
);
```

### Post-Adoption Support Tables

```sql
-- ============================================
-- POST-ADOPTION SUPPORT
-- ============================================

CREATE TABLE adoptions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id),
    adopter_id UUID REFERENCES users(id),
    shelter_id UUID REFERENCES shelters(id),

    adoption_date DATE NOT NULL,

    -- How they found the dog
    found_via VARCHAR(50),              -- 'search', 'matching', 'share', 'featured'
    was_sponsored BOOLEAN DEFAULT FALSE,
    sponsorship_id UUID REFERENCES sponsorships(id),

    -- Follow-up tracking
    last_checkin_at TIMESTAMP,
    checkin_count INTEGER DEFAULT 0,

    -- Outcome
    status VARCHAR(20) DEFAULT 'active', -- 'active', 'returned', 'rehomed'
    return_date DATE,
    return_reason TEXT,

    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE adoption_checkins (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    adoption_id UUID REFERENCES adoptions(id),

    checkin_type VARCHAR(20),           -- 'day1', 'week1', 'month1', 'month3', 'year1'

    -- Email sent
    email_sent_at TIMESTAMP,
    email_opened BOOLEAN DEFAULT FALSE,
    email_clicked BOOLEAN DEFAULT FALSE,

    -- Response (if user responds)
    responded BOOLEAN DEFAULT FALSE,
    response_date TIMESTAMP,

    -- Survey responses
    how_is_adjustment VARCHAR(20),      -- 'great', 'good', 'struggling', 'considering_return'
    needs_help BOOLEAN,
    help_topic VARCHAR(50),             -- 'behavior', 'health', 'training', 'supplies'
    comments TEXT,

    created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE success_stories (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    adoption_id UUID REFERENCES adoptions(id),
    user_id UUID REFERENCES users(id),
    dog_id UUID REFERENCES dogs(id),

    title VARCHAR(255),
    story TEXT NOT NULL,
    photo_urls TEXT[],

    -- Moderation
    status VARCHAR(20) DEFAULT 'pending', -- 'pending', 'approved', 'rejected'
    approved_at TIMESTAMP,
    featured BOOLEAN DEFAULT FALSE,

    created_at TIMESTAMP DEFAULT NOW()
);
```

### Analytics Tables

```sql
-- ============================================
-- ANALYTICS
-- ============================================

CREATE TABLE dog_analytics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id),
    date DATE NOT NULL,

    profile_views INTEGER DEFAULT 0,
    search_appearances INTEGER DEFAULT 0,
    share_clicks INTEGER DEFAULT 0,
    favorite_adds INTEGER DEFAULT 0,
    adoption_url_clicks INTEGER DEFAULT 0,

    UNIQUE(dog_id, date)
);

CREATE TABLE shelter_analytics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    shelter_id UUID REFERENCES shelters(id),
    date DATE NOT NULL,

    -- Listings
    total_dogs INTEGER,
    new_listings INTEGER DEFAULT 0,
    adopted_count INTEGER DEFAULT 0,

    -- Engagement
    profile_views INTEGER DEFAULT 0,
    adoption_url_clicks INTEGER DEFAULT 0,

    -- Performance
    avg_days_to_adoption DECIMAL(10,2),
    avg_wait_days DECIMAL(10,2),

    UNIQUE(shelter_id, date)
);

CREATE TABLE platform_analytics (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    date DATE NOT NULL UNIQUE,

    -- Totals
    total_dogs INTEGER,
    total_shelters INTEGER,
    total_users INTEGER,

    -- Activity
    new_dogs INTEGER DEFAULT 0,
    adoptions INTEGER DEFAULT 0,
    new_users INTEGER DEFAULT 0,

    -- Engagement
    searches INTEGER DEFAULT 0,
    profile_views INTEGER DEFAULT 0,
    shares INTEGER DEFAULT 0,
    quiz_completions INTEGER DEFAULT 0,

    -- Sponsorships
    sponsorships_count INTEGER DEFAULT 0,
    sponsorship_amount DECIMAL(10,2) DEFAULT 0,

    -- Transport
    transport_requests INTEGER DEFAULT 0,
    transports_completed INTEGER DEFAULT 0
);

-- Track what makes dogs get adopted
CREATE TABLE adoption_factors (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id),
    adoption_id UUID REFERENCES adoptions(id),

    -- At time of listing
    had_video BOOLEAN,
    had_story BOOLEAN,
    photo_count INTEGER,
    description_length INTEGER,
    was_featured BOOLEAN,
    was_overlooked_angels BOOLEAN,
    was_sponsored BOOLEAN,

    -- Engagement before adoption
    total_views INTEGER,
    total_shares INTEGER,
    times_favorited INTEGER,

    -- Time
    days_to_adoption INTEGER,

    created_at TIMESTAMP DEFAULT NOW()
);
```

### Sharing & Social Tables

```sql
-- ============================================
-- SOCIAL SHARING
-- ============================================

CREATE TABLE shares (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dog_id UUID REFERENCES dogs(id),
    user_id UUID REFERENCES users(id),  -- NULL for anonymous

    platform VARCHAR(20),               -- 'facebook', 'twitter', 'instagram', 'text', 'email', 'copy_link'

    -- Generated share image
    share_image_url VARCHAR(500),

    -- Tracking
    click_count INTEGER DEFAULT 0,

    created_at TIMESTAMP DEFAULT NOW()
);

-- Track clicks from shared links
CREATE TABLE share_clicks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    share_id UUID REFERENCES shares(id),

    referrer VARCHAR(500),
    user_agent TEXT,
    ip_hash VARCHAR(64),                -- Hashed for privacy

    resulted_in_adoption BOOLEAN DEFAULT FALSE,

    created_at TIMESTAMP DEFAULT NOW()
);
```

### Resource Library Tables

```sql
-- ============================================
-- ADOPTER RESOURCES
-- ============================================

CREATE TABLE resources (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    title VARCHAR(255) NOT NULL,
    slug VARCHAR(255) NOT NULL UNIQUE,
    category VARCHAR(50),               -- 'training', 'health', 'adjustment', 'supplies', 'behavior'

    summary TEXT,
    content TEXT NOT NULL,              -- Markdown content

    -- Targeting
    for_new_adopters BOOLEAN DEFAULT TRUE,
    for_dog_age TEXT[],                 -- ['puppy', 'adult', 'senior']
    for_special_needs BOOLEAN DEFAULT FALSE,

    -- Media
    featured_image VARCHAR(500),
    video_url VARCHAR(500),

    -- SEO
    meta_description TEXT,

    -- Status
    is_published BOOLEAN DEFAULT TRUE,
    view_count INTEGER DEFAULT 0,

    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE resource_views (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    resource_id UUID REFERENCES resources(id),
    user_id UUID REFERENCES users(id),

    created_at TIMESTAMP DEFAULT NOW()
);
```

### Indexes

```sql
-- ============================================
-- INDEXES
-- ============================================

-- Dogs
CREATE INDEX idx_dogs_shelter ON dogs(shelter_id);
CREATE INDEX idx_dogs_status ON dogs(status) WHERE status = 'adoptable';
CREATE INDEX idx_dogs_wait_start ON dogs(wait_start_date);
CREATE INDEX idx_dogs_breed ON dogs(breed_primary);
CREATE INDEX idx_dogs_overlooked ON dogs(is_overlooked) WHERE is_overlooked = TRUE;
CREATE INDEX idx_dogs_sponsored ON dogs(fee_sponsored) WHERE fee_sponsored = TRUE;
CREATE INDEX idx_dogs_state ON dogs(shelter_id); -- Join to shelters for state

-- Shelters
CREATE INDEX idx_shelters_state ON shelters(state_id);
CREATE INDEX idx_shelters_verification ON shelters(verification_status);
CREATE INDEX idx_shelters_location ON shelters USING gist (
    ll_to_earth(latitude, longitude)
) WHERE latitude IS NOT NULL;

-- Matching
CREATE INDEX idx_match_scores_profile ON match_scores(profile_id);
CREATE INDEX idx_match_scores_score ON match_scores(score DESC);

-- Housing
CREATE INDEX idx_housing_state ON pet_friendly_housing(state_id);
CREATE INDEX idx_housing_city ON pet_friendly_housing(city);
CREATE INDEX idx_housing_verified ON pet_friendly_housing(verified) WHERE verified = TRUE;

-- Transport
CREATE INDEX idx_transport_status ON transport_requests(status);
CREATE INDEX idx_transport_pickup_state ON transport_requests(pickup_state_id);

-- Analytics
CREATE INDEX idx_dog_analytics_date ON dog_analytics(date);
CREATE INDEX idx_shelter_analytics_date ON shelter_analytics(date);

-- Full-text search
CREATE INDEX idx_dogs_search ON dogs USING gin(
    to_tsvector('english', name || ' ' || COALESCE(breed_primary, '') || ' ' || COALESCE(description, ''))
);
```

---

## LED Wait Time Counter

### Format: YY:MM:DD:HH:MM:SS

Example: `02:03:15:08:45:32` = 2 years, 3 months, 15 days, 8 hours, 45 minutes, 32 seconds

### Implementation

1. **Server** (C++) calculates from `wait_start_date`
2. **WebSocket** pushes updates every second
3. **JavaScript** displays animated LED digits

### WebSocket Protocol

```json
// Subscribe
{"action": "subscribe", "type": "dogs", "ids": ["uuid1", "uuid2"]}

// Server pushes every second
{
    "type": "waittime",
    "dogId": "uuid1",
    "ledDisplay": "02:03:15:08:45:32",
    "totalDays": 835
}
```

### LED Styling

```css
.led-counter {
    font-family: 'Digital-7', monospace;
    background: linear-gradient(180deg, #1a1a1a 0%, #0d0d0d 100%);
    color: #ff3333;
    text-shadow: 0 0 10px #ff0000, 0 0 20px #ff0000, 0 0 30px #ff0000;
    padding: 15px 25px;
    border-radius: 8px;
    border: 2px solid #333;
}
```

---

## Lifestyle Matching Quiz

### Quiz Flow

```
┌─────────────────────────────────────────────────────┐
│ Step 1: Living Situation                            │
│ "What type of home do you live in?"                │
│ ○ Apartment  ○ Condo  ○ House  ○ Farm              │
│ "Do you have a yard?" ○ Yes  ○ No                  │
│ "Is it fenced?" ○ Yes  ○ No  ○ Partially          │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Step 2: Household                                   │
│ "Do you have children?" → Ages?                    │
│ "Do you have other dogs?" → How many?              │
│ "Do you have cats?"                                │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Step 3: Lifestyle                                   │
│ "How active are you?"                              │
│ ○ Couch potato  ○ Moderate  ○ Active  ○ Athlete   │
│ "How many hours are you home daily?"               │
│ "Do you work from home?"                           │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Step 4: Experience & Preferences                    │
│ "Have you owned a dog before?"                     │
│ "Are you willing to train a dog?"                  │
│ "Preferred size?" □ Small □ Medium □ Large        │
│ "Open to special needs dogs?"                      │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Step 5: Housing Restrictions                        │
│ "Does your landlord have breed restrictions?"      │
│ "Is there a weight limit for pets?"                │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ RESULTS: "Dogs That Match Your Life"                │
│ Sorted by match score + wait time                  │
│ Each dog shows: Match %, why they match, wait time │
└─────────────────────────────────────────────────────┘
```

### Matching Algorithm

```cpp
float calculateMatchScore(const LifestyleProfile& profile, const Dog& dog) {
    float score = 100.0f;

    // Hard disqualifiers (score = 0)
    if (profile.has_breed_restrictions &&
        isRestrictedBreed(dog.breed_primary, profile.restricted_breeds)) {
        return 0.0f;
    }
    if (profile.weight_limit_lbs > 0 && dog.weight_lbs > profile.weight_limit_lbs) {
        return 0.0f;
    }

    // Compatibility scoring
    if (profile.has_children && dog.good_with_children == false) {
        score -= 50.0f;  // Major factor
    }
    if (profile.has_other_dogs && dog.good_with_dogs == false) {
        score -= 40.0f;
    }
    if (profile.has_cats && dog.good_with_cats == false) {
        score -= 40.0f;
    }

    // Living situation
    if (profile.home_type == "apartment" && !dog.apartment_suitable) {
        score -= 30.0f;
    }
    if (!profile.has_yard && dog.exercise_needs == "high") {
        score -= 20.0f;
    }

    // Energy matching
    int energyDiff = abs(energyToInt(profile.activity_level) - energyToInt(dog.energy_level));
    score -= energyDiff * 10.0f;

    // Experience matching
    if (profile.dog_experience == "none" && dog.special_needs) {
        score -= 25.0f;
    }

    // Preference bonuses
    if (contains(profile.preferred_size, dog.size)) {
        score += 5.0f;
    }
    if (contains(profile.preferred_age, dog.age_category)) {
        score += 5.0f;
    }

    return std::max(0.0f, std::min(100.0f, score));
}
```

---

## Overlooked Angels Program

### Criteria for "Overlooked" Flag

| Category | Criteria | Why They're Overlooked |
|----------|----------|------------------------|
| **Black Dogs** | Primary color is black | Don't photograph well, "Black Dog Syndrome" |
| **Senior Dogs** | Age 8+ years | People want puppies |
| **Bully Breeds** | Pit Bull, Am Staff, etc. | Housing discrimination, stigma |
| **Bonded Pairs** | Must be adopted together | Harder to place two dogs |
| **Special Needs** | Medical/behavioral needs | Extra care requirements |
| **Long Waiters** | 6+ months in shelter | Passed over repeatedly |

### Automatic Flagging (Daily Worker)

```cpp
void OverlookedWorker::flagOverlookedDogs() {
    // Black dogs
    db.exec("UPDATE dogs SET is_overlooked = TRUE, overlooked_reason = 'black' "
            "WHERE color_primary ILIKE '%black%' AND is_overlooked = FALSE");

    // Senior dogs
    db.exec("UPDATE dogs SET is_overlooked = TRUE, overlooked_reason = 'senior' "
            "WHERE age_category = 'senior' AND is_overlooked = FALSE");

    // Bully breeds
    db.exec("UPDATE dogs SET is_overlooked = TRUE, overlooked_reason = 'bully_breed' "
            "WHERE breed_primary ILIKE ANY(ARRAY['%pit%', '%staffordshire%', '%bull terrier%']) "
            "AND is_overlooked = FALSE");

    // Long waiters (6+ months)
    db.exec("UPDATE dogs SET is_overlooked = TRUE, overlooked_reason = 'long_wait' "
            "WHERE wait_start_date < NOW() - INTERVAL '180 days' "
            "AND is_overlooked = FALSE");
}
```

### Featured Placement

- Dedicated "Overlooked Angels" section on homepage
- Badge/ribbon on dog cards: "Senior Sweetheart", "Gentle Giant", etc.
- Extra social media promotion
- Priority in matching results (small score bonus)

---

## Sponsor-a-Fee Program

### How It Works

```
┌─────────────────────────────────────────────────────┐
│ Donor visits dog profile                            │
│ Sees: "Adoption fee: $150"                         │
│ Button: "Sponsor This Dog's Fee"                   │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Sponsor checkout                                    │
│ Amount: $150 (full fee) or custom amount           │
│ Display name: "Sarah M." or "Anonymous"            │
│ Optional message to future adopter                 │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Dog profile now shows:                              │
│ "Adoption fee: $150 — SPONSORED!"                  │
│ "Sponsored by Sarah M."                            │
│ "Message: 'Every dog deserves a chance'"           │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ When dog is adopted:                                │
│ - Sponsor notified: "Max found a home!"            │
│ - Adopter sees sponsor message                     │
│ - Success story shared with sponsor                │
└─────────────────────────────────────────────────────┘
```

### Corporate Sponsorship

- Monthly commitment: "Chewy sponsors 50 dogs/month"
- Logo display on sponsored dogs
- Quarterly impact report
- Tax-deductible donation receipt

---

## Post-Adoption Email Sequence

### Automated Check-ins

| Timing | Subject | Purpose |
|--------|---------|---------|
| Day 1 | "Welcome home, [Dog Name]!" | Immediate resources, set expectations |
| Day 3 | "How's the first 72 hours going?" | Check for early struggles |
| Week 1 | "One week with [Dog Name]" | Survey: how's adjustment? |
| Month 1 | "Happy 1-month anniversary!" | Deeper check-in, offer help |
| Month 3 | "3 months of love" | Celebrate, ask for success story |
| Year 1 | "Happy Gotcha Day!" | Anniversary celebration, photo request |

### Triggered Emails

| Trigger | Email |
|---------|-------|
| User clicks "Need Help" | Connect with behavior resources |
| Survey says "Struggling" | Personal outreach, trainer referrals |
| Survey says "Considering return" | Intervention email with support options |

### Email Content Example (Day 1)

```
Subject: Welcome home, Max! Here's what to expect

Hi [Name],

Congratulations on adopting Max! He's been waiting 847 days for this moment.

THE FIRST 72 HOURS
- Max may be overwhelmed. Give him a quiet space.
- Don't force interaction. Let him come to you.
- Stick to a routine: same feeding times, same potty spot.

RESOURCES FOR YOU
- [The 3-3-3 Rule of Adoption](link)
- [Setting Up Your Home](link)
- [Common First-Week Challenges](link)

NEED HELP?
We're here. Reply to this email or visit our Help Center.

Max waited 2 years, 3 months, and 15 days for you. Thank you for giving him a home.

With gratitude,
The WaitingTheLongest Team

P.S. We'll check in again in a few days. You've got this!
```

---

## Transport Network

### Request Flow

```
┌─────────────────────────────────────────────────────┐
│ Adopter finds dog 300 miles away                   │
│ Button: "Request Transport Help"                   │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Transport Request Form                              │
│ - Pickup: [Shelter address]                        │
│ - Destination: [Your address]                      │
│ - Preferred date: [Date picker]                    │
│ - Flexible on dates? ○ Yes ○ No                   │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ System calculates route                             │
│ - Direct: 1 volunteer needed                       │
│ - Relay: 3 volunteers, 2 handoff points           │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Volunteers notified                                 │
│ "New transport request: Austin, TX → Denver, CO"   │
│ "Distance: 935 miles | Date: Feb 15"              │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Volunteers claim legs                               │
│ Leg 1: Austin → Amarillo (volunteer A)            │
│ Leg 2: Amarillo → Albuquerque (volunteer B)       │
│ Leg 3: Albuquerque → Denver (volunteer C)         │
└─────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────┐
│ Transport confirmed                                 │
│ - All parties notified                             │
│ - GPS tracking enabled                             │
│ - Check-in at each handoff                        │
└─────────────────────────────────────────────────────┘
```

---

## Social Sharing

### One-Click Share

Every dog profile has:
- Facebook share button
- Twitter/X share button
- Instagram (copy to clipboard)
- Text message share
- Email share
- Copy link button

### Auto-Generated Share Image

```
┌─────────────────────────────────────────────────────┐
│ ┌─────────────────────────────────────────────────┐ │
│ │                                                 │ │
│ │              [DOG PHOTO]                        │ │
│ │                                                 │ │
│ └─────────────────────────────────────────────────┘ │
│                                                     │
│   MAX has been waiting                              │
│   ╔═══════════════════════════════════════════╗    │
│   ║  02 : 03 : 15 : 08 : 45 : 32              ║    │
│   ║  YR   MO   DY   HR   MI   SC              ║    │
│   ╚═══════════════════════════════════════════╝    │
│                                                     │
│   Can you help him find a home?                    │
│   waitingthelongest.com/dogs/max                   │
│                                                     │
└─────────────────────────────────────────────────────┘
```

### "I Can't Adopt But I Can Share" Button

Prominent button for visitors who can't adopt:
- One click generates shareable image
- Pre-written message: "This is Max. He's been waiting 2 years for a home. Can you help?"
- Tracking: which shares lead to adoptions

---

## Pet-Friendly Housing Directory

### Search Interface

```
┌─────────────────────────────────────────────────────┐
│ Find Pet-Friendly Housing                           │
│                                                     │
│ City: [________________] State: [▼ Select]         │
│                                                     │
│ Property Type: □ Apartment □ Condo □ House         │
│                                                     │
│ My dog is:                                          │
│   Weight: [__] lbs                                 │
│   Breed: [________________]                        │
│                                                     │
│ [Search]                                           │
└─────────────────────────────────────────────────────┘
```

### Results Show

- Property name and address
- Pet policy details (deposit, rent, restrictions)
- Amenities (dog park, pet wash, trails)
- "Verified" badge if confirmed
- User reviews/reports

### Community Contributions

- Users can submit new properties
- Users can report outdated info
- Moderator review before publishing

---

## Shelter Analytics Dashboard

### Metrics Provided to Shelters

| Metric | What It Shows |
|--------|---------------|
| **Avg Days to Adoption** | How quickly their dogs find homes vs. state average |
| **Profile Views** | Which dogs get seen, which don't |
| **Search Appearances** | How often their dogs appear in searches |
| **Conversion Rate** | Views → Adoption URL clicks |
| **Photo Performance** | Do dogs with videos get more views? |
| **Description Analysis** | Optimal description length, keywords |

### Recommendations Engine

Based on data, suggest improvements:
- "Dogs with videos get 3x more views. 15 of your dogs don't have videos."
- "Your average description is 50 words. The optimal length is 150-200 words."
- "Max has been viewed 200 times but 0 adoption clicks. Consider updating his profile."

---

## Partner Network

### Planned Integrations

| Partner Type | Benefit to Adopters |
|--------------|---------------------|
| **Vets** | First checkup discount |
| **Trainers** | Free first session |
| **Pet Stores** | Starter kit discount (Chewy, Petco, local) |
| **Insurance** | Pet insurance trial |
| **Photographers** | Free post-adoption photo session |

### Implementation

- Partner directory searchable by zip code
- Discount codes issued at adoption
- Track redemption rates

---

## Data-Driven Optimization

### Track What Works

```sql
-- What makes dogs get adopted faster?
SELECT
    had_video,
    had_story,
    was_sponsored,
    AVG(days_to_adoption) as avg_days,
    COUNT(*) as adoptions
FROM adoption_factors
GROUP BY had_video, had_story, was_sponsored
ORDER BY avg_days;
```

### Use Insights

- Share findings with shelters
- Feature dogs with best practices
- A/B test profile layouts
- Optimize search result ranking

---

## API Endpoints Summary

### Public Endpoints
```
GET  /api/dogs                      # List dogs (paginated, filtered)
GET  /api/dogs/{id}                 # Dog details
GET  /api/dogs/longest-waiting      # Top N longest waiting
GET  /api/dogs/overlooked           # Overlooked angels
GET  /api/dogs/sponsored            # Fee-sponsored dogs
GET  /api/shelters                  # List shelters
GET  /api/shelters/{id}             # Shelter details
GET  /api/states                    # List states with counts
GET  /api/states/{code}             # State details + dogs
GET  /api/search                    # Full-text search
GET  /api/housing                   # Pet-friendly housing search
GET  /api/resources                 # Adopter resource library
GET  /api/success-stories           # Adoption success stories
```

### Authenticated Endpoints (Adopters)
```
POST /api/auth/register             # Create account
POST /api/auth/login                # Login
GET  /api/me                        # Current user
POST /api/matching/quiz             # Submit lifestyle quiz
GET  /api/matching/results          # Get matched dogs
GET  /api/favorites                 # User's favorited dogs
POST /api/favorites/{dogId}         # Add favorite
DELETE /api/favorites/{dogId}       # Remove favorite
GET  /api/saved-searches            # Saved searches
POST /api/saved-searches            # Create saved search
POST /api/transport/request         # Request transport
GET  /api/adoptions/mine            # My adoptions
POST /api/checkins/{adoptionId}     # Submit check-in survey
POST /api/success-stories           # Submit success story
POST /api/housing/submit            # Submit housing listing
```

### Agency Endpoints
```
GET  /api/agency/dogs               # Shelter's dogs
POST /api/agency/dogs               # Add dog
PUT  /api/agency/dogs/{id}          # Update dog
DELETE /api/agency/dogs/{id}        # Remove dog
GET  /api/agency/analytics          # Shelter analytics
PUT  /api/agency/profile            # Update shelter profile
```

### Admin Endpoints
```
GET  /api/admin/verifications       # Pending verifications
PUT  /api/admin/verifications/{id}  # Approve/reject
GET  /api/admin/analytics           # Platform analytics
GET  /api/admin/housing/pending     # Pending housing submissions
```

### WebSocket
```
WS   /ws/waittime                   # Real-time wait time updates
```

---

## Acceptance Criteria

### Core Features
- [ ] LED counter updates every second in browser
- [ ] Search returns results in <200ms
- [ ] WebSocket handles 1000+ concurrent connections
- [ ] All 50 states have dedicated pages
- [ ] Dogs sorted by wait time by default

### Adoption-Maximizing Features
- [ ] Lifestyle quiz completes and shows matched dogs
- [ ] Match scores calculated correctly based on criteria
- [ ] Video profiles play smoothly
- [ ] Share buttons generate trackable links
- [ ] Share images generate with dog photo + wait time
- [ ] Sponsor-a-fee payment flow works
- [ ] Sponsored dogs show sponsor info
- [ ] Post-adoption emails send on schedule
- [ ] Check-in surveys record responses
- [ ] Overlooked dogs flagged automatically
- [ ] Overlooked Angels section displays correctly
- [ ] Housing search returns relevant results
- [ ] Transport requests create and notify volunteers
- [ ] Resource library articles display correctly
- [ ] Shelter analytics dashboard shows metrics

### Data & Sync
- [ ] RescueGroups sync runs hourly
- [ ] Deduplication prevents duplicate dogs
- [ ] Existing shelter data imported correctly
- [ ] Daily analytics jobs run successfully

---

## Implementation Phases

### Phase 1: Core Platform (Weeks 1-4)
- Project setup, database schema
- Dog/Shelter/State CRUD
- LED counter + WebSocket
- Basic search and filtering
- 5 pilot states active

### Phase 2: Engagement Features (Weeks 5-8)
- Lifestyle matching quiz
- Video profile support
- Social sharing with image generation
- User accounts and favorites
- 20 states active

### Phase 3: Adoption Support (Weeks 9-12)
- Sponsor-a-fee system
- Post-adoption email sequences
- Overlooked Angels program
- Success stories
- All 50 states active

### Phase 4: Ecosystem (Weeks 13-16)
- Pet-friendly housing directory
- Transport network
- Adopter resource library
- Shelter analytics dashboard
- Partner integrations

### Phase 5: Optimization (Ongoing)
- A/B testing
- Performance tuning
- Data analysis and insights
- Feature refinement based on metrics

---

## Files to Create First

1. `CMakeLists.txt` - Build configuration
2. `src/main.cc` - Application entry point
3. `sql/001_core_schema.sql` - Core database tables
4. `src/services/WaitTimeCalculator.cc` - LED counter logic
5. `src/controllers/websocket/WaitTimeWebSocket.cc` - Real-time updates
6. `src/services/MatchingService.cc` - Lifestyle matching algorithm
7. `src/aggregators/RescueGroupsAggregator.cc` - Data sync
8. `static/js/led-counter.js` - Client-side counter
9. `static/js/matching-quiz.js` - Quiz interface
10. `static/js/social-share.js` - Share functionality

---

## Success Metrics

### Primary KPIs
| Metric | Target |
|--------|--------|
| Dogs adopted via platform | Track monthly |
| Average days to adoption | Decrease over time |
| Return rate | < 10% |
| Share-to-adoption conversion | Track and improve |

### Secondary KPIs
| Metric | Target |
|--------|--------|
| Quiz completion rate | > 60% |
| Sponsor-a-fee participation | 10% of dogs sponsored |
| Post-adoption email open rate | > 40% |
| Transport requests fulfilled | > 80% |
| Monthly active users | Growth month-over-month |

---

## The Mission

Every feature, every line of code, every design decision serves one purpose:

**Get dogs out of shelters and into loving homes.**

The LED counter isn't just a feature - it's a reminder that every second matters to a dog waiting in a kennel.

The matching quiz isn't just convenience - it's preventing returns by connecting the right dog to the right family.

The sponsor program isn't just fundraising - it's removing the single biggest barrier to adoption.

This isn't a database. It's a mission.
