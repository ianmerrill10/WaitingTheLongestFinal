import { NextRequest, NextResponse } from 'next/server';
import { verifySessionToken, SESSION_COOKIE_NAME } from '@/lib/auth/signed-session';

const MAINTENANCE_HTML = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Down for Maintenance — WaitingTheLongest.com</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e2e8f0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:24px}
    .card{max-width:480px;width:100%;text-align:center}
    .paw{font-size:64px;margin-bottom:24px}
    h1{font-size:28px;font-weight:700;margin-bottom:12px;color:#f8fafc}
    p{color:#94a3b8;line-height:1.6;margin-bottom:8px}
    .eta{margin-top:24px;padding:16px;background:#1e293b;border-radius:12px;border:1px solid #334155;font-size:14px;color:#64748b}
  </style>
</head>
<body>
  <div class="card">
    <div class="paw">🐾</div>
    <h1>Down for Maintenance</h1>
    <p>We're making improvements to help more dogs find homes.</p>
    <p>We'll be back shortly.</p>
    <div class="eta">WaitingTheLongest.com &mdash; Every dog deserves a home.</div>
  </div>
</body>
</html>`;

export async function middleware(request: NextRequest) {
  const { pathname } = request.nextUrl;

  // ── MAINTENANCE MODE ──────────────────────────────────────────────────────
  // Set NEXT_PUBLIC_MAINTENANCE=true in Vercel env vars to activate.
  // /api/health still passes through so uptime monitors keep working.
  if (process.env.NEXT_PUBLIC_MAINTENANCE === 'true') {
    if (pathname === '/api/health' || pathname.startsWith('/_next/') || pathname.startsWith('/favicon')) {
      return NextResponse.next();
    }
    return new NextResponse(MAINTENANCE_HTML, {
      status: 503,
      headers: {
        'Content-Type': 'text/html; charset=utf-8',
        'Retry-After': '3600',
        'Cache-Control': 'no-store',
      },
    });
  }
  // ─────────────────────────────────────────────────────────────────────────

  // Allow static assets and Next.js internals
  if (
    pathname.startsWith('/_next/') ||
    pathname.startsWith('/favicon') ||
    pathname === '/robots.txt' ||
    pathname === '/sitemap.xml' ||
    pathname === '/manifest.json'
  ) {
    return NextResponse.next();
  }

  // Allow the login page itself
  if (pathname === '/login') {
    return NextResponse.next();
  }

  // Allow legal pages to be publicly accessible
  if (pathname.startsWith('/legal/')) {
    return NextResponse.next();
  }

  // Allow public API routes (crons, webhooks, v1 partner API, health, etc.)
  // But BLOCK /api/admin/* unless authenticated
  if (pathname.startsWith('/api/')) {
    if (pathname.startsWith('/api/admin/')) {
      const authCookie = request.cookies.get(SESSION_COOKIE_NAME);
      if (!(await verifySessionToken(authCookie?.value))) {
        return NextResponse.json({ error: 'Unauthorized' }, { status: 401 });
      }
    }
    return NextResponse.next();
  }

  // Check for auth cookie (signed session token)
  const authCookie = request.cookies.get(SESSION_COOKIE_NAME);
  if (await verifySessionToken(authCookie?.value)) {
    return NextResponse.next();
  }

  // Redirect to login — sanitize the redirect path to prevent open redirects
  const loginUrl = new URL('/login', request.url);
  // Only set 'from' if it's a safe relative path (no protocol, no host)
  if (pathname.startsWith('/') && !pathname.includes('://') && !pathname.startsWith('//')) {
    loginUrl.searchParams.set('from', pathname);
  }
  return NextResponse.redirect(loginUrl);
}

export const config = {
  matcher: ['/((?!_next/static|_next/image|favicon.ico).*)'],
};
