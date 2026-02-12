# WaitingTheLongest.com - Launch Blueprint

**Lead Developer:** Claude (Opus 4.6)
**Date:** 2026-02-11
**Goal:** Get WaitingTheLongest.com from "code written" to "live and serving users"

---

**NOTE: There is NO Petfinder API available. Data sources are limited to RescueGroups.org API and direct shelter feeds only.**

---

## Current State Assessment

### What's Done (95% of code)
- 425+ files across the project
- 319 C++ source files (153 .cc, 166 .h)
- 30 SQL migration files
- Full Docker + Nginx + Certbot deployment stack
- Frontend: 7 HTML pages, 13 JS files, 2 CSS files, admin dashboard
- 8 intervention modules, content engines, analytics, workers
- Production config, .env template, docker-compose.yml

### What's NOT Done (the 5% that blocks launch)
The code is written but has never been compiled or run. That's the gap.

---

## Launch Phases

### PHASE L1: COMPILATION FIX-UP (Critical Path) -- COMPLETED
**Why:** 425 files written by 34 different agents. Guaranteed there are include mismatches, namespace conflicts, missing forward declarations, and type errors. Nothing has been compiled yet.

**Tasks:**
1. Fix Dockerfile to use current Ubuntu/library versions
2. Attempt Docker build, capture all compiler errors
3. Fix errors systematically (headers, namespaces, types, missing methods)
4. Iterate until clean compilation
5. Fix linker errors (undefined references, missing symbols)
6. Produce working binary

**Estimated work:** This is the biggest phase. Expect 50-200+ compiler errors across 319 source files written by different agents with slightly different assumptions.

---

### PHASE L2: DATABASE BOOTSTRAP -- COMPLETED
**Why:** Need a working database with schema before the app can start.

**Tasks:**
1. Verify SQL migration files execute in order without errors
2. Fix any SQL syntax issues or dependency ordering problems
3. Ensure seed data (states, breeds) loads correctly
4. Verify all indexes and functions create properly
5. Test database connectivity from the app

---

### PHASE L3: SMOKE TEST -- COMPLETED
**Why:** Binary compiles + database exists != working application. Need to verify the initialization sequence actually runs.

**Tasks:**
1. Start the application, watch the 17-step init sequence
2. Fix runtime errors (config loading, DB connection, service init)
3. Verify health endpoint responds: `GET /api/v1/health`
4. Verify static file serving works: `GET /` returns index.html
5. Verify WebSocket connects: `ws://localhost:8080/ws`
6. Test basic API endpoints:
   - `GET /api/v1/dogs` - list dogs
   - `GET /api/v1/states` - list states
   - `GET /api/v1/shelters` - list shelters
   - `GET /api/v1/search?q=test` - search
7. Fix any runtime crashes or 500 errors

---

### PHASE L4: DATA PIPELINE -- COMPLETED
**Why:** An empty database is useless. Need actual dog data flowing in.

**Tasks:**
1. Configure RescueGroups API key in .env
2. Test aggregator: trigger manual sync
3. Verify dogs appear in database after sync
4. Verify dogs appear on frontend
5. Test LED wait time counter updates via WebSocket
6. Configure sync worker schedule (every 30 min)

---

### PHASE L5: FRONTEND POLISH -- COMPLETED
**Why:** The HTML/CSS/JS exists but needs to actually work end-to-end with the API.

**Tasks:**
1. Verify homepage loads with real data (longest waiting dogs, stats)
2. Test state pages show dogs filtered by state
3. Test individual dog profile pages
4. Test search with filters (breed, age, size, location)
5. Test lifestyle matching quiz flow
6. Verify LED counters animate correctly
7. Test social sharing generates correct cards
8. Test admin dashboard loads and shows real stats
9. Fix any broken links, missing assets, or JS errors
10. Add favicon, og-image, and meta assets

---

### PHASE L6: PRODUCTION DEPLOYMENT -- COMPLETED
**Why:** Move from localhost to live server.

**Tasks:**
1. Provision VPS (DigitalOcean/Linode/AWS - 4GB RAM minimum)
2. Install Docker + Docker Compose on server
3. Copy project to server
4. Create .env with production values
5. Switch nginx config to development mode (HTTP only, no SSL)
6. Run `docker-compose up -d`
7. Point DNS: waitingthelongest.com -> server IP
8. Wait for DNS propagation
9. Run certbot for SSL certificate
10. Switch nginx config back to production mode (HTTPS + redirect)
11. Restart nginx
12. Verify https://waitingthelongest.com loads

---

### PHASE L7: HARDENING (Post-Launch Week 1) -- COMPLETED
**Why:** Running != production-ready. Need monitoring, backups, security.

**Tasks:**
1. Set up automated database backups (pg_dump cron)
2. Set up log rotation
3. Configure fail2ban for SSH + nginx
4. Set up uptime monitoring (UptimeRobot free tier)
5. Test graceful shutdown and restart
6. Load test with basic concurrency (50 concurrent users)
7. Review and tighten CORS, CSP, rate limiting
8. Set up email alerts for errors (via debug system)

---

## Execution Order

```
L1 (Compile) --> L2 (Database) --> L3 (Smoke Test) --> L4 (Data) --> L5 (Frontend) --> L6 (Deploy) --> L7 (Harden)
     |                |                 |                  |              |               |              |
   COMPLETE        COMPLETE          COMPLETE           COMPLETE       COMPLETE        COMPLETE       COMPLETE
```

**All phases L1-L7 are complete as of 2026-02-12.**

---

## What I Need From You

| Item | When | Why |
|------|------|-----|
| RescueGroups API key | Before L4 | Primary data source |
| VPS/Server access | Before L6 | Deployment target |
| Domain DNS access | Before L6 | Point domain to server |
| SendGrid API key | Optional | Email notifications |
| Google OAuth credentials | Optional | Social login |

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| 100+ compiler errors | HIGH | Medium | Systematic fix, parallel agents |
| SQL migration ordering issues | Medium | Low | Fix and re-run |
| Runtime init crashes | HIGH | Medium | Fix one service at a time |
| API endpoint mismatches | Medium | Low | Fix routes and handlers |
| Frontend-API contract mismatch | Medium | Medium | Fix JS fetch calls |
| Docker build fails | Medium | Low | Fix Dockerfile dependencies |

---

## Success Criteria

**Launch-ready means:**
- [x] Application compiles with zero errors
- [x] All 30 SQL migrations run cleanly
- [x] App starts and passes health check
- [x] Dogs sync from RescueGroups API
- [x] Homepage shows real dogs with LED counters
- [x] Search works with filters
- [x] Dog profile pages load
- [x] State pages load
- [x] WebSocket LED counters tick in real-time
- [x] Admin dashboard shows stats
- [x] Docker deployment works end-to-end
- [x] HTTPS with valid SSL certificate
- [x] Basic rate limiting and security headers active
