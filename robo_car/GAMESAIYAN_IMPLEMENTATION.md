# GameSaiyan — Complete Implementation Blueprint

> **Domain:** [https://gamesaiyan.com](https://gamesaiyan.com)
> **Tagline:** *Level Up Your Gameplay*
> **Stack:** Next.js 15 (App Router) · Payload CMS 3.x · MongoDB Atlas · Bunny CDN · Vercel
> **Inspiration:** [QuestDuo](https://questduo.com) — but darker, more tactical, and interactive
> **Last Updated:** May 2026

---

## Table of Contents

1. [Project Overview & Vision](#1-project-overview--vision)
2. [Brand Identity & Design System](#2-brand-identity--design-system)
3. [Tech Stack Deep-Dive](#3-tech-stack-deep-dive)
4. [Project Structure & Architecture](#4-project-structure--architecture)
5. [Payload CMS Configuration](#5-payload-cms-configuration)
6. [Database Schema (MongoDB Atlas)](#6-database-schema-mongodb-atlas)
7. [Image Pipeline — Bunny CDN](#7-image-pipeline--bunny-cdn)
8. [Page-by-Page Specifications](#8-page-by-page-specifications)
9. [Guide Article Features (Interactive)](#9-guide-article-features-interactive)
10. [Tier List / Build Page System](#10-tier-list--build-page-system)
11. [SEO Architecture](#11-seo-architecture)
12. [Performance & Core Web Vitals](#12-performance--core-web-vitals)
13. [Monetization — Ads & Affiliates](#13-monetization--ads--affiliates)
14. [Premium / Paid Guides System](#14-premium--paid-guides-system)
15. [Light/Dark Mode & Theming](#15-lightdark-mode--theming)
16. [Authentication & User Features](#16-authentication--user-features)
17. [Deployment Pipeline (Vercel)](#17-deployment-pipeline-vercel)
18. [Analytics & Tracking](#18-analytics--tracking)
19. [Testing Strategy](#19-testing-strategy)
20. [Phase-wise Rollout Plan](#20-phase-wise-rollout-plan)
21. [Environment Variables Reference](#21-environment-variables-reference)
22. [Appendix: File-by-File Scaffold](#appendix-file-by-file-scaffold)

---

## 1. Project Overview & Vision

### 1.1 What is GameSaiyan?

GameSaiyan is a strategy game content hub — a second-screen companion for gamers seeking walkthroughs, build guides, tier lists, and tactical breakdowns. The name channels "Saiyan" energy: relentless training, powering up, reaching the next level. Every aspect of the design should evoke the feeling of entering a war room — tactical, focused, and high-energy.

### 1.2 Content Pillars

| Pillar | Description | Examples |
|---|---|---|
| **Walkthroughs** | Step-by-step story/mission guides with spoiler toggles | "Elden Ring: Shadow of the Erdtree — Full Walkthrough" |
| **Build Guides** | Character builds, loadouts, gear sets | "Best Pyromancer Build — Dark Souls III" |
| **Tier Lists** | Ranked comparisons with drag-and-drop interactivity | "Genshin Impact 5.0 Character Tier List" |
| **Strategy Guides** | Deep tactical breakdowns, boss strategies | "How to Beat Malenia — Phase 2 No-Hit Strategy" |
| **News & Updates** | Patch notes, game announcements | "Monster Hunter Wilds: Patch 2.1 Breakdown" |

### 1.3 Target Audience

Primary: Gamers ages 16–35, predominantly mobile-first (60%+ traffic expected from phones and tablets), looking for quick, actionable information while playing. Secondary: content creators, streamers, and competitive players seeking reference material.

### 1.4 Differentiators vs. QuestDuo / IGN / GameFAQs

| Feature | QuestDuo | IGN | GameSaiyan |
|---|---|---|---|
| Design vibe | Clean, minimal | Corporate, ad-heavy | Dark tactical HUD, neon accents |
| Interactive checklists | No | No | **Yes — mark steps complete** |
| Spoiler toggles | No | No | **Yes — per-section** |
| Reading progress | No | No | **Yes — sticky progress bar** |
| Tier list builder | No | Static images | **Interactive drag-and-drop** |
| Theme customization | Dark/Light only | No | **Dark/Light + Accent Color + Layout Density** |
| Premium guides | No | Ad-gated | **Paywall with preview** |
| Mobile UX | Good | Poor (ad overload) | **Game-pad-like navigation, swipe gestures** |

---

## 2. Brand Identity & Design System

### 2.1 Logo — "GameSaiyan" Wordmark

**Concept:** A custom wordmark where "GAME" is in a heavy weight and "SAIYAN" has a power-aura glow effect (neon cyan or electric orange). The "A" in SAIYAN can be stylized as an upward arrow / power-up chevron (▲) to reinforce the "level up" motif.

**Logo Specifications:**
- Primary Logo: Horizontal wordmark for header (height: 40px on desktop, 32px on mobile)
- Icon Mark: A standalone "GS" monogram or the stylized "▲" for favicons (32×32, 16×16), app icons (192×192, 512×512)
- Color variants: Full color on dark, full color on light, monochrome white, monochrome black
- Format: SVG (web), PNG (fallback), ICO (favicon)
- Clear space: Minimum 1x the height of the "G" on all sides
- Minimum size: 80px wide for readability

**Typography in Logo:**
- "GAME" — Rajdhani Bold 700 (geometric, techy, aggressive)
- "SAIYAN" — Rajdhani Bold 700 with CSS `text-shadow` glow: `0 0 20px var(--accent-primary), 0 0 40px var(--accent-primary-50)`

### 2.2 Color Palette

The palette draws from tactical HUD interfaces, RGB gaming peripherals, and sci-fi cockpit displays.

```
CORE PALETTE
═══════════════════════════════════════════════════

DARK MODE (Primary)
──────────────────
--bg-primary:          #0A0A0F      /* Near-black with blue undertone */
--bg-secondary:        #12121A      /* Card backgrounds */
--bg-tertiary:         #1A1A28      /* Elevated surfaces */
--bg-overlay:          #0A0A0F/90   /* Modal overlays */
--surface-glass:       rgba(255,255,255,0.03)  /* Glassmorphism panels */
--border-subtle:       rgba(255,255,255,0.06)
--border-default:      rgba(255,255,255,0.10)
--border-accent:       var(--accent-primary)/30

TEXT
──────────────────
--text-primary:        #E8E8F0      /* Primary body text */
--text-secondary:      #8A8A9A      /* Secondary / muted */
--text-tertiary:       #55556A      /* Disabled / placeholder */
--text-inverse:        #0A0A0F      /* On accent backgrounds */

ACCENT SYSTEM (User-switchable — default: Neon Cyan)
──────────────────
Neon Cyan (Default):
  --accent-primary:      #00F0FF
  --accent-primary-50:   #00F0FF80
  --accent-hover:        #33F5FF
  --accent-pressed:      #00C8D4
  --accent-glow:         0 0 20px #00F0FF40

Electric Orange:
  --accent-primary:      #FF6B2C
  --accent-primary-50:   #FF6B2C80
  --accent-hover:        #FF8652
  --accent-pressed:      #E55A1F
  --accent-glow:         0 0 20px #FF6B2C40

Plasma Purple:
  --accent-primary:      #B44CFF
  --accent-primary-50:   #B44CFF80
  --accent-hover:        #C76FFF
  --accent-pressed:      #9A33E0
  --accent-glow:         0 0 20px #B44CFF40

Toxin Green:
  --accent-primary:      #39FF14
  --accent-primary-50:   #39FF1480
  --accent-hover:        #5FFF42
  --accent-pressed:      #2ED60F
  --accent-glow:         0 0 20px #39FF1440

Crimson Red:
  --accent-primary:      #FF2E4C
  --accent-primary-50:   #FF2E4C80
  --accent-hover:        #FF5670
  --accent-pressed:      #D4233D
  --accent-glow:         0 0 20px #FF2E4C40

SEMANTIC
──────────────────
--success:             #22C55E
--warning:             #F59E0B
--error:               #EF4444
--info:                var(--accent-primary)

TIER LIST COLORS
──────────────────
--tier-s:              #FF2E4C      /* S-tier — red/gold */
--tier-a:              #FF6B2C      /* A-tier — orange */
--tier-b:              #F59E0B      /* B-tier — yellow */
--tier-c:              #22C55E      /* C-tier — green */
--tier-d:              #3B82F6      /* D-tier — blue */
--tier-f:              #6B7280      /* F-tier — gray */

LIGHT MODE
──────────────────
--bg-primary:          #F5F5FA
--bg-secondary:        #FFFFFF
--bg-tertiary:         #EAEAF0
--text-primary:        #1A1A28
--text-secondary:      #6A6A7A
--border-subtle:       rgba(0,0,0,0.06)
--border-default:      rgba(0,0,0,0.10)
/* Accents remain the same but with reduced glow intensity */
```

### 2.3 Typography

```
FONT STACK
═══════════════════════════════════════════════════

Display / Headlines:
  Family: "Orbitron", sans-serif
  Source: Google Fonts
  Weights: 700 (Bold), 900 (Black)
  Usage: Page titles, section headers, hero text, logo fallback
  Letter-spacing: 0.02em — 0.06em (tighter at large sizes)
  Text-transform: uppercase for H1-H2

Body / Content:
  Family: "Exo 2", sans-serif
  Source: Google Fonts
  Weights: 400 (Regular), 500 (Medium), 600 (SemiBold), 700 (Bold)
  Usage: All body text, guide content, UI labels
  Line-height: 1.7 for guide reading, 1.5 for UI

Monospace / Code / Stats:
  Family: "JetBrains Mono", monospace
  Source: Google Fonts
  Weights: 400, 500
  Usage: Stat values, damage numbers, code snippets, keyboard shortcuts

TYPE SCALE (desktop / mobile)
──────────────────
--text-hero:       clamp(2.5rem, 5vw, 4rem)    /* Hero headlines */
--text-h1:         clamp(1.75rem, 3vw, 2.5rem)  /* Page titles */
--text-h2:         clamp(1.375rem, 2.5vw, 1.75rem)
--text-h3:         clamp(1.125rem, 2vw, 1.375rem)
--text-h4:         clamp(1rem, 1.5vw, 1.125rem)
--text-body:       1rem (16px)
--text-body-sm:    0.875rem (14px)
--text-caption:    0.75rem (12px)
--text-overline:   0.6875rem (11px), letter-spacing: 0.1em, uppercase
```

### 2.4 Spacing & Layout Density

Three density modes the user can toggle:

```
COMPACT (Content-dense, power user):
  --spacing-unit: 4px
  --card-padding: 12px
  --section-gap: 24px
  --sidebar-width: 260px
  Guide paragraphs: line-height 1.5

DEFAULT (Balanced):
  --spacing-unit: 8px
  --card-padding: 20px
  --section-gap: 40px
  --sidebar-width: 300px
  Guide paragraphs: line-height 1.7

SPACIOUS (Relaxed reading):
  --spacing-unit: 12px
  --card-padding: 28px
  --section-gap: 56px
  --sidebar-width: 340px
  Guide paragraphs: line-height 1.9
```

### 2.5 Motion & Animation

```
ANIMATION TOKENS
═══════════════════════════════════════════════════

--ease-out-expo:    cubic-bezier(0.16, 1, 0.3, 1)
--ease-in-out:      cubic-bezier(0.65, 0, 0.35, 1)
--duration-fast:    150ms
--duration-default: 300ms
--duration-slow:    500ms
--duration-hero:    800ms

ANIMATION PATTERNS:
1. Page Load — staggered fade-up with blur (opacity 0 → 1, translateY 20px → 0, filter blur 4px → 0)
2. Card Hover — scale(1.02) + elevated shadow + border-accent glow
3. Navigation — slide-in from right (mobile drawer), fade (desktop dropdown)
4. Spoiler Toggle — height auto animation via grid-template-rows 0fr → 1fr
5. Progress Bar — width transition with ease-out-expo
6. Checklist Check — checkbox scale bounce + strikethrough sweep left-to-right
7. Scroll Reveal — IntersectionObserver triggered fade-up for guide sections
8. Hero Background — subtle parallax (translateY at 0.3x scroll speed)
9. RGB Border Sweep — animated gradient border on featured cards via @property and conic-gradient
10. Page Transitions — Next.js View Transitions API for smooth cross-page navigation

REDUCED MOTION:
@media (prefers-reduced-motion: reduce) → disable all transforms, reduce to opacity-only fades
```

### 2.6 Iconography & UI Elements

```
ICON SYSTEM
──────────────────
Library: Lucide React (consistent stroke-based icons, tree-shakable)
Size: 16px (inline), 20px (buttons), 24px (navigation), 32px (feature icons)
Stroke: 1.5px (default), 2px (emphasis)

CUSTOM ICONS NEEDED:
- Platform badges: PlayStation, Xbox, PC, Switch, Mobile
- Content type badges: Walkthrough, Build Guide, Tier List, Boss Guide, Tips
- Difficulty indicators: Easy (1 skull), Medium (2 skulls), Hard (3 skulls), Extreme (4 skulls)
- Progress indicators: Percentage circle, step counter

HUD-STYLE UI ELEMENTS:
- Corner brackets: CSS pseudo-elements creating tactical bracket corners on cards
  .hud-brackets::before, ::after {
    content: '';
    position: absolute;
    width: 12px; height: 12px;
    border: 2px solid var(--accent-primary);
    /* top-left: border-top + border-left, etc. */
  }
- Scan-line overlay: repeating-linear-gradient for retro CRT feel on hero images
- Glitch text effect: CSS keyframe animation with clip-path for page titles
- Dot grid background: radial-gradient pattern for empty states
```

---

## 3. Tech Stack Deep-Dive

### 3.1 Core Stack Decision Matrix

| Layer | Technology | Version | Why |
|---|---|---|---|
| **Framework** | Next.js (App Router) | 15.x (latest) | RSC, streaming, ISR, image optimization, View Transitions |
| **CMS** | Payload CMS | 3.x | Runs inside Next.js (single deploy), TypeScript-native, extensible |
| **Database** | MongoDB Atlas | M10+ cluster | Payload's primary DB adapter, flexible schemas, Atlas Search for full-text |
| **Image CDN** | Bunny CDN | Pull Zone | Global edge delivery, WebP/AVIF auto-conversion, lazy loading |
| **Hosting** | Vercel | Pro plan | Edge functions, ISR, preview deployments, analytics |
| **Styling** | Tailwind CSS | 4.x | Utility-first, JIT, custom theme integration |
| **State** | Zustand | 5.x | Lightweight client state for theme/checklist/progress |
| **Auth** | Payload Auth (built-in) | — | Cookie-based sessions, email/password + OAuth adapters |
| **Search** | MongoDB Atlas Search | — | Full-text search index on guides, zero additional infra |
| **Email** | Resend | — | Transactional emails (password reset, premium receipt) |
| **Payments** | Stripe | — | Premium guide purchases, subscription potential |
| **Analytics** | Vercel Analytics + Plausible | — | Privacy-friendly, no cookie banner needed |

### 3.2 Why Payload CMS 3.x Inside Next.js?

Payload CMS 3.x runs as a Next.js plugin — there is no separate CMS server. This gives you:

1. **Single deployment** — One Vercel project, one `next build`, one URL
2. **Shared types** — Payload generates TypeScript types from collections, usable in frontend components
3. **Server Components integration** — Call `payload.find()` directly inside RSC (no REST/GraphQL overhead)
4. **Admin UI** — Auto-generated at `/admin` route, fully customizable with React
5. **Built-in auth** — Users collection with session management
6. **Media management** — Upload collection with hooks for Bunny CDN sync
7. **Versioning & drafts** — Built-in content versioning for guides
8. **Access control** — Row-level and field-level permissions
9. **Live preview** — Edit content and see changes in real-time

### 3.3 Runtime Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         VERCEL                               │
│  ┌────────────────────────────────────────────────────────┐  │
│  │              Next.js 15 Application                    │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │  │
│  │  │  App Router   │  │ Payload CMS  │  │  API Routes  │ │  │
│  │  │  (Frontend)   │  │  (/admin)    │  │  (/api/*)    │ │  │
│  │  │  RSC + Client │  │  Admin UI    │  │  Search,     │ │  │
│  │  │  Components   │  │  Collections │  │  Webhooks,   │ │  │
│  │  │              │  │  Globals     │  │  Stripe      │ │  │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘ │  │
│  │         │                  │                  │         │  │
│  │         └──────────────────┼──────────────────┘         │  │
│  │                            │                            │  │
│  │                   Payload Local API                     │  │
│  │                     payload.find()                      │  │
│  │                     payload.create()                    │  │
│  └────────────────────────┬───────────────────────────────┘  │
│                            │                                  │
│  ┌────────────┐  ┌────────┴────────┐  ┌──────────────────┐  │
│  │ Vercel KV  │  │  MongoDB Atlas  │  │  Vercel Blob     │  │
│  │ (sessions, │  │  (all content,  │  │  (temp upload    │  │
│  │  rate-limit)│  │   users, media │  │   before Bunny)  │  │
│  │            │  │   metadata)     │  │                  │  │
│  └────────────┘  └─────────────────┘  └────────┬─────────┘  │
│                                                 │            │
└─────────────────────────────────────────────────┼────────────┘
                                                  │
                                    ┌─────────────┴──────────┐
                                    │       Bunny CDN        │
                                    │    (image storage &    │
                                    │     edge delivery)     │
                                    │   cdn.gamesaiyan.com   │
                                    └────────────────────────┘

External Services:
  ├── Stripe (payments)
  ├── Resend (transactional email)
  ├── Plausible / Vercel Analytics
  └── Google Search Console / Indexing API
```

---

## 4. Project Structure & Architecture

### 4.1 Directory Layout

```
gamesaiyan/
├── .env.local                          # Local environment variables
├── .env.production                     # Production env vars (Vercel)
├── next.config.ts                      # Next.js + Payload plugin config
├── payload.config.ts                   # Payload CMS root configuration
├── tailwind.config.ts                  # Tailwind theme (maps to design tokens)
├── tsconfig.json
├── package.json
│
├── public/
│   ├── fonts/                          # Self-hosted font files (woff2)
│   │   ├── Orbitron-Bold.woff2
│   │   ├── Orbitron-Black.woff2
│   │   ├── Exo2-Regular.woff2
│   │   ├── Exo2-Medium.woff2
│   │   ├── Exo2-SemiBold.woff2
│   │   ├── Exo2-Bold.woff2
│   │   └── JetBrainsMono-Regular.woff2
│   ├── icons/                          # Platform & content type SVG icons
│   ├── og-default.jpg                  # Default OG image (1200×630)
│   ├── robots.txt                      # Managed via next.config metadata
│   └── sitemap.xml                     # Auto-generated by Next.js
│
├── src/
│   ├── app/
│   │   ├── (frontend)/                 # Route group — public-facing site
│   │   │   ├── layout.tsx              # Root layout: fonts, theme provider, nav, footer
│   │   │   ├── page.tsx                # Homepage
│   │   │   ├── loading.tsx             # Global loading skeleton
│   │   │   ├── not-found.tsx           # 404 page
│   │   │   ├── error.tsx               # Error boundary
│   │   │   │
│   │   │   ├── games/
│   │   │   │   ├── page.tsx            # Browse all games (category/browse page)
│   │   │   │   └── [gameSlug]/
│   │   │   │       ├── page.tsx        # Game hub page (single game)
│   │   │   │       └── [guideSlug]/
│   │   │   │           └── page.tsx    # Individual guide/walkthrough article
│   │   │   │
│   │   │   ├── tier-lists/
│   │   │   │   ├── page.tsx            # Browse all tier lists
│   │   │   │   └── [slug]/
│   │   │   │       └── page.tsx        # Individual tier list
│   │   │   │
│   │   │   ├── builds/
│   │   │   │   ├── page.tsx            # Browse all builds
│   │   │   │   └── [slug]/
│   │   │   │       └── page.tsx        # Individual build guide
│   │   │   │
│   │   │   ├── search/
│   │   │   │   └── page.tsx            # Search results page
│   │   │   │
│   │   │   ├── about/
│   │   │   │   └── page.tsx            # About / contact
│   │   │   │
│   │   │   └── premium/
│   │   │       ├── page.tsx            # Premium guides listing
│   │   │       └── [slug]/
│   │   │           └── page.tsx        # Premium guide (paywall gated)
│   │   │
│   │   ├── (auth)/                     # Route group — auth pages
│   │   │   ├── login/page.tsx
│   │   │   ├── register/page.tsx
│   │   │   └── forgot-password/page.tsx
│   │   │
│   │   ├── (payload)/                  # Route group — Payload admin
│   │   │   └── admin/
│   │   │       └── [[...segments]]/
│   │   │           └── page.tsx        # Payload admin catch-all
│   │   │
│   │   ├── api/
│   │   │   ├── search/route.ts         # Search API endpoint (Atlas Search)
│   │   │   ├── checklist/route.ts      # Save/load user checklists
│   │   │   ├── stripe-webhook/route.ts # Stripe payment webhook
│   │   │   ├── revalidate/route.ts     # On-demand ISR revalidation
│   │   │   └── [...payload]/route.ts   # Payload REST API catch-all
│   │   │
│   │   ├── sitemap.ts                  # Dynamic sitemap generation
│   │   └── robots.ts                   # Dynamic robots.txt
│   │
│   ├── collections/                    # Payload CMS collections
│   │   ├── Games.ts
│   │   ├── Guides.ts
│   │   ├── TierLists.ts
│   │   ├── Builds.ts
│   │   ├── Categories.ts
│   │   ├── Tags.ts
│   │   ├── Authors.ts
│   │   ├── Media.ts
│   │   ├── Users.ts
│   │   ├── Purchases.ts
│   │   └── AdSlots.ts
│   │
│   ├── globals/                        # Payload CMS globals
│   │   ├── SiteSettings.ts
│   │   ├── Navigation.ts
│   │   └── Footer.ts
│   │
│   ├── blocks/                         # Payload rich text blocks
│   │   ├── SpoilerBlock.ts
│   │   ├── ChecklistBlock.ts
│   │   ├── TipBlock.ts                 # Pro tips, warnings, notes
│   │   ├── ImageGalleryBlock.ts
│   │   ├── VideoEmbedBlock.ts
│   │   ├── ComparisonTableBlock.ts
│   │   ├── ItemCardBlock.ts            # In-game item/weapon card
│   │   ├── StatBlock.ts                # Character stat table
│   │   └── AffiliateBlock.ts           # Affiliate product callout
│   │
│   ├── components/
│   │   ├── layout/
│   │   │   ├── Header.tsx              # Global nav, search, theme toggle
│   │   │   ├── Footer.tsx
│   │   │   ├── MobileNav.tsx           # Slide-out mobile drawer
│   │   │   ├── Sidebar.tsx             # Guide sidebar (TOC + ads)
│   │   │   └── Breadcrumbs.tsx
│   │   │
│   │   ├── ui/                         # Atomic UI components
│   │   │   ├── Button.tsx
│   │   │   ├── Card.tsx                # Base card with HUD brackets
│   │   │   ├── GameCard.tsx            # Game hub card (image + title + count)
│   │   │   ├── GuideCard.tsx           # Guide list card (image + meta)
│   │   │   ├── Badge.tsx               # Platform, difficulty, category badges
│   │   │   ├── SearchBar.tsx           # Command palette style (⌘K)
│   │   │   ├── SearchResults.tsx
│   │   │   ├── Skeleton.tsx            # Loading skeletons
│   │   │   ├── Pagination.tsx
│   │   │   ├── Tag.tsx
│   │   │   └── Modal.tsx
│   │   │
│   │   ├── guide/                      # Guide-specific components
│   │   │   ├── ReadingProgressBar.tsx   # Sticky top progress indicator
│   │   │   ├── SpoilerToggle.tsx        # Collapsible spoiler content
│   │   │   ├── InteractiveChecklist.tsx # Checkable steps with persistence
│   │   │   ├── TableOfContents.tsx      # Scrollspy TOC sidebar
│   │   │   ├── GuideMeta.tsx            # Author, date, reading time, difficulty
│   │   │   ├── RelatedGuides.tsx        # "Continue reading" section
│   │   │   └── GuideNavigation.tsx      # Prev/Next guide navigation
│   │   │
│   │   ├── tier-list/
│   │   │   ├── TierListView.tsx         # Interactive tier list display
│   │   │   ├── TierRow.tsx              # Single tier (S/A/B/C/D/F)
│   │   │   └── TierItem.tsx             # Draggable tier item
│   │   │
│   │   ├── home/
│   │   │   ├── HeroSection.tsx          # Full-bleed hero with search
│   │   │   ├── FeaturedGuides.tsx       # Editor's picks carousel
│   │   │   ├── LatestGuides.tsx         # Recent guides grid
│   │   │   ├── GameGrid.tsx             # Browse by game
│   │   │   └── TrendingSection.tsx      # Trending/popular content
│   │   │
│   │   ├── ads/
│   │   │   ├── AdSlot.tsx               # Configurable ad placement
│   │   │   ├── StickyAdRail.tsx         # Sidebar sticky ad
│   │   │   └── InContentAd.tsx          # Between-paragraph ad
│   │   │
│   │   ├── premium/
│   │   │   ├── PaywallGate.tsx          # Content preview + purchase CTA
│   │   │   ├── PurchaseButton.tsx       # Stripe checkout trigger
│   │   │   └── PremiumBadge.tsx         # Crown icon badge
│   │   │
│   │   └── theme/
│   │       ├── ThemeProvider.tsx         # Context provider for theme state
│   │       ├── ThemeToggle.tsx           # Dark/Light switch
│   │       ├── AccentPicker.tsx          # Color accent selector
│   │       └── DensityToggle.tsx         # Layout density switcher
│   │
│   ├── hooks/
│   │   ├── useReadingProgress.ts        # Scroll-based reading progress
│   │   ├── useChecklist.ts              # Checklist state + localStorage persistence
│   │   ├── useScrollSpy.ts              # Active section detection for TOC
│   │   ├── useSearch.ts                 # Debounced search with Atlas Search
│   │   ├── useTheme.ts                  # Theme state hook
│   │   └── useIntersection.ts           # Scroll reveal trigger
│   │
│   ├── lib/
│   │   ├── payload.ts                   # Payload client initialization
│   │   ├── mongodb.ts                   # Direct MongoDB connection (for search)
│   │   ├── bunny.ts                     # Bunny CDN upload/URL helpers
│   │   ├── stripe.ts                    # Stripe client + helpers
│   │   ├── seo.ts                       # SEO metadata generator functions
│   │   ├── reading-time.ts              # Estimate reading time from rich text
│   │   └── utils.ts                     # General utilities (cn, formatDate, etc.)
│   │
│   ├── store/
│   │   ├── themeStore.ts                # Zustand store: theme, accent, density
│   │   └── checklistStore.ts            # Zustand store: guide checklist state
│   │
│   ├── types/
│   │   ├── payload-types.ts             # Auto-generated by Payload
│   │   └── index.ts                     # Shared custom types
│   │
│   └── styles/
│       ├── globals.css                  # Tailwind directives + CSS variables
│       ├── fonts.css                    # @font-face declarations
│       └── animations.css              # Keyframe animations
│
└── scripts/
    ├── seed.ts                          # Seed database with sample data
    └── generate-sitemap.ts              # Backup sitemap generator
```

---

## 5. Payload CMS Configuration

### 5.1 Root Config — `payload.config.ts`

```typescript
import { buildConfig } from 'payload'
import { mongooseAdapter } from '@payloadcms/db-mongodb'
import { lexicalEditor } from '@payloadcms/richtext-lexical'
import { vercelBlobStorage } from '@payloadcms/storage-vercel-blob'
import path from 'path'

// Collections
import { Games } from './collections/Games'
import { Guides } from './collections/Guides'
import { TierLists } from './collections/TierLists'
import { Builds } from './collections/Builds'
import { Categories } from './collections/Categories'
import { Tags } from './collections/Tags'
import { Authors } from './collections/Authors'
import { Media } from './collections/Media'
import { Users } from './collections/Users'
import { Purchases } from './collections/Purchases'
import { AdSlots } from './collections/AdSlots'

// Globals
import { SiteSettings } from './globals/SiteSettings'
import { Navigation } from './globals/Navigation'
import { Footer } from './globals/Footer'

// Custom Blocks
import { SpoilerBlock } from './blocks/SpoilerBlock'
import { ChecklistBlock } from './blocks/ChecklistBlock'
import { TipBlock } from './blocks/TipBlock'
// ... other blocks

export default buildConfig({
  serverURL: process.env.NEXT_PUBLIC_SERVER_URL || 'http://localhost:3000',

  admin: {
    user: Users.slug,
    meta: {
      titleSuffix: '— GameSaiyan Admin',
      favicon: '/icons/favicon.ico',
      ogImage: '/og-default.jpg',
    },
    components: {
      // Custom admin dashboard widgets can go here
    },
  },

  editor: lexicalEditor({
    features: ({ defaultFeatures }) => [
      ...defaultFeatures,
      // Custom features for game guide content
    ],
  }),

  collections: [
    Games,
    Guides,
    TierLists,
    Builds,
    Categories,
    Tags,
    Authors,
    Media,
    Users,
    Purchases,
    AdSlots,
  ],

  globals: [SiteSettings, Navigation, Footer],

  db: mongooseAdapter({
    url: process.env.MONGODB_URI!,
    // connectOptions for Atlas
  }),

  // Temporary upload storage (before Bunny CDN sync)
  upload: {
    limits: {
      fileSize: 10_000_000, // 10MB max
    },
  },

  plugins: [
    vercelBlobStorage({
      collections: {
        media: true,
      },
      token: process.env.BLOB_READ_WRITE_TOKEN!,
    }),
  ],

  typescript: {
    outputFile: path.resolve(__dirname, 'types/payload-types.ts'),
  },
})
```

### 5.2 Collection Definitions

#### `Games.ts`

```typescript
import type { CollectionConfig } from 'payload'

export const Games: CollectionConfig = {
  slug: 'games',
  admin: {
    useAsTitle: 'title',
    defaultColumns: ['title', 'platform', 'status', 'updatedAt'],
    group: 'Content',
  },
  access: {
    read: () => true,  // Public
    create: ({ req }) => req.user?.role === 'admin' || req.user?.role === 'editor',
    update: ({ req }) => req.user?.role === 'admin' || req.user?.role === 'editor',
    delete: ({ req }) => req.user?.role === 'admin',
  },
  fields: [
    { name: 'title', type: 'text', required: true },
    { name: 'slug', type: 'text', required: true, unique: true, admin: { position: 'sidebar' } },
    {
      name: 'coverImage',
      type: 'upload',
      relationTo: 'media',
      required: true,
    },
    {
      name: 'heroImage',
      type: 'upload',
      relationTo: 'media',
      admin: { description: 'Full-bleed hero image for game hub page (1920×600 recommended)' },
    },
    { name: 'description', type: 'textarea', required: true },
    {
      name: 'platform',
      type: 'select',
      hasMany: true,
      options: [
        { label: 'PlayStation 5', value: 'ps5' },
        { label: 'PlayStation 4', value: 'ps4' },
        { label: 'Xbox Series X/S', value: 'xsx' },
        { label: 'Xbox One', value: 'xone' },
        { label: 'Nintendo Switch', value: 'switch' },
        { label: 'PC', value: 'pc' },
        { label: 'Mobile', value: 'mobile' },
      ],
    },
    {
      name: 'genre',
      type: 'select',
      hasMany: true,
      options: [
        { label: 'RPG', value: 'rpg' },
        { label: 'Action', value: 'action' },
        { label: 'Strategy', value: 'strategy' },
        { label: 'Survival', value: 'survival' },
        { label: 'FPS', value: 'fps' },
        { label: 'MMO', value: 'mmo' },
        { label: 'Souls-like', value: 'soulslike' },
        { label: 'Open World', value: 'openworld' },
        { label: 'JRPG', value: 'jrpg' },
        { label: 'Roguelike', value: 'roguelike' },
        { label: 'MOBA', value: 'moba' },
        { label: 'Gacha', value: 'gacha' },
      ],
    },
    { name: 'releaseDate', type: 'date' },
    { name: 'developer', type: 'text' },
    { name: 'publisher', type: 'text' },
    {
      name: 'status',
      type: 'select',
      defaultValue: 'active',
      options: [
        { label: 'Active', value: 'active' },
        { label: 'Completed', value: 'completed' },
        { label: 'Upcoming', value: 'upcoming' },
      ],
      admin: { position: 'sidebar' },
    },
    {
      name: 'guideCount',
      type: 'number',
      admin: { readOnly: true, description: 'Auto-calculated' },
    },

    // SEO Fields Group
    {
      name: 'seo',
      type: 'group',
      fields: [
        { name: 'metaTitle', type: 'text', maxLength: 60 },
        { name: 'metaDescription', type: 'textarea', maxLength: 160 },
        { name: 'ogImage', type: 'upload', relationTo: 'media' },
        { name: 'canonicalUrl', type: 'text' },
        { name: 'noIndex', type: 'checkbox', defaultValue: false },
      ],
    },
  ],
  hooks: {
    beforeChange: [
      // Auto-generate slug from title if not set
      ({ data }) => {
        if (data.title && !data.slug) {
          data.slug = data.title.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/(^-|-$)/g, '')
        }
        return data
      },
    ],
  },
}
```

#### `Guides.ts`

```typescript
import type { CollectionConfig } from 'payload'

export const Guides: CollectionConfig = {
  slug: 'guides',
  admin: {
    useAsTitle: 'title',
    defaultColumns: ['title', 'game', 'category', 'status', 'publishedAt'],
    group: 'Content',
  },
  versions: {
    drafts: {
      autosave: { interval: 30000 }, // 30-second autosave
    },
    maxPerDoc: 10,
  },
  access: {
    read: ({ req }) => {
      // Premium guides: only show to purchasers or in preview mode
      return {
        or: [
          { isPremium: { equals: false } },
          { 'purchasedBy': { contains: req.user?.id } },
          // Admin/editors can always read
        ],
      }
    },
    create: ({ req }) => req.user?.role === 'admin' || req.user?.role === 'editor',
    update: ({ req }) => req.user?.role === 'admin' || req.user?.role === 'editor',
    delete: ({ req }) => req.user?.role === 'admin',
  },
  fields: [
    { name: 'title', type: 'text', required: true, maxLength: 100 },
    { name: 'slug', type: 'text', required: true, unique: true },
    { name: 'subtitle', type: 'text', maxLength: 200 },

    // Relationships
    { name: 'game', type: 'relationship', relationTo: 'games', required: true },
    { name: 'author', type: 'relationship', relationTo: 'authors', required: true },
    {
      name: 'category',
      type: 'select',
      required: true,
      options: [
        { label: 'Walkthrough', value: 'walkthrough' },
        { label: 'Boss Guide', value: 'boss-guide' },
        { label: 'Build Guide', value: 'build-guide' },
        { label: 'Tips & Tricks', value: 'tips' },
        { label: 'Collectibles', value: 'collectibles' },
        { label: 'Side Quests', value: 'side-quests' },
        { label: 'Beginners Guide', value: 'beginners' },
        { label: 'Endgame', value: 'endgame' },
        { label: 'Patch Notes', value: 'patch-notes' },
      ],
    },
    { name: 'tags', type: 'relationship', relationTo: 'tags', hasMany: true },

    // Content
    {
      name: 'heroImage',
      type: 'upload',
      relationTo: 'media',
      required: true,
      admin: { description: '16:9 ratio, minimum 1200×675px' },
    },
    { name: 'excerpt', type: 'textarea', required: true, maxLength: 300 },
    {
      name: 'content',
      type: 'richText',
      required: true,
      // Lexical editor with custom blocks
      admin: {
        description: 'Full guide content. Use blocks for spoilers, checklists, tips, etc.',
      },
    },

    // Checklist Data (structured)
    {
      name: 'checklistSteps',
      type: 'array',
      admin: { description: 'Ordered steps that users can mark as complete' },
      fields: [
        { name: 'stepId', type: 'text', required: true },
        { name: 'label', type: 'text', required: true },
        { name: 'section', type: 'text' },
        { name: 'isOptional', type: 'checkbox', defaultValue: false },
      ],
    },

    // Guide metadata
    {
      name: 'difficulty',
      type: 'select',
      options: [
        { label: 'Easy', value: 'easy' },
        { label: 'Medium', value: 'medium' },
        { label: 'Hard', value: 'hard' },
        { label: 'Extreme', value: 'extreme' },
      ],
    },
    { name: 'estimatedTime', type: 'text', admin: { description: 'e.g., "2-3 hours"' } },
    { name: 'readingTime', type: 'number', admin: { readOnly: true, description: 'Auto-calculated minutes' } },

    // Premium
    {
      name: 'isPremium',
      type: 'checkbox',
      defaultValue: false,
      admin: { position: 'sidebar' },
    },
    {
      name: 'price',
      type: 'number',
      admin: {
        condition: (data) => data.isPremium,
        description: 'Price in USD cents (e.g., 499 = $4.99)',
      },
    },
    {
      name: 'previewPercentage',
      type: 'number',
      defaultValue: 30,
      admin: {
        condition: (data) => data.isPremium,
        description: 'Percentage of content shown for free (0-100)',
      },
    },

    // Navigation
    {
      name: 'prevGuide',
      type: 'relationship',
      relationTo: 'guides',
      admin: { description: 'Previous guide in walkthrough sequence' },
    },
    {
      name: 'nextGuide',
      type: 'relationship',
      relationTo: 'guides',
      admin: { description: 'Next guide in walkthrough sequence' },
    },

    // Publishing
    { name: 'publishedAt', type: 'date', admin: { position: 'sidebar' } },
    {
      name: 'status',
      type: 'select',
      defaultValue: 'draft',
      options: [
        { label: 'Draft', value: 'draft' },
        { label: 'Published', value: 'published' },
        { label: 'Archived', value: 'archived' },
      ],
      admin: { position: 'sidebar' },
    },
    { name: 'featured', type: 'checkbox', defaultValue: false, admin: { position: 'sidebar' } },
    { name: 'viewCount', type: 'number', defaultValue: 0, admin: { readOnly: true, position: 'sidebar' } },

    // SEO
    {
      name: 'seo',
      type: 'group',
      fields: [
        { name: 'metaTitle', type: 'text', maxLength: 60 },
        { name: 'metaDescription', type: 'textarea', maxLength: 160 },
        { name: 'ogImage', type: 'upload', relationTo: 'media' },
        { name: 'canonicalUrl', type: 'text' },
        { name: 'focusKeyword', type: 'text' },
        { name: 'noIndex', type: 'checkbox', defaultValue: false },
      ],
    },
  ],

  hooks: {
    beforeChange: [
      // Auto-slug, auto-reading-time, auto-publish-date
      async ({ data, operation }) => {
        if (data.title && !data.slug) {
          data.slug = data.title.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/(^-|-$)/g, '')
        }
        if (operation === 'create' && data.status === 'published' && !data.publishedAt) {
          data.publishedAt = new Date().toISOString()
        }
        // Reading time calculation would go here
        return data
      },
    ],
    afterChange: [
      // Trigger ISR revalidation
      async ({ doc }) => {
        if (doc.status === 'published') {
          await fetch(`${process.env.NEXT_PUBLIC_SERVER_URL}/api/revalidate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              secret: process.env.REVALIDATION_SECRET,
              paths: [
                `/games/${doc.game.slug}/${doc.slug}`,
                `/games/${doc.game.slug}`,
                '/',
              ],
            }),
          })
        }
      },
    ],
  },
}
```

#### `Media.ts`

```typescript
import type { CollectionConfig } from 'payload'

export const Media: CollectionConfig = {
  slug: 'media',
  upload: {
    staticDir: 'media',  // Temporary local dir
    mimeTypes: ['image/jpeg', 'image/png', 'image/webp', 'image/avif', 'image/gif', 'image/svg+xml'],
    imageSizes: [
      { name: 'thumbnail', width: 300, height: 200, position: 'centre', formatOptions: { format: 'webp', options: { quality: 80 } } },
      { name: 'card', width: 600, height: 400, position: 'centre', formatOptions: { format: 'webp', options: { quality: 80 } } },
      { name: 'hero', width: 1200, height: 675, position: 'centre', formatOptions: { format: 'webp', options: { quality: 85 } } },
      { name: 'full', width: 1920, height: undefined, position: 'centre', formatOptions: { format: 'webp', options: { quality: 85 } } },
    ],
    adminThumbnail: 'thumbnail',
  },
  access: {
    read: () => true,
  },
  fields: [
    { name: 'alt', type: 'text', required: true },
    { name: 'caption', type: 'text' },
    {
      name: 'bunnyUrl',
      type: 'text',
      admin: { readOnly: true, description: 'Auto-populated after Bunny CDN sync' },
    },
    {
      name: 'source',
      type: 'text',
      admin: { description: 'Credit / source attribution for the image' },
    },
  ],
  hooks: {
    afterChange: [
      // Sync to Bunny CDN after upload
      async ({ doc, operation }) => {
        if (operation === 'create' || operation === 'update') {
          // Upload to Bunny CDN and update bunnyUrl
          // See Section 7 for implementation
        }
      },
    ],
  },
}
```

#### `TierLists.ts`

```typescript
import type { CollectionConfig } from 'payload'

export const TierLists: CollectionConfig = {
  slug: 'tier-lists',
  admin: {
    useAsTitle: 'title',
    group: 'Content',
  },
  fields: [
    { name: 'title', type: 'text', required: true },
    { name: 'slug', type: 'text', required: true, unique: true },
    { name: 'game', type: 'relationship', relationTo: 'games', required: true },
    { name: 'author', type: 'relationship', relationTo: 'authors', required: true },
    { name: 'heroImage', type: 'upload', relationTo: 'media' },
    { name: 'description', type: 'textarea', required: true },
    { name: 'lastUpdated', type: 'date' },
    { name: 'patchVersion', type: 'text', admin: { description: 'e.g., "Patch 5.2"' } },
    {
      name: 'tiers',
      type: 'array',
      required: true,
      fields: [
        {
          name: 'tier',
          type: 'select',
          required: true,
          options: [
            { label: 'S — Must Pick', value: 'S' },
            { label: 'A — Excellent', value: 'A' },
            { label: 'B — Good', value: 'B' },
            { label: 'C — Average', value: 'C' },
            { label: 'D — Below Average', value: 'D' },
            { label: 'F — Avoid', value: 'F' },
          ],
        },
        { name: 'description', type: 'text' },
        {
          name: 'items',
          type: 'array',
          fields: [
            { name: 'name', type: 'text', required: true },
            { name: 'image', type: 'upload', relationTo: 'media' },
            { name: 'notes', type: 'textarea' },
            { name: 'tags', type: 'text', hasMany: true },
          ],
        },
      ],
    },
    {
      name: 'methodology',
      type: 'richText',
      admin: { description: 'How the tier list was determined' },
    },
    // SEO group (same pattern as Guides)
    {
      name: 'seo',
      type: 'group',
      fields: [
        { name: 'metaTitle', type: 'text', maxLength: 60 },
        { name: 'metaDescription', type: 'textarea', maxLength: 160 },
        { name: 'ogImage', type: 'upload', relationTo: 'media' },
      ],
    },
    { name: 'status', type: 'select', defaultValue: 'draft', options: [
      { label: 'Draft', value: 'draft' },
      { label: 'Published', value: 'published' },
    ]},
    { name: 'publishedAt', type: 'date' },
  ],
}
```

#### `Authors.ts`

```typescript
import type { CollectionConfig } from 'payload'

export const Authors: CollectionConfig = {
  slug: 'authors',
  admin: { useAsTitle: 'name', group: 'People' },
  fields: [
    { name: 'name', type: 'text', required: true },
    { name: 'slug', type: 'text', required: true, unique: true },
    { name: 'avatar', type: 'upload', relationTo: 'media' },
    { name: 'bio', type: 'textarea' },
    { name: 'role', type: 'text', admin: { description: 'e.g., "Senior Guide Writer"' } },
    {
      name: 'social',
      type: 'group',
      fields: [
        { name: 'twitter', type: 'text' },
        { name: 'youtube', type: 'text' },
        { name: 'twitch', type: 'text' },
        { name: 'discord', type: 'text' },
      ],
    },
    { name: 'user', type: 'relationship', relationTo: 'users' },
  ],
}
```

#### `Users.ts`

```typescript
import type { CollectionConfig } from 'payload'

export const Users: CollectionConfig = {
  slug: 'users',
  auth: {
    tokenExpiration: 60 * 60 * 24 * 7, // 7 days
    cookies: {
      secure: process.env.NODE_ENV === 'production',
      sameSite: 'lax',
    },
  },
  admin: {
    useAsTitle: 'email',
    group: 'People',
  },
  fields: [
    { name: 'displayName', type: 'text' },
    {
      name: 'role',
      type: 'select',
      defaultValue: 'reader',
      options: [
        { label: 'Admin', value: 'admin' },
        { label: 'Editor', value: 'editor' },
        { label: 'Reader', value: 'reader' },
      ],
      admin: { position: 'sidebar' },
      access: {
        update: ({ req }) => req.user?.role === 'admin',
      },
    },
    {
      name: 'purchases',
      type: 'relationship',
      relationTo: 'purchases',
      hasMany: true,
      admin: { readOnly: true },
    },
    {
      name: 'checklistProgress',
      type: 'json',
      admin: { description: 'JSON object mapping guideId → completed step IDs' },
    },
    {
      name: 'themePreferences',
      type: 'group',
      fields: [
        { name: 'mode', type: 'select', options: ['dark', 'light', 'system'] },
        { name: 'accent', type: 'text' },
        { name: 'density', type: 'select', options: ['compact', 'default', 'spacious'] },
      ],
    },
  ],
}
```

### 5.3 Globals

#### `SiteSettings.ts`

```typescript
import type { GlobalConfig } from 'payload'

export const SiteSettings: GlobalConfig = {
  slug: 'site-settings',
  admin: { group: 'Settings' },
  fields: [
    { name: 'siteName', type: 'text', defaultValue: 'GameSaiyan' },
    { name: 'tagline', type: 'text', defaultValue: 'Level Up Your Gameplay' },
    { name: 'siteUrl', type: 'text', defaultValue: 'https://gamesaiyan.com' },
    { name: 'logo', type: 'upload', relationTo: 'media' },
    { name: 'favicon', type: 'upload', relationTo: 'media' },
    { name: 'defaultOgImage', type: 'upload', relationTo: 'media' },
    {
      name: 'seo',
      type: 'group',
      fields: [
        { name: 'defaultMetaTitle', type: 'text' },
        { name: 'defaultMetaDescription', type: 'textarea' },
        { name: 'googleSiteVerification', type: 'text' },
        { name: 'bingSiteVerification', type: 'text' },
      ],
    },
    {
      name: 'social',
      type: 'group',
      fields: [
        { name: 'twitter', type: 'text' },
        { name: 'youtube', type: 'text' },
        { name: 'discord', type: 'text' },
        { name: 'instagram', type: 'text' },
      ],
    },
    {
      name: 'ads',
      type: 'group',
      fields: [
        { name: 'adsEnabled', type: 'checkbox', defaultValue: true },
        { name: 'adNetworkScript', type: 'code', admin: { language: 'html' } },
        { name: 'adsTxtContent', type: 'textarea' },
      ],
    },
    {
      name: 'analytics',
      type: 'group',
      fields: [
        { name: 'plausibleDomain', type: 'text' },
        { name: 'googleAnalyticsId', type: 'text' },
      ],
    },
  ],
}
```

---

## 6. Database Schema (MongoDB Atlas)

### 6.1 Atlas Cluster Setup

1. **Cluster tier:** M10 (recommended for production) — shared vCPUs, 2GB RAM, 10GB storage
2. **Region:** AWS Mumbai (ap-south-1) or closest to target audience
3. **Backup:** Continuous backup enabled
4. **Network:** IP allowlist for Vercel's IP ranges + your dev machine; or use `0.0.0.0/0` behind MongoDB Atlas's built-in auth

### 6.2 Database Name

```
Database: gamesaiyan_production
```

### 6.3 Collections (Auto-created by Payload)

Payload auto-creates MongoDB collections from your `CollectionConfig`. The resulting documents follow your field schemas. Key collections:

```
gamesaiyan_production/
├── games
├── guides
├── tier-lists
├── builds
├── categories
├── tags
├── authors
├── media
├── users
├── purchases
├── ad-slots
├── payload-preferences      (Payload internal)
├── payload-migrations       (Payload internal)
├── site-settings             (Global — single document)
├── navigation                (Global — single document)
└── footer                    (Global — single document)
```

### 6.4 Required Indexes

Create these indexes in Atlas for performance:

```javascript
// Guides — most queried collection
db.guides.createIndex({ slug: 1 }, { unique: true })
db.guides.createIndex({ game: 1, status: 1, publishedAt: -1 })
db.guides.createIndex({ category: 1, status: 1 })
db.guides.createIndex({ featured: 1, status: 1, publishedAt: -1 })
db.guides.createIndex({ tags: 1 })
db.guides.createIndex({ status: 1, publishedAt: -1 })  // Latest guides query

// Games
db.games.createIndex({ slug: 1 }, { unique: true })
db.games.createIndex({ status: 1, title: 1 })

// Tier Lists
db['tier-lists'].createIndex({ slug: 1 }, { unique: true })
db['tier-lists'].createIndex({ game: 1, status: 1 })

// Users
db.users.createIndex({ email: 1 }, { unique: true })
```

### 6.5 Atlas Search Index (Full-Text Search)

Create an Atlas Search index on the `guides` collection for the search feature:

```json
{
  "name": "guides_search",
  "definition": {
    "mappings": {
      "dynamic": false,
      "fields": {
        "title": {
          "type": "string",
          "analyzer": "lucene.standard",
          "searchAnalyzer": "lucene.standard"
        },
        "excerpt": {
          "type": "string",
          "analyzer": "lucene.standard"
        },
        "content": {
          "type": "string",
          "analyzer": "lucene.standard"
        },
        "game": {
          "type": "document",
          "fields": {
            "title": {
              "type": "string",
              "analyzer": "lucene.standard"
            }
          }
        },
        "tags": {
          "type": "string",
          "analyzer": "lucene.keyword"
        },
        "category": {
          "type": "string",
          "analyzer": "lucene.keyword"
        },
        "status": {
          "type": "string",
          "analyzer": "lucene.keyword"
        }
      }
    }
  }
}
```

**Search API endpoint** (`src/app/api/search/route.ts`):

```typescript
import { NextRequest, NextResponse } from 'next/server'
import clientPromise from '@/lib/mongodb'

export async function GET(req: NextRequest) {
  const query = req.nextUrl.searchParams.get('q')
  const page = parseInt(req.nextUrl.searchParams.get('page') || '1')
  const limit = parseInt(req.nextUrl.searchParams.get('limit') || '10')

  if (!query || query.length < 2) {
    return NextResponse.json({ results: [], total: 0 })
  }

  const client = await clientPromise
  const db = client.db('gamesaiyan_production')

  const results = await db.collection('guides').aggregate([
    {
      $search: {
        index: 'guides_search',
        compound: {
          should: [
            { text: { query, path: 'title', score: { boost: { value: 5 } } } },
            { text: { query, path: 'excerpt', score: { boost: { value: 2 } } } },
            { text: { query, path: 'content' } },
          ],
          filter: [
            { text: { query: 'published', path: 'status' } },
          ],
        },
      },
    },
    { $skip: (page - 1) * limit },
    { $limit: limit },
    {
      $project: {
        title: 1,
        slug: 1,
        excerpt: 1,
        heroImage: 1,
        game: 1,
        category: 1,
        publishedAt: 1,
        score: { $meta: 'searchScore' },
      },
    },
  ]).toArray()

  return NextResponse.json({ results, total: results.length })
}
```

---

## 7. Image Pipeline — Bunny CDN

### 7.1 Bunny CDN Setup

1. **Create a Storage Zone** in Bunny dashboard: `gamesaiyan-media`
2. **Create a Pull Zone** linked to that storage zone: `cdn.gamesaiyan.com`
3. **Configure CNAME**: Add DNS record `cdn.gamesaiyan.com → [your-pullzone].b-cdn.net`
4. **Enable Bunny Optimizer**: Auto WebP/AVIF conversion, lazy loading, image resizing via URL parameters

### 7.2 Upload Flow

```
Author uploads image in Payload Admin
          │
          ▼
   Payload processes image
   (creates thumbnail, card, hero, full sizes)
          │
          ▼
   afterChange hook fires
          │
          ▼
   Upload all sizes to Bunny Storage Zone
   via Bunny Storage API (PUT request)
          │
          ▼
   Store Bunny CDN URL in media.bunnyUrl field
          │
          ▼
   Frontend references: cdn.gamesaiyan.com/media/{filename}
```

### 7.3 Bunny Upload Utility — `src/lib/bunny.ts`

```typescript
const BUNNY_STORAGE_ZONE = process.env.BUNNY_STORAGE_ZONE!        // 'gamesaiyan-media'
const BUNNY_STORAGE_PASSWORD = process.env.BUNNY_STORAGE_PASSWORD! // API key
const BUNNY_CDN_URL = process.env.NEXT_PUBLIC_BUNNY_CDN_URL!       // 'https://cdn.gamesaiyan.com'
const BUNNY_STORAGE_HOSTNAME = 'storage.bunnycdn.com'

/**
 * Upload a file buffer to Bunny CDN Storage
 */
export async function uploadToBunny(
  buffer: Buffer,
  remotePath: string // e.g., '/media/guides/hero-image.webp'
): Promise<string> {
  const url = `https://${BUNNY_STORAGE_HOSTNAME}/${BUNNY_STORAGE_ZONE}${remotePath}`

  const response = await fetch(url, {
    method: 'PUT',
    headers: {
      AccessKey: BUNNY_STORAGE_PASSWORD,
      'Content-Type': 'application/octet-stream',
    },
    body: buffer,
  })

  if (!response.ok) {
    throw new Error(`Bunny upload failed: ${response.statusText}`)
  }

  return `${BUNNY_CDN_URL}${remotePath}`
}

/**
 * Delete a file from Bunny CDN Storage
 */
export async function deleteFromBunny(remotePath: string): Promise<void> {
  const url = `https://${BUNNY_STORAGE_HOSTNAME}/${BUNNY_STORAGE_ZONE}${remotePath}`

  await fetch(url, {
    method: 'DELETE',
    headers: { AccessKey: BUNNY_STORAGE_PASSWORD },
  })
}

/**
 * Generate optimized Bunny CDN image URL with query parameters
 * Bunny Optimizer handles resize + format conversion on the edge
 */
export function getBunnyImageUrl(
  path: string,
  options?: {
    width?: number
    height?: number
    quality?: number
    format?: 'webp' | 'avif' | 'auto'
  }
): string {
  const params = new URLSearchParams()
  if (options?.width) params.set('width', options.width.toString())
  if (options?.height) params.set('height', options.height.toString())
  if (options?.quality) params.set('quality', options.quality.toString())

  const queryString = params.toString()
  return `${BUNNY_CDN_URL}${path}${queryString ? `?${queryString}` : ''}`
}
```

### 7.4 Responsive Image Component

```tsx
// src/components/ui/BunnyImage.tsx
'use client'

import Image from 'next/image'
import { getBunnyImageUrl } from '@/lib/bunny'

interface BunnyImageProps {
  src: string       // Bunny CDN path (e.g., '/media/guides/hero.webp')
  alt: string
  width: number
  height: number
  priority?: boolean
  className?: string
  sizes?: string
}

export function BunnyImage({ src, alt, width, height, priority, className, sizes }: BunnyImageProps) {
  const loader = ({ src, width, quality }: { src: string; width: number; quality?: number }) => {
    return getBunnyImageUrl(src, { width, quality: quality || 80 })
  }

  return (
    <Image
      loader={loader}
      src={src}
      alt={alt}
      width={width}
      height={height}
      priority={priority}
      className={className}
      sizes={sizes || '(max-width: 768px) 100vw, (max-width: 1200px) 50vw, 33vw'}
    />
  )
}
```

### 7.5 Scenario Coverage

| Scenario | Handling |
|---|---|
| Bunny CDN is down | Fallback to Vercel Blob URL stored in media document |
| Image upload fails to Bunny | Retry 3x with exponential backoff; if all fail, keep Vercel Blob as primary and queue for later sync |
| Author uploads non-image file | Reject at Payload's `mimeTypes` filter (only image types allowed) |
| Oversized image (>10MB) | Reject at Payload's `fileSize` limit |
| Bunny Storage Zone full | Monitor via Bunny dashboard alerts; upgrade plan |
| Cache invalidation needed | Use Bunny Purge API to clear specific URLs |
| Migration from existing images | Script to batch-upload all Media documents to Bunny and update `bunnyUrl` fields |

---

## 8. Page-by-Page Specifications

### 8.1 Homepage (`/`)

**Purpose:** Get the reader into a guide fast. Search-first design with curated content below.

```
┌──────────────────────────────────────────────────────────────────┐
│ [HEADER: Logo | Games ▾ | Tier Lists | Builds | ⌘K Search | ☾] │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │           HERO SECTION (full-bleed cinematic BG)           │  │
│  │                                                            │  │
│  │   ╔══════════════════════════════════════════════════╗     │  │
│  │   ║  LEVEL UP YOUR GAMEPLAY                         ║     │  │
│  │   ║  Find the strategy you need. Instantly.         ║     │  │
│  │   ║                                                  ║     │  │
│  │   ║  ┌──────────────────────────────────────────┐   ║     │  │
│  │   ║  │ 🔍  Search guides, games, builds...      │   ║     │  │
│  │   ║  └──────────────────────────────────────────┘   ║     │  │
│  │   ║                                                  ║     │  │
│  │   ║  Popular: Elden Ring · Genshin · Monster Hunter ║     │  │
│  │   ╚══════════════════════════════════════════════════╝     │  │
│  │                                                            │  │
│  │  CRT scan-line overlay · parallax scroll · animated BG    │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ── FEATURED GUIDES ─────────────────────── [View All →]         │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐               │
│  │ ▓▓▓▓▓▓▓ │ │ ▓▓▓▓▓▓▓ │ │ ▓▓▓▓▓▓▓ │ │ ▓▓▓▓▓▓▓ │               │
│  │  HERO   │ │  HERO   │ │  HERO   │ │  HERO   │  ← Horizontal │
│  │  IMAGE  │ │  IMAGE  │ │  IMAGE  │ │  IMAGE  │    scroll on   │
│  │         │ │         │ │         │ │         │    mobile      │
│  │ [Game]  │ │ [Game]  │ │ [Game]  │ │ [Game]  │               │
│  │ Title   │ │ Title   │ │ Title   │ │ Title   │               │
│  │ Author· │ │ Author· │ │ Author· │ │ Author· │               │
│  │ 5m read │ │ 8m read │ │ 3m read │ │ 12m rea │               │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘               │
│                                                                  │
│  ── AD SLOT: LEADERBOARD (728×90 / 320×100 mobile) ──           │
│                                                                  │
│  ── BROWSE BY GAME ──────────────────────── [All Games →]        │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐        │
│  │COVER │ │COVER │ │COVER │ │COVER │ │COVER │ │COVER │        │
│  │ IMG  │ │ IMG  │ │ IMG  │ │ IMG  │ │ IMG  │ │ IMG  │        │
│  │      │ │      │ │      │ │      │ │      │ │      │        │
│  │ Name │ │ Name │ │ Name │ │ Name │ │ Name │ │ Name │        │
│  │ 24   │ │ 18   │ │ 45   │ │ 12   │ │ 30   │ │ 8    │        │
│  │guides│ │guide │ │guide │ │guide │ │guide │ │guide │        │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘        │
│                                                                  │
│  ── LATEST GUIDES ───────────────────────── [View All →]         │
│  ┌────────────────────────────────────┐  ┌──────────────────┐   │
│  │ LARGE FEATURED CARD                │  │ SMALL CARD 1     │   │
│  │ (hero image, full meta, excerpt)   │  │ Title · Game     │   │
│  │                                    │  │ 3m · Boss Guide  │   │
│  │                                    │  ├──────────────────┤   │
│  │                                    │  │ SMALL CARD 2     │   │
│  │                                    │  │ Title · Game     │   │
│  │                                    │  │ 5m · Walkthrough │   │
│  │                                    │  ├──────────────────┤   │
│  │                                    │  │ SMALL CARD 3     │   │
│  └────────────────────────────────────┘  └──────────────────┘   │
│                                                                  │
│  ── AD SLOT: IN-CONTENT (300×250 / responsive) ──               │
│                                                                  │
│  ── TRENDING NOW ────────────────────────                        │
│  (Horizontal scroll of trending guides with view counts)         │
│                                                                  │
│  ── TIER LISTS ──────────────────────────── [All Lists →]        │
│  (3-column grid of latest tier lists)                            │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│ [FOOTER: Logo · Nav links · Social · Legal · © 2026]             │
└──────────────────────────────────────────────────────────────────┘
```

**Data Fetching (RSC):**
```typescript
// src/app/(frontend)/page.tsx
import { getPayload } from 'payload'
import config from '@payload-config'

export default async function HomePage() {
  const payload = await getPayload({ config })

  const [featuredGuides, latestGuides, games, tierLists] = await Promise.all([
    payload.find({
      collection: 'guides',
      where: { featured: { equals: true }, status: { equals: 'published' } },
      limit: 4,
      sort: '-publishedAt',
    }),
    payload.find({
      collection: 'guides',
      where: { status: { equals: 'published' } },
      limit: 10,
      sort: '-publishedAt',
    }),
    payload.find({
      collection: 'games',
      where: { status: { not_equals: 'upcoming' } },
      limit: 12,
      sort: 'title',
    }),
    payload.find({
      collection: 'tier-lists',
      where: { status: { equals: 'published' } },
      limit: 6,
      sort: '-publishedAt',
    }),
  ])

  return (
    <>
      <HeroSection />
      <FeaturedGuides guides={featuredGuides.docs} />
      <AdSlot position="home-leaderboard" />
      <GameGrid games={games.docs} />
      <LatestGuides guides={latestGuides.docs} />
      <AdSlot position="home-in-content" />
      <TrendingSection />
      <TierListsSection tierLists={tierLists.docs} />
    </>
  )
}
```

### 8.2 Game Hub Page (`/games/[gameSlug]`)

**Purpose:** Central hub for all content related to a single game. Think "mini-wiki landing page."

**Layout:**
- Full-bleed hero image (game key art) with gradient overlay
- Game title, platforms, genre badges, guide count
- Tabbed sections: All Guides | Walkthroughs | Boss Guides | Builds | Tier Lists
- Sidebar: Quick links, ad slots, related games
- Filterable/sortable guide list within each tab

**Data Fetching:**
```typescript
export default async function GameHubPage({ params }: { params: { gameSlug: string } }) {
  const payload = await getPayload({ config })

  const game = await payload.find({
    collection: 'games',
    where: { slug: { equals: params.gameSlug } },
    limit: 1,
  }).then(res => res.docs[0])

  if (!game) notFound()

  const guides = await payload.find({
    collection: 'guides',
    where: {
      game: { equals: game.id },
      status: { equals: 'published' },
    },
    sort: '-publishedAt',
    limit: 50,
  })

  // ... render
}
```

**SEO:** Dynamic `generateMetadata` returning game-specific meta, OG image, and JSON-LD (VideoGame schema).

### 8.3 Guide / Walkthrough Article Page (`/games/[gameSlug]/[guideSlug]`)

**Purpose:** The core reading experience. This is where users spend 80% of their time.

**Layout:**
```
┌──────────────────────────────────────────────────────────────────┐
│ ▓▓▓▓▓▓ READING PROGRESS BAR (sticky top, accent color) ▓▓▓▓▓▓▓ │
├──────────────────────────────────────────────────────────────────┤
│ [HEADER]                                                         │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Breadcrumbs: Home > Elden Ring > Shadow of the Erdtree > ...    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │              HERO IMAGE (full-width, 16:9)                 │  │
│  │          with gradient fade to background                  │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌─────────────────────────────┐  ┌───────────────────────────┐ │
│  │    MAIN CONTENT COLUMN      │  │    SIDEBAR (sticky)       │ │
│  │    (max-width: 720px)       │  │                           │ │
│  │                             │  │  ┌─────────────────────┐  │ │
│  │  [H1] Guide Title          │  │  │ TABLE OF CONTENTS   │  │ │
│  │                             │  │  │                     │  │ │
│  │  [META BAR]                 │  │  │ • Section 1 ← active│  │ │
│  │  Author · May 7, 2026      │  │  │   Section 2         │  │ │
│  │  8 min read · Hard 💀💀💀  │  │  │   Section 3         │  │ │
│  │  PS5 · PC                   │  │  │   Section 4         │  │ │
│  │                             │  │  │   Section 5         │  │ │
│  │  [RICH TEXT CONTENT]        │  │  └─────────────────────┘  │ │
│  │                             │  │                           │ │
│  │  ## Section 1               │  │  ┌─────────────────────┐  │ │
│  │  Paragraph text...          │  │  │ AD SLOT             │  │ │
│  │                             │  │  │ (300×250)           │  │ │
│  │  ┌── SPOILER ────────────┐  │  │  └─────────────────────┘  │ │
│  │  │ ⚠️ Spoiler: Boss Phase│  │  │                           │ │
│  │  │ [Click to reveal]     │  │  │  ┌─────────────────────┐  │ │
│  │  └───────────────────────┘  │  │  │ CHECKLIST PROGRESS  │  │ │
│  │                             │  │  │ ██████░░░░ 60%      │  │ │
│  │  ┌── CHECKLIST ──────────┐  │  │  │ 6/10 steps done     │  │ │
│  │  │ ☑ Step 1: Get item    │  │  │  └─────────────────────┘  │ │
│  │  │ ☑ Step 2: Talk to NPC │  │  │                           │ │
│  │  │ ☐ Step 3: Enter cave  │  │  │  ┌─────────────────────┐  │ │
│  │  │ ☐ Step 4: Defeat boss │  │  │  │ RELATED GUIDES      │  │ │
│  │  └───────────────────────┘  │  │  │                     │  │ │
│  │                             │  │  │ • Next: Phase 2     │  │ │
│  │  ┌── TIP BLOCK ──────────┐  │  │  │ • Prev: Intro       │  │ │
│  │  │ 💡 Pro Tip: Use fire  │  │  │  │ • Related: Build    │  │ │
│  │  │ resistance before...  │  │  │  └─────────────────────┘  │ │
│  │  └───────────────────────┘  │  │                           │ │
│  │                             │  │  ┌─────────────────────┐  │ │
│  │  == AD SLOT (in-content) == │  │  │ STICKY AD           │  │ │
│  │                             │  │  │ (follows scroll)    │  │ │
│  │  ## Section 2               │  │  └─────────────────────┘  │ │
│  │  More content...            │  │                           │ │
│  │                             │  └───────────────────────────┘ │
│  │  ┌─── PREV / NEXT NAV ──┐  │                                │
│  │  │ ← Previous Guide      │  │                                │
│  │  │   Next Guide →         │  │                                │
│  │  └───────────────────────┘  │                                │
│  │                             │                                │
│  │  [AUTHOR CARD]              │                                │
│  │  [RELATED GUIDES GRID]      │                                │
│  │  [COMMENTS / DISCUSSION]    │                                │
│  └─────────────────────────────┘                                │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│ [FOOTER]                                                         │
└──────────────────────────────────────────────────────────────────┘
```

**Mobile Adaptation:**
- Sidebar collapses to a bottom sheet (TOC) accessible via floating button
- Checklist progress moves to a floating bottom bar
- Full-width content column
- Swipe left/right for prev/next guide navigation

### 8.4 Tier List / Build Page (`/tier-lists/[slug]`, `/builds/[slug]`)

See Section 10 for full specification.

### 8.5 Category / Browse Page (`/games`)

- Grid layout of all game cards with filters (platform, genre, status)
- Search within games
- Sort by: name, guide count, most recent update
- Responsive: 4 columns desktop → 2 columns tablet → 1 column mobile

### 8.6 About / Contact (`/about`)

- Mission statement, team section (author cards)
- Contact form (Payload form submission or Resend-powered email)
- FAQ section
- Social links

---

## 9. Guide Article Features (Interactive)

### 9.1 Reading Progress Bar

```tsx
// src/components/guide/ReadingProgressBar.tsx
'use client'

import { useReadingProgress } from '@/hooks/useReadingProgress'

export function ReadingProgressBar() {
  const progress = useReadingProgress()

  return (
    <div
      className="fixed top-0 left-0 z-50 h-1 bg-[var(--accent-primary)] transition-all duration-150 ease-out"
      style={{ width: `${progress}%` }}
      role="progressbar"
      aria-valuenow={Math.round(progress)}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-label="Reading progress"
    />
  )
}

// src/hooks/useReadingProgress.ts
'use client'
import { useState, useEffect } from 'react'

export function useReadingProgress() {
  const [progress, setProgress] = useState(0)

  useEffect(() => {
    const handleScroll = () => {
      const article = document.querySelector('[data-guide-content]')
      if (!article) return

      const { top, height } = article.getBoundingClientRect()
      const windowHeight = window.innerHeight
      const scrolled = Math.max(0, -top)
      const total = height - windowHeight
      const pct = Math.min(100, (scrolled / total) * 100)
      setProgress(pct)
    }

    window.addEventListener('scroll', handleScroll, { passive: true })
    return () => window.removeEventListener('scroll', handleScroll)
  }, [])

  return progress
}
```

### 9.2 Spoiler Toggle

```tsx
// src/components/guide/SpoilerToggle.tsx
'use client'

import { useState } from 'react'
import { Eye, EyeOff } from 'lucide-react'

interface SpoilerToggleProps {
  label: string
  children: React.ReactNode
}

export function SpoilerToggle({ label, children }: SpoilerToggleProps) {
  const [revealed, setRevealed] = useState(false)

  return (
    <div className="my-6 rounded-lg border border-[var(--border-accent)] bg-[var(--surface-glass)] overflow-hidden">
      <button
        onClick={() => setRevealed(!revealed)}
        className="w-full flex items-center gap-3 px-4 py-3 text-left
          text-[var(--accent-primary)] hover:bg-[var(--surface-glass)] transition-colors"
        aria-expanded={revealed}
      >
        {revealed ? <EyeOff size={18} /> : <Eye size={18} />}
        <span className="font-semibold text-sm uppercase tracking-wider">
          {revealed ? 'Hide Spoiler' : `⚠️ Spoiler: ${label}`}
        </span>
      </button>

      {/* CSS grid trick for smooth height animation */}
      <div
        className="grid transition-[grid-template-rows] duration-300 ease-out"
        style={{ gridTemplateRows: revealed ? '1fr' : '0fr' }}
      >
        <div className="overflow-hidden">
          <div className="px-4 pb-4 pt-2">{children}</div>
        </div>
      </div>
    </div>
  )
}
```

### 9.3 Interactive Checklist

```tsx
// src/components/guide/InteractiveChecklist.tsx
'use client'

import { useChecklistStore } from '@/store/checklistStore'
import { Check } from 'lucide-react'

interface ChecklistItem {
  stepId: string
  label: string
  isOptional?: boolean
}

interface InteractiveChecklistProps {
  guideId: string
  steps: ChecklistItem[]
  title?: string
}

export function InteractiveChecklist({ guideId, steps, title }: InteractiveChecklistProps) {
  const { toggleStep, isCompleted, getProgress } = useChecklistStore()
  const progress = getProgress(guideId, steps.map(s => s.stepId))

  return (
    <div className="my-6 rounded-lg border border-[var(--border-default)] bg-[var(--bg-secondary)] overflow-hidden">
      {/* Header with progress */}
      <div className="px-4 py-3 border-b border-[var(--border-subtle)] flex items-center justify-between">
        <span className="font-semibold text-sm uppercase tracking-wider text-[var(--text-secondary)]">
          {title || 'Checklist'}
        </span>
        <span className="text-xs text-[var(--accent-primary)] font-mono">
          {progress.completed}/{progress.total} ({Math.round(progress.percent)}%)
        </span>
      </div>

      {/* Progress bar */}
      <div className="h-0.5 bg-[var(--border-subtle)]">
        <div
          className="h-full bg-[var(--accent-primary)] transition-all duration-300"
          style={{ width: `${progress.percent}%` }}
        />
      </div>

      {/* Steps */}
      <ul className="divide-y divide-[var(--border-subtle)]">
        {steps.map((step) => {
          const completed = isCompleted(guideId, step.stepId)
          return (
            <li key={step.stepId}>
              <button
                onClick={() => toggleStep(guideId, step.stepId)}
                className={`w-full flex items-start gap-3 px-4 py-3 text-left transition-colors
                  hover:bg-[var(--surface-glass)]
                  ${completed ? 'opacity-60' : ''}`}
              >
                {/* Checkbox */}
                <span className={`mt-0.5 flex-shrink-0 w-5 h-5 rounded border-2 flex items-center justify-center
                  transition-all duration-200
                  ${completed
                    ? 'bg-[var(--accent-primary)] border-[var(--accent-primary)] scale-110'
                    : 'border-[var(--border-default)]'
                  }`}
                >
                  {completed && <Check size={12} className="text-[var(--text-inverse)]" />}
                </span>

                {/* Label */}
                <span className={`text-sm leading-relaxed transition-all duration-200
                  ${completed ? 'line-through text-[var(--text-tertiary)]' : 'text-[var(--text-primary)]'}`}
                >
                  {step.label}
                  {step.isOptional && (
                    <span className="ml-2 text-xs text-[var(--text-tertiary)]">(optional)</span>
                  )}
                </span>
              </button>
            </li>
          )
        })}
      </ul>
    </div>
  )
}
```

**Persistence — Zustand Store with localStorage:**

```typescript
// src/store/checklistStore.ts
import { create } from 'zustand'
import { persist } from 'zustand/middleware'

interface ChecklistState {
  completedSteps: Record<string, string[]> // guideId → stepId[]
  toggleStep: (guideId: string, stepId: string) => void
  isCompleted: (guideId: string, stepId: string) => boolean
  getProgress: (guideId: string, allStepIds: string[]) => {
    completed: number; total: number; percent: number
  }
  resetGuide: (guideId: string) => void
}

export const useChecklistStore = create<ChecklistState>()(
  persist(
    (set, get) => ({
      completedSteps: {},

      toggleStep: (guideId, stepId) => {
        set((state) => {
          const current = state.completedSteps[guideId] || []
          const updated = current.includes(stepId)
            ? current.filter(id => id !== stepId)
            : [...current, stepId]
          return {
            completedSteps: { ...state.completedSteps, [guideId]: updated },
          }
        })
      },

      isCompleted: (guideId, stepId) => {
        return (get().completedSteps[guideId] || []).includes(stepId)
      },

      getProgress: (guideId, allStepIds) => {
        const completed = (get().completedSteps[guideId] || [])
          .filter(id => allStepIds.includes(id))
        return {
          completed: completed.length,
          total: allStepIds.length,
          percent: allStepIds.length > 0 ? (completed.length / allStepIds.length) * 100 : 0,
        }
      },

      resetGuide: (guideId) => {
        set((state) => {
          const { [guideId]: _, ...rest } = state.completedSteps
          return { completedSteps: rest }
        })
      },
    }),
    {
      name: 'gamesaiyan-checklist',
    }
  )
)
```

**Scenario: Logged-in user syncing checklist across devices:**
If the user is authenticated, the `checklistStore` should also sync to the `Users.checklistProgress` field in Payload via an API route (`/api/checklist`). Implement a debounced sync (2 seconds after last change) to avoid excessive API calls. On page load, merge localStorage state with server state (server wins for conflicts).

### 9.4 Table of Contents (ScrollSpy)

```tsx
// src/components/guide/TableOfContents.tsx
'use client'

import { useScrollSpy } from '@/hooks/useScrollSpy'

interface TocItem {
  id: string
  text: string
  level: number // 2 = h2, 3 = h3
}

interface TableOfContentsProps {
  items: TocItem[]
}

export function TableOfContents({ items }: TableOfContentsProps) {
  const activeId = useScrollSpy(items.map(i => i.id))

  return (
    <nav aria-label="Table of contents" className="sticky top-20">
      <h3 className="text-xs font-bold uppercase tracking-widest text-[var(--text-tertiary)] mb-4">
        On This Page
      </h3>
      <ul className="space-y-1 border-l-2 border-[var(--border-subtle)]">
        {items.map((item) => (
          <li key={item.id}>
            <a
              href={`#${item.id}`}
              className={`block py-1.5 text-sm transition-colors border-l-2 -ml-[2px]
                ${item.level === 3 ? 'pl-6' : 'pl-4'}
                ${activeId === item.id
                  ? 'border-[var(--accent-primary)] text-[var(--accent-primary)] font-medium'
                  : 'border-transparent text-[var(--text-secondary)] hover:text-[var(--text-primary)]'
                }`}
            >
              {item.text}
            </a>
          </li>
        ))}
      </ul>
    </nav>
  )
}
```

---

## 10. Tier List / Build Page System

### 10.1 Tier List Interactive View

The tier list is displayed as an interactive, visually rich component where items are grouped into labeled tiers (S through F). Each tier row has a distinct color from the palette.

**Features:**
- Hover on any item → tooltip with notes/explanation
- Click on item → modal with detailed breakdown
- Filter items by tags (e.g., "DPS", "Support", "Tank")
- Search within the tier list
- "Last Updated" badge with patch version
- Share button → copies URL with specific tier anchored
- Print-friendly view (for offline use)

**Layout:**
```
┌──────────────────────────────────────────────────────────┐
│  TIER LIST: Genshin Impact 5.0 Character Rankings        │
│  Updated: Patch 5.0 · May 2026 · by AuthorName          │
│                                                          │
│  [Filter: All ▾]  [Search items...]  [Share] [Print]     │
│                                                          │
│  ┌─── S ──────────────────────────────────────────────┐  │
│  │ ███ │  [Char1]  [Char2]  [Char3]                   │  │
│  │ RED │  w/image  w/image  w/image                   │  │
│  └────────────────────────────────────────────────────┘  │
│  ┌─── A ──────────────────────────────────────────────┐  │
│  │ ███ │  [Char4]  [Char5]  [Char6]  [Char7]         │  │
│  │ ORG │                                              │  │
│  └────────────────────────────────────────────────────┘  │
│  ┌─── B ──────────────────────────────────────────────┐  │
│  │ ███ │  [Char8]  [Char9]  [Char10] [Char11] ...    │  │
│  │ YEL │                                              │  │
│  └────────────────────────────────────────────────────┘  │
│  ... C, D, F tiers ...                                   │
│                                                          │
│  ── METHODOLOGY ──                                       │
│  [Rich text explaining ranking criteria]                 │
└──────────────────────────────────────────────────────────┘
```

### 10.2 Build Guide Page

Build guides are a special category of guides with structured data for gear/skill/stat tables.

**Additional structured fields:**
- Build name, class/character
- Stat allocation table (name → value pairs)
- Equipment slots (weapon, armor, accessory, etc.) each with item name, image, and notes
- Skill tree / ability selection (ordered list with images)
- Playstyle description
- Strengths / Weaknesses comparison
- Video embed (optional)

---

## 11. SEO Architecture

### 11.1 Meta Tags (via Next.js Metadata API)

Every page exports a `generateMetadata` function:

```typescript
// src/app/(frontend)/games/[gameSlug]/[guideSlug]/page.tsx
import type { Metadata } from 'next'
import { getPayload } from 'payload'
import config from '@payload-config'

export async function generateMetadata({ params }): Promise<Metadata> {
  const payload = await getPayload({ config })
  const guide = await payload.find({
    collection: 'guides',
    where: { slug: { equals: params.guideSlug } },
    limit: 1,
    depth: 2, // Populate game + author
  }).then(res => res.docs[0])

  if (!guide) return {}

  const title = guide.seo?.metaTitle || `${guide.title} — ${guide.game.title} | GameSaiyan`
  const description = guide.seo?.metaDescription || guide.excerpt
  const ogImage = guide.seo?.ogImage?.bunnyUrl || guide.heroImage?.bunnyUrl || '/og-default.jpg'

  return {
    title,
    description,
    openGraph: {
      title,
      description,
      url: `https://gamesaiyan.com/games/${params.gameSlug}/${params.guideSlug}`,
      siteName: 'GameSaiyan',
      images: [{ url: ogImage, width: 1200, height: 630, alt: guide.title }],
      type: 'article',
      publishedTime: guide.publishedAt,
      modifiedTime: guide.updatedAt,
      authors: [guide.author?.name],
      section: guide.category,
      tags: guide.tags?.map(t => t.name),
    },
    twitter: {
      card: 'summary_large_image',
      title,
      description,
      images: [ogImage],
    },
    alternates: {
      canonical: guide.seo?.canonicalUrl || `https://gamesaiyan.com/games/${params.gameSlug}/${params.guideSlug}`,
    },
    robots: guide.seo?.noIndex ? { index: false, follow: false } : { index: true, follow: true },
  }
}
```

### 11.2 JSON-LD / Schema.org

Inject structured data per page type:

```typescript
// src/lib/seo.ts

export function generateGuideJsonLd(guide: Guide, game: Game) {
  return {
    '@context': 'https://schema.org',
    '@type': 'Article',
    headline: guide.title,
    description: guide.excerpt,
    image: guide.heroImage?.bunnyUrl,
    datePublished: guide.publishedAt,
    dateModified: guide.updatedAt,
    author: {
      '@type': 'Person',
      name: guide.author?.name,
      url: `https://gamesaiyan.com/authors/${guide.author?.slug}`,
    },
    publisher: {
      '@type': 'Organization',
      name: 'GameSaiyan',
      logo: {
        '@type': 'ImageObject',
        url: 'https://gamesaiyan.com/logo.svg',
      },
    },
    mainEntityOfPage: {
      '@type': 'WebPage',
      '@id': `https://gamesaiyan.com/games/${game.slug}/${guide.slug}`,
    },
    about: {
      '@type': 'VideoGame',
      name: game.title,
      gamePlatform: game.platform,
      genre: game.genre,
    },
    wordCount: guide.readingTime ? guide.readingTime * 200 : undefined,
    // HowTo schema for walkthroughs with steps
    ...(guide.category === 'walkthrough' && guide.checklistSteps?.length
      ? {
          '@type': ['Article', 'HowTo'],
          step: guide.checklistSteps.map((step, i) => ({
            '@type': 'HowToStep',
            position: i + 1,
            name: step.label,
          })),
        }
      : {}),
  }
}

export function generateGameJsonLd(game: Game) {
  return {
    '@context': 'https://schema.org',
    '@type': 'VideoGame',
    name: game.title,
    description: game.description,
    image: game.coverImage?.bunnyUrl,
    gamePlatform: game.platform,
    genre: game.genre,
    datePublished: game.releaseDate,
    publisher: { '@type': 'Organization', name: game.publisher },
    developer: { '@type': 'Organization', name: game.developer },
  }
}

export function generateTierListJsonLd(tierList: TierList, game: Game) {
  return {
    '@context': 'https://schema.org',
    '@type': 'ItemList',
    name: tierList.title,
    description: tierList.description,
    numberOfItems: tierList.tiers.reduce((acc, t) => acc + t.items.length, 0),
    itemListElement: tierList.tiers.flatMap((tier, tierIdx) =>
      tier.items.map((item, itemIdx) => ({
        '@type': 'ListItem',
        position: tierIdx * 100 + itemIdx + 1,
        name: `${tier.tier}-Tier: ${item.name}`,
        description: item.notes,
      }))
    ),
    about: {
      '@type': 'VideoGame',
      name: game.title,
    },
  }
}

export function generateBreadcrumbJsonLd(crumbs: { name: string; url: string }[]) {
  return {
    '@context': 'https://schema.org',
    '@type': 'BreadcrumbList',
    itemListElement: crumbs.map((crumb, i) => ({
      '@type': 'ListItem',
      position: i + 1,
      name: crumb.name,
      item: crumb.url,
    })),
  }
}

export function generateFaqJsonLd(faqs: { question: string; answer: string }[]) {
  return {
    '@context': 'https://schema.org',
    '@type': 'FAQPage',
    mainEntity: faqs.map(faq => ({
      '@type': 'Question',
      name: faq.question,
      acceptedAnswer: {
        '@type': 'Answer',
        text: faq.answer,
      },
    })),
  }
}

// WebSite schema for sitelinks searchbox in Google
export function generateWebsiteJsonLd() {
  return {
    '@context': 'https://schema.org',
    '@type': 'WebSite',
    name: 'GameSaiyan',
    url: 'https://gamesaiyan.com',
    description: 'Strategy game guides, walkthroughs, tier lists, and build guides. Level up your gameplay.',
    potentialAction: {
      '@type': 'SearchAction',
      target: {
        '@type': 'EntryPoint',
        urlTemplate: 'https://gamesaiyan.com/search?q={search_term_string}',
      },
      'query-input': 'required name=search_term_string',
    },
  }
}
```

**Inject in pages:**

```tsx
// In any page component
<script
  type="application/ld+json"
  dangerouslySetInnerHTML={{ __html: JSON.stringify(generateGuideJsonLd(guide, game)) }}
/>
```

### 11.3 Dynamic Sitemap

```typescript
// src/app/sitemap.ts
import type { MetadataRoute } from 'next'
import { getPayload } from 'payload'
import config from '@payload-config'

export default async function sitemap(): Promise<MetadataRoute.Sitemap> {
  const payload = await getPayload({ config })

  // Fetch all published content
  const [games, guides, tierLists, builds] = await Promise.all([
    payload.find({ collection: 'games', limit: 1000, where: { status: { not_equals: 'upcoming' } } }),
    payload.find({ collection: 'guides', limit: 5000, where: { status: { equals: 'published' } } }),
    payload.find({ collection: 'tier-lists', limit: 500, where: { status: { equals: 'published' } } }),
    payload.find({ collection: 'builds', limit: 500, where: { status: { equals: 'published' } } }),
  ])

  const baseUrl = 'https://gamesaiyan.com'

  return [
    // Static pages
    { url: baseUrl, lastModified: new Date(), changeFrequency: 'daily', priority: 1.0 },
    { url: `${baseUrl}/games`, lastModified: new Date(), changeFrequency: 'daily', priority: 0.9 },
    { url: `${baseUrl}/tier-lists`, lastModified: new Date(), changeFrequency: 'weekly', priority: 0.8 },
    { url: `${baseUrl}/builds`, lastModified: new Date(), changeFrequency: 'weekly', priority: 0.8 },
    { url: `${baseUrl}/about`, lastModified: new Date(), changeFrequency: 'monthly', priority: 0.3 },

    // Game hubs
    ...games.docs.map(game => ({
      url: `${baseUrl}/games/${game.slug}`,
      lastModified: new Date(game.updatedAt),
      changeFrequency: 'weekly' as const,
      priority: 0.8,
    })),

    // Guides
    ...guides.docs.map(guide => ({
      url: `${baseUrl}/games/${guide.game.slug}/${guide.slug}`,
      lastModified: new Date(guide.updatedAt),
      changeFrequency: 'weekly' as const,
      priority: 0.7,
    })),

    // Tier lists
    ...tierLists.docs.map(tl => ({
      url: `${baseUrl}/tier-lists/${tl.slug}`,
      lastModified: new Date(tl.updatedAt),
      changeFrequency: 'weekly' as const,
      priority: 0.7,
    })),

    // Builds
    ...builds.docs.map(build => ({
      url: `${baseUrl}/builds/${build.slug}`,
      lastModified: new Date(build.updatedAt),
      changeFrequency: 'weekly' as const,
      priority: 0.6,
    })),
  ]
}
```

### 11.4 robots.txt

```typescript
// src/app/robots.ts
import type { MetadataRoute } from 'next'

export default function robots(): MetadataRoute.Robots {
  return {
    rules: [
      {
        userAgent: '*',
        allow: '/',
        disallow: ['/admin/', '/api/', '/login', '/register', '/forgot-password'],
      },
    ],
    sitemap: 'https://gamesaiyan.com/sitemap.xml',
  }
}
```

### 11.5 Additional SEO Measures

| Measure | Implementation |
|---|---|
| Canonical URLs | Every page sets `<link rel="canonical">` via metadata |
| Open Graph tags | All pages generate OG title, description, image, type |
| Twitter Card tags | All pages set `twitter:card`, `twitter:title`, etc. |
| hreflang | Not needed initially (English only), but ready in metadata API |
| Internal linking | "Related Guides" component, breadcrumbs, prev/next navigation |
| Image alt text | Required field on all media uploads |
| Heading hierarchy | Enforce single H1 per page, proper H2→H3 nesting |
| URL structure | Clean, keyword-rich: `/games/elden-ring/malenia-boss-guide` |
| Page speed | See Section 12 |
| Structured data | JSON-LD on every page type (see 11.2) |
| Google Search Console | Verify site, submit sitemap, monitor indexing |
| Google Indexing API | Programmatic URL submission on publish (via afterChange hook) |

---

## 12. Performance & Core Web Vitals

### 12.1 Targets

| Metric | Target | Strategy |
|---|---|---|
| **LCP** | < 2.5s | Hero images as `priority`, Bunny CDN edge delivery, RSC streaming |
| **FID / INP** | < 100ms | Minimal client JS, use RSC, defer non-critical scripts |
| **CLS** | < 0.1 | Explicit width/height on all images, font `size-adjust`, skeleton loaders |
| **TTFB** | < 200ms | ISR with stale-while-revalidate, Vercel edge network |
| **FCP** | < 1.8s | Critical CSS inline, font preload, no render-blocking scripts |

### 12.2 Optimization Strategies

1. **React Server Components (RSC):** All data-fetching pages are server components. Client components (`'use client'`) only where interactivity is needed (checklist, spoiler toggle, theme toggle, search).

2. **Incremental Static Regeneration (ISR):**
   ```typescript
   // In page.tsx
   export const revalidate = 3600 // Revalidate every hour
   // Or use on-demand revalidation via /api/revalidate when content changes
   ```

3. **Image Optimization:**
   - Bunny CDN with Optimizer enabled (auto WebP/AVIF based on Accept header)
   - Next.js `<Image>` with custom Bunny loader for responsive srcset
   - `priority` attribute on above-the-fold images
   - Lazy loading for all below-fold images
   - `sizes` attribute to prevent oversized downloads

4. **Font Loading:**
   - Self-host all fonts (no Google Fonts CDN — faster, no external requests)
   - Preload critical fonts: Orbitron-Bold and Exo2-Regular
   - Use `font-display: swap` with `size-adjust` to minimize CLS
   - Subset fonts (latin only) to reduce file size

5. **JavaScript Budget:**
   - Target < 100KB first-load JS (compressed)
   - Code split per route (automatic with App Router)
   - Dynamic imports for heavy components (tier list drag-and-drop, search modal)
   - Tree-shake Lucide icons (named imports only)

6. **CSS:**
   - Tailwind CSS JIT — only ships used classes
   - Critical CSS inlined by Next.js automatically
   - CSS variables for theme tokens (no runtime style recalculation)

7. **Ad Loading:**
   - Lazy-load ad scripts with `loading="lazy"` on iframes
   - Use `requestIdleCallback` for ad initialization
   - Reserve exact ad slot dimensions to prevent CLS

8. **Caching Headers:**
   - Static assets: `Cache-Control: public, max-age=31536000, immutable`
   - ISR pages: Managed by Vercel's edge cache
   - API responses: `stale-while-revalidate` where appropriate

---

## 13. Monetization — Ads & Affiliates

### 13.1 Display Ad Slots

Defined ad positions with fallback dimensions to prevent CLS:

| Slot ID | Position | Desktop Size | Mobile Size | Page |
|---|---|---|---|---|
| `home-leaderboard` | Below featured guides | 728×90 | 320×100 | Homepage |
| `home-in-content` | Between latest & trending | 300×250 | 300×250 | Homepage |
| `guide-sidebar-top` | Top of sidebar | 300×250 | Hidden | Guide |
| `guide-sidebar-sticky` | Sticky sidebar bottom | 300×600 | Hidden | Guide |
| `guide-in-content-1` | After 3rd paragraph | 728×90 | 300×250 | Guide |
| `guide-in-content-2` | After 6th paragraph | 728×90 | 300×250 | Guide |
| `guide-bottom` | After guide content | 728×90 | 320×100 | Guide |
| `game-hub-sidebar` | Game hub sidebar | 300×250 | 300×250 | Game Hub |
| `category-in-feed` | Every 4th item in grid | 300×250 | 300×250 | Browse |
| `tier-list-bottom` | Below tier list | 728×90 | 320×100 | Tier List |

**Ad Component:**
```tsx
// src/components/ads/AdSlot.tsx
'use client'

import { useEffect, useRef } from 'react'

interface AdSlotProps {
  position: string
  desktopSize: [number, number]
  mobileSize: [number, number]
  className?: string
}

export function AdSlot({ position, desktopSize, mobileSize, className }: AdSlotProps) {
  const containerRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    // Initialize ad network code here
    // e.g., Google AdSense, Mediavine, Raptive
    if (typeof window !== 'undefined' && window.adsbygoogle) {
      try { (window.adsbygoogle = window.adsbygoogle || []).push({}) } catch {}
    }
  }, [])

  return (
    <div
      ref={containerRef}
      data-ad-slot={position}
      className={`flex items-center justify-center bg-[var(--bg-tertiary)] rounded-lg overflow-hidden ${className}`}
      style={{
        minWidth: mobileSize[0],
        minHeight: mobileSize[1],
      }}
      aria-label="Advertisement"
    >
      <ins
        className="adsbygoogle"
        style={{ display: 'block' }}
        data-ad-client={process.env.NEXT_PUBLIC_ADSENSE_ID}
        data-ad-slot={position}
        data-ad-format="auto"
        data-full-width-responsive="true"
      />
    </div>
  )
}
```

### 13.2 Affiliate Integration

**Affiliate Block (CMS Block):**
```typescript
// src/blocks/AffiliateBlock.ts
export const AffiliateBlock = {
  slug: 'affiliate',
  labels: { singular: 'Affiliate Product', plural: 'Affiliate Products' },
  fields: [
    { name: 'productName', type: 'text', required: true },
    { name: 'productImage', type: 'upload', relationTo: 'media' },
    { name: 'description', type: 'textarea' },
    { name: 'price', type: 'text' },
    { name: 'affiliateUrl', type: 'text', required: true },
    { name: 'store', type: 'select', options: [
      { label: 'Amazon', value: 'amazon' },
      { label: 'Steam', value: 'steam' },
      { label: 'PlayStation Store', value: 'psn' },
      { label: 'Xbox Store', value: 'xbox' },
      { label: 'Nintendo eShop', value: 'nintendo' },
      { label: 'Epic Games', value: 'epic' },
      { label: 'Other', value: 'other' },
    ]},
    { name: 'badge', type: 'text', admin: { description: 'e.g., "Best Value", "Editor\'s Pick"' } },
  ],
}
```

**Affiliate Component:**
```tsx
// Renders as a styled product card with clear "Affiliate Link" disclosure
// Must include: rel="noopener noreferrer sponsored" on all affiliate links
// Must include visible disclosure text: "As an affiliate, we may earn commission..."
```

### 13.3 Monetization Scenarios

| Scenario | Handling |
|---|---|
| Ad blocker detected | Show non-intrusive banner asking to whitelist; do not block content |
| Premium guide + ads | Premium guides show zero ads (clean reading experience) |
| Mobile ad spacing | Minimum 300px between ad units on mobile (Google policy) |
| GDPR consent | Cookie consent banner for EU visitors (use Cookiebot or custom) |
| Affiliate link tracking | Use UTM parameters + Bunny redirect for click tracking |
| No ads initially | Launch with ad slot placeholders; integrate network after traffic milestone (50K+ monthly) |

---

## 14. Premium / Paid Guides System

### 14.1 Flow

```
User lands on premium guide
          │
          ▼
   Shows first 30% of content (configurable via previewPercentage)
   with blurred preview of remaining content
          │
          ▼
   ┌─── PAYWALL GATE ────────────────────────┐
   │                                          │
   │  🔒 This is a Premium Guide              │
   │                                          │
   │  Get instant access to the full guide    │
   │  with interactive checklists and more.   │
   │                                          │
   │  ┌──────────────────────────────────┐    │
   │  │  💳 Unlock for $4.99            │    │
   │  │     One-time purchase            │    │
   │  └──────────────────────────────────┘    │
   │                                          │
   │  Already purchased? [Log in]             │
   └──────────────────────────────────────────┘
          │
          ▼ (User clicks "Unlock")
   Stripe Checkout Session created
          │
          ▼
   User completes payment on Stripe
          │
          ▼
   Stripe Webhook → /api/stripe-webhook
   Creates Purchase record in Payload
   Links Purchase to User + Guide
          │
          ▼
   User redirected back → full guide unlocked
   (Cookie-based session from Payload Auth)
```

### 14.2 Stripe Integration

```typescript
// src/lib/stripe.ts
import Stripe from 'stripe'

export const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!, {
  apiVersion: '2024-12-18.acacia',
})

export async function createCheckoutSession(guideId: string, guideTitle: string, priceInCents: number, userId: string) {
  const session = await stripe.checkout.sessions.create({
    mode: 'payment',
    payment_method_types: ['card'],
    line_items: [{
      price_data: {
        currency: 'usd',
        product_data: {
          name: `Premium Guide: ${guideTitle}`,
          description: 'Full access to the premium guide with interactive features.',
        },
        unit_amount: priceInCents,
      },
      quantity: 1,
    }],
    metadata: {
      guideId,
      userId,
    },
    success_url: `${process.env.NEXT_PUBLIC_SERVER_URL}/games/{CHECKOUT_SESSION_ID}?success=true`,
    cancel_url: `${process.env.NEXT_PUBLIC_SERVER_URL}/games/{CHECKOUT_SESSION_ID}?canceled=true`,
  })

  return session
}
```

### 14.3 Purchase Collection

```typescript
// src/collections/Purchases.ts
export const Purchases: CollectionConfig = {
  slug: 'purchases',
  fields: [
    { name: 'user', type: 'relationship', relationTo: 'users', required: true },
    { name: 'guide', type: 'relationship', relationTo: 'guides', required: true },
    { name: 'stripeSessionId', type: 'text', required: true },
    { name: 'stripePaymentIntentId', type: 'text' },
    { name: 'amountPaid', type: 'number', required: true },
    { name: 'currency', type: 'text', defaultValue: 'usd' },
    { name: 'status', type: 'select', options: ['completed', 'refunded', 'disputed'], defaultValue: 'completed' },
    { name: 'purchasedAt', type: 'date', required: true },
  ],
}
```

### 14.4 Scenarios

| Scenario | Handling |
|---|---|
| User not logged in clicks "Unlock" | Redirect to login/register, then back to guide with checkout |
| User already purchased | Check `Purchases` collection; if exists, show full content |
| Stripe webhook fails | Retry logic (Stripe retries automatically); manual verification endpoint |
| Refund requested | Admin marks Purchase as "refunded" in Payload; user loses access |
| Future subscription model | Add Stripe recurring billing; create a "Subscription" collection |
| Free trial / preview | `previewPercentage` field controls how much is shown free |
| Multiple guides purchased | User's purchase history visible in account settings |

---

## 15. Light/Dark Mode & Theming

### 15.1 Theme Provider

```tsx
// src/components/theme/ThemeProvider.tsx
'use client'

import { createContext, useContext, useEffect, useState } from 'react'
import { useThemeStore } from '@/store/themeStore'

type ThemeMode = 'dark' | 'light' | 'system'
type AccentColor = 'cyan' | 'orange' | 'purple' | 'green' | 'red'
type LayoutDensity = 'compact' | 'default' | 'spacious'

interface ThemeContextType {
  mode: ThemeMode
  resolvedMode: 'dark' | 'light'
  accent: AccentColor
  density: LayoutDensity
  setMode: (mode: ThemeMode) => void
  setAccent: (accent: AccentColor) => void
  setDensity: (density: LayoutDensity) => void
}

const ThemeContext = createContext<ThemeContextType | null>(null)

export function ThemeProvider({ children }: { children: React.ReactNode }) {
  const { mode, accent, density, setMode, setAccent, setDensity } = useThemeStore()
  const [resolvedMode, setResolvedMode] = useState<'dark' | 'light'>('dark')

  useEffect(() => {
    const resolve = () => {
      if (mode === 'system') {
        return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
      }
      return mode
    }

    const resolved = resolve()
    setResolvedMode(resolved)
    document.documentElement.setAttribute('data-theme', resolved)
    document.documentElement.setAttribute('data-accent', accent)
    document.documentElement.setAttribute('data-density', density)
  }, [mode, accent, density])

  return (
    <ThemeContext.Provider value={{ mode, resolvedMode, accent, density, setMode, setAccent, setDensity }}>
      {children}
    </ThemeContext.Provider>
  )
}

export function useTheme() {
  const ctx = useContext(ThemeContext)
  if (!ctx) throw new Error('useTheme must be used within ThemeProvider')
  return ctx
}
```

### 15.2 CSS Variable Setup

```css
/* src/styles/globals.css */
@tailwind base;
@tailwind components;
@tailwind utilities;

:root {
  /* Default: dark mode, cyan accent, default density */
  --bg-primary: #0A0A0F;
  --bg-secondary: #12121A;
  /* ... all variables from Section 2.2 ... */
}

[data-theme="light"] {
  --bg-primary: #F5F5FA;
  --bg-secondary: #FFFFFF;
  /* ... light mode overrides ... */
}

[data-accent="cyan"]   { --accent-primary: #00F0FF; /* ... */ }
[data-accent="orange"] { --accent-primary: #FF6B2C; /* ... */ }
[data-accent="purple"] { --accent-primary: #B44CFF; /* ... */ }
[data-accent="green"]  { --accent-primary: #39FF14; /* ... */ }
[data-accent="red"]    { --accent-primary: #FF2E4C; /* ... */ }

[data-density="compact"]  { --spacing-unit: 4px;  --card-padding: 12px; /* ... */ }
[data-density="default"]  { --spacing-unit: 8px;  --card-padding: 20px; /* ... */ }
[data-density="spacious"] { --spacing-unit: 12px; --card-padding: 28px; /* ... */ }
```

### 15.3 Theme Persistence

Theme preferences persist in:
1. **localStorage** (immediate, no auth required) — via Zustand `persist` middleware
2. **Server (Users.themePreferences)** — synced when logged in
3. **Inline `<script>` in `<head>`** — prevents flash of wrong theme on page load:

```tsx
// In root layout.tsx <head>
<script dangerouslySetInnerHTML={{ __html: `
  (function() {
    try {
      var stored = JSON.parse(localStorage.getItem('gamesaiyan-theme') || '{}');
      var mode = stored.state?.mode || 'dark';
      var accent = stored.state?.accent || 'cyan';
      var density = stored.state?.density || 'default';
      if (mode === 'system') {
        mode = window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
      }
      document.documentElement.setAttribute('data-theme', mode);
      document.documentElement.setAttribute('data-accent', accent);
      document.documentElement.setAttribute('data-density', density);
    } catch(e) {
      document.documentElement.setAttribute('data-theme', 'dark');
    }
  })()
`}} />
```

### 15.4 Theming Scenarios

| Scenario | Handling |
|---|---|
| First-time visitor | Default: dark mode, cyan accent, default density |
| System preference changes (e.g., OS switches to light at sunset) | `matchMedia` listener updates resolved mode in real-time |
| User picks accent color | Applied instantly via CSS variable swap (no re-render) |
| User logs in on new device | Theme preferences loaded from server and applied |
| Print mode | `@media print` overrides to light theme, hides ads/navigation |
| Embedded/iframe context | Respect parent theme or default to dark |
| Accessibility: forced-colors mode | Respect `@media (forced-colors: active)` — override accents with system colors |

---

## 16. Authentication & User Features

### 16.1 Auth Flow (Payload Built-in)

Payload CMS provides cookie-based authentication. Users can:

1. **Register** — email + password + display name
2. **Login** — email + password → secure httpOnly cookie
3. **Forgot Password** — email with reset link (via Resend)
4. **OAuth (future)** — Google, Discord via Payload auth strategies

### 16.2 User-Facing Features Requiring Auth

| Feature | Auth Required | Details |
|---|---|---|
| Read free guides | No | Public access |
| Interactive checklist (local) | No | localStorage persistence |
| Interactive checklist (cloud sync) | Yes | Synced to user profile |
| Purchase premium guides | Yes | Stripe checkout linked to user |
| Theme preferences sync | Yes (optional) | Synced to user profile |
| Save favorite guides | Yes | Bookmarks collection |
| Comment on guides | Yes (future) | Moderated comments |

### 16.3 Scenarios

| Scenario | Handling |
|---|---|
| Unauthenticated user uses checklist | Works fine with localStorage; prompt to create account for cloud sync |
| User logs in on second device | Checklist state merges (server wins on conflicts, localStorage updates) |
| Session expires | Silent re-auth via refresh token; if fails, soft redirect to login |
| Admin creates new editor account | Admin panel → Users → create with "editor" role |
| Brute-force login attempts | Payload rate limiting + optional Vercel KV-based rate limiter |
| Account deletion request | Admin deletes user; cascade deletes purchases (or anonymize) |

---

## 17. Deployment Pipeline (Vercel)

### 17.1 Setup

1. **Connect GitHub repo** to Vercel project
2. **Framework preset:** Next.js (auto-detected)
3. **Build command:** `next build` (default)
4. **Output directory:** `.next` (default)
5. **Node.js version:** 20.x
6. **Environment variables:** Set all from Section 21

### 17.2 Branch Strategy

| Branch | Vercel Environment | URL |
|---|---|---|
| `main` | Production | gamesaiyan.com |
| `develop` | Preview | develop-gamesaiyan.vercel.app |
| `feature/*` | Preview | feature-xxx-gamesaiyan.vercel.app |

### 17.3 Build & Deploy Flow

```
Developer pushes to GitHub
          │
          ▼
   Vercel detects push → triggers build
          │
          ▼
   next build runs:
     1. Payload generates types
     2. Tailwind compiles CSS
     3. Next.js builds pages (RSC + static)
     4. ISR pages pre-rendered
          │
          ▼
   Deploy to Vercel Edge Network
     - Serverless functions for API routes
     - Edge runtime for middleware
     - Static assets to CDN
          │
          ▼
   Post-deploy:
     - Run health checks
     - Warm ISR cache for top pages
     - Notify team (Slack/Discord webhook)
```

### 17.4 Vercel Configuration — `vercel.json`

```json
{
  "framework": "nextjs",
  "headers": [
    {
      "source": "/fonts/(.*)",
      "headers": [
        { "key": "Cache-Control", "value": "public, max-age=31536000, immutable" }
      ]
    },
    {
      "source": "/(.*)",
      "headers": [
        { "key": "X-Content-Type-Options", "value": "nosniff" },
        { "key": "X-Frame-Options", "value": "DENY" },
        { "key": "X-XSS-Protection", "value": "1; mode=block" },
        { "key": "Referrer-Policy", "value": "strict-origin-when-cross-origin" },
        { "key": "Permissions-Policy", "value": "camera=(), microphone=(), geolocation=()" }
      ]
    }
  ],
  "redirects": [
    { "source": "/admin", "destination": "/admin/", "permanent": true }
  ]
}
```

### 17.5 Scenarios

| Scenario | Handling |
|---|---|
| Build fails | Vercel keeps previous deployment live; team notified |
| MongoDB Atlas connection fails during build | Build retries 3x; check Atlas network allowlist includes Vercel IPs |
| Vercel serverless cold start | Use Vercel's "Always On" for critical functions (Pro plan) |
| High traffic spike (game launch day) | Vercel auto-scales; ISR cache absorbs most reads; MongoDB Atlas auto-scales read capacity |
| Preview deployment for content review | Editors use preview URLs to review draft content before publishing |
| Rollback needed | Vercel instant rollback to any previous deployment |
| Domain DNS migration | Update Squarespace nameservers → Vercel DNS; add CNAME for cdn.gamesaiyan.com → Bunny |

---

## 18. Analytics & Tracking

### 18.1 Analytics Stack

| Tool | Purpose | Privacy |
|---|---|---|
| **Vercel Analytics** | Core Web Vitals, page views, real-user monitoring | Privacy-friendly, no cookies |
| **Plausible Analytics** | Detailed traffic analytics, UTM tracking, goals | Privacy-friendly, no cookies, GDPR-compliant |
| **Google Search Console** | Search performance, indexing status, crawl errors | Server-side only |
| **Stripe Dashboard** | Revenue tracking, payment analytics | Server-side only |

### 18.2 Custom Events to Track

| Event | Tool | Trigger |
|---|---|---|
| `guide_view` | Plausible | Page load on guide page |
| `search_query` | Plausible | Search executed |
| `checklist_complete` | Plausible | User completes all steps |
| `spoiler_reveal` | Plausible | User clicks spoiler toggle |
| `theme_change` | Plausible | User changes theme/accent/density |
| `premium_purchase` | Plausible + Stripe | Successful checkout |
| `affiliate_click` | Plausible | Click on affiliate link |
| `ad_impression` | Ad network | Ad loaded and visible |

---

## 19. Testing Strategy

### 19.1 Testing Layers

| Layer | Tool | What to Test |
|---|---|---|
| **Unit Tests** | Vitest | Utility functions (seo.ts, reading-time.ts, bunny.ts), Zustand stores |
| **Component Tests** | Vitest + Testing Library | UI components (SpoilerToggle, Checklist, ThemeToggle) |
| **Integration Tests** | Playwright | Full page flows (search, guide reading, checkout) |
| **E2E Tests** | Playwright | Critical paths: homepage → game → guide → checklist |
| **Visual Regression** | Playwright screenshots | Layout consistency across themes and density modes |
| **Performance** | Lighthouse CI | Core Web Vitals on every PR |
| **Accessibility** | axe-core + manual | WCAG 2.1 AA compliance |
| **SEO Validation** | Custom script | Verify JSON-LD, meta tags, sitemap on every page type |

### 19.2 Critical Test Scenarios

| Scenario | Test Type | Expected |
|---|---|---|
| Search returns results for valid query | Integration | Results appear with game/guide info |
| Search returns empty for gibberish | Integration | "No results" message shown |
| Checklist persists across page reload | Component | localStorage state matches UI |
| Spoiler starts hidden, reveals on click | Component | Content invisible → visible on toggle |
| Dark→Light mode switch | Component | All CSS variables update, no FOUC |
| Premium guide shows paywall for non-purchasers | Integration | Only preview content visible |
| Premium guide shows full content for purchasers | Integration | All content visible, no paywall |
| Mobile navigation works | E2E | Hamburger → drawer → link → page loads |
| Sitemap includes all published guides | Integration | sitemap.xml has all expected URLs |
| JSON-LD is valid on guide page | Integration | Schema validated against schema.org |
| Page loads under 3s on 3G | Performance | LCP < 2.5s on simulated 3G |
| All images have alt text | Accessibility | axe-core passes |
| Keyboard navigation works | Accessibility | All interactive elements focusable + operable |
| Stripe webhook processes payment | Integration | Purchase record created, user gains access |
| ISR revalidation triggers on publish | Integration | Page content updates within revalidation window |

---

## 20. Phase-wise Rollout Plan

### Phase 1 — Foundation (Weeks 1–3)

| Task | Details | Deliverable |
|---|---|---|
| Project setup | Next.js 15 + Payload CMS 3 + MongoDB Atlas + Tailwind | Running dev environment |
| Design system | CSS variables, fonts, core components (Button, Card, Badge) | Storybook-like component showcase |
| Payload collections | Games, Guides, Media, Users, Categories, Tags, Authors | Admin panel with all collections |
| Bunny CDN integration | Upload pipeline, image component, URL helpers | Images served from cdn.gamesaiyan.com |
| Homepage | Hero, search bar, game grid (with seed data) | Functional homepage |
| Game hub page | Dynamic route, guide list, filtering | Functional game pages |

### Phase 2 — Core Content Experience (Weeks 4–6)

| Task | Details | Deliverable |
|---|---|---|
| Guide article page | Full layout, rich text rendering, sidebar TOC | Reading experience |
| Reading progress bar | Scroll-based progress indicator | Working component |
| Spoiler toggles | Collapsible spoiler blocks in rich text | Working component |
| Interactive checklists | Checkable steps with localStorage | Working component |
| Search | Atlas Search index, search API, search UI (⌘K) | Working search |
| SEO | Metadata API, JSON-LD, sitemap, robots | SEO-ready pages |
| Breadcrumbs | Dynamic breadcrumb navigation | Working component |

### Phase 3 — Extended Features (Weeks 7–9)

| Task | Details | Deliverable |
|---|---|---|
| Tier list pages | Interactive tier list display | Tier list reading |
| Build guide pages | Structured build data + display | Build guide reading |
| Browse/category page | Filterable game grid with platform/genre filters | Category browsing |
| Dark/Light mode | Theme provider, toggle, persistence | Theme switching |
| Accent color picker | 5 accent options, CSS variable swap | Accent customization |
| Layout density toggle | 3 density modes | Density switching |
| About page | Static page with team info | About page |
| Auth pages | Login, register, forgot password | Authentication |

### Phase 4 — Monetization & Polish (Weeks 10–12)

| Task | Details | Deliverable |
|---|---|---|
| Ad slot integration | Placeholder ad components, defined positions | Ad slots ready |
| Affiliate block | CMS block + frontend component | Affiliate links |
| Premium guides | Paywall gate, Stripe checkout, webhook | Premium purchases |
| Mobile optimization | Bottom sheets, swipe gestures, touch targets | Mobile-ready |
| Performance audit | Lighthouse CI, Core Web Vitals optimization | < 2.5s LCP |
| Accessibility audit | axe-core, keyboard nav, screen reader testing | WCAG AA |
| Content seeding | 5+ games, 20+ guides, 3+ tier lists | Launch content |
| Domain migration | DNS from Squarespace to Vercel, SSL | Live on gamesaiyan.com |

### Phase 5 — Post-Launch (Ongoing)

| Task | Details |
|---|---|
| Content pipeline | Regular guide publishing workflow |
| Ad network integration | Apply to Mediavine/Raptive once traffic threshold met |
| Comments system | Payload-based or third-party (Disqus alternative) |
| Newsletter | Email capture + weekly digest via Resend |
| Social sharing | OG images, Twitter cards, Discord embeds |
| PWA capabilities | Service worker for offline guide access |
| Internationalization | i18n framework setup for multi-language (if needed) |
| Video guides | YouTube embed integration, video player component |

---

## 21. Environment Variables Reference

```bash
# ══════════════════════════════════════════
# .env.local / Vercel Environment Variables
# ══════════════════════════════════════════

# ── Application ──
NEXT_PUBLIC_SERVER_URL=https://gamesaiyan.com          # or http://localhost:3000 locally
NEXT_PUBLIC_SITE_NAME=GameSaiyan
NODE_ENV=production

# ── MongoDB Atlas ──
MONGODB_URI=mongodb+srv://<user>:<password>@<cluster>.mongodb.net/gamesaiyan_production?retryWrites=true&w=majority

# ── Payload CMS ──
PAYLOAD_SECRET=<random-32-char-string>                 # Used for JWT signing, encryption
REVALIDATION_SECRET=<random-string>                    # For on-demand ISR revalidation

# ── Vercel ──
BLOB_READ_WRITE_TOKEN=<vercel-blob-token>              # For temporary media storage
VERCEL_URL=<auto-set-by-vercel>

# ── Bunny CDN ──
BUNNY_STORAGE_ZONE=gamesaiyan-media
BUNNY_STORAGE_PASSWORD=<bunny-storage-api-key>
BUNNY_STORAGE_HOSTNAME=storage.bunnycdn.com            # or region-specific hostname
NEXT_PUBLIC_BUNNY_CDN_URL=https://cdn.gamesaiyan.com

# ── Stripe ──
STRIPE_SECRET_KEY=sk_live_<...>
NEXT_PUBLIC_STRIPE_PUBLISHABLE_KEY=pk_live_<...>
STRIPE_WEBHOOK_SECRET=whsec_<...>

# ── Email (Resend) ──
RESEND_API_KEY=re_<...>
RESEND_FROM_EMAIL=noreply@gamesaiyan.com

# ── Analytics ──
NEXT_PUBLIC_PLAUSIBLE_DOMAIN=gamesaiyan.com
NEXT_PUBLIC_ADSENSE_ID=ca-pub-<...>                    # If using Google AdSense
GOOGLE_SITE_VERIFICATION=<verification-code>

# ── Optional ──
GOOGLE_INDEXING_API_KEY=<json-key>                     # For programmatic URL submission
DISCORD_WEBHOOK_URL=<webhook-url>                      # Deploy notifications
```

---

## Appendix: File-by-File Scaffold

Quick-start command to scaffold the entire project:

```bash
# 1. Create Next.js project with Payload CMS
npx create-payload-app@latest gamesaiyan \
  --template blank \
  --db mongodb \
  --package-manager pnpm

cd gamesaiyan

# 2. Install additional dependencies
pnpm add zustand stripe @stripe/stripe-js lucide-react \
  @payloadcms/storage-vercel-blob resend

# 3. Install dev dependencies
pnpm add -D @types/node @playwright/test vitest \
  @testing-library/react axe-core

# 4. Create directory structure
mkdir -p src/{collections,globals,blocks,components/{layout,ui,guide,tier-list,home,ads,premium,theme},hooks,lib,store,types,styles}
mkdir -p src/app/{\"(frontend)\"/{games/\"[gameSlug]\"/\"[guideSlug]\",tier-lists/\"[slug]\",builds/\"[slug]\",search,about,premium/\"[slug]\"},\"(auth)\"/{login,register,forgot-password},\"(payload)\"/admin/\"[[...segments]]\",api/{search,checklist,stripe-webhook,revalidate}}
mkdir -p public/{fonts,icons}
mkdir -p scripts

# 5. Copy font files to public/fonts/
# Download from Google Fonts and convert to woff2

# 6. Set up environment variables
cp .env.example .env.local
# Fill in all values from Section 21

# 7. Start development server
pnpm dev
```

---

## Appendix: Key Implementation Notes for Claude Code

When handing this project to Claude Code for implementation, keep these notes handy:

1. **Image paths:** All image `src` values in components should use Bunny CDN URLs. During development, use placeholder images. In production, all media goes through the Payload → Bunny pipeline described in Section 7.

2. **Type safety:** Run `pnpm payload generate:types` after any collection/global change. Import types from `@/types/payload-types.ts` everywhere.

3. **Rich text rendering:** Use Payload's Lexical serializer to convert rich text nodes to React components. Map custom blocks (Spoiler, Checklist, Tip, etc.) to their respective React components.

4. **Search implementation:** MongoDB Atlas Search requires the index to be created in the Atlas UI (cannot be created via code). Share the index definition from Section 6.5 with whoever provisions the database.

5. **Stripe webhooks:** In development, use Stripe CLI (`stripe listen --forward-to localhost:3000/api/stripe-webhook`) to test webhooks locally.

6. **ISR revalidation:** The `afterChange` hook in Guides collection calls `/api/revalidate` to invalidate stale pages. Ensure the `REVALIDATION_SECRET` matches between the hook and the API route.

7. **Font loading:** Self-hosted fonts must have correct `@font-face` declarations with `font-display: swap` and appropriate `unicode-range` for subsetting.

8. **Mobile-first:** All Tailwind classes should be written mobile-first (base → `md:` → `lg:` → `xl:`). Test on 375px viewport minimum.

9. **Accessibility:** Every interactive element needs `aria-label` or visible label, `role` where appropriate, keyboard operability (Enter + Space for buttons, Escape to close modals), and focus management.

10. **Error boundaries:** Every route segment should have an `error.tsx` boundary. Use Next.js's `notFound()` function for 404s.

---

*This document is the single source of truth for the GameSaiyan project. All implementation decisions should reference this blueprint. Update this document as decisions evolve.*
