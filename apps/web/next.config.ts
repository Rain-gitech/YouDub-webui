import type { NextConfig } from "next";

function apiProxyTarget() {
  const configured =
    process.env.NEXT_SERVER_API_BASE_URL ||
    "http://127.0.0.1:8000";
  return configured.replace(/\/$/, "");
}

const nextConfig: NextConfig = {
  allowedDevOrigins: ["172.27.2.90", "100.94.222.54"],
  async rewrites() {
    const publicationTarget = (process.env.NEXT_SERVER_PUBLICATION_API_BASE_URL || apiProxyTarget()).replace(/\/$/, "");
    return [
      {
        source: "/api/tasks/:taskId/publication",
        destination: `${publicationTarget}/api/tasks/:taskId/publication`,
      },
      {
        source: "/api/tasks/:taskId/artifact/publication",
        destination: `${publicationTarget}/api/tasks/:taskId/artifact/publication`,
      },
      {
        source: "/api/:path*",
        destination: `${apiProxyTarget()}/api/:path*`,
      },
    ];
  },
};

export default nextConfig;
