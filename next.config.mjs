/** @type {import('next').NextConfig} */
const nextConfig = {
  images: {
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
