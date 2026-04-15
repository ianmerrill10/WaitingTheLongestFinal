import { NextRequest, NextResponse } from 'next/server';
import { verifySessionToken, SESSION_COOKIE_NAME } from '@/lib/auth/signed-session';

export async function middleware(request: NextRequest) {
  const { pathname } = request.nextUrl;

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
