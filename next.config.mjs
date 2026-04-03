/** @type {import('next').NextConfig} */
const nextConfig = {
  staticPageGenerationTimeout: 300,
  images: {
    formats: ['image/avif', 'image/webp'],
    minimumCacheTTL: 86400,
    deviceSizes: [640, 750, 828, 1080, 1200, 1920],
    imageSizes: [16, 32, 48, 64, 96, 128, 256],
    remotePatterns: [
      {
        protocol: "https",
        hostname: "**.rescuegroups.org",
      },
      {
        protocol: "https",
        hostname: "**.thedogapi.com",
      },
      {
        protocol: "https",
        hostname: "cdn.rescuegroups.org",
      },
    ],
  },
};

export default nextConfig;
