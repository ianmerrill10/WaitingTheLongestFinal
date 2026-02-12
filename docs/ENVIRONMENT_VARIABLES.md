# Environment Variables

All environment variables used by WaitingTheLongest.com.

## Required Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `DB_HOST` | PostgreSQL host | `db` (Docker) or `localhost` |
| `DB_PORT` | PostgreSQL port | `5432` |
| `DB_NAME` | Database name | `waitingthelongest` |
| `DB_USER` | Database user | `postgres` |
| `DB_PASSWORD` | Database password | (generate a 32+ char random string) |
| `REDIS_HOST` | Redis host | `redis` (Docker) or `localhost` |
| `REDIS_PORT` | Redis port | `6379` |
| `REDIS_PASSWORD` | Redis AUTH password | (generate a 32+ char random string) |
| `JWT_SECRET` | JWT signing secret | (generate a 64+ char random string) |
| `JWT_ISSUER` | JWT issuer claim | `waitingthelongest.com` |

## Optional Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `LOG_LEVEL` | Logging level (debug/info/warn/error) | `info` |
| `DOMAIN` | Application domain | `localhost` |
| `REDIS_DB` | Redis database number | `0` |
| `JWT_EXPIRATION_HOURS` | Access token lifetime | `24` |
| `REFRESH_TOKEN_EXPIRATION_DAYS` | Refresh token lifetime | `30` |
| `BCRYPT_COST` | Password hashing cost factor | `12` |

## External API Keys

| Variable | Description | Required |
|----------|-------------|----------|
| `RESCUEGROUPS_API_KEY` | RescueGroups.org API key | For data sync |
| `THEDOGAPI_KEY_1` through `_4` | TheDogAPI keys (up to 4) | For breed images |
| `GOOGLE_OAUTH_CLIENT_ID` | Google OAuth client ID | For social login |
| `GOOGLE_OAUTH_CLIENT_SECRET` | Google OAuth client secret | For social login |
| `SENDGRID_API_KEY` | SendGrid API key | For email notifications |
| `FROM_EMAIL` | Sender email address | For emails |

## Deployment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `SSL_EMAIL` | Email for Let's Encrypt | Required for SSL |
| `AMAZON_ASSOCIATE_TAG` | Amazon Associates tag | For product links |
| `DEBUG_MODE` | Enable debug mode | `false` |
| `ENABLE_CORS` | Enable CORS headers | `true` |
| `CORS_ALLOWED_ORIGINS` | Allowed CORS origins | `http://localhost:8080` |

## Important Notes

- **Never commit `.env` to version control** - it contains secrets
- Copy `.env.example` to `.env` and fill in real values
- Docker Compose reads `.env` automatically
- **There is NO Petfinder API** - do not add Petfinder keys
