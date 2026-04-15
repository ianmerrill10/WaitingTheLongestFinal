import { NextRequest, NextResponse } from 'next/server';
import { timingSafeEqual } from 'crypto';
import { createSessionToken, SESSION_COOKIE_NAME, SESSION_MAX_AGE_SECONDS } from '@/lib/auth/signed-session';

export async function POST(request: NextRequest) {
  try {
    const { password } = await request.json();
    const sitePassword = process.env.SITE_PASSWORD;

    if (!sitePassword) {
      // In production, SITE_PASSWORD must be set — refuse login
      if (process.env.NODE_ENV === 'production') {
        console.error('[Login] SITE_PASSWORD not configured in production');
        return NextResponse.json({ error: 'Authentication unavailable' }, { status: 503 });
      }
      // In dev, allow access without password — issue a signed token if possible
      const token = (await createSessionToken()) || 'authenticated';
      const response = NextResponse.json({ ok: true });
      response.cookies.set(SESSION_COOKIE_NAME, token, {
        httpOnly: true,
        secure: false,
        sameSite: 'lax',
        maxAge: SESSION_MAX_AGE_SECONDS,
        path: '/',
      });
      return response;
    }

    // Timing-safe comparison to prevent timing attacks
    const pwBuf = Buffer.from(String(password || ''));
    const secretBuf = Buffer.from(sitePassword);
    const maxLen = Math.max(pwBuf.length, secretBuf.length);
    const a = Buffer.alloc(maxLen);
    const b = Buffer.alloc(maxLen);
    pwBuf.copy(a);
    secretBuf.copy(b);
    if (!timingSafeEqual(a, b) || pwBuf.length !== secretBuf.length) {
      return NextResponse.json({ error: 'Invalid password' }, { status: 401 });
    }

    const token = await createSessionToken();
    if (!token) {
      console.error('[Login] Failed to create session token');
      return NextResponse.json({ error: 'Authentication unavailable' }, { status: 503 });
    }

    const response = NextResponse.json({ ok: true });
    response.cookies.set(SESSION_COOKIE_NAME, token, {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
      maxAge: SESSION_MAX_AGE_SECONDS,
      path: '/',
    });
    return response;
  } catch {
    return NextResponse.json({ error: 'Bad request' }, { status: 400 });
  }
}
