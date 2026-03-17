import type { Config } from "tailwindcss";

const config: Config = {
  content: [
    "./src/pages/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/components/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/app/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  theme: {
    extend: {
      colors: {
        // Brand colors
        wtl: {
          blue: "#1e3a5f",
          green: "#22c55e",
          red: "#ef4444",
          orange: "#f97316",
          yellow: "#eab308",
          dark: "#0f172a",
        },
        // Urgency colors
        urgency: {
          critical: "#dc2626",
          high: "#ea580c",
          medium: "#ca8a04",
          normal: "#16a34a",
        },
        // LED counter colors
        led: {
          green: "#00ff41",
          red: "#ff0000",
          orange: "#ff8c00",
          dim: "#1a3a1a",
          bg: "#0a0a0a",
        },
      },
      animation: {
        "pulse-critical": "pulse-critical 1s ease-in-out infinite",
        "led-glow": "led-glow 2s ease-in-out infinite",
      },
      keyframes: {
        "pulse-critical": {
          "0%, 100%": { opacity: "1" },
          "50%": { opacity: "0.5" },
        },
        "led-glow": {
          "0%, 100%": { filter: "brightness(1)" },
          "50%": { filter: "brightness(1.3)" },
        },
      },
    },
  },
  plugins: [],
};

export default config;
