# Contributing to WaitingTheLongest.com

Thank you for your interest in helping save shelter dogs!

## Getting Started

1. Fork the repository
2. Clone your fork
3. Copy `.env.example` to `.env` and configure
4. Run `docker compose up -d`
5. Make your changes
6. Submit a pull request

## Code Standards

- Follow [CODING_STANDARDS.md](CODING_STANDARDS.md)
- C++17 with Drogon framework conventions
- Namespaces: `wtl::core::services`, `wtl::modules`, etc.
- All services use singleton pattern: `ClassName::getInstance()`
- Frontend: Vanilla HTML/CSS/JS only (no frameworks)

## Important Notes

- **There is NO Petfinder API.** Do not reference or add Petfinder integrations.
- LED counter format is YY:MM:DD:HH:MM:SS (final, do not change)
- No payment processing (we redirect to shelter websites)
- Modules must work independently when others are disabled

## Pull Request Process

1. Update documentation if you change interfaces
2. Ensure Docker build completes successfully
3. Test your changes with `scripts/smoke_test.sh`
4. Describe your changes clearly in the PR description

## Reporting Issues

Open an issue with:
- Clear description of the problem
- Steps to reproduce
- Expected vs actual behavior
- Environment details
