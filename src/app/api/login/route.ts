import { NextRequest, NextResponse } from 'next/server';
import { timingSafeEqual } from 'crypto';

export async function POST(request: NextRequest) {
  try {
    const { password } = await request.json();
    const sitePassword = process.env.SITE_PASSWORD;

    if (!sitePassword) {
      // If no password is set, allow access (fail open for dev)
      const response = NextResponse.json({ ok: true });
      response.cookies.set('site_auth', 'authenticated', {
        httpOnly: true,
        secure: process.env.NODE_ENV === 'production',
        sameSite: 'lax',
        maxAge: 60 * 60 * 24 * 30, // 30 days
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

    const response = NextResponse.json({ ok: true });
    response.cookies.set('site_auth', 'authenticated', {
      httpOnly: true,
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
      maxAge: 60 * 60 * 24 * 30, // 30 days
      path: '/',
    });
    return response;
  } catch {
    return NextResponse.json({ error: 'Bad request' }, { status: 400 });
  }
}
