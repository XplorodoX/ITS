import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "AALeC Quiz — Beamer",
  description: "Beamer view for the AALeC multiplayer quiz",
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="de">
      <body>{children}</body>
    </html>
  );
}
