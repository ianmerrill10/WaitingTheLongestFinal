# Changelog

All notable changes to WaitingTheLongest.com are documented here.

## [Unreleased] - 2026-02-12

### Phase L1: Compilation
- Fixed 684 compilation errors across 9 iterative builds
- All C++ source files now compile successfully

### Phase L2: Database Bootstrap
- Created 86 PostgreSQL tables across core and module schemas
- Seeded 50 states and 200+ dog breeds

### Phase L3: Smoke Test
- Application starts and serves health check endpoint
- All services initialize from configuration

### Phase L4: API Testing
- Verified 24+ API endpoints respond correctly
- Fixed runtime SQL column mismatches

### Phase L5: Frontend Fixes
- Fixed all HTML pages to load correctly
- Added favicon, OG tags, and Twitter Cards

### Phase L6: Runtime SQL Fixes
- Fixed HousingController state_id -> state_code joins
- Fixed TransportController parameter name

### Phase L7: Docker Deployment
- Full Docker Compose stack operational
- PostgreSQL, Redis, Nginx, Certbot configured

### Phase L8: Hardening
- Generated secure credentials (DB, Redis, JWT)
- Created error pages (404, 500)
- Created operational scripts (backup, deploy validation, smoke test)
- Removed dead PetfinderAggregator code
- Added log rotation configuration

### Phase L9: 100-Point Improvement Sprint
- Created .gitignore
- Fixed Redis healthcheck with AUTH support
- Removed deprecated docker-compose version key
- Pinned Docker image versions
- Added resource limits to all containers
- Added security headers to nginx
- Added CORS configuration
- Added accessibility improvements (skip links, ARIA, noscript)
- Added canonical URLs to all pages
- Added JSON-LD structured data
- Created robots.txt, sitemap.xml, manifest.json
- Created security.txt, humans.txt
- Added .editorconfig
- Created README.md, LICENSE, CONTRIBUTING.md
- And 70+ more improvements (see IMPROVEMENT_CHECKLIST.md)
