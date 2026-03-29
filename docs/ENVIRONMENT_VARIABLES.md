# Environment Variables

All environment variables used by WaitingTheLongest.com.

> **Last updated:** 2026-03-29

## Required (Supabase)

| Variable | Description |
|----------|-------------|
| `NEXT_PUBLIC_SUPABASE_URL` | Supabase project URL (e.g., `https://xxxxx.supabase.co`) |
| `NEXT_PUBLIC_SUPABASE_ANON_KEY` | Supabase anonymous/public key |
| `SUPABASE_SERVICE_ROLE_KEY` | Supabase admin key (server-side only, never expose to client) |

## Required (Auth & Security)

| Variable | Description |
|----------|-------------|
| `CRON_SECRET` | Bearer token for all cron/admin/debug API routes. All protected routes check `Authorization: Bearer $CRON_SECRET` |
| `SITE_PASSWORD` | Password for site-wide login gate (middleware enforced). In production, login fails closed if unset |
| `SUPABASE_DB_PASSWORD` | Database password for direct PostgreSQL connections (used by setup route) |

## Data Sources

| Variable | Description | Required |
|----------|-------------|----------|
| `RESCUEGROUPS_API_KEY` | RescueGroups.org API v5 key | Yes — primary data source |
| `THEDOGAPI_KEY_1` through `_4` | TheDogAPI keys (up to 4, rotated) | For breed images |

## Email & Outreach

| Variable | Description | Required |
|----------|-------------|----------|
| `SENDGRID_API_KEY` | SendGrid API key for outreach emails | For email campaigns |
| `FROM_EMAIL` | Sender email address | For outreach |

## Social Media

| Variable | Description | Required |
|----------|-------------|----------|
| `TWITTER_API_KEY` | Twitter/X API key | For social posting |
| `TWITTER_API_SECRET` | Twitter/X API secret | For social posting |
| `TWITTER_ACCESS_TOKEN` | Twitter/X access token | For social posting |
| `TWITTER_ACCESS_SECRET` | Twitter/X access token secret | For social posting |
| `BLUESKY_HANDLE` | Bluesky handle | For social posting |
| `BLUESKY_APP_PASSWORD` | Bluesky app password | For social posting |
| `REDDIT_CLIENT_ID` | Reddit app client ID | For social posting |
| `REDDIT_CLIENT_SECRET` | Reddit app client secret | For social posting |
| `REDDIT_USERNAME` | Reddit username | For social posting |
| `REDDIT_PASSWORD` | Reddit password | For social posting |
| `YOUTUBE_API_KEY` | YouTube Data API key | For video content |

## Monetization

| Variable | Description | Default |
|----------|-------------|---------|
| `AMAZON_ASSOCIATE_TAG` | Amazon Associates ID | `waitingthelon-20` |

## Optional

| Variable | Description |
|----------|-------------|
| `NEXT_PUBLIC_SITE_URL` | Site URL for OpenGraph tags (defaults to `https://waitingthelongest.com`) |

## Where Variables Are Set

- **Vercel:** All production variables are stored in Vercel project settings (Environment Variables)
- **Local dev:** `.env.local` file (git-ignored)
- **Agents:** `agent/*.ts` files load dotenv from `.env.local`

## Important Notes

- **Never commit `.env.local` to version control** — it contains secrets
- **CRON_SECRET** protects all admin, cron, debug, social, outreach, and backup API routes
- **All routes require auth** — there are no unauthenticated admin/cron endpoints
- **There is NO Petfinder API** — do not add Petfinder keys
- **No Docker, Redis, or JWT** — the project uses Supabase (hosted PostgreSQL) and Vercel (serverless)
