import { Suspense } from 'react';

export const metadata = {
  title: 'Login | WaitingTheLongest.com',
};

export default function LoginLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return <Suspense fallback={null}>{children}</Suspense>;
}
